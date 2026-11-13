#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

// One deterministic executable serves as both test FFprobe and yt-dlp.
// yt-dlp writes only under the isolated preview root used by the test.
int main() {
    namespace fs = std::filesystem;
    wchar_t module[32768]{};
    if (!GetModuleFileNameW(nullptr, module, DWORD(std::size(module)))) return 1;
    const fs::path executable(module);
    if (executable.filename() == L"yt-dlp.exe") {
        const auto root = executable.parent_path().parent_path();
        if (root.filename() != L"preview-data" || executable.parent_path().filename() != L"tools") return 2;
        const auto output = root / L"job-format" / L"media.mp4";
        fs::create_directories(output.parent_path());
        std::ofstream(output, std::ios::binary) << "invalid media";
        std::ofstream(root / L"tools" / L"attempts.txt", std::ios::app) << '1';
        const auto widePath = output.wstring();
        const auto size = WideCharToMultiByte(CP_UTF8, 0, widePath.data(), int(widePath.size()), nullptr, 0, nullptr, nullptr);
        if (!size) return 3;
        std::string path(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, widePath.data(), int(widePath.size()), path.data(), size, nullptr, nullptr);
        std::cout << "STREAMSHELF_FINAL:" << path << '\n';
    } else {
        std::cout << "{\"streams\":[]}";
    }
}
