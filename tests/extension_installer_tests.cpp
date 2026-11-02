#include "extension_installer.hpp"
#include "extension_guide_state.hpp"
#include "extension_presence.hpp"
#include "uia_result.hpp"
#include <iostream>
#include <shellapi.h>
using namespace ss;
int main() {
    int checks = 0;
    auto check = [&](bool ok, const char *label) {
        if (!ok) throw std::runtime_error(label);
        ++checks; std::cout << "PASS " << label << '\n';
    };
    try {
        check(!verifiedExtensionPage(4, true, true), "Typed address before Enter is never verified as a loaded page");
        check(!verifiedExtensionPage(5, true, false), "After Enter, URL without extensions-page controls is still unverified");
        check(verifiedExtensionPage(5, true, true), "Only submitted URL and actual extensions controls verify navigation");
        check(!browserToolbarButton(420, 73, 1300, 115, 0, 0, 1920, 1080, 96, true), "search/address bar labelled Extensions is never selected as toolbar button");
        check(!browserToolbarButton(180, 220, 220, 260, 0, 0, 1920, 1080, 96, true), "page toolbar is not a browser extensions button");
        check(browserToolbarButton(1720, 75, 1760, 115, 0, 0, 1920, 1080, 96, true), "compact upper-right browser toolbar control is eligible");
        check(!browserToolbarButton(1720, 75, 1760, 115, 0, 0, 1920, 1080, 96, false), "unscoped control is not a browser toolbar button");
        using installer_detail::hasInterface;
        int *missing = nullptr, object = 42;
        check(!hasInterface(S_OK, missing), "Chrome S_OK plus null COM interface is unavailable, not dereferenced");
        auto provide = [&]() -> HRESULT { missing = &object; return S_OK; };
        check(hasInterface(provide(), missing), "COM out variable is read after call regardless of argument evaluation order");
        check(!hasInterface(E_FAIL, missing), "Failed COM result cannot become available");
        GuideFlow flow;
        flow.observe({}, 100);
        check(flow.stage == GuideStage::Developer, "Unavailable provider does not advance guide");
        GuideObservation page; page.page = page.developerKnown = true;
        flow.observe(page, 200);
        check(flow.stage == GuideStage::Developer, "Developer mode off waits for checkbox");
        page.developerOn = true; flow.observe(page, 300);
        check(flow.stage == GuideStage::Load, "Checked developer mode automatically advances to load button");
        flow.observe({}, 400);
        check(flow.stage == GuideStage::Load, "Focus changes do not reset step");
        GuideObservation picker; picker.folderDialog = true;
        flow.observe(picker, 500);
        check(flow.stage == GuideStage::Folder, "Opening picker automatically advances to folder navigation");
        flow.observe(picker, 600);
        check(flow.stage == GuideStage::Folder, "Wrong folder does not advance to confirmation");
        picker.folderReady = true; flow.observe(picker, 700);
        check(flow.stage == GuideStage::Confirm, "Observed extension folder advances to select-folder button");
        flow.observe(page, 800);
        check(flow.stage == GuideStage::Verify, "Dialog closing waits for actual installed card");
        flow.observe(page, 2500);
        check(flow.stage == GuideStage::Verify, "Slow card detection does not restart installation after 1.6 seconds");
        flow.observe(page, 120000);
        check(flow.stage == GuideStage::Verify, "Unknown install state never loops back to load");
        page.installedCard = true; flow.observe(page, 2600);
        check(flow.stage == GuideStage::Enable, "Installed extension with unknown state gets an explicit enable step");
        page.enabledKnown = true; page.enabled = false; flow.observe(page, 2700);
        check(flow.stage == GuideStage::Enable, "Disabled installed extension waits for its enable toggle");
        auto transient = page; transient.installedCard = false; flow.observe(transient, 3000);
        check(flow.stage == GuideStage::Enable, "Temporary missing card cannot restart installation");
        page.enabled = true; flow.observe(page, 3100);
        check(flow.stage == GuideStage::Connect, "Only enabled extension advances to connection");
        flow.observe(transient, 3200);
        check(flow.stage == GuideStage::Connect, "Missing snapshot cannot move an installed extension back to loading");
        page.enabled = false; flow.observe(page, 3300);
        check(flow.stage == GuideStage::Enable, "Turning extension off restores enable step");
        page.enabled = true;
        flow.stage = GuideStage::Done; page.installedCard = false; flow.observe(page, 3000);
        check(flow.stage == GuideStage::Done, "Confirmed native connection remains complete");
        GuideFlow existing; page.installedCard = true; existing.observe(page, 100);
        check(existing.stage == GuideStage::Connect, "Previously installed extension skips redundant installation");
        page.developerOn = false; page.enabled = false; existing.observe(page, 110);
        check(existing.stage == GuideStage::Developer, "Developer mode has priority over disabled installed extension");
        page.enabled = true; existing.observe(page, 120);
        check(existing.stage == GuideStage::Developer, "Even enabled installed extension waits for developer mode");
        page.developerOn = true; page.enabled = false; existing.observe(page, 130);
        check(existing.stage == GuideStage::Enable, "Developer mode confirmation resumes enable step without reloading");
        GuideFlow unknownDeveloper; auto unknownPage = page; unknownPage.developerKnown = false;
        unknownDeveloper.observe(unknownPage, 140);
        check(unknownDeveloper.stage == GuideStage::Developer, "Unverified developer checkbox cannot skip prerequisite");
        GuideFlow cancelled; cancelled.stage = GuideStage::Verify; cancelled.retryInstallation();
        cancelled.observe(page = GuideObservation{true, true, true}, 200);
        check(cancelled.stage == GuideStage::Load, "Retrying loading requires an explicit retry action");
        Json prefs = {{"extensions", {{"settings", {{ExtensionId, {{"path", "C:/Connector"}, {"disable_reasons", Json::array()}}}}}}}};
        auto presence = extensionPresence(prefs);
        check(presence.installed && presence.enabledKnown && presence.enabled, "Modern unpacked extension with no manifest is recognized by fixed ID and path");
        prefs["extensions"]["settings"][ExtensionId]["disable_reasons"] = {1}; presence = extensionPresence(prefs);
        check(presence.installed && presence.enabledKnown && !presence.enabled, "Disabled extension is distinct from absent extension");
        prefs["extensions"]["settings"][ExtensionId] = Json::object();
        check(!extensionPresence(prefs).installed, "Empty Yandex preference tombstone is not an installed extension");
        check(!extensionPresence(Json::object()).installed, "Missing settings remain unknown rather than installation success");
        check(guidedBrowserProduct(L"Google Chrome", L"chrome.exe") == GuidedBrowser::Chrome, "Chrome supported");
        check(guidedBrowserProduct(L"Yandex", L"browser.exe") == GuidedBrowser::Yandex, "Yandex supported");
        check(guidedBrowserProduct(L"Yandex Browser", L"BROWSER.EXE") == GuidedBrowser::Yandex, "Yandex product variant");
        check(guidedBrowserProduct(L"Firefox", L"firefox.exe") == GuidedBrowser::Firefox, "Firefox product recognized");
        check(extensionPageUrl(GuidedBrowser::Firefox) == L"about:addons", "Firefox opens add-ons manager");
        check(extensionLaunchArguments(GuidedBrowser::Firefox, nullptr).front() == L"-new-window", "Firefox uses its own launch arguments");
        check(guidedBrowserProduct(L"ЯНДЕКС", L"BROWSER.EXE") == GuidedBrowser::Yandex, "Cyrillic labels normalize independently of C locale");
        for (const auto &product : {L"Chromium", L"Microsoft Edge", L"Brave", L"Opera", L"Vivaldi", L"Firefox", L""})
            check(guidedBrowserProduct(product, L"chrome.exe") == GuidedBrowser::None, "Other browsers manual only");
        check(guidedBrowserProduct(L"Google Chrome", L"msedge.exe") == GuidedBrowser::None, "Executable must match product");
        auto chrome = extensionLaunchArguments(GuidedBrowser::Chrome, nullptr);
        auto yandex = extensionLaunchArguments(GuidedBrowser::Yandex, nullptr);
        check(extensionPageUrl(GuidedBrowser::Chrome) == L"chrome://extensions/", "Chrome extensions URL");
        check(extensionPageUrl(GuidedBrowser::Yandex) == L"browser://extensions/", "Yandex extensions URL");
        check(chrome.back() == L"about:blank" && yandex.back() == L"about:blank", "Browser starts with a URL accepted by running instances");
        Profile profile{"Profile 2", "C:\\Users\\Арсений\\Browser Data\\User Data\\Profile 2", ""};
        auto args = extensionLaunchArguments(GuidedBrowser::Chrome, &profile);
        check(std::find(args.begin(), args.end(), L"--profile-directory=Profile 2") != args.end(), "Selected profile preserved");
        check(std::find(args.begin(), args.end(), L"--user-data-dir=C:\\Users\\Арсений\\Browser Data\\User Data") != args.end(), "Unicode profile root preserved");
        std::wstring command = L"browser.exe";
        for (auto &arg : args) command += L" " + quoteWide(arg);
        int argc = 0; auto argv = CommandLineToArgvW(command.c_str(), &argc);
        check(argv && argc == int(args.size()) + 1, "Arguments roundtrip with spaces and Unicode");
        for (int i = 1; i < argc; ++i) check(argv[i] == args[i - 1], "Argument exact roundtrip");
        LocalFree(argv);
        bool rejected = false;
        try { extensionLaunchArguments(GuidedBrowser::None, nullptr); } catch (...) { rejected = true; }
        check(rejected, "Unsupported browser cannot launch wizard");
        for (auto path : {"C:\\User Data\\Bad Profile", "C:\\User Data\\Profile 2 --load-extension=x", "C:\\Other\\Default"}) {
            Profile custom{"custom", path, ""}; rejected = false;
            try { extensionLaunchArguments(GuidedBrowser::Chrome, &custom); } catch (...) { rejected = true; }
            check(rejected, "Nonstandard profile refused before launch");
        }
        Browser installed{"Chrome", "chromium", "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe"};
        if (fs::exists(wide(installed.exe)))
            check(guidedBrowser(installed) == GuidedBrowser::Chrome, "Installed Chrome product metadata recognized");
        Browser installedYandex{"Яндекс", "chromium", "C:\\Program Files\\Yandex\\YandexBrowser\\Application\\browser.exe"};
        if (fs::exists(wide(installedYandex.exe)))
            check(guidedBrowser(installedYandex) == GuidedBrowser::Yandex, "Installed Yandex product metadata recognized");
        ExtensionInstaller installer;
        check(!installer.active(), "Installer starts inactive");
        std::cout << checks << " installer checks passed\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
