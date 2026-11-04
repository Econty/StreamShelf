#pragma once
namespace ss {
enum class FirefoxGuideStage { OpenManager, OpenMenu, InstallFile, SelectFile, ConfirmInstall, Enable, Connect, Done };
struct FirefoxGuideObservation {
    bool manager = false, menu = false, picker = false, installPrompt = false, addedNotice = false, awaitingInstall = false;
    bool installedKnown = false, installed = false, enabledKnown = false, enabled = false;
    bool extensionRunning = false, connectionConfirmed = false;
};
// Match a switch on the same row as the actual Firefox add-on title.
inline bool firefoxCardSwitch(int tl, int tt, int tr, int tb, int sl, int st, int sr, int sb, unsigned dpi) {
    const int scale = dpi ? int(dpi) : 96;
    const int maxDx = 1000 * scale / 96, maxSize = 100 * scale / 96, maxDy = 35 * scale / 96;
    if (tr <= tl || tb <= tt || sr <= sl || sb <= st || sl < tr || sl - tr > maxDx) return false;
    if (sr-sl > maxSize || sb-st > maxSize) return false;
    int dy = (st + sb) - (tt + tb);
    return dy >= -2 * maxDy && dy <= 2 * maxDy;
}
// Unknown accessibility/profile state never counts as successful installation.
inline FirefoxGuideStage firefoxGuideStage(const FirefoxGuideObservation &o) {
    if (o.connectionConfirmed) return FirefoxGuideStage::Done;
    if (o.installPrompt || o.addedNotice) return FirefoxGuideStage::ConfirmInstall;
    if (o.picker) return FirefoxGuideStage::SelectFile;
    if (o.installedKnown && o.installed && o.enabledKnown && !o.enabled) return FirefoxGuideStage::Enable;
    if (o.extensionRunning || (o.installedKnown && o.installed && o.enabledKnown && o.enabled)) return FirefoxGuideStage::Connect;
    if (o.installedKnown && o.installed && !o.enabledKnown) return FirefoxGuideStage::Enable;
    if (o.awaitingInstall) return FirefoxGuideStage::ConfirmInstall;
    if (o.menu) return FirefoxGuideStage::InstallFile;
    if (o.manager) return FirefoxGuideStage::OpenMenu;
    return FirefoxGuideStage::OpenManager;
}
}
