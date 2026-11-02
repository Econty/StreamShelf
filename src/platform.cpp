#include "platform.hpp"
#include "extension_presence.hpp"
#include <winhttp.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <tlhelp32.h>
#include <sddl.h>
#include <fstream>
#include <iomanip>
#include <set>
#include <ctime>
namespace ss {
std::wstring wide(const std::string &s) {
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()), nullptr, 0);
    if (!n)
        throw std::runtime_error("Неверный UTF-8");
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), int(s.size()), w.data(), n);
    return w;
}
std::string utf8(const std::wstring &w) {
    if (w.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}
fs::path executable() {
    std::wstring w(32768, 0);
    DWORD n = GetModuleFileNameW(nullptr, w.data(), DWORD(w.size()));
    w.resize(n);
    return fs::path(w);
}
fs::path dataRoot() {
    PWSTR p = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p)))
        throw std::runtime_error("Нет LocalAppData");
    fs::path r = fs::path(p) / L"StreamShelfNative";
    CoTaskMemFree(p);
    fs::create_directories(r);
    return r;
}
std::string readFile(const fs::path &p, size_t limit) {
    std::ifstream f(p, std::ios::binary);
    if (!f)
        throw std::runtime_error("Не удалось прочитать файл");
    f.seekg(0, std::ios::end);
    auto n = f.tellg();
    if (n < 0 || uint64_t(n) > limit)
        throw std::runtime_error("Файл слишком большой");
    f.seekg(0);
    std::string s(size_t(n), '\0');
    if (!f.read(s.data(), n))
        throw std::runtime_error("Ошибка чтения файла");
    return s;
}
void writeFile(const fs::path &p, const std::string &s) {
    if (!p.parent_path().empty())
        fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f.write(s.data(), s.size()))
        throw std::runtime_error("Ошибка записи файла");
}
void protectFile(const fs::path &p, const std::string &s) {
    DATA_BLOB in{DWORD(s.size()), (BYTE *)s.data()}, out{};
    if (!CryptProtectData(&in, L"StreamShelfNative", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                          &out))
        throw std::runtime_error("Ошибка DPAPI");
    std::string v((char *)out.pbData, out.cbData);
    LocalFree(out.pbData);
    writeFile(p, v);
}
std::string unprotectFile(const fs::path &p) {
    auto s = readFile(p);
    DATA_BLOB in{DWORD(s.size()), (BYTE *)s.data()}, out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        throw std::runtime_error("DPAPI: другой пользователь или повреждённый файл");
    std::string v((char *)out.pbData, out.cbData);
    LocalFree(out.pbData);
    return v;
}
std::string sha256(const fs::path &p) {
    BCRYPT_ALG_HANDLE alg{};
    BCRYPT_HASH_HANDLE hash{};
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        throw std::runtime_error("SHA-256 недоступен");
    DWORD size = 0, n = 0;
    BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&size, sizeof(size), &n, 0);
    std::vector<UCHAR> object(size);
    if (BCryptCreateHash(alg, &hash, object.data(), size, nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        throw std::runtime_error("SHA-256 init");
    }
    std::ifstream f(p, std::ios::binary);
    char buf[65536];
    bool ok = bool(f);
    while (f.read(buf, sizeof(buf)) || f.gcount())
        if (BCryptHashData(hash, (PUCHAR)buf, ULONG(f.gcount()), 0) < 0)
            ok = false;
    UCHAR digest[32];
    if (BCryptFinishHash(hash, digest, 32, 0) < 0)
        ok = false;
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (!ok)
        throw std::runtime_error("SHA-256 read failed");
    std::ostringstream out;
    for (auto c : digest)
        out << std::hex << std::setw(2) << std::setfill('0') << int(c);
    return out.str();
}
ProcessResult runProcess(const fs::path &exe, const std::vector<std::wstring> &args, Log log,
                         std::atomic_bool &cancel, bool capture, bool shellCommand) {
    std::wstring cmd = quoteWide(exe.wstring());
    std::string display = utf8(exe.filename().wstring());
    if(shellCommand && !args.empty()) {
        // cmd.exe parses /c itself; CRT quoting around the entire compound
        // command makes it treat "chcp ... & command" as an executable name.
        cmd += L" /d /c " + args.back();
        display += " /d /c " + utf8(args.back());
    } else for (const auto &a:args) {
        cmd += L' ' + quoteWide(a);
        display += ' ' + utf8(a);
    }
    if (log)
        log(redact(display));
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE r = nullptr, w = nullptr, er = nullptr, ew = nullptr;
    if (!CreatePipe(&r, &w, &sa, 0))
        throw std::runtime_error("CreatePipe stdout");
    Handle read(r), write(w);
    if (!CreatePipe(&er, &ew, &sa, 0))
        throw std::runtime_error("CreatePipe stderr");
    Handle errorRead(er), errorWrite(ew);
    SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(er, HANDLE_FLAG_INHERIT, 0);
    Handle input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0,
                             nullptr));
    STARTUPINFOW si{sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = input;
    si.hStdOutput = w;
    si.hStdError = ew;
    PROCESS_INFORMATION pi{};
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    std::vector<BYTE> storage(bytes);
    STARTUPINFOEXW sx{};
    sx.StartupInfo = si;
    sx.StartupInfo.cb = sizeof(sx);
    sx.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)storage.data();
    if (!InitializeProcThreadAttributeList(sx.lpAttributeList, 1, 0, &bytes))
        throw std::runtime_error("Attribute list");
    HANDLE inherited[] = {w, ew, input.h};
    if (!UpdateProcThreadAttribute(sx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited,
                                   sizeof(inherited), nullptr, nullptr)) {
        DeleteProcThreadAttributeList(sx.lpAttributeList);
        throw std::runtime_error("Handle list");
    }
    BOOL started = CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, TRUE,
                                  CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr,
                                  nullptr, &sx.StartupInfo, &pi);
    DeleteProcThreadAttributeList(sx.lpAttributeList);
    if (!started)
        throw std::runtime_error("Не удалось запустить " + utf8(exe.filename().wstring()));
    Handle proc(pi.hProcess), thread(pi.hThread), job(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) ||
        !AssignProcessToJobObject(job, proc)) {
        TerminateProcess(proc, 1);
        throw std::runtime_error("Не удалось создать группу отмены процесса");
    }
    ResumeThread(thread);
    CloseHandle(write.release());
    CloseHandle(errorWrite.release());
    ProcessResult result{};
    std::string outLine, errLine;
    auto pump = [&](HANDLE pipe, std::string &line, std::string *stored) {
        DWORD available = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) || !available)
            return false;
        char buffer[8192];
        DWORD n = 0;
        if (!ReadFile(pipe, buffer, std::min<DWORD>(sizeof(buffer), available), &n, nullptr))
            return false;
        if (stored) {
            if (stored->size() + n > 64 * 1024 * 1024) {
                TerminateJobObject(job, 1);
                throw std::runtime_error("Ответ процесса слишком большой");
            }
            stored->append(buffer, n);
        }
        if (log)
            for (DWORD i = 0; i < n; i++) {
                char ch = buffer[i];
                if (ch == '\n' || ch == '\r') {
                    if (!line.empty())
                        log(redact(line));
                    line.clear();
                } else if (line.size() < 65536)
                    line += ch;
            }
        return n > 0;
    };
    for (;;) {
        if (cancel)
            TerminateJobObject(job, ERROR_CANCELLED);
        bool gotOut = pump(read, outLine, capture ? &result.output : nullptr),
             gotError = pump(errorRead, errLine, capture ? &result.error : nullptr);
        if (!gotOut && !gotError && WaitForSingleObject(proc, 10) == WAIT_OBJECT_0) {
            // Output can be written after the last peek but before process exit.
            // Drain both pipes after observing exit so short-lived tools such as
            // ffprobe cannot lose their entire JSON response.
            for (;;) {
                bool tailOut = pump(read, outLine, capture ? &result.output : nullptr);
                bool tailError = pump(errorRead, errLine, capture ? &result.error : nullptr);
                if (!tailOut && !tailError) break;
            }
            break;
        }
    }
    if (log) {
        if (!outLine.empty())
            log(redact(outLine));
        if (!errLine.empty())
            log(redact(errLine));
    }
    WaitForSingleObject(proc, INFINITE);
    GetExitCodeProcess(proc, &result.code);
    return result;
}
struct Internet {
    HINTERNET h{};
    Internet(HINTERNET v = nullptr) : h(v) {}
    ~Internet() {
        if (h)
            WinHttpCloseHandle(h);
    }
    operator HINTERNET() const {
        return h;
    }
};
static void fetch(const std::string &url, size_t limit,
                  const std::function<void(const char *, size_t, uint64_t)> &sink, std::atomic_bool *cancel,
                  const Json &headers, const std::string &cookieText) {
    if (!httpUrl(url))
        throw std::runtime_error("Нужен HTTP/HTTPS URL без логина в адресе");
    auto u = wide(url.substr(0, url.find('#')));
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = DWORD(-1);
    if (!WinHttpCrackUrl(u.c_str(), 0, 0, &parts))
        throw std::runtime_error("Некорректный URL");
    std::wstring host(parts.lpszHostName, parts.dwHostNameLength),
        path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength)
        path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    Internet session(
        WinHttpOpen(L"StreamShelfNative/2.2", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0));
    if (!session.h)
        throw std::runtime_error("WinHTTP init");
    WinHttpSetTimeouts(session, 15000, 15000, 20000, 30000);
    Internet connection(WinHttpConnect(session, host.c_str(), parts.nPort, 0));
    Internet request(WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                        WINHTTP_DEFAULT_ACCEPT_TYPES,
                                        parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0));
    if (!request.h)
        throw std::runtime_error("WinHTTP request");
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
    std::wstring extra = L"Accept: */*\r\n";
    for (auto &[k, v] : headers.items()) {
        std::string l = k;
        std::transform(l.begin(), l.end(), l.begin(), ::tolower);
        if (l != "user-agent" && l != "referer" && l != "authorization" && l != "origin")
            continue;
        auto value = v.is_string() ? v.get<std::string>() : "";
        if (value.find_first_of("\r\n") != value.npos)
            continue;
        extra += wide(k + ": " + value + "\r\n");
    }
    if (!cookieText.empty()) {
        auto now = std::time(nullptr);
        std::istringstream in(cookieText);
        std::string line, all;
        std::string h = utf8(host);
        std::transform(h.begin(), h.end(), h.begin(), ::tolower);
        std::string requestPath = utf8(path);
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.rfind("#HttpOnly_", 0) == 0)
                line.erase(0, 10);
            if (line.empty() || line[0] == '#')
                continue;
            std::vector<std::string> f;
            std::istringstream row(line);
            std::string part;
            while (std::getline(row, part, '\t'))
                f.push_back(part);
            if (f.size() != 7)
                continue;
            auto d = f[0];
            bool sub = f[1] == "TRUE";
            if (!d.empty() && d[0] == '.')
                d.erase(0, 1);
            std::transform(d.begin(), d.end(), d.begin(), ::tolower);
            bool domain =
                h == d || (sub && h.size() > d.size() && h.compare(h.size() - d.size(), d.size(), d) == 0 &&
                           h[h.size() - d.size() - 1] == '.');
            bool pp = requestPath.rfind(f[2], 0) == 0 &&
                      (f[2].empty() || f[2].back() == '/' || requestPath.size() == f[2].size() ||
                       requestPath[f[2].size()] == '/' || requestPath[f[2].size()] == '?');
            long long expiry = 0;
            try {
                expiry = std::stoll(f[4]);
            } catch (...) {
                continue;
            }
            if (domain && pp && (f[3] != "TRUE" || parts.nScheme == INTERNET_SCHEME_HTTPS) &&
                (expiry == 0 || expiry > now)) {
                if (!all.empty())
                    all += "; ";
                all += f[5] + "=" + f[6];
            }
        }
        if (!all.empty())
            extra += wide("Cookie: " + all + "\r\n");
    }
    if (!WinHttpSendRequest(request, extra.c_str(), DWORD(extra.size()), nullptr, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, nullptr))
        throw std::runtime_error("Ошибка HTTPS-запроса");
    DWORD status = 0, n = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &n,
                        nullptr);
    if (status >= 300 && status < 400) {
        wchar_t loc[32768];
        n = sizeof(loc);
        if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, nullptr, loc, &n, nullptr))
            throw std::runtime_error("Перенаправление без Location");
        static thread_local int depth = 0;
        if (depth >= 5)
            throw std::runtime_error("Слишком много перенаправлений");
        std::string next = utf8(loc);
        if (!httpUrl(next)) {
            std::wstring combined(65536, L'\0');
            DWORD length = DWORD(combined.size());
            if (FAILED(UrlCombineW(u.c_str(), loc, combined.data(), &length, 0)))
                throw std::runtime_error("Некорректное перенаправление");
            combined.resize(length);
            next = utf8(combined);
        }
        // Sensitive headers are never forwarded. Cookies are reselected for the destination.
        if (parts.nScheme == INTERNET_SCHEME_HTTPS && next.rfind("http://", 0) == 0)
            throw std::runtime_error("Небезопасное перенаправление HTTPS → HTTP");
        depth++;
        try {
            fetch(next, limit, sink, cancel, Json::object(), cookieText);
        } catch (...) {
            depth--;
            throw;
        }
        depth--;
        return;
    }
    if (status < 200 || status >= 300)
        throw std::runtime_error("HTTP " + std::to_string(status));
    wchar_t length[64];
    n = sizeof(length);
    uint64_t total = 0;
    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH, nullptr, length, &n, nullptr))
        total = _wcstoui64(length, nullptr, 10);
    size_t received = 0;
    char buffer[65536];
    for (;;) {
        if (cancel && *cancel)
            throw std::runtime_error("Отменено");
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer, sizeof(buffer), &read))
            throw std::runtime_error("Обрыв соединения");
        if (!read)
            break;
        if (read > limit || received > limit - read)
            throw std::runtime_error("Превышен размер ответа");
        received += read;
        sink(buffer, read, total);
    }
}
std::string httpPreview(const std::string &url, std::atomic_bool &cancel, const Json &headers, const std::string &cookieText) {
    std::string result;
    fetch(url,8*1024*1024,[&](const char* data,size_t size,uint64_t){result.append(data,size);},&cancel,headers,cookieText);
    return result;
}
std::string httpGet(const std::string &url, size_t limit) {
    std::string result;
    fetch(
        url, limit, [&](const char *p, size_t n, uint64_t) { result.append(p, n); }, nullptr, Json::object(),
        "");
    return result;
}
void httpDownload(const std::string &url, const fs::path &p, std::atomic_bool &cancel,
                  std::function<void(double)> progress, const Json &headers, const std::string &cookieText,
                  std::function<void(uint64_t,uint64_t)> onBytes) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    uint64_t count = 0;
    fetch(
        url, SIZE_MAX,
        [&](const char *b, size_t n, uint64_t total) {
            if (!f.write(b, n))
                throw std::runtime_error("Не удалось записать файл");
            count += n;
            if(onBytes)onBytes(count,total);
            if (total) progress(100. * count / total);
        },
        &cancel, headers, cookieText);
}
void openPath(const fs::path &p) {
    ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
std::string pickFile(HWND owner, bool folder, bool save) {
    HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(init))
        throw std::runtime_error("COM init");
    IFileDialog *d = nullptr;
    HRESULT hr = CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d));
    std::string out;
    if (SUCCEEDED(hr)) {
        DWORD options;
        d->GetOptions(&options);
        d->SetOptions(options | FOS_FORCEFILESYSTEM | (folder ? FOS_PICKFOLDERS : 0));
        hr = d->Show(owner);
        if (SUCCEEDED(hr)) {
            IShellItem *i = nullptr;
            if (SUCCEEDED(d->GetResult(&i))) {
                PWSTR p = nullptr;
                if (SUCCEEDED(i->GetDisplayName(SIGDN_FILESYSPATH, &p))) {
                    out = utf8(p);
                    CoTaskMemFree(p);
                }
                i->Release();
            }
        }
        d->Release();
    }
    CoUninitialize();
    return out;
}
void copyText(const std::string &s) {
    auto w = wide(s);
    if (!OpenClipboard(nullptr))
        return;
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t));
    if (h) {
        void *p = GlobalLock(h);
        memcpy(p, w.c_str(), (w.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(h);
        if (!SetClipboardData(CF_UNICODETEXT, h))
            GlobalFree(h);
    }
    CloseClipboard();
}
static std::wstring env(const wchar_t *k) {
    DWORD n = GetEnvironmentVariableW(k, nullptr, 0);
    std::wstring w(n, 0);
    if (n) {
        GetEnvironmentVariableW(k, w.data(), n);
        w.resize(n - 1);
    }
    return w;
}
std::vector<Browser> discoverBrowsers() {
    std::set<std::wstring> running;
    Handle processes(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    PROCESSENTRY32W pe{sizeof(pe)};
    if (Process32FirstW(processes, &pe))
        do {
            std::wstring e = pe.szExeFile;
            std::transform(e.begin(), e.end(), e.begin(), ::towlower);
            running.insert(e);
        } while (Process32NextW(processes, &pe));
    struct Spec {
        const char *name;
        const wchar_t *exe, *rel;
        const char *family;
    };
    Spec specs[] = {
        {"Google Chrome", L"Google\\Chrome\\Application\\chrome.exe", L"Google\\Chrome\\User Data",
         "chromium"},
        {"Microsoft Edge", L"Microsoft\\Edge\\Application\\msedge.exe", L"Microsoft\\Edge\\User Data",
         "chromium"},
        {"Mozilla Firefox", L"Mozilla Firefox\\firefox.exe", L"Mozilla\\Firefox\\Profiles", "firefox"},
        {"Brave", L"BraveSoftware\\Brave-Browser\\Application\\brave.exe",
         L"BraveSoftware\\Brave-Browser\\User Data", "chromium"},
        {"Vivaldi", L"Vivaldi\\Application\\vivaldi.exe", L"Vivaldi\\User Data", "chromium"},
        {"Яндекс Браузер", L"Yandex\\YandexBrowser\\Application\\browser.exe",
         L"Yandex\\YandexBrowser\\User Data", "chromium"},
        {"Opera", L"Programs\\Opera\\opera.exe", L"Opera Software\\Opera Stable", "chromium"},
        {"Opera GX", L"Programs\\Opera GX\\opera.exe", L"Opera Software\\Opera GX Stable", "chromium"},
        {"Chromium", L"Chromium\\Application\\chrome.exe", L"Chromium\\User Data", "chromium"}};
    std::vector<Browser> out;
    for (auto &s : specs) {
        fs::path exe;
        for (auto base : {env(L"LOCALAPPDATA"), env(L"ProgramFiles"), env(L"ProgramFiles(x86)")}) {
            auto p = fs::path(base) / s.exe;
            if (fs::exists(p)) {
                exe = p;
                break;
            }
        }
        if (exe.empty())
            continue;
        std::wstring name = exe.filename();
        std::transform(name.begin(), name.end(), name.begin(), ::towlower);
        Browser b{s.name, s.family, utf8(exe.wstring()), running.count(name) > 0, {}};
        auto profiles = fs::path(env(b.family == "firefox" || std::string(s.name).rfind("Opera", 0) == 0
                                         ? L"APPDATA"
                                         : L"LOCALAPPDATA")) /
                        s.rel;
        if (fs::exists(profiles)) {
            auto inspect = [&](fs::path p) {
                std::string status = "Расширение не найдено";
                if (b.family == "firefox") {
                    if (fs::exists(p / L"extensions.json")) {
                        try {
                            auto j = Json::parse(readFile(p / L"extensions.json", 16 * 1024 * 1024));
                            for (auto &a : j.value("addons", Json::array()))
                                if (str(a, "id") == FirefoxId)
                                    status = "Расширение " + str(a, "version");
                        } catch (...) {
                        }
                    }
                } else {
                    for (auto filename : {L"Secure Preferences", L"Preferences"}) {
                        try {
                            auto present = extensionPresence(Json::parse(readFile(p / filename, 64 * 1024 * 1024)));
                            if (present.installed) { status = present.enabledKnown && !present.enabled ? "Расширение установлено · выключено" : "Расширение установлено"; break; }
                        } catch (...) { /* Profile may be temporarily locked or unavailable. */ }
                    }
                    auto e = p / L"Extensions" / wide(ExtensionId);
                    if (fs::exists(e))
                        for (auto &v : fs::directory_iterator(e))
                            if (v.is_directory())
                                status = "Расширение " + utf8(v.path().filename());
                }
                b.profiles.push_back({utf8(p.filename()), utf8(p.wstring()), status});
            };
            if (fs::exists(profiles / L"Preferences"))
                inspect(profiles);
            for (auto &p : fs::directory_iterator(profiles, fs::directory_options::skip_permission_denied))
                if (p.is_directory() && (b.family == "firefox" || fs::exists(p.path() / L"Preferences")))
                    inspect(p.path());
        }
        out.push_back(std::move(b));
    }
    std::stable_sort(out.begin(), out.end(), [](auto &a, auto &b) { return a.running > b.running; });
    return out;
}
} // namespace ss
