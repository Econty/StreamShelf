#include "platform.hpp"
#include <array>
#include <iostream>
#include <iterator>

using namespace ss;

int main() {
    const auto tempRoot = fs::temp_directory_path() /
        (L"StreamShelf-profile-cache-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
    const auto local = tempRoot / L"local";
    const auto chrome = local / L"Google" / L"Chrome";
    const auto app = chrome / L"Application";
    const auto profileRoot = chrome / L"User Data";
    const auto profile = profileRoot / L"Default";
    const auto executablePath = app / L"chrome.exe";
    const auto preferences = profile / L"Preferences";
    const std::array<const wchar_t *,4> environmentNames={L"LOCALAPPDATA",L"APPDATA",L"ProgramFiles",L"ProgramFiles(x86)"};
    std::vector<std::pair<std::wstring,std::optional<std::wstring>>> savedEnvironment;
    for(const auto *name:environmentNames) {
        const DWORD length=GetEnvironmentVariableW(name,nullptr,0);
        if(!length) {savedEnvironment.emplace_back(name,std::nullopt);continue;}
        std::wstring value(length,L'\0');
        const DWORD copied=GetEnvironmentVariableW(name,value.data(),length);
        value.resize(copied);
        savedEnvironment.emplace_back(name,std::move(value));
    }
    auto cleanup = [&] {
        for(const auto &[name,value]:savedEnvironment)
            SetEnvironmentVariableW(name.c_str(),value?value->c_str():nullptr);
        std::error_code ec;
        fs::remove(preferences,ec);fs::remove(executablePath,ec);
        for(const auto &dir:{profile,profileRoot,app,chrome,local/L"Google",local,tempRoot})fs::remove(dir,ec);
    };
    try {
        fs::create_directories(profile);
        fs::create_directories(app);
        writeFile(executablePath,"test");
        for(const auto *name:environmentNames)
            if(!SetEnvironmentVariableW(name,local.c_str()))throw std::runtime_error("Could not isolate browser profile paths");
        auto writePreferences = [&](int state) {
            Json data={{"extensions",{{"settings",{{ExtensionId,{{"path","connector"},{"state",state}}}}}}}};
            writeFile(preferences,data.dump()+(state?"":" "));
        };
        auto readStatus = [&] {
            const auto browsers=discoverBrowsers();
            for(const auto &browser:browsers)if(wide(browser.exe)==executablePath.wstring())
                for(const auto &entry:browser.profiles)if(wide(entry.path)==profile.wstring())return entry.status;
            throw std::runtime_error("Synthetic browser profile was not discovered");
        };
        writePreferences(1);
        if(readStatus()!="Расширение установлено" || readStatus()!="Расширение установлено")
            throw std::runtime_error("Installed extension was not detected");
        writePreferences(0);
        if(readStatus()!="Расширение установлено · выключено")
            throw std::runtime_error("Changed profile was not re-read");
        writePreferences(1);
        if(readStatus()!="Расширение установлено")
            throw std::runtime_error("Re-enabled extension was not detected");
        cleanup();
        std::cout << "Browser profile status refreshes after changes\n";
        return 0;
    } catch(const std::exception &e) {
        cleanup();
        std::cerr << e.what() << '\n';
        return 1;
    }
}
