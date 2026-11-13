#pragma once
#include <cstdint>
#include <string>
namespace ss {
enum class FirefoxGuideStage { OpenManager, OpenMenu, InstallFile, SelectFile, ConfirmInstall, Enable, Connect, Done };
struct FirefoxGuideObservation {
    bool manager = false, menu = false, picker = false, installPrompt = false, addedNotice = false, awaitingInstall = false;
    bool installedKnown = false, installed = false, enabledKnown = false, enabled = false;
    bool extensionRunning = false, connectionConfirmed = false, removedNotice = false;
};
inline bool firefoxRemovedNotice(const std::wstring &name) {
    if (name.find(L"streamshelf connector") == std::wstring::npos) return false;
    for (auto phrase : {L"удален", L"удалён", L"удалено", L"удалены", L"was removed", L"has been removed"})
        if (name.find(phrase) != std::wstring::npos) return true;
    return false;
}
enum class FirefoxPickerAction { Navigate, SelectFile, OpenFile, Wait };
// A browser Open dialog is only a possible add-on picker immediately after
// the installation menu was observed in the target browser window. A dialog
// from any other window must not advance the guide.
struct FirefoxPickerGuard {
    uint64_t menuSeenAt = 0;
    bool accepted = false;
    void observeMenu(bool manager, bool menu, uint64_t now) {
        if (manager && menu) menuSeenAt = now;
    }
    bool observePicker(bool picker, bool sameOwner, uint64_t now) {
        if (!picker) {
            if (accepted) { accepted = false; menuSeenAt = 0; }
            return false;
        }
        if (!sameOwner) { accepted = false; menuSeenAt = 0; return false; }
        if (accepted) return true;
        if (menuSeenAt && now >= menuSeenAt && now - menuSeenAt <= 5000) return accepted = true;
        return false;
    }
};
inline FirefoxPickerAction firefoxPickerAction(bool folderReady, bool fileFound, bool fileSelected, bool filenameReady) {
    if (filenameReady || fileSelected) return FirefoxPickerAction::OpenFile;
    if (fileFound) return FirefoxPickerAction::SelectFile;
    return folderReady ? FirefoxPickerAction::Wait : FirefoxPickerAction::Navigate;
}
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
    // A visible removal notice is newer evidence than a cached profile entry or session.
    if (o.removedNotice) {
        if (o.menu) return FirefoxGuideStage::InstallFile;
        return o.manager ? FirefoxGuideStage::OpenMenu : FirefoxGuideStage::OpenManager;
    }
    if (o.installedKnown && o.installed && o.enabledKnown && !o.enabled) return FirefoxGuideStage::Enable;
    if (o.extensionRunning || (o.installedKnown && o.installed && o.enabledKnown && o.enabled)) return FirefoxGuideStage::Connect;
    if (o.installedKnown && o.installed && !o.enabledKnown) return FirefoxGuideStage::Enable;
    if (o.awaitingInstall) return FirefoxGuideStage::ConfirmInstall;
    if (o.menu) return FirefoxGuideStage::InstallFile;
    if (o.manager) return FirefoxGuideStage::OpenMenu;
    return FirefoxGuideStage::OpenManager;
}
}
