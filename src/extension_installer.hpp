#pragma once
#include "platform.hpp"

namespace ss {
enum class GuidedBrowser { None, Chrome, Yandex, Firefox };
// Product metadata prevents Chromium/Edge or a renamed executable from entering the guided flow.
GuidedBrowser guidedBrowserProduct(const std::wstring &product, const std::wstring &filename);
GuidedBrowser guidedBrowser(const Browser &);
std::wstring extensionPageUrl(GuidedBrowser);
std::vector<std::wstring> extensionLaunchArguments(GuidedBrowser, const Profile *);

class ExtensionInstaller {
    struct Impl;
    std::unique_ptr<Impl> impl;
  public:
    ExtensionInstaller();
    ~ExtensionInstaller();
    ExtensionInstaller(const ExtensionInstaller &) = delete;
    ExtensionInstaller &operator=(const ExtensionInstaller &) = delete;
    void start(HWND owner, const Browser &, const Profile *, const fs::path &extension, bool reduced, bool light = false);
    bool active() const;
    bool translate(MSG &);
    void connectionEstablished();
    void setExtensionReloadRequired(bool required);
    void setRetryConnection(std::function<void()> callback);
    void setOpenSetup(std::function<bool()> callback);
    void setExtensionRunning(bool running);
    void close();
    Json diagnostics() const;
#ifdef STREAMSHELF_INSTALLER_TESTING
    static int runNativeRegression(const fs::path &);
    static int runBrowserCardRegression(const std::wstring &browserExe);
#endif
};
} // namespace ss
