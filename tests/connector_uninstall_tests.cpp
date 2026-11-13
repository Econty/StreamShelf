#include "connector_uninstall.hpp"
#include <iostream>

using namespace ss;
static void require(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
static void setValue(HKEY root, const wchar_t *key, const wchar_t *name, const fs::path &path) {
    HKEY h{};
    require(RegCreateKeyExW(root, key, 0, nullptr, 0, KEY_WRITE, nullptr, &h, nullptr) == ERROR_SUCCESS, "create test key");
    auto text = path.wstring();
    auto result = RegSetValueExW(h, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(text.c_str()),
                                 DWORD((text.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(h);
    require(result == ERROR_SUCCESS, "write test value");
}
static bool hasValue(HKEY root, const wchar_t *key, const wchar_t *name) {
    HKEY h{};
    if (RegOpenKeyExW(root, key, 0, KEY_QUERY_VALUE, &h) != ERROR_SUCCESS) return false;
    auto result = RegQueryValueExW(h, name, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(h);
    return result == ERROR_SUCCESS;
}
int main() {
    const auto suffix = std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    const auto key = L"Software\\StreamShelf-UninstallTest-" + suffix;
    const auto root = fs::temp_directory_path() / (L"StreamShelf-UninstallTest-" + suffix);
    HKEY testRegistry{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, key.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &testRegistry, nullptr) != ERROR_SUCCESS) return 2;
    int result = 0;
    try {
        auto app = root / L"app" / L"StreamShelf.exe";
        auto connector = root / L"connector";
        writeFile(connector / L"app-path.txt", utf8(app.wstring()));
        const wchar_t *chrome = L"Software\\Google\\Chrome\\NativeMessagingHosts\\dev.streamshelf.connector";
        const wchar_t *firefox = L"Software\\Mozilla\\NativeMessagingHosts\\dev.streamshelf.connector";
        setValue(testRegistry, chrome, nullptr, connector / L"chromium-host.json");
        setValue(testRegistry, chrome, L"OtherValue", L"keep");
        setValue(testRegistry, firefox, nullptr, root / L"other" / L"firefox-host.json");
        require(!unregisterConnectorIfOwned(root, root / L"other" / L"StreamShelf.exe", testRegistry), "another copy must not unregister");
        require(hasValue(testRegistry, chrome, nullptr), "owner mismatch changed registration");
        require(unregisterConnectorIfOwned(root, app, testRegistry), "owner should match");
        require(!hasValue(testRegistry, chrome, nullptr), "owned registration remains");
        require(hasValue(testRegistry, chrome, L"OtherValue"), "unrelated value deleted");
        require(hasValue(testRegistry, firefox, nullptr), "different manifest deleted");
        require(fs::is_regular_file(connector / L"app-path.txt"), "user data deleted");
        std::cout << "Owned registrations removed; other registrations and data preserved\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; result = 1; }
    RegCloseKey(testRegistry);
    RegDeleteTreeW(HKEY_CURRENT_USER, key.c_str());
    std::error_code ec;
    fs::remove(root / L"connector" / L"app-path.txt", ec);
    fs::remove(root / L"connector", ec);
    fs::remove(root, ec);
    return result;
}
