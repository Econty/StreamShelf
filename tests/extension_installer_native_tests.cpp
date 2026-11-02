#include "extension_installer.hpp"
#include <iostream>
int main(int argc, char **argv) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    try {
        int count = argc > 1 ? ss::ExtensionInstaller::runBrowserCardRegression(ss::wide(argv[1])) :
            ss::ExtensionInstaller::runNativeRegression(ss::fs::current_path() / L"extension" / L"chromium");
        std::cout << count << " native guide checks passed\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << "FAIL native guide: " << e.what() << '\n'; return 1; }
}
