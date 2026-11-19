#pragma once
#include "platform.hpp"

namespace ss {
inline bool isConnectorHostFilename(const std::wstring &filename) {
    if (filename == L"StreamShelf.Host.exe") return true;
    constexpr auto prefix = L"StreamShelf.Host-";
    constexpr size_t prefixLength = 17;
    if (filename.size() != prefixLength + 64 + 4 || filename.rfind(prefix, 0) != 0 ||
        filename.compare(filename.size() - 4, 4, L".exe") != 0) return false;
    for (size_t i = prefixLength; i < prefixLength + 64; ++i)
        if (!((filename[i] >= L'0' && filename[i] <= L'9') || (filename[i] >= L'a' && filename[i] <= L'f'))) return false;
    return true;
}
// Native Messaging keeps its executable locked for the lifetime of a browser
// session. Stage each distinct binary under its content hash, so updating or
// moving the application never overwrites a running host.
inline fs::path stageConnectorHost(const fs::path &app, const fs::path &directory) {
    const auto digest = sha256(app);
    const auto destination = directory / (L"StreamShelf.Host-" + wide(digest) + L".exe");
    fs::create_directories(directory);
    if (fs::exists(destination)) {
        if (!fs::is_regular_file(destination) || sha256(destination) != digest)
            throw std::runtime_error("Повреждён файл локального подключения StreamShelf");
        return destination;
    }
    static std::atomic_uint64_t serial{0};
    auto temporary = destination;
    temporary += L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++serial);
    try {
        if (!CopyFileW(app.c_str(), temporary.c_str(), TRUE))
            throw std::runtime_error("Не удалось подготовить локальное подключение. Код Windows: " + std::to_string(GetLastError()));
        if (sha256(temporary) != digest)
            throw std::runtime_error("Файл программы изменился во время подготовки подключения. Повторите действие.");
        if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH)) {
            // Another copy may have installed the same content concurrently.
            if (!fs::is_regular_file(destination) || sha256(destination) != digest)
                throw std::runtime_error("Не удалось сохранить локальное подключение. Код Windows: " + std::to_string(GetLastError()));
            std::error_code ignored; fs::remove(temporary, ignored);
        }
    } catch (...) {
        std::error_code ignored; fs::remove(temporary, ignored); throw;
    }
    return destination;
}
} // namespace ss
