#pragma once

#include <windows.h>
#include <restartmanager.h>
#include <optional>
#include <string>
#include <vector>

namespace ss {
// Restart Manager identifies a process by both PID and creation time, so a
// recycled PID cannot silently become the selected browser.
struct FirefoxProfileOwner {
    DWORD processId = 0;
    FILETIME startTime{};
};

inline bool firefoxProfileOwnerMatches(const FirefoxProfileOwner &owner, DWORD pid) {
    if (!pid || owner.processId != pid) return false;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    FILETIME created{}, exited{}, kernel{}, user{};
    const bool ok = GetProcessTimes(process, &created, &exited, &kernel, &user) &&
                    CompareFileTime(&created, &owner.startTime) == 0;
    CloseHandle(process);
    return ok;
}

// Resolve only an unambiguous owner of the selected Firefox profile's lock.
// No process is stopped or restarted. Failure, an unlocked profile, and
// multiple users of the lock all return no identity.
inline std::optional<FirefoxProfileOwner> firefoxProfileLockOwner(const std::wstring &profileDirectory) {
    if (profileDirectory.empty()) return std::nullopt;
    std::wstring lock = profileDirectory;
    if (lock.back() != L'\\' && lock.back() != L'/') lock += L'\\';
    lock += L"parent.lock";
    const auto attributes = GetFileAttributesW(lock.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
        return std::nullopt;

    HMODULE module = LoadLibraryExW(L"rstrtmgr.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) return std::nullopt;
    struct LibraryScope { HMODULE value; ~LibraryScope() { FreeLibrary(value); } } libraryScope{module};
    auto start = reinterpret_cast<decltype(&RmStartSession)>(GetProcAddress(module, "RmStartSession"));
    auto reg = reinterpret_cast<decltype(&RmRegisterResources)>(GetProcAddress(module, "RmRegisterResources"));
    auto list = reinterpret_cast<decltype(&RmGetList)>(GetProcAddress(module, "RmGetList"));
    auto end = reinterpret_cast<decltype(&RmEndSession)>(GetProcAddress(module, "RmEndSession"));
    if (!start || !reg || !list || !end) return std::nullopt;

    DWORD session = 0;
    WCHAR key[CCH_RM_SESSION_KEY + 1]{};
    if (start(&session, 0, key) != ERROR_SUCCESS) return std::nullopt;
    struct EndSession {
        decltype(&RmEndSession) close;
        DWORD value;
        ~EndSession() { close(value); }
    } endSession{end, session};
    LPCWSTR resources[] = {lock.c_str()};
    if (reg(session, 1, resources, 0, nullptr, 0, nullptr) != ERROR_SUCCESS)
        return std::nullopt;

    try {
        UINT needed = 0, count = 0;
        DWORD reasons = 0;
        DWORD result = list(session, &needed, &count, nullptr, &reasons);
        if (result == ERROR_SUCCESS && needed == 0) return std::nullopt;
        if (result != ERROR_MORE_DATA || !needed || needed > 128) return std::nullopt;
        for (int attempt = 0; attempt < 3; ++attempt) {
            std::vector<RM_PROCESS_INFO> processes(needed);
            count = needed;
            result = list(session, &needed, &count, processes.data(), &reasons);
            if (result == ERROR_MORE_DATA && needed && needed <= 128) continue;
            if (result != ERROR_SUCCESS || count != 1) return std::nullopt;
            const auto &identity = processes[0].Process;
            if (!identity.dwProcessId ||
                (!identity.ProcessStartTime.dwLowDateTime && !identity.ProcessStartTime.dwHighDateTime))
                return std::nullopt;
            FirefoxProfileOwner owner{identity.dwProcessId, identity.ProcessStartTime};
            return firefoxProfileOwnerMatches(owner, owner.processId)
                ? std::optional<FirefoxProfileOwner>(owner) : std::nullopt;
        }
    } catch (...) {
        // Treat memory or API errors as an inability to establish identity.
    }
    return std::nullopt;
}
} // namespace ss
