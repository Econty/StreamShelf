#pragma once
#include <cstdint>
#include <string>
namespace ss {
inline bool verifiedExtensionPage(int navigationPhase, bool internalUrl, bool pageControls) {
    return navigationPhase == 5 && internalUrl && pageControls;
}
// A selected tab is only a candidate until its URL and page controls are checked.
inline bool extensionsTabTitle(const std::wstring &name, bool firefox = false) {
    if (firefox && (name == L"управление дополнениями" || name.rfind(L"управление дополнениями — ", 0) == 0 || name.rfind(L"управление дополнениями - ", 0) == 0 || name == L"add-ons manager" || name.rfind(L"add-ons manager - ", 0) == 0 || name == L"addons manager")) return true;
    for (const auto *title : {L"расширения", L"extensions"}) {
        std::wstring prefix(title);
        if (name == prefix || name.rfind(prefix + L" - ", 0) == 0 ||
            name.rfind(prefix + L" — ", 0) == 0 || name.rfind(prefix + L" – ", 0) == 0) return true;
    }
    return false;
}
inline bool connectorSetupReady(bool currentRevision, bool hasSiteAccess) {
    return currentRevision && hasSiteAccess;
}
// A browser's internal page may itself contain a toolbar and a button named
// "Extensions". Accept only small controls in the upper-right browser chrome;
// otherwise leave the target unknown and show manual instructions.
inline bool browserToolbarButton(int left, int top, int right, int bottom,
                                 int windowLeft, int windowTop, int windowRight, int windowBottom,
                                 unsigned dpi, bool inToolbar) {
    if (!inToolbar || windowRight <= windowLeft || windowBottom <= windowTop) return false;
    int scale = int(dpi ? dpi : 96);
    int maxSize = 80 * scale / 96, maxTop = 160 * scale / 96;
    int width = right - left, height = bottom - top;
    return width >= 8 && height >= 8 && width <= maxSize && height <= maxSize &&
        width <= height * 2 && left >= windowLeft + (windowRight - windowLeft) * 3 / 4 &&
        top >= windowTop && bottom <= windowTop + maxTop && right <= windowRight;
}
// Match only a compact, focusable element at the right edge of the same card
// and on the same row as that card's Remove button. Never guess coordinates.
inline bool cardFooterSwitch(int left, int top, int right, int bottom,
                             int cardLeft, int cardTop, int cardRight, int cardBottom,
                             int removeLeft, int removeTop, int removeRight, int removeBottom,
                             unsigned dpi, bool focusable) {
    int scale = int(dpi ? dpi : 96), w = right - left, h = bottom - top;
    int cardW = cardRight - cardLeft, cardH = cardBottom - cardTop;
    if (!focusable || cardW < 220 * scale / 96 || cardW > 1100 * scale / 96 ||
        cardH < 100 * scale / 96 || cardH > 650 * scale / 96 ||
        w < 8 * scale / 96 || h < 8 * scale / 96 ||
        w > 80 * scale / 96 || h > 60 * scale / 96) return false;
    if (left < cardLeft || right > cardRight || top < cardTop || bottom > cardBottom ||
        removeLeft < cardLeft || removeRight > cardRight || removeTop < cardTop || removeBottom > cardBottom ||
        left < removeRight || (left + right) / 2 < cardLeft + cardW * 4 / 5) return false;
    int offset = (top + bottom - removeTop - removeBottom) / 2;
    if (offset < 0) offset = -offset;
    return offset <= 20 * scale / 96;
}
// Some browser UIA trees flatten generic card containers. Associate the title,
// Remove button and compact switch only when their relative positions agree.
inline bool flattenedCardSwitch(int titleLeft, int titleTop, int titleRight, int titleBottom,
                                int removeLeft, int removeTop, int removeRight, int removeBottom,
                                int left, int top, int right, int bottom, unsigned dpi) {
    const int scale = int(dpi ? dpi : 96);
    const int width = right - left, height = bottom - top;
    if (titleRight <= titleLeft || titleBottom <= titleTop || removeRight <= removeLeft || removeBottom <= removeTop ||
        width < 8 * scale / 96 || width > 80 * scale / 96 || height < 8 * scale / 96 || height > 60 * scale / 96) return false;
    const int removeDx = removeLeft - titleLeft, removeDy = (removeTop + removeBottom - titleTop - titleBottom) / 2;
    if (removeDx < -140 * scale / 96 || removeDx > 220 * scale / 96 ||
        removeDy < 40 * scale / 96 || removeDy > 350 * scale / 96) return false;
    if (left < removeRight + 20 * scale / 96 || left > removeRight + 600 * scale / 96) return false;
    int rowDifference = (top + bottom - removeTop - removeBottom) / 2;
    if (rowDifference < 0) rowDifference = -rowDifference;
    return rowDifference <= 20 * scale / 96;
}
enum class GuideStage { Developer, Load, Folder, Confirm, Verify, Enable, Connect, Done };
struct GuideObservation {
    bool page = false, developerKnown = false, developerOn = false;
    bool folderDialog = false, folderReady = false, installedCard = false;
    bool enabledKnown = false, enabled = false;
    bool confirmationDialog = false;
    bool installedAbsent = false;
};
inline bool guideConnectionRetryDue(GuideStage stage, uint64_t now, uint64_t lastRetry) {
    return stage == GuideStage::Connect && now - lastRetry >= 2000;
}
// Only observed browser state advances the flow; closing a picker is not installation success.
struct GuideFlow {
    GuideStage stage = GuideStage::Developer;
    uint64_t pickerClosedAt = 0;
    bool sawPicker = false;
    bool installedSeen = false;
    bool developerVerified = false;
    bool installedEnabledKnown = false, installedEnabled = false;
    void observe(const GuideObservation &o, uint64_t now) {
        if (o.page && o.installedAbsent && !o.installedCard && !o.folderDialog &&
            stage != GuideStage::Folder && stage != GuideStage::Confirm && stage != GuideStage::Verify) {
            installedSeen = false; installedEnabledKnown = false; installedEnabled = false;
        }
        if (o.installedCard) {
            installedSeen = true;
            if (o.enabledKnown) { installedEnabledKnown = true; installedEnabled = o.enabled; }
        }
        if (o.page && o.developerKnown) developerVerified = o.developerOn;
        if ((o.page || o.confirmationDialog) && !developerVerified) {
            stage = GuideStage::Developer; return;
        }
        if (stage == GuideStage::Done) return;
        if (o.folderDialog) {
            sawPicker = true;
            stage = o.folderReady ? GuideStage::Confirm : GuideStage::Folder;
            return;
        }
        if (!o.page && !o.confirmationDialog) return;
        if (installedSeen) {
            if (installedEnabledKnown) stage = installedEnabled ? GuideStage::Connect : GuideStage::Enable;
            else if (stage != GuideStage::Connect) stage = GuideStage::Enable;
            return;
        }
        if (stage == GuideStage::Folder || stage == GuideStage::Confirm) {
            stage = GuideStage::Verify; pickerClosedAt = now;
        }
        // Missing/transient accessibility data is not evidence of a failed install.
        // Never restart installation merely because a card was not found quickly.
        if (stage == GuideStage::Verify) return;
        if (o.developerKnown) stage = o.developerOn ? GuideStage::Load : GuideStage::Developer;
    }
    void retryInstallation() {
        stage = GuideStage::Developer; installedSeen = false; installedEnabledKnown = false;
        installedEnabled = false; developerVerified = false; sawPicker = false; pickerClosedAt = 0;
    }
    bool expectsPicker() const {
        return stage == GuideStage::Load || stage == GuideStage::Folder || stage == GuideStage::Confirm;
    }
};
} // namespace ss
