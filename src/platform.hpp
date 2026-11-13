#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <filesystem>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <memory>
#include <set>
#include "core.hpp"
namespace ss {
namespace fs = std::filesystem;
std::wstring wide(const std::string &);
std::string utf8(const std::wstring &);
fs::path executable(), dataRoot();
std::string readFile(const fs::path &, size_t limit = 64 * 1024 * 1024);
void writeFile(const fs::path &, const std::string &);
void protectFile(const fs::path &, const std::string &, size_t limit = size_t(-1));
std::string unprotectFile(const fs::path &, size_t limit = 64 * 1024 * 1024);
std::string sha256(const fs::path &);
struct Handle {
    HANDLE h = nullptr;
    Handle() = default;
    Handle(HANDLE x) : h(x) {}
    ~Handle() {
        if (h && h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
    }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    operator HANDLE() const {
        return h;
    }
    HANDLE release() {
        auto x = h;
        h = nullptr;
        return x;
    }
};
using Log = std::function<void(const std::string &)>;
struct ProcessResult {
    DWORD code;
    std::string output, error;
};
ProcessResult runProcess(const fs::path &, const std::vector<std::wstring> &, Log, std::atomic_bool &,
                         bool capture = false, bool shellCommand = false, const fs::path &extraPath = {});
std::string httpGet(const std::string &, size_t limit = 32 * 1024 * 1024);
std::string httpPreview(const std::string &, std::atomic_bool &, const Json &, const std::string &);
PreviewImage decodePreview(const std::string &);
void httpDownload(const std::string &, const fs::path &, std::atomic_bool &, std::function<void(double)>,
                  const Json &headers = Json::object(), const std::string &cookieText = "",
                  std::function<void(uint64_t,uint64_t)> onBytes = {});
void openPath(const fs::path &);
std::string pickFile(HWND, bool folder = false, bool save = false);
void copyText(const std::string &);
struct Profile {
    std::string name, path, status;
};
struct Browser {
    std::string name, family, exe;
    bool running = false;
    std::vector<Profile> profiles;
};
std::vector<Browser> discoverBrowsers();
std::wstring pipeName();
int nativeHostMain();
void installConnector(const fs::path &);
void updateConnectorExtensions(const fs::path &);
// Older extensions may connect for status and manual use, but scoped browser
// credentials require the capability introduced in 1.0.5.
inline constexpr const char *ConnectorVersion = "1.0.5";
inline constexpr const wchar_t *SignedFirefoxConnectorFilename = L"StreamShelf-Connector-1.0.5.xpi";
inline bool compatibleExtensionHello(const Json &hello) {
    const auto version = str(hello, "version");
    return version == ConnectorVersion || version == "1.0.4" || version == "1.0.3";
}
void unregisterConnector();
// A media item may only use credentials from the exact browser document that
// discovered it. A new selection/navigation receives a new generation.
struct BrowserSource {
    int tab = -1;
    uint64_t generation = 0;
    std::string page, store;
    std::optional<uint64_t> extensionSource;
    bool valid() const { return tab >= 0 && extensionSource.has_value(); }
    bool operator==(const BrowserSource &) const = default;
};
struct BrowserSourceChanged : std::runtime_error {
    BrowserSourceChanged() : std::runtime_error("Источник браузера или учётная запись изменились либо расширение устарело. Обновите расширение и повторите анализ. Для общедоступной ссылки можно выбрать режим «Без браузера».") {}
};
struct Session {
    inline static std::atomic_uint64_t nextInstanceSerial{0};
    const uint64_t instanceSerial = ++nextInstanceSerial;
    HANDLE pipe = INVALID_HANDLE_VALUE;
    std::mutex writeMutex, mutex;
    std::condition_variable cv;
    std::map<std::string, Json> replies;
    std::set<std::string> pendingCredentials;
    void acceptCredentialReply(const Json &);
    // Snapshots belong to this browser session and the exact requested URL.
    struct CredentialSnapshot { Json data; ULONGLONG captured; };
    std::map<std::string, CredentialSnapshot> credentialSnapshots;
    uint64_t credentialEpoch = 0, credentialScanId = 0;
    std::atomic_bool alive{true};
    std::atomic<DWORD> readerId{0};
    std::atomic_bool readerEnded{false};
    std::atomic_bool verified{false}, setupReady{false}, setupApiSupported{false}, languageSupported{false};
    std::atomic<ULONGLONG> lastPong{0};
    std::atomic<ULONGLONG> verificationSent{0};
    std::string id, url, title, browser, browserExecutable, verificationToken;
    std::string store;
    uint64_t sourceGeneration = 0;
    std::optional<uint64_t> extensionSource;
    std::atomic_bool scopedCredentialsSupported{false};
    int tab = -1;
    BrowserSource source();
    bool sourceMatches(const BrowserSource &);
    void updateTab(const Json &);
    void closeTab();
    void send(const Json &);
    void verifyConnection();
    bool acceptVerification(const std::string &);
    Json credentials(const std::string &, std::atomic_bool &);
    Json credentials(const std::string &, const BrowserSource &, std::atomic_bool &);
};
class Connector {
    std::thread acceptThread;
    std::atomic_bool stopping{false}, acceptEnded{false};
    std::atomic<DWORD> acceptId{0};
    std::mutex mutex;
    std::vector<std::shared_ptr<Session>> sessions;
    std::vector<std::thread> readers;

  public:
    std::function<void(std::shared_ptr<Session>, const Json &)> message;
    void start();
    void stop();
    std::vector<std::shared_ptr<Session>> list();
    ~Connector();
};
} // namespace ss
