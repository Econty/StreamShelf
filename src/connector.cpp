#include "platform.hpp"
#include <wincrypt.h>
#include "connector_install.hpp"
#include "browser_identity.hpp"
#include "pipe_io.hpp"
#include <sddl.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <fstream>
namespace ss {
// Identify the actual browser that launched the native host, independently of
// the extension's user-agent string. Chromium may insert cmd.exe in the chain.
static std::string browserProcessPath(HANDLE pipe) {
    ULONG pid = 0;
    if (!GetNamedPipeClientProcessId(pipe, &pid) || !pid) return {};
    Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!snapshot.h || snapshot.h == INVALID_HANDLE_VALUE) return {};
    std::map<DWORD, DWORD> parents;
    PROCESSENTRY32W item{}; item.dwSize = sizeof(item);
    if (!Process32FirstW(snapshot, &item)) return {};
    do { parents[item.th32ProcessID] = item.th32ParentProcessID; } while (Process32NextW(snapshot, &item));
    return browserFromAncestors(pid, parents, [](DWORD pid) -> std::wstring {
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
        if (process.h) {
            wchar_t path[32768]{}; DWORD length = 32768;
            if (QueryFullProcessImageNameW(process, 0, path, &length)) return std::wstring(path, length);
        }
        return {};
    });
}
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
static bool frameRead(HANDLE h, Json &out, bool overlapped = false) {
    uint32_t size = 0;
    auto read = [&](void* data, size_t count) {
        return overlapped ? pipeio::transfer(h, data, count, false)
                          : transfer(h, data, count, false);
    };
    if (!read(&size, 4) || size == 0 || size > MaxFrame)
        return false;
    std::string s(size, 0);
    if (!read(s.data(), size))
        return false;
    out = Json::parse(s, nullptr, false);
    return out.is_object();
}
static bool frameWrite(HANDLE h, const Json &j, bool overlapped = false) {
    auto s = j.dump();
    uint32_t n = uint32_t(s.size());
    auto write = [&](void* data, size_t count) {
        return overlapped ? pipeio::transfer(h, data, count, true, 5000)
                          : transfer(h, data, count, true);
    };
    return n > 0 && n <= MaxFrame && write(&n, 4) && write(s.data(), n);
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
#ifdef STREAMSHELF_UI_PREVIEW
    return L"\\\\.\\pipe\\StreamShelfPreview-" + userSid() + L"-" + std::to_wstring(session);
#else
    return L"\\\\.\\pipe\\StreamShelfNative-" + userSid() + L"-" + std::to_wstring(session);
#endif
}
void Session::send(const Json &j) {
    std::lock_guard lock(writeMutex);
    if (!alive || !frameWrite(pipe, j, true)) {
        alive = false;
        CancelIoEx(pipe, nullptr);
        cv.notify_all();
        throw std::runtime_error("Расширение не принимает команды или отключено. Подключите расширение заново.");
    }
}
void Session::verifyConnection(const std::string &requestToken) {
    static std::atomic_uint64_t serial{0};
    auto token = requestToken.empty() ? std::to_string(GetTickCount64()) + "-" + std::to_string(++serial) : requestToken;
    { std::lock_guard lock(mutex); verificationToken = token; verificationSent = GetTickCount64(); }
    send({{"type", "ping"}, {"requestId", token}});
}
bool Session::acceptVerification(const std::string &token) {
    std::lock_guard lock(mutex);
    if (verificationToken.empty() || token != verificationToken) return false;
    verificationToken.clear(); lastPong = GetTickCount64(); verified = true; return true;
}
void Session::acceptCredentialReply(const Json &reply) {
    std::lock_guard lock(mutex);
    auto request = str(reply, "requestId");
    if (pendingCredentials.count(request)) {
        replies.insert_or_assign(request, reply);
        cv.notify_all();
    }
}
BrowserSource Session::source() {
    std::lock_guard lock(mutex);
    return {tab, sourceGeneration, url, store, extensionSource, credentialRevision};
}
bool Session::sourceMatches(const BrowserSource &expected) {
    std::lock_guard lock(mutex);
    return alive && scopedCredentialsSupported && expected.valid() &&
        expected == BrowserSource{tab, sourceGeneration, url, store, extensionSource};
}
void Session::updateTab(const Json &j) {
    std::lock_guard lock(mutex);
    auto newTab = int(number(j, "tabId", -1));
    auto newUrl = str(j, "url"), newStore = str(j, "storeId");
    std::optional<uint64_t> newSource;
    if (j.contains("sourceId") && j["sourceId"].is_number_unsigned()) newSource = j["sourceId"].get<uint64_t>();
    else if (j.contains("sourceId") && j["sourceId"].is_number_integer() && j["sourceId"].get<int64_t>() >= 0) newSource = uint64_t(j["sourceId"].get<int64_t>());
    if (tab != newTab || url != newUrl || store != newStore || extensionSource != newSource) {
        ++sourceGeneration; ++credentialEpoch; credentialSnapshots.clear(); cv.notify_all();
    }
    std::optional<uint64_t> revision;
    if (j.contains("credentialRevision") && j["credentialRevision"].is_number_unsigned()) revision = j["credentialRevision"].get<uint64_t>();
    if (credentialRevision != revision) { credentialSnapshots.clear(); ++credentialEpoch; cv.notify_all(); }
    credentialRevision = revision;
    tab = newTab; url = std::move(newUrl); store = std::move(newStore); extensionSource = newSource;
    title = str(j, "title");
}
void Session::updateCredentials(const Json &j) {
    std::lock_guard lock(mutex);
    if (int(number(j, "tabId", -1)) != tab || str(j, "storeId") != store || !extensionSource ||
        !j.contains("sourceId") || !j["sourceId"].is_number_unsigned() || j["sourceId"].get<uint64_t>() != *extensionSource ||
        !j.contains("credentialRevision") || !j["credentialRevision"].is_number_unsigned()) return;
    const auto revision = j["credentialRevision"].get<uint64_t>();
    if (credentialRevision && revision <= *credentialRevision) return;
    credentialRevision = revision; ++credentialEpoch; credentialSnapshots.clear(); cv.notify_all();
}
void Session::closeTab() {
    std::lock_guard lock(mutex);
    ++sourceGeneration; ++credentialEpoch; credentialSnapshots.clear(); tab = -1; url.clear(); store.clear(); extensionSource.reset(); credentialRevision.reset(); cv.notify_all();
}
std::optional<Session::CredentialSnapshot> Session::pinCredentials(const std::string &url, const BrowserSource &expected) {
    std::lock_guard lock(mutex);
    const auto found = credentialSnapshots.find(url);
    if (!alive || !expected.credentialRevision || found == credentialSnapshots.end() || found->second.owner != instanceSerial ||
        found->second.source != expected || found->second.source.credentialRevision != credentialRevision ||
        expected.credentialRevision != credentialRevision) return {};
    return found->second;
}
namespace {
std::string protectSnapshot(const Json &value) {
    auto text = value.dump();
    DATA_BLOB input{DWORD(text.size()), reinterpret_cast<BYTE *>(text.data())}, output{};
    const bool ok = CryptProtectData(&input, L"StreamShelf credential snapshot", nullptr, nullptr, nullptr,
        CRYPTPROTECT_UI_FORBIDDEN, &output) != FALSE;
    if (!text.empty()) SecureZeroMemory(text.data(), text.size());
    if (!ok) throw std::runtime_error("Не удалось защитить снимок доступа");
    std::string result(reinterpret_cast<char *>(output.pbData), output.cbData);
    SecureZeroMemory(output.pbData, output.cbData); LocalFree(output.pbData); return result;
}
}
Json Session::credentials(const std::string &url, std::atomic_bool &cancel) {
    return credentials(url, source(), cancel);
}
Json Session::credentials(const std::string &url, const BrowserSource &expected, std::atomic_bool &cancel, CredentialSnapshot *pinned) {
    if (cancel) throw std::runtime_error("Отменено");
    static std::atomic_uint64_t serial{0};
    std::string request = std::to_string(++serial);
    uint64_t epoch;
    {
        std::lock_guard l(mutex);
        if (!alive || !scopedCredentialsSupported || !expected.valid() || expected != BrowserSource{tab, sourceGeneration, this->url, store, extensionSource}) throw BrowserSourceChanged();
        if (expected.credentialRevision != credentialRevision) throw BrowserCredentialsRefreshPending();
        epoch = credentialEpoch;
        // A snapshot belongs to an in-flight transfer, not to a future request.
        // Even a live revision can lag a browser cookie event, so always ask the
        // extension for a fresh authoritative jar before another network attempt.
    }
    if (cancel) throw std::runtime_error("Отменено");
    { std::lock_guard lock(mutex); pendingCredentials.insert(request); }
    struct PendingGuard {
        Session &session; const std::string &request;
        ~PendingGuard() { std::lock_guard lock(session.mutex); session.pendingCredentials.erase(request); session.replies.erase(request); }
    } guard{*this, request};
    for (int attempt = 0; attempt < 2; ++attempt) {
        try {
            Json command{{"type", "credentials"}, {"requestId", request}, {"tabId", expected.tab}, {"sourceId", *expected.extensionSource}, {"storeId", expected.store}, {"url", url}};
            if (expected.credentialRevision) command["credentialRevision"] = *expected.credentialRevision;
            send(command);
        } catch (...) {
            if (cancel) throw;
            // Browser-bound tasks must not silently fall back to anonymous I/O
            // when the channel fails before we receive an authoritative reply.
            throw BrowserSourceChanged();
        }
        std::unique_lock l(mutex);
        auto until = std::chrono::steady_clock::now() + std::chrono::seconds(25);
        while (alive && !cancel && !replies.count(request) && expected == BrowserSource{tab, sourceGeneration, this->url, store, extensionSource} && epoch == credentialEpoch) {
            if (cv.wait_for(l, std::chrono::milliseconds(100)) == std::cv_status::timeout &&
                std::chrono::steady_clock::now() > until)
                break;
        }
        if (cancel)
            throw std::runtime_error("Отменено");
        if (!alive || !scopedCredentialsSupported || expected != BrowserSource{tab, sourceGeneration, this->url, store, extensionSource}) throw BrowserSourceChanged();
        if (expected.credentialRevision != credentialRevision || epoch != credentialEpoch) throw BrowserCredentialsRefreshPending();
        if (!replies.count(request)) {
            throw BrowserSourceChanged();
        }
        auto result = replies[request];
        replies.erase(request);
        // A refusal can be caused by a changed account before the asynchronous
        // tabSelected update reaches us. Never substitute a stale snapshot or
        // let the caller retry anonymously in that window.
        const auto message = str(result, "message");
        // Legacy 1.0.5 increments a selection counter when the same tab gains
        // focus. Retry that narrow refusal once, asking the extension again;
        // never substitute cached credentials after an explicit refusal.
        if (attempt == 0 && !expected.credentialRevision && str(result, "type") == "error" &&
            (message == "Вкладка изменилась" || message == "The tab changed")) {
            credentialSnapshots.erase(url); if (pinned) *pinned = {};
            pendingCredentials.erase(request);
            request = std::to_string(++serial);
            pendingCredentials.insert(request);
            continue;
        }
        auto refuse = [&] { credentialSnapshots.erase(url); if (pinned) *pinned = {}; };
        if (str(result, "type") != "credentialsResult" || !result.contains("cookies") || !result["cookies"].is_array() ||
            !result.contains("headers") || !result["headers"].is_object()) {
            refuse();
            if (str(result, "type") == "error" &&
                (message == "Данные доступа обновились. Повторите запрос." || message == "Credentials were refreshed. Retry the request."))
                throw BrowserCredentialsRefreshPending();
            if (str(result, "code") == "credentials_changed" || message == "Доступ к сайту изменился" || message == "Site credentials changed")
                throw BrowserCredentialsChanged();
            throw BrowserSourceChanged();
        }
        if (str(result, "url") != url)
            { refuse(); throw BrowserSourceChanged(); }
        if (int(number(result, "tabId", -1)) != expected.tab || !result.contains("sourceId") || !result["sourceId"].is_number_unsigned() || result["sourceId"].get<uint64_t>() != *expected.extensionSource || str(result, "storeId") != expected.store) { refuse(); throw BrowserSourceChanged(); }
        if (expected.credentialRevision && (!result.contains("credentialRevision") || !result["credentialRevision"].is_number_unsigned() ||
            result["credentialRevision"].get<uint64_t>() != *expected.credentialRevision)) { refuse(); throw BrowserCredentialsChanged(); }
        // Validate the entire representation before remembering it. Cookies
        // remain domain/path/Secure scoped and a newer authoritative jar replaces
        // the old jar, including cookies removed by the site.
        try { (void)cookies(result["cookies"]); }
        catch (...) { refuse(); throw BrowserCookieFormatUnsupported(); }
        if (pinned && !expected.credentialRevision)
            *pinned = {std::make_shared<std::string>(protectSnapshot(result)), expected, instanceSerial, GetTickCount64()};
        if (expected.credentialRevision) {
            auto blob = std::make_shared<std::string>(protectSnapshot(result));
            if (auto previous = credentialSnapshots.find(url); previous != credentialSnapshots.end() &&
                previous->second.owner == instanceSerial && previous->second.source == expected &&
                previous->second.source.credentialRevision == credentialRevision && previous->second.protectedData) {
                *previous->second.protectedData = std::move(*blob);
                blob = previous->second.protectedData;
            }
            CredentialSnapshot snapshot{std::move(blob), expected, instanceSerial, GetTickCount64()};
            credentialSnapshots.insert_or_assign(url, snapshot);
            if (pinned) *pinned = std::move(snapshot);
            else snapshot = {};
            // Pins keep their cache slot so a subsequent authoritative response
            // updates every queued/active job. Only unreferenced slots are evicted.
            for (;;) {
                size_t unused = 0;
                auto oldest = credentialSnapshots.end();
                for (auto it = credentialSnapshots.begin(); it != credentialSnapshots.end(); ++it) {
                    if (!it->second.protectedData || it->second.protectedData.use_count() != 1) continue;
                    ++unused;
                    if (oldest == credentialSnapshots.end() || it->second.captured < oldest->second.captured) oldest = it;
                }
                if (unused <= 128) break;
                credentialSnapshots.erase(oldest);
            }
        }
        return result;
    }
    throw BrowserSourceChanged();
}
void Connector::start() {
    acceptThread = std::thread([this] {
        acceptId = GetCurrentThreadId();
        try {
        bool firstInstance=true;
        while (!stopping) {
            PSECURITY_DESCRIPTOR sd = nullptr;
            auto acl = L"D:P(A;;GA;;;" + userSid() + L")";
            if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(), SDDL_REVISION_1, &sd,
                                                                      nullptr))
                throw std::runtime_error("Не удалось подготовить права канала расширения");
            SECURITY_ATTRIBUTES sa{sizeof(sa), sd, FALSE};
            HANDLE p =
                CreateNamedPipeW(pipeName().c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | (firstInstance ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0),
                                 PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                 16, MaxFrame, MaxFrame, 0, &sa);
            LocalFree(sd);
            if (p == INVALID_HANDLE_VALUE)
                throw std::runtime_error("Не удалось создать канал расширения (код " + std::to_string(GetLastError()) + ")");
            firstInstance=false;
            BOOL connected = pipeio::connect(p, stopping);
            if (!connected || stopping) {
                CloseHandle(p);
                continue;
            }
            auto s = std::make_shared<Session>();
            s->pipe = p;
            s->browserExecutable = browserProcessPath(p);
            {
                std::lock_guard l(mutex);
                sessions.push_back(s);
                readers.emplace_back([this, s] {
                    s->readerId = GetCurrentThreadId();
                    try {
                        if (stopping)
                            throw std::runtime_error("Stopped");
                        Json j;
                        if (!frameRead(s->pipe, j, true) || str(j, "type") != "hello" ||
                            number(j, "protocol") != 1)
                            throw std::runtime_error("Несовместимое расширение");
                        if (!compatibleExtensionHello(j)) {
                            if (message) message(nullptr, {{"type", "connectorError"}, {"message", "Установлено устаревшее расширение StreamShelf Connector. Установите текущую версию и перезагрузите расширение в браузере."}});
                            throw std::runtime_error("Устаревшее расширение");
                        }
                        {
                            std::lock_guard sl(s->mutex);
                            s->id = str(j, "instanceId");
                            s->browser = str(j, "browser");
                        }
                        if (j.contains("capabilities") && j["capabilities"].is_array())
                            for (const auto &capability : j["capabilities"])
                                if (capability.is_string()) {
                                    if (capability.get<std::string>() == "language") s->languageSupported = true;
                                    if (capability.get<std::string>() == "scopedCredentials") s->scopedCredentialsSupported = true;
                                }
                        s->send({{"type", "helloAck"}, {"protocol", 1}, {"version", ConnectorVersion}});
                        s->verifyConnection();
                        if (message)
                            message(s, j);
                        while (!stopping && frameRead(s->pipe, j, true)) {
                            std::string type = str(j, "type");
                            if (type == "pong") {
                                if (s->acceptVerification(str(j, "requestId")) && message) message(s, j);
                                continue;
                            }
                            if (type == "credentialsResult" || (type == "error" && j.contains("requestId"))) {
                                s->acceptCredentialReply(j);
                            } else {
                                if (type == "tabSelected") s->updateTab(j);
                                if (type == "credentialsChanged") s->updateCredentials(j);
                                if (type == "tabClosed") s->closeTab();
                                if (message)
                                    message(s, j);
                            }
                        }
                    } catch (...) {
                    }
                    s->alive = false;
                    s->closeTab();
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
        } catch (const std::exception &e) {
            if (!stopping && message) message(nullptr, {{"type","connectorError"},{"message",e.what()}});
        } catch (...) {
            if (!stopping && message) message(nullptr, {{"type","connectorError"},{"message","Ошибка канала расширения"}});
        }
        acceptEnded = true;
    });
}
std::vector<std::shared_ptr<Session>> Connector::list() {
    std::lock_guard l(mutex);
    for (size_t i=0; i<sessions.size();) {
        if (sessions[i]->readerEnded) {
            if (readers[i].joinable()) readers[i].join();
            sessions.erase(sessions.begin()+i);
            readers.erase(readers.begin()+i);
        } else ++i;
    }
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
    if (acceptThread.joinable()) acceptThread.join();
    for (auto &s : sessions) {
        s->alive = false;
        s->cv.notify_all();
        std::lock_guard lock(s->writeMutex);
        if (s->pipe != INVALID_HANDLE_VALUE) CancelIoEx(s->pipe, nullptr);
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
void updateConnectorExtensions(const fs::path &app) {
    // Load Chromium directly from the application bundle, so the wizard's path
    // and the files that will be loaded by the browser are always the same.
    auto source = app.parent_path() / L"extensions" / L"chromium-development";
    for (auto name : {L"manifest.json", L"background.js", L"content.js", L"popup.html", L"popup.js"})
        if (!fs::is_regular_file(source / name))
            throw std::runtime_error("В папке программы отсутствует файл расширения Chromium: " + utf8((source / name).wstring()));
}
void installConnector(const fs::path &app) {
    updateConnectorExtensions(app);
    auto dir = dataRoot() / L"connector";
    const auto host = stageConnectorHost(app, dir);
    writeFile(dir / L"app-path.txt", utf8(app.wstring()));
    Json base = {{"name", "dev.streamshelf.connector"},
                 {"description", "StreamShelf C++ Native Messaging"},
                 {"path", utf8(host.wstring())},
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
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE), output = GetStdHandle(STD_OUTPUT_HANDLE);
    Json first;
    if (!frameRead(input, first) || str(first, "type") != "hello") return 6;
    auto name = pipeName();
    HANDLE p = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (p == INVALID_HANDLE_VALUE) {
        if (!first.value("allowLaunch", true)) return 5;
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
            p = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED,
                            nullptr);
        }
    }
    if (p == INVALID_HANDLE_VALUE)
        return 5;
    Handle pipe(p);
    if (!frameWrite(pipe, first, true)) return 5;
    std::atomic_bool ended{false}, quit{false};
    std::atomic<DWORD> inboundId{0};
    std::thread inbound([&] {
        inboundId = GetCurrentThreadId();
        Json j;
        while (!quit && frameRead(input, j) && frameWrite(pipe, j, true)) {
        }
        ended = true;
    });
    Json j;
    while (!ended) {
        DWORD n = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &n, nullptr))
            break;
        if (n >= 4) {
            if (!frameRead(pipe, j, true) || !frameWrite(output, j))
                break;
        } else
            Sleep(5);
    }

    quit = true;
    CancelIoEx(pipe, nullptr);
    while (!ended) {
        cancelThread(inboundId);
        Sleep(1);
    }
    inbound.join();
    return 0;
}
} // namespace ss
