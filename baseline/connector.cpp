#include "platform.hpp"
#include <sddl.h>
#include <shellapi.h>
#include <fstream>
namespace ss {
static void cancelThread(DWORD id) {
    if (!id)
        return;
    Handle h(OpenThread(THREAD_TERMINATE, FALSE, id));
    if (h.h)
        CancelSynchronousIo(h);
}

static bool transfer(HANDLE h, void *p, size_t size, bool write) {
    auto *b = (BYTE *)p;
    while (size) {
        DWORD n = 0;
        BOOL ok =
            write ? WriteFile(h, b, DWORD(size), &n, nullptr) : ReadFile(h, b, DWORD(size), &n, nullptr);
        if (!ok || !n)
            return false;
        b += n;
        size -= n;
    }
    return true;
}
static bool frameRead(HANDLE h, Json &out) {
    uint32_t size = 0;
    if (!transfer(h, &size, 4, false) || size == 0 || size > MaxFrame)
        return false;
    std::string s(size, 0);
    if (!transfer(h, s.data(), size, false))
        return false;
    out = Json::parse(s, nullptr, false);
    return out.is_object();
}
static bool frameWrite(HANDLE h, const Json &j) {
    auto s = j.dump();
    uint32_t n = uint32_t(s.size());
    return n > 0 && n <= MaxFrame && transfer(h, &n, 4, true) && transfer(h, s.data(), n, true);
}
static std::wstring userSid() {
    Handle token;
    HANDLE t = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t))
        throw std::runtime_error("Token");
    token.h = t;
    DWORD n = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &n);
    std::vector<BYTE> b(n);
    if (!GetTokenInformation(token, TokenUser, b.data(), n, &n))
        throw std::runtime_error("SID");
    PWSTR s = nullptr;
    if (!ConvertSidToStringSidW(((TOKEN_USER *)b.data())->User.Sid, &s))
        throw std::runtime_error("SID string");
    std::wstring out = s;
    LocalFree(s);
    return out;
}
std::wstring pipeName() {
    DWORD session = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &session);
    return L"\\\\.\\pipe\\StreamShelfNative-" + userSid() + L"-" + std::to_wstring(session);
}
void Session::send(const Json &j) {
    std::lock_guard lock(writeMutex);
    if (!alive || !frameWrite(pipe, j))
        throw std::runtime_error("Расширение отключено");
}
Json Session::credentials(const std::string &url, std::atomic_bool &cancel) {
    static std::atomic_uint64_t serial{0};
    std::string request = std::to_string(++serial);
    int selected;
    {
        std::lock_guard l(mutex);
        selected = tab;
    }
    send({{"type", "credentials"}, {"requestId", request}, {"tabId", selected}, {"url", url}});
    std::unique_lock l(mutex);
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(25);
    while (alive && !cancel && !replies.count(request)) {
        if (cv.wait_for(l, std::chrono::milliseconds(100)) == std::cv_status::timeout &&
            std::chrono::steady_clock::now() > until)
            throw std::runtime_error("Расширение не ответило на запрос cookies");
    }
    if (cancel)
        throw std::runtime_error("Отменено");
    if (!replies.count(request))
        throw std::runtime_error("Браузерная сессия закрыта");
    auto result = replies[request];
    replies.erase(request);
    if (str(result, "type") == "error")
        throw std::runtime_error(str(result, "message", "Ошибка расширения"));
    if (str(result, "url") != url)
        throw std::runtime_error("Адрес снимка cookies не совпал");
    return result;
}
void Connector::start() {
    acceptThread = std::thread([this] {
        acceptId = GetCurrentThreadId();
        while (!stopping) {
            PSECURITY_DESCRIPTOR sd = nullptr;
            auto acl = L"D:P(A;;GA;;;" + userSid() + L")";
            if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(), SDDL_REVISION_1, &sd,
                                                                      nullptr))
                break;
            SECURITY_ATTRIBUTES sa{sizeof(sa), sd, FALSE};
            HANDLE p =
                CreateNamedPipeW(pipeName().c_str(), PIPE_ACCESS_DUPLEX,
                                 PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                 16, MaxFrame, MaxFrame, 0, &sa);
            LocalFree(sd);
            if (p == INVALID_HANDLE_VALUE)
                break;
            BOOL connected = ConnectNamedPipe(p, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED;
            if (!connected || stopping) {
                CloseHandle(p);
                continue;
            }
            auto s = std::make_shared<Session>();
            s->pipe = p;
            {
                std::lock_guard l(mutex);
                sessions.push_back(s);
                readers.emplace_back([this, s] {
                    s->readerId = GetCurrentThreadId();
                    try {
                        if (stopping)
                            throw std::runtime_error("Stopped");
                        Json j;
                        if (!frameRead(s->pipe, j) || str(j, "type") != "hello" ||
                            number(j, "protocol") != 1 || str(j, "version") != "1.0.0")
                            throw std::runtime_error("Несовместимое расширение");
                        {
                            std::lock_guard sl(s->mutex);
                            s->id = str(j, "instanceId");
                            s->browser = str(j, "browser");
                        }
                        s->send({{"type", "helloAck"}, {"protocol", 1}, {"version", "1.0.0"}});
                        if (message)
                            message(s, j);
                        while (!stopping && frameRead(s->pipe, j)) {
                            std::string type = str(j, "type");
                            if (type == "credentialsResult" || (type == "error" && j.contains("requestId"))) {
                                std::lock_guard l(s->mutex);
                                if (s->replies.size() < 100)
                                    s->replies[str(j, "requestId")] = j;
                                s->cv.notify_all();
                            } else {
                                if (type == "tabSelected") {
                                    std::lock_guard l(s->mutex);
                                    s->tab = int(number(j, "tabId", -1));
                                    s->url = str(j, "url");
                                    s->title = str(j, "title");
                                }
                                if (type == "tabClosed") {
                                    std::lock_guard l(s->mutex);
                                    s->tab = -1;
                                    s->url.clear();
                                }
                                if (message)
                                    message(s, j);
                            }
                        }
                    } catch (...) {
                    }
                    s->alive = false;
                    s->cv.notify_all();
                    {
                        std::lock_guard l(s->writeMutex);
                        CloseHandle(s->pipe);
                        s->pipe = INVALID_HANDLE_VALUE;
                    }
                    if (message)
                        message(s, {{"type", "disconnected"}});
                    s->readerEnded = true;
                });
            }
        }
        acceptEnded = true;
    });
}
std::vector<std::shared_ptr<Session>> Connector::list() {
    std::lock_guard l(mutex);
    std::vector<std::shared_ptr<Session>> out;
    for (auto &s : sessions) {
        std::lock_guard sl(s->mutex);
        if (s->alive && !s->id.empty())
            out.push_back(s);
    }
    return out;
}
Connector::~Connector() {
    stop();
}
void Connector::stop() {
    if (stopping.exchange(true))
        return;
    if (acceptThread.joinable()) {
        while (!acceptEnded) {
            cancelThread(acceptId);
            Handle wake(CreateFileW(pipeName().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                    OPEN_EXISTING, 0, nullptr));
            Sleep(1);
        }
        acceptThread.join();
    }
    for (auto &s : sessions) {
        s->alive = false;
        s->cv.notify_all();
        while (!s->readerEnded) {
            cancelThread(s->readerId);
            Sleep(1);
        }
    }
    for (auto &t : readers)
        if (t.joinable())
            t.join();
}
static void registry(const std::wstring &key, const fs::path &value) {
    HKEY h;
    auto err =
        RegCreateKeyExW(HKEY_CURRENT_USER, key.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &h, nullptr);
    if (err != ERROR_SUCCESS)
        throw std::runtime_error("Ошибка регистрации Native Messaging");
    auto s = value.wstring();
    err = RegSetValueExW(h, nullptr, 0, REG_SZ, (const BYTE *)s.c_str(),
                         DWORD((s.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(h);
    if (err != ERROR_SUCCESS)
        throw std::runtime_error("Ошибка записи реестра");
}
static std::vector<std::wstring> vendors() {
    return {L"Google\\Chrome", L"Microsoft\\Edge",       L"Chromium",      L"BraveSoftware\\Brave-Browser",
            L"Vivaldi",        L"Yandex\\YandexBrowser", L"Opera Software"};
}
void installConnector(const fs::path &app) {
    auto dir = dataRoot() / L"connector";
    fs::create_directories(dir);
    fs::copy_file(app, dir / L"StreamShelf.Host.exe", fs::copy_options::overwrite_existing);
    writeFile(dir / L"app-path.txt", utf8(app.wstring()));
    auto ext = app.parent_path() / L"extensions";
    if (!fs::exists(ext))
        ext = app.parent_path() / L"extension";
    for (auto family : {L"chromium", L"firefox"}) {
        auto dest = dir / L"extensions" / family;
        fs::create_directories(dest);
        fs::copy(ext / family, dest, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    }
    Json base = {{"name", "dev.streamshelf.connector"},
                 {"description", "StreamShelf C++ Native Messaging"},
                 {"path", utf8((dir / L"StreamShelf.Host.exe").wstring())},
                 {"type", "stdio"}};
    auto chrome = base;
    chrome["allowed_origins"] = {std::string("chrome-extension://") + ExtensionId + "/"};
    writeFile(dir / L"chromium-host.json", chrome.dump(2));
    auto firefox = base;
    firefox["allowed_extensions"] = {FirefoxId};
    writeFile(dir / L"firefox-host.json", firefox.dump(2));
    for (auto &v : vendors())
        registry(L"Software\\" + v + L"\\NativeMessagingHosts\\dev.streamshelf.connector",
                 dir / L"chromium-host.json");
    registry(L"Software\\Mozilla\\NativeMessagingHosts\\dev.streamshelf.connector",
             dir / L"firefox-host.json");
}
void unregisterConnector() {
    for (auto &v : vendors())
        RegDeleteTreeW(HKEY_CURRENT_USER,
                       (L"Software\\" + v + L"\\NativeMessagingHosts\\dev.streamshelf.connector").c_str());
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Mozilla\\NativeMessagingHosts\\dev.streamshelf.connector");
}
int nativeHostMain() {
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool origin = false;
    for (int i = 1; i < argc; i++)
        if (std::wstring(argv[i]) == L"chrome-extension://" + wide(ExtensionId) + L"/" ||
            std::wstring(argv[i]) == wide(FirefoxId))
            origin = true;
    LocalFree(argv);
    if (!origin)
        return 3;
    auto name = pipeName();
    HANDLE p = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (p == INVALID_HANDLE_VALUE) {
        try {
            auto app = wide(readFile(executable().parent_path() / L"app-path.txt", 65536));
            STARTUPINFOW si{sizeof(si)};
            PROCESS_INFORMATION pi{};
            auto cmd = quoteWide(app);
            if (CreateProcessW(app.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si,
                               &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
        } catch (...) {
            return 4;
        }
        for (int i = 0; i < 100 && p == INVALID_HANDLE_VALUE; i++) {
            Sleep(100);
            p = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0,
                            nullptr);
        }
    }
    if (p == INVALID_HANDLE_VALUE)
        return 5;
    Handle pipe(p);
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE), output = GetStdHandle(STD_OUTPUT_HANDLE);
    std::atomic_bool ended{false}, quit{false};
    std::atomic<DWORD> inboundId{0};
    std::thread inbound([&] {
        inboundId = GetCurrentThreadId();
        Json j;
        while (!quit && frameRead(input, j) && frameWrite(pipe, j)) {
        }
        ended = true;
    });
    Json j;
    while (!ended) {
        DWORD n = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &n, nullptr))
            break;
        if (n >= 4) {
            if (!frameRead(pipe, j) || !frameWrite(output, j))
                break;
        } else
            Sleep(5);
    }
    quit = true;
    while (!ended) {
        cancelThread(inboundId);
        Sleep(1);
    }
    inbound.join();
    return 0;
}
} // namespace ss
