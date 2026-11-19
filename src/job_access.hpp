#pragma once
#include "platform.hpp"
#include <bcrypt.h>
#include <winhttp.h>

namespace ss {
using DownloadAccess = std::pair<std::string, Json>;
struct JobDownloadAccess {
    DownloadAccess target;
    std::optional<DownloadAccess> page;
};

inline bool sameDownloadOrigin(const std::string &left, const std::string &right) {
    if (!httpUrl(left) || !httpUrl(right)) return false;
    auto lhs = wide(left), rhs = wide(right);
    URL_COMPONENTS first{}, second{};
    first.dwStructSize = sizeof(first);
    second.dwStructSize = sizeof(second);
    first.dwHostNameLength = second.dwHostNameLength = DWORD(-1);
    if (!WinHttpCrackUrl(lhs.c_str(), 0, 0, &first) || !WinHttpCrackUrl(rhs.c_str(), 0, 0, &second) ||
        !first.dwHostNameLength || !second.dwHostNameLength) return false;
    return first.nScheme == second.nScheme && first.nPort == second.nPort && first.dwHostNameLength == second.dwHostNameLength &&
        CompareStringOrdinal(first.lpszHostName, first.dwHostNameLength, second.lpszHostName, second.dwHostNameLength, TRUE) == CSTR_EQUAL;
}

inline DownloadAccess subtitleDownloadAccess(const std::string &page, const std::string &target, const DownloadAccess &pageAccess) {
    DownloadAccess result{"", Json::object()};
    if (pageAccess.second.contains("User-Agent")) result.second["User-Agent"] = pageAccess.second.at("User-Agent");
    if (sameDownloadOrigin(page, target)) {
        result.first = pageAccess.first;
        result.second["Referer"] = page;
    }
    return result;
}

inline constexpr size_t JobAccessLimit = 4 * 1024 * 1024;
inline constexpr size_t JobAccessFileLimit = JobAccessLimit + 4096;

inline bool validJobAccessKey(std::string_view key) {
    return key.size() == 32 && key.find_first_not_of("0123456789abcdef") == key.npos;
}

inline std::string newJobAccessKey() {
    unsigned char random[16];
    if (BCryptGenRandom(nullptr, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        throw std::runtime_error("Не удалось создать защищённый идентификатор загрузки");
    constexpr std::string_view digits = "0123456789abcdef";
    std::string result;
    result.reserve(sizeof(random) * 2);
    for (auto value : random) {
        result += digits[value >> 4];
        result += digits[value & 15];
    }
    return result;
}

inline fs::path jobAccessPath(const fs::path &root, const std::string &key) {
    if (!validJobAccessKey(key)) throw std::runtime_error("Некорректный идентификатор загрузки");
    return root / L"download-access" / (wide(key) + L".dpapi");
}

inline void checkDownloadAccess(const DownloadAccess &entry) {
    if (!entry.second.is_object() || entry.first.size() > JobAccessLimit || entry.first.find('\0') != entry.first.npos)
        throw std::runtime_error("Некорректные данные доступа загрузки");
    if (!entry.first.empty() && entry.first != "# Netscape HTTP Cookie File\n") (void)validateCookies(entry.first);
    for (const auto &[key, value] : entry.second.items()) {
        if ((key != "User-Agent" && key != "Referer" && key != "Origin" && key != "Authorization") || !value.is_string())
            throw std::runtime_error("Некорректный заголовок доступа загрузки");
        const auto &header = value.get_ref<const std::string &>();
        if (header.find_first_of("\r\n") != header.npos || header.find('\0') != header.npos || header.size() > 16384)
            throw std::runtime_error("Некорректный заголовок доступа загрузки");
    }
}

inline Json encodeDownloadAccess(const DownloadAccess &entry) {
    checkDownloadAccess(entry);
    if (entry.second.contains("Authorization")) throw std::runtime_error("Заголовок авторизации не сохраняется в очереди");
    return Json{{"cookies", entry.first}, {"headers", entry.second}};
}

inline DownloadAccess decodeDownloadAccess(const Json &entry) {
    DownloadAccess result{entry.at("cookies").get<std::string>(), entry.at("headers")};
    checkDownloadAccess(result);
    if (result.second.contains("Authorization")) throw std::runtime_error("Invalid checkpoint authorization");
    return result;
}

inline void saveJobAccess(const fs::path &root, const std::string &key, const Json &scope, const JobDownloadAccess &access) {
    auto contents = Json{{"version", 1}, {"key", key}, {"scope", scope}, {"target", encodeDownloadAccess(access.target)},
                        {"page", access.page ? encodeDownloadAccess(*access.page) : Json()}}.dump();
    struct ClearText {
        std::string &value;
        ~ClearText() { if (!value.empty()) SecureZeroMemory(value.data(), value.size()); }
    } clear{contents};
    if (contents.size() > JobAccessLimit) throw std::runtime_error("Снимок доступа загрузки слишком большой");
    protectFile(jobAccessPath(root, key), contents, JobAccessFileLimit);
}

inline std::optional<JobDownloadAccess> readJobAccess(const fs::path &root, const std::string &key, const Json &scope) {
    if (key.empty()) return std::nullopt;
    const auto path = jobAccessPath(root, key);
    if (!fs::exists(path)) return std::nullopt;
    try {
        const auto status = fs::symlink_status(path);
        if (!fs::is_regular_file(status)) throw std::runtime_error("Invalid checkpoint type");
        auto contents = unprotectFile(path, JobAccessFileLimit);
        struct ClearText {
            std::string &value;
            ~ClearText() { if (!value.empty()) SecureZeroMemory(value.data(), value.size()); }
        } clear{contents};
        const auto saved = Json::parse(contents);
        if (saved.at("version") != 1 || saved.at("key") != key || saved.at("scope") != scope)
            throw std::runtime_error("Invalid checkpoint scope");
        JobDownloadAccess result{decodeDownloadAccess(saved.at("target")), std::nullopt};
        if (!saved.at("page").is_null()) result.page = decodeDownloadAccess(saved.at("page"));
        return result;
    } catch (...) {
        throw std::runtime_error("Не удалось прочитать сохранённый доступ загрузки. Повторите анализ страницы.");
    }
}

inline void eraseJobAccess(const fs::path &root, const std::string &key) {
    if (validJobAccessKey(key)) fs::remove(jobAccessPath(root, key));
}

inline void removeOrphanJobAccess(const fs::path &root, const std::set<std::string> &retained) {
    const auto folder = root / L"download-access";
    std::error_code error;
    if (!fs::is_directory(fs::symlink_status(folder, error))) return;
    for (fs::directory_iterator item(folder, error), end; item != end && !error; item.increment(error)) {
        const auto filename = utf8(item->path().filename().wstring());
        if (filename.size() < 38 || !validJobAccessKey(std::string_view(filename).substr(0, 32))) continue;
        const auto suffix = std::string_view(filename).substr(32);
        if ((suffix == ".dpapi" && !retained.count(filename.substr(0, 32))) || suffix.starts_with(".dpapi.tmp-")) {
            std::error_code ignored;
            if (item->is_regular_file(ignored)) fs::remove(item->path(), ignored);
        }
    }
}
} // namespace ss
