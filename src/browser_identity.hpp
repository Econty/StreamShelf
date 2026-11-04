#pragma once
#include "platform.hpp"
namespace ss {
inline std::string browserFromAncestors(DWORD pid, const std::map<DWORD, DWORD> &parents,
                                        const std::function<std::wstring(DWORD)> &pathForPid) {
    for (int depth = 0; pid && depth < 12; ++depth) {
        fs::path path(pathForPid(pid));
        auto name = path.filename().wstring();
        if (!name.empty()) CharLowerBuffW(name.data(), DWORD(name.size()));
        if (name == L"chrome.exe" || name == L"browser.exe" || name == L"firefox.exe" || name == L"msedge.exe" || name == L"brave.exe" || name == L"vivaldi.exe" || name == L"opera.exe") return utf8(path.wstring());
        auto it = parents.find(pid);
        if (it == parents.end() || it->second == pid) break;
        pid = it->second;
    }
    return {};
}
inline bool sameBrowserExecutable(const std::string &actual, const std::string &selected) {
    if (actual.empty() || selected.empty()) return false;
    auto normalize = [](const std::string &input) {
        auto value = fs::path(wide(input)).lexically_normal().wstring();
        if (!value.empty()) CharLowerBuffW(value.data(), DWORD(value.size()));
        return value;
    };
    return normalize(actual) == normalize(selected);
}
}
