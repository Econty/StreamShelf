#pragma once
#include "platform.hpp"

namespace ss {
inline bool sameConnectorPath(const fs::path &left, const fs::path &right) {
    const auto a = left.lexically_normal().wstring();
    const auto b = right.lexically_normal().wstring();
    return !a.empty() && !b.empty() &&
           CompareStringOrdinal(a.c_str(), int(a.size()), b.c_str(), int(b.size()), TRUE) == CSTR_EQUAL;
}

// The native host registration is shared by all copies of StreamShelf. Remove
// only values still belonging to the copy being uninstalled, and preserve any
// other application's registration as well as all user data.
inline bool unregisterConnectorIfOwned(const fs::path &root, const fs::path &app, HKEY registryRoot) {
    const auto dir = root / L"connector";
    const auto marker = dir / L"app-path.txt";
    if (!fs::is_regular_file(marker) || !sameConnectorPath(wide(readFile(marker, 32768)), app)) return false;

    const std::pair<const wchar_t *, const wchar_t *> entries[] = {
        {L"Software\\Google\\Chrome\\NativeMessagingHosts\\dev.streamshelf.connector", L"chromium-host.json"},
        {L"Software\\Microsoft\\Edge\\NativeMessagingHosts\\dev.streamshelf.connector", L"chromium-host.json"},
        {L"Software\\Chromium\\NativeMessagingHosts\\dev.streamshelf.connector", L"chromium-host.json"},
        {L"Software\\BraveSoftware\\Brave-Browser\\NativeMessagingHosts\\dev.streamshelf.connector", L"chromium-host.json"},
        {L"Software\\Vivaldi\\NativeMessagingHosts\\dev.streamshelf.connector", L"chromium-host.json"},
        {L"Software\\Yandex\\YandexBrowser\\NativeMessagingHosts\\dev.streamshelf.connector", L"chromium-host.json"},
        {L"Software\\Opera Software\\NativeMessagingHosts\\dev.streamshelf.connector", L"chromium-host.json"},
        {L"Software\\Mozilla\\NativeMessagingHosts\\dev.streamshelf.connector", L"firefox-host.json"}
    };
    for (const auto &[key, manifest] : entries) {
        HKEY h{};
        auto opened = RegOpenKeyExW(registryRoot, key, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &h);
        if (opened == ERROR_FILE_NOT_FOUND || opened == ERROR_PATH_NOT_FOUND) continue;
        if (opened != ERROR_SUCCESS) throw std::runtime_error("Could not inspect native host registration");
        DWORD type = 0, bytes = 0;
        LONG deletion = ERROR_SUCCESS;
        if (RegQueryValueExW(h, nullptr, nullptr, &type, nullptr, &bytes) == ERROR_SUCCESS &&
            type == REG_SZ && bytes >= sizeof(wchar_t) && bytes <= 65536 && bytes % sizeof(wchar_t) == 0) {
            std::vector<wchar_t> value(bytes / sizeof(wchar_t) + 1, L'\0');
            if (RegQueryValueExW(h, nullptr, nullptr, &type, reinterpret_cast<BYTE *>(value.data()), &bytes) == ERROR_SUCCESS &&
                type == REG_SZ && sameConnectorPath(fs::path(value.data()), dir / manifest)) {
                deletion = RegDeleteValueW(h, nullptr);
            }
        }
        RegCloseKey(h);
        if (deletion != ERROR_SUCCESS && deletion != ERROR_FILE_NOT_FOUND)
            throw std::runtime_error("Could not remove native host registration");
    }
    return true;
}
} // namespace ss
