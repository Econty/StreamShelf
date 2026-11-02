#include "extension_guide_state.hpp"
#include <cstdlib>
#include <iostream>
using namespace ss;

static void expect(bool ok, const char *message) {
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
int main() {
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
    resumed.observe(page, 7); expect(resumed.stage == GuideStage::Developer, "developer mode takes priority");
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
    GuideFlow alreadyEnabled; page.enabled = true;
    alreadyEnabled.observe(page, 15); expect(alreadyEnabled.stage == GuideStage::Connect, "fresh guide resumes at enabled extension");
    GuideFlow unknownState; page.enabledKnown = false;
    unknownState.observe(page, 16); expect(unknownState.stage == GuideStage::Enable, "unknown extension state asks for enable check");
    GuideObservation confirm{}; confirm.confirmationDialog = true; confirm.installedCard = confirm.enabledKnown = true; confirm.enabled = false;
    alreadyEnabled.observe(confirm, 17); expect(alreadyEnabled.stage == GuideStage::Enable, "browser enable confirmation stays on enable step");
    page.developerOn = false; page.enabledKnown = true; page.enabled = false;
    alreadyEnabled.observe(page, 18); expect(alreadyEnabled.stage == GuideStage::Developer, "turning developer mode off always takes precedence");
    GuideFlow removed; page.developerOn = true; page.installedCard = true; page.enabled = false;
    removed.observe(page, 19); expect(removed.stage == GuideStage::Enable, "installed disabled before removal");
    page.installedCard = false; page.enabledKnown = false;
    removed.observe(page, 20); expect(removed.stage == GuideStage::Enable, "temporary missing card is not removal");
    page.installedAbsent = true; removed.observe(page, 21); expect(removed.stage == GuideStage::Load, "confirmed removal returns to load step");
    GuideFlow installing; installing.observe(GuideObservation{true,true,true}, 22);
    installing.observe(folder, 23); installing.observe(GuideObservation{true,true,true}, 24);
    auto absentDuringInstall = page; absentDuringInstall.installedAbsent = true;
    installing.observe(absentDuringInstall, 25); expect(installing.stage == GuideStage::Verify, "absence during installation cannot cause a load loop");
    std::cout << "PASS independent guide stages and resume transitions\n";
}
