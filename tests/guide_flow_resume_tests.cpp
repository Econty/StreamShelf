#include "extension_guide_state.hpp"
#include "guide_overlay_motion.hpp"
#include <cstdlib>
#include <iostream>
using namespace ss;

static void expect(bool ok, const char *message) {
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
int main() {
    const GuideOverlayBox start{10, 20, 80, 70}, end{710, 220, 810, 295};
    for (double time : {.1, .25, .5, .75, .9}) {
        const auto box = guideOverlayInterpolate(start, end, time);
        const auto x = (box.left - start.left) / (end.left - start.left);
        const auto y = (box.top - start.top) / (end.top - start.top);
        expect(x > 0 && x < 1 && std::abs(x - y) < 1e-10, "both axes move together along the diagonal");
    }
    expect(guideOverlayEase(.1) < .1 && guideOverlayEase(.9) > .9 &&
           guideOverlayEase(.5) - guideOverlayEase(.4) > guideOverlayEase(.2) - guideOverlayEase(.1),
           "the overlay accelerates and brakes smoothly");
    expect(guideOverlayFade(0) == 255 && guideOverlayFade(120) == 128 && guideOverlayFade(240) == 0,
           "the same per-pixel surface can fade from opaque to fully transparent");
    const auto opening = guideOverlayOpeningBox(end, 4.0);
    expect(opening.left > end.left && opening.top > end.top &&
           opening.left + opening.right == end.left + end.right &&
           opening.top + opening.bottom == end.top + end.bottom &&
           guideOverlayInterpolate(opening, end, 1).right == end.right,
           "the border opens outward from the center of the same target");
    expect(guideOverlayReveal(0) == 0 && guideOverlayReveal(140) == 128 &&
           guideOverlayReveal(280) == 255 && guideOverlayReveal(0, 110) == 110 &&
           guideOverlayFade(0, 110) == 110 && guideOverlayFade(120, 110) == 55,
           "interrupting the reveal or fade preserves the current opacity");
    expect(guideWaitForProbePage(300, true, false) &&
           !guideWaitForProbePage(300, true, true) &&
           guideWaitForProbePage(1999, false, true) &&
           !guideWaitForProbePage(2500, false, false),
           "allow the page to paint before advancing without waiting indefinitely");
    expect(guideWaitSeconds(100, 30'100) == 30, "countdown starts with the full wait");
    expect(guideWaitSeconds(101, 30'100) == 30 && guideWaitSeconds(1'100, 30'100) == 29, "partial seconds round up");
    expect(guideWaitSeconds(30'099, 30'100) == 1 && guideWaitSeconds(30'100, 30'100) == 0 &&
           guideWaitSeconds(30'101, 30'100) == 0 && guideWaitSeconds(30'101, 0) == 0, "expired and unset waits do not show a countdown");
    expect(guideOmniboxBounds(320, 1022, 1320, 1060, 0, 0, 1920, 1080, 96, true) &&
           !guideOmniboxBounds(320, 1022, 1320, 1060, 0, 0, 1920, 1080, 96, false) &&
           !guideOmniboxBounds(320, 490, 1320, 530, 0, 0, 1920, 1080, 96, true),
           "only Yandex permits browser controls at the lower edge, never inside the page");
    expect(yandexOmniboxIdentity(L"", L"умная строка") && !yandexOmniboxIdentity(L"", L"поиск"),
           "generic search fields cannot replace SmartBox identity");
    expect(extensionsTabTitle(L"дополнения — яндекс браузер", false, true) &&
           !extensionsTabTitle(L"дополнения — яндекс браузер"), "Yandex-specific tab candidates");
    expect(guideInternalPage(L"browser://extensions/", L"browser://extensions") &&
           !guideInternalPage(L"browser://extensions.example/", L"browser://extensions") &&
           !guideInternalPage(L"browser://tune/", L"browser://extensions"), "extension manager address boundaries");
    const std::wstring probeTitle = L"streamshelf 0123456789abcdef0123456789abcdef";
    expect(guideYandexProbeCaption(probeTitle, probeTitle) &&
           guideYandexProbeCaption(probeTitle + L" — яндекс браузер", probeTitle) &&
           !guideYandexProbeCaption(L"streamshelf", probeTitle) &&
           !guideYandexProbeCaption(probeTitle + L"0 — яндекс браузер", probeTitle),
           "only the exact probe title identifies the active Yandex tab");
    expect(guideDeveloperModeLabel(L"режим разработчика") &&
           guideDeveloperModeWarning(L"включите режим разработчика, чтобы использовать это расширение") &&
           guideDeveloperModeWarning(L"to use this extension that can't be verified by the chrome web store, turn on developer mode") &&
           !guideDeveloperModeWarning(L"режим разработчика") &&
           !guideDeveloperModeWarning(L"turn on developer mode"), "switch label differs from the blocked-card warning");
    expect(guideDeveloperModeSwitch(1694,158,1853,174, 1873,155,1905,176,96) &&
           guideDeveloperModeSwitch(1694,178,1853,195, 1873,176,1905,196,96) &&
           !guideDeveloperModeSwitch(1694,158,1853,174, 946,744,980,768,96),
           "find the developer switch next to its label without selecting the disabled card switch");
    expect(guideConnectorWarningPosition(619,612,626, 648,643,96) &&
           guideConnectorWarningPosition(619,633,649, 648,664,96) &&
           !guideConnectorWarningPosition(619,612,626, 1168,643,96), "warning belongs to the connector's own card");
    expect(extensionsTabTitle(L"вкладка \"расширения\" использует 64,4 мб памяти"), "Chrome Russian memory label preserves extension tab candidate");
    expect(extensionsTabTitle(L"вкладка \"extensions\" использует 64,4 мб памяти"), "mixed Russian browser and English page title are recognized");
    expect(extensionsTabTitle(L"tab \"расширения\" is using 64.4 mb of memory"), "mixed English browser and Russian page title are recognized");
    expect(!extensionsTabTitle(L"вкладка \"расширения для файлов\" использует 64,4 мб памяти"), "memory wrapper cannot turn an unrelated page into extension tab");
    expect(!extensionsTabTitle(L"вкладка \"расширения\" неизвестный текст"), "unknown title wrapper remains untrusted");
    expect(extensionsTabTitle(L"add-ons manager — mozilla firefox", true), "Firefox English title supports Unicode title separator");
    expect(cardFooterSwitch(947,625,980,645, 505,405,1005,665, 640,613,742,657,120,true), "visible compact switch on Connector card is found");
    expect(!cardFooterSwitch(753,613,857,657, 505,405,1005,665, 640,613,742,657,120,true), "errors button is not the switch");
    expect(!cardFooterSwitch(947,899,980,919, 505,405,1005,665, 640,613,742,657,120,true), "switch belonging to another card is ignored");
    expect(!cardFooterSwitch(947,625,980,645, 505,405,1005,665, 640,613,742,657,120,false), "nonfocusable decorative element is ignored");
    expect(cardFooterSwitch(947,629,980,649, 505,409,1005,669, 658,620,719,658,120,true), "Yandex disabled Connector switch matches its own card");
    expect(!cardFooterSwitch(1341,487,1465,552, 505,409,1005,669, 658,620,719,658,120,true), "guide panel is never treated as Connector switch");
    GuideObservation page{}; page.page = page.developerKnown = true;
    GuideFlow f;
    f.observe(page, 1); expect(f.stage == GuideStage::Developer, "developer mode off");
    page.developerOn = true; f.observe(page, 2); expect(f.stage == GuideStage::Load, "fresh install after developer mode");
    GuideObservation folder{}; folder.folderDialog = folder.folderReady = true;
    f.observe(folder, 3); expect(f.stage == GuideStage::Confirm, "folder ready");
    f.observe(page, 4); expect(f.stage == GuideStage::Verify, "picker closes before card");
    page.installedCard = page.enabledKnown = true; page.enabled = false;
    f.observe(page, 5); expect(f.stage == GuideStage::Enable, "newly installed disabled extension");
    page.enabled = true; f.observe(page, 6); expect(f.stage == GuideStage::Connect, "enabled extension");

    GuideFlow resumed; page.developerOn = false; page.enabled = false;
    resumed.observe(page, 7); expect(resumed.stage == GuideStage::Enable, "installed extension does not require developer mode");
    auto transient = page; transient.developerOn = true; transient.installedCard = false; transient.enabledKnown = false;
    resumed.observe(transient, 8); expect(resumed.stage == GuideStage::Enable, "previously observed disabled extension survives page refresh");
    page.developerOn = true; resumed.observe(page, 9); expect(resumed.stage == GuideStage::Enable, "resume directly at disabled extension");
    page.enabled = true; resumed.observe(page, 10); expect(resumed.stage == GuideStage::Connect, "resume at connection");
    resumed.observe(transient, 11); expect(resumed.stage == GuideStage::Connect, "transient missing card does not regress");
    page.enabled = false; resumed.observe(page, 12); expect(resumed.stage == GuideStage::Enable, "disabled again is reflected");
    resumed.retryInstallation(); page.installedCard = false; page.enabledKnown = false;
    resumed.observe(page, 13); expect(resumed.stage == GuideStage::Load, "explicit retry resets remembered installation");
    GuideFlow alreadyDisabled; page.installedCard = page.enabledKnown = true; page.enabled = false;
    alreadyDisabled.observe(page, 14); expect(alreadyDisabled.stage == GuideStage::Enable, "fresh guide resumes at disabled extension");
    auto blockedByDeveloperMode = page;
    blockedByDeveloperMode.developerOn = false;
    blockedByDeveloperMode.developerRequired = true;
    alreadyDisabled.observe(blockedByDeveloperMode, 14);
    expect(alreadyDisabled.stage == GuideStage::Developer, "browser warning returns the installed connector to developer mode");
    alreadyDisabled.observe(page, 15);
    expect(alreadyDisabled.stage == GuideStage::Enable, "after developer mode the existing connector can be enabled");
    GuideFlow alreadyEnabled; page.enabled = true;
    alreadyEnabled.observe(page, 15); expect(alreadyEnabled.stage == GuideStage::Connect, "fresh guide resumes at enabled extension");
    GuideFlow unknownState; page.enabledKnown = false;
    unknownState.observe(page, 16); expect(unknownState.stage == GuideStage::Enable, "unknown extension state asks for enable check");
    GuideObservation confirm{}; confirm.confirmationDialog = true; confirm.installedCard = confirm.enabledKnown = true; confirm.enabled = false;
    alreadyEnabled.observe(confirm, 17); expect(alreadyEnabled.stage == GuideStage::Enable, "browser enable confirmation stays on enable step");
    page.developerOn = false; page.enabledKnown = true; page.enabled = false;
    alreadyEnabled.observe(page, 18); expect(alreadyEnabled.stage == GuideStage::Enable, "turning developer mode off does not hide installed extension");
    GuideFlow removed; page.developerOn = true; page.installedCard = true; page.enabled = false;
    removed.observe(page, 19); expect(removed.stage == GuideStage::Enable, "installed disabled before removal");
    page.installedCard = false; page.enabledKnown = false;
    removed.observe(page, 20); expect(removed.stage == GuideStage::Enable, "temporary missing card is not removal");
    page.installedAbsent = true; removed.observe(page, 21); expect(removed.stage == GuideStage::Load, "confirmed removal returns to load step");
    GuideFlow installing; installing.observe(GuideObservation{true,true,true}, 22);
    installing.observe(folder, 23); installing.observe(GuideObservation{true,true,true}, 24);
    auto absentDuringInstall = page; absentDuringInstall.installedAbsent = true;
    installing.observe(absentDuringInstall, 25); expect(installing.stage == GuideStage::Verify, "absence during installation cannot cause a load loop");
    installing.observe(absentDuringInstall, 3024); expect(installing.stage == GuideStage::Load, "stable absence after cancelled installation allows retry");
    GuideFlow manual; GuideObservation installed{}; installed.page = installed.installedCard = installed.enabledKnown = installed.enabled = true;
    manual.observe(installed, 100); expect(manual.stage == GuideStage::Connect, "manually installed enabled extension skips prior steps");
    auto absent = installed; absent.installedCard = false; absent.installedAbsent = true; absent.enabledKnown = false;
    manual.observe(absent, 101); expect(manual.stage == GuideStage::Developer, "confirmed removal with developer mode unknown requests developer mode");
    manual.observe(installed, 102); expect(manual.stage == GuideStage::Connect, "reinstallation outside guide resumes connection");
    std::cout << "PASS independent guide stages and resume transitions\n";
}
