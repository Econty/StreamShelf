#pragma once
namespace ss {
enum class FirefoxGuideStage { OpenManager, OpenMenu, InstallFile, SelectFile, ConfirmInstall, Enable, Connect, Done };
struct FirefoxGuideObservation {
    bool manager = false, menu = false, picker = false, installPrompt = false, awaitingInstall = false;
    bool installedKnown = false, installed = false, enabledKnown = false, enabled = false;
    bool extensionRunning = false, connectionConfirmed = false;
};
// Unknown accessibility/profile state never counts as successful installation.
inline FirefoxGuideStage firefoxGuideStage(const FirefoxGuideObservation &o) {
    if (o.connectionConfirmed) return FirefoxGuideStage::Done;
    if (o.installPrompt) return FirefoxGuideStage::ConfirmInstall;
    if (o.picker) return FirefoxGuideStage::SelectFile;
    if (o.installedKnown && o.installed && o.enabledKnown && !o.enabled) return FirefoxGuideStage::Enable;
    if (o.extensionRunning || (o.installedKnown && o.installed && o.enabledKnown && o.enabled)) return FirefoxGuideStage::Connect;
    if (o.awaitingInstall) return FirefoxGuideStage::ConfirmInstall;
    if (o.menu) return FirefoxGuideStage::InstallFile;
    if (o.manager) return FirefoxGuideStage::OpenMenu;
    return FirefoxGuideStage::OpenManager;
}
}
