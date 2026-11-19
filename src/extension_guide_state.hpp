#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
namespace ss {
constexpr uint64_t guideWaitSeconds(uint64_t now, uint64_t deadline) {
    if (now >= deadline) return 0;
    const auto left = deadline - now;
    return left / 1000 + (left % 1000 != 0);
}
constexpr bool guideWaitForProbePage(uint64_t elapsed, bool extensionSeen, bool pageReady) {
    return (!extensionSeen && elapsed < 2000) || (!pageReady && elapsed < 2500);
}
inline bool guideProcessDescendsFrom(uint32_t process, uint32_t ancestor, const std::map<uint32_t,uint32_t> &parents) {
    if (!process || !ancestor) return false;
    for (int depth = 0; depth < 16 && process; ++depth) {
        if (process == ancestor) return true;
        auto it = parents.find(process);
        if (it == parents.end() || it->second == process) return false;
        process = it->second;
    }
    return false;
}
// A window found solely by executable name does not identify a browser profile.
// Once a profile-specific launch has produced a new window, that exact window
// can be reused for a retry within the same guide session.
inline bool guideCanReuseBrowserWindow(bool explicitProfile, bool windowBoundToLaunch) {
    return !explicitProfile || windowBoundToLaunch;
}
inline bool verifiedExtensionPage(int navigationPhase, bool internalUrl, bool pageControls) {
    return navigationPhase == 5 && internalUrl && pageControls;
}
// A selected tab is only a candidate until its URL and page controls are checked.
inline bool extensionsTabTitle(const std::wstring &name, bool firefox = false, bool yandex = false) {
    std::wstring title = name;
    // Chromium can add a localized memory-usage description to the UIA tab
    // name. The browser language and page title can differ. Strip only the
    // known wrapper; the selected tab still requires URL/page verification.
    for (const auto *label : {L"вкладка \"", L"tab \""}) {
        const std::wstring prefix(label);
        if (title.rfind(prefix, 0) != 0) continue;
        const auto end = title.find(L'\"', prefix.size());
        if (end == std::wstring::npos) break;
        const auto suffix = title.substr(end + 1);
        if (suffix.rfind(L" использует ", 0) == 0 || suffix.rfind(L" is using ", 0) == 0)
            title = title.substr(prefix.size(), end - prefix.size());
        break;
    }
    for (const auto *label : {L"расширения", L"extensions", L"дополнения", L"add-ons", L"управление дополнениями", L"add-ons manager", L"addons manager"}) {
        std::wstring prefix(label);
        if (prefix == L"дополнения" || prefix == L"add-ons") {
            if (!yandex) continue;
        } else if (!firefox && prefix != L"расширения" && prefix != L"extensions") continue;
        if (title == prefix || title.rfind(prefix + L" - ", 0) == 0 ||
            title.rfind(prefix + L" — ", 0) == 0 || title.rfind(prefix + L" – ", 0) == 0) return true;
    }
    return false;
}
inline bool guideInternalPage(const std::wstring &address, std::wstring_view prefix) {
    if (!address.starts_with(prefix)) return false;
    return address.size() == prefix.size() || std::wstring_view(L"/?#").find(address[prefix.size()]) != std::wstring_view::npos;
}
inline bool guideOmniboxBounds(int left, int top, int right, int bottom,
                               int windowLeft, int windowTop, int windowRight, int windowBottom,
                               unsigned dpi, bool yandex) {
    const int scale = int(dpi ? dpi : 96);
    const int edge = (yandex ? 180 : 160) * scale / 96;
    return left >= windowLeft && right <= windowRight && top >= windowTop && bottom <= windowBottom &&
           bottom > top && right - left >= 120 * scale / 96 &&
           (bottom <= windowTop + edge || (yandex && top >= windowBottom - edge));
}
inline bool yandexOmniboxIdentity(std::wstring_view id, std::wstring_view name) {
    return id == L"view_1012" || id == L"urlbar-input" || id == L"omnibox" ||
           name == L"умная строка" || name == L"smartbox" || name == L"smart line" ||
           name == L"адресная строка" || name == L"address bar" ||
           name == L"адресная строка и поиск" || name == L"address and search bar";
}
inline bool guideYandexProbeCaption(std::wstring_view caption, std::wstring_view expectedTitle) {
    if (expectedTitle.empty() || !caption.starts_with(expectedTitle)) return false;
    caption.remove_prefix(expectedTitle.size());
    return caption.empty() || caption == L" — яндекс браузер" || caption == L" - яндекс браузер" ||
           caption == L" — yandex browser" || caption == L" - yandex browser" ||
           caption == L" — яндекс" || caption == L" - яндекс" ||
           caption == L" — yandex" || caption == L" - yandex";
}
inline bool guideDeveloperModeLabel(std::wstring_view name) {
    return name == L"режим разработчика" || name == L"developer mode";
}
inline bool guideDeveloperModeWarning(std::wstring_view name) {
    return name.find(L"включите режим разработчика,") != name.npos ||
           name.find(L"turn on developer mode to use") != name.npos ||
           name.find(L"enable developer mode to use") != name.npos ||
           (name.find(L"to use this extension") != name.npos &&
            (name.find(L"turn on developer mode") != name.npos || name.find(L"enable developer mode") != name.npos));
}
inline bool guideDeveloperModeSwitch(int labelLeft, int labelTop, int labelRight, int labelBottom,
                                      int left, int top, int right, int bottom, unsigned dpi) {
    const int scale = int(dpi ? dpi : 96);
    const int width = right - left, height = bottom - top, gap = left - labelRight;
    const int row = top + bottom - labelTop - labelBottom;
    return labelRight > labelLeft && labelBottom > labelTop && width >= 8 && height >= 8 &&
           width <= 75 * scale / 96 && height <= 45 * scale / 96 &&
           gap >= 0 && gap <= 48 * scale / 96 && row >= -24 * scale / 96 && row <= 24 * scale / 96;
}
inline bool guideConnectorWarningPosition(int titleLeft, int titleTop, int titleBottom, int left, int top, unsigned dpi) {
    const int scale = int(dpi ? dpi : 96), inset = left - titleLeft;
    return inset >= -20 * scale / 96 && inset <= 75 * scale / 96 &&
           top >= titleBottom && top - titleTop <= 125 * scale / 96;
}
inline bool connectorPopupButton(const std::wstring &name) {
    return name == L"connect extension" || name == L"подключить расширение" ||
           name == L"подключить текущую вкладку" || name == L"подключить выбранную вкладку";
}
inline bool connectorSetupReady(bool currentRevision, bool hasSiteAccess) {
    return currentRevision && hasSiteAccess;
}
// A browser's internal page may itself contain a toolbar and a button named
// "Extensions". Accept only small controls in the upper-right browser chrome;
// otherwise leave the target unknown and show manual instructions.
inline bool browserToolbarButton(int left, int top, int right, int bottom,
                                 int windowLeft, int windowTop, int windowRight, int windowBottom,
                                 unsigned dpi, bool inToolbar, bool yandex = false) {
    if (!inToolbar || windowRight <= windowLeft || windowBottom <= windowTop) return false;
    int scale = int(dpi ? dpi : 96);
    int maxSize = 80 * scale / 96, maxTop = 160 * scale / 96;
    int width = right - left, height = bottom - top;
    return width >= 8 && height >= 8 && width <= maxSize && height <= maxSize &&
        width <= height * 2 && left >= windowLeft + (windowRight - windowLeft) * 3 / 4 &&
        top >= windowTop && bottom <= windowBottom && right <= windowRight &&
        (bottom <= windowTop + maxTop || (yandex && top >= windowBottom - maxTop));
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
    bool developerRequired = false;
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
        const bool verifyAbsenceReady = stage == GuideStage::Verify && now >= pickerClosedAt &&
            now - pickerClosedAt >= 3000;
        if (o.page && o.installedAbsent && !o.installedCard && !o.folderDialog &&
            stage != GuideStage::Folder && stage != GuideStage::Confirm &&
            (stage != GuideStage::Verify || verifyAbsenceReady)) {
            installedSeen = false; installedEnabledKnown = false; installedEnabled = false;
        }
        if (o.installedCard) {
            installedSeen = true;
            if (o.enabledKnown) { installedEnabledKnown = true; installedEnabled = o.enabled; }
        }
        if (o.page && o.developerKnown) developerVerified = o.developerOn;
        if (stage == GuideStage::Done) return;
        if (o.folderDialog) {
            sawPicker = true;
            stage = o.folderReady ? GuideStage::Confirm : GuideStage::Folder;
            return;
        }
        if (!o.page && !o.confirmationDialog) return;
        if (o.page && o.developerRequired && !developerVerified) {
            stage = GuideStage::Developer;
            return;
        }
        if (installedSeen) {
            if (installedEnabledKnown) stage = installedEnabled ? GuideStage::Connect : GuideStage::Enable;
            else if (stage != GuideStage::Connect) stage = GuideStage::Enable;
            return;
        }
        // Developer mode is only necessary for loading the unpacked extension,
        // not for connecting an extension already installed by another route.
        if (!developerVerified) {
            stage = GuideStage::Developer; return;
        }
        if (stage == GuideStage::Folder || stage == GuideStage::Confirm) {
            stage = GuideStage::Verify; pickerClosedAt = now;
        }
        // Only a stable, independently confirmed absence permits retry after
        // the picker closes; transiently missing UIA nodes are not enough.
        if (stage == GuideStage::Verify) {
            if (o.page && o.installedAbsent && now >= pickerClosedAt && now - pickerClosedAt >= 3000)
                stage = GuideStage::Load;
            return;
        }
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
