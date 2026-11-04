#include "platform.hpp"
#include "component_manager.hpp"
#include <iostream>
using namespace ss;
int main() {
    try {
        wchar_t system[MAX_PATH]{};
        if (!GetSystemDirectoryW(system, MAX_PATH)) return 1;
        auto shell = fs::path(system) / L"cmd.exe";
        auto folder = fs::temp_directory_path() / (L"StreamShelf terminal test Путь " + std::to_wstring(GetCurrentProcessId()));
        fs::create_directories(folder);
        auto fake = folder / L"streamshelf-test-tool.exe";
        fs::copy_file(shell, fake, fs::copy_options::overwrite_existing);
        std::atomic_bool cancel{false};
        auto run = [&](const std::wstring &command, const fs::path &path) {
            return runProcess(shell, {L"/d", L"/c", command}, nullptr, cancel, true, true, path);
        };
        auto result = run(L"streamshelf-test-tool /d /c echo STREAMSHELF_TERMINAL_OK", folder);
        if (result.code || result.output.find("STREAMSHELF_TERMINAL_OK") == std::string::npos)
            throw std::runtime_error("tool in Unicode path with spaces was not found");
        std::vector<std::string> observed;
        auto firstLine = runProcess(shell, {L"/d", L"/c", L"chcp 65001>nul & echo VERSION_FIRST_LINE"},
            [&](const std::string &line) { observed.push_back(line); }, cancel, true, true, folder);
        if (firstLine.code || observed.empty() || observed.front().find("VERSION_FIRST_LINE") == std::string::npos)
            throw std::runtime_error("first command output line was lost");
        auto fakeYt = folder / L"yt-dlp.exe";
        fs::copy_file(shell, fakeYt, fs::copy_options::overwrite_existing);
        auto resolved = terminalComponentCommand(L"yt-dlp /d /c echo COMPONENT_PATH_OK", folder);
        auto resolvedResult = run(L"chcp 65001>nul & " + resolved, {});
        if (resolvedResult.code || resolvedResult.output.find("COMPONENT_PATH_OK") == std::string::npos)
            throw std::runtime_error("terminal did not execute exact component path");
        fs::remove(fakeYt);
        bool missingRejected = false;
        try { terminalComponentCommand(L"yt-dlp --version", folder); } catch (...) { missingRejected = true; }
        if (!missingRejected) throw std::runtime_error("terminal searched PATH for missing component");
        auto inherited = run(L"streamshelf-test-tool /d /c echo UNEXPECTED", {});
        if (inherited.code == 0) throw std::runtime_error("tool path leaked into parent environment");
        auto builtin = run(L"echo BUILTIN_OK", folder);
        if (builtin.code || builtin.output.find("BUILTIN_OK") == std::string::npos)
            throw std::runtime_error("built-in command failed");
        auto tools = dataRoot() / L"tools";
        if (fs::is_regular_file(tools / L"yt-dlp.exe") && fs::is_regular_file(tools / L"ffmpeg.exe")) {
            auto ytdlp = run(L"chcp 65001>nul & yt-dlp --version", tools);
            auto ffmpeg = run(L"chcp 65001>nul & ffmpeg -version", tools);
            if (ytdlp.code || ytdlp.output.empty() || ffmpeg.code || ffmpeg.output.find("ffmpeg version") == std::string::npos)
                throw std::runtime_error("bundled tool version commands failed");
            std::cout << "yt-dlp and ffmpeg version commands passed\n";
        }
        fs::remove(fake);
        fs::remove(folder);
        std::cout << "Terminal PATH isolation and Unicode path checks passed\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
