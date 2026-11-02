// Read-only guide preview: opens the browser page and native overlay, without registering a host
// or installing an extension. Pass the browser EXE and an extension directory for manual UI QA.
#include "extension_installer.hpp"
#include <shellapi.h>
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    using namespace ss;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int argc = 0; auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || (argc != 3 && argc != 4)) { if (argv) LocalFree(argv); return 2; }
    Browser browser{"Browser", "chromium", utf8(argv[1])};
    fs::path folder = argv[2], report = argc == 4 ? argv[3] : L""; LocalFree(argv);
    browser.name = guidedBrowser(browser) == GuidedBrowser::Yandex ? "Яндекс Браузер" : "Google Chrome";
    try {
        ExtensionInstaller guide;
        guide.start(nullptr, browser, nullptr, folder, false);
        auto began = GetTickCount64(); bool done = false;
        while (guide.active()) {
            MSG message;
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (!guide.translate(message)) { TranslateMessage(&message); DispatchMessageW(&message); }
            }
            if (!report.empty()) {
                auto status = guide.diagnostics();
                writeFile(report, status.dump(2));
                if (GetTickCount64() - began > 5000 && status.value("page_verified", false) && status.value("outline_visible", false)) { done = true; guide.close(); }
                else if (GetTickCount64() - began > 25000) guide.close();
            }
            MsgWaitForMultipleObjects(0, nullptr, FALSE, 20, QS_ALLINPUT);
        }
        if (!report.empty()) return done ? 0 : 3;
    } catch (const std::exception &e) { MessageBoxW(nullptr, wide(e.what()).c_str(), L"Guide preview", MB_OK); return 1; }
    return 0;
}
