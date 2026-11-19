#include "connector_install.hpp"
#include <future>
#include <iostream>

using namespace ss;
int main() {
    const auto root = fs::temp_directory_path() / (L"StreamShelf-HostStageTest-" +
        std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
    int result = 0;
    try {
        auto check = [](bool value, const char *label) { if (!value) throw std::runtime_error(label); std::cout << "PASS " << label << '\n'; };
        const auto app = root / L"app" / L"StreamShelf.exe";
        const auto directory = root / L"connector";
        const auto legacy = directory / L"StreamShelf.Host.exe";
        writeFile(app, "current build"); writeFile(legacy, "running legacy build");
        Handle legacyLock(CreateFileW(legacy.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
        check(legacyLock.h != INVALID_HANDLE_VALUE, "legacy executable locked against replacement");
        auto first = stageConnectorHost(app, directory);
        check(first != legacy && readFile(first) == "current build" && readFile(legacy) == "running legacy build", "new host stages alongside locked legacy host");
        check(isConnectorHostFilename(first.filename().wstring()), "versioned host enters native messaging mode");
        check(isConnectorHostFilename(L"StreamShelf.Host.exe") && !isConnectorHostFilename(L"StreamShelf.exe") &&
              !isConnectorHostFilename(L"StreamShelf.Host-untrusted.exe") &&
              !isConnectorHostFilename(L"StreamShelf.Host-" + std::wstring(64, L'g') + L".exe"), "only legacy or complete SHA256 host filenames enter native mode");
        Handle firstLock(CreateFileW(first.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
        check(firstLock.h != INVALID_HANDLE_VALUE && stageConnectorHost(app, directory) == first, "same build reuses locked host without writing it");
        writeFile(app, "updated build");
        auto second = stageConnectorHost(app, directory);
        check(second != first && readFile(second) == "updated build" && readFile(first) == "current build", "changed build stages without replacing running previous host");
        writeFile(app, "concurrently staged build");
        auto worker = std::async(std::launch::async, [&] { return stageConnectorHost(app, directory); });
        auto third = stageConnectorHost(app, directory);
        check(worker.get() == third && readFile(third) == "concurrently staged build", "concurrent staging converges on the same verified host");
    } catch (const std::exception &e) { std::cerr << "FAIL " << e.what() << '\n'; result = 1; }
    // The test owns this freshly created directory, never application data.
    std::error_code ignored; fs::remove_all(root, ignored);
    return result;
}
