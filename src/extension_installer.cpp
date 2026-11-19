#include "guide_profile_challenge.hpp"
#include "focus_visual.hpp"
#include "extension_installer.hpp"
#include "firefox_guide_state.hpp"
#include "render.hpp"
#include "extension_guide_state.hpp"
#include "guide_overlay_motion.hpp"
#include "firefox_profile_identity.hpp"
#include "extension_presence.hpp"
#include "uia_result.hpp"
#include <objbase.h>
#include <uiautomation.h>
#include <windowsx.h>
#include <winver.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <chrono>
#include <cwctype>
#include <array>
#include <set>
#ifdef STREAMSHELF_INSTALLER_TESTING
#include <shlobj.h>
#include <future>
#include <iostream>
#endif

namespace ss {
namespace {
std::wstring lower(std::wstring value) {
    if (!value.empty()) CharLowerBuffW(value.data(), DWORD(value.size()));
    return value;
}
std::wstring productName(const fs::path &path) {
    DWORD unused = 0, bytes = GetFileVersionInfoSizeW(path.c_str(), &unused);
    if (!bytes || bytes > 4 * 1024 * 1024) return {};
    std::vector<BYTE> data(bytes);
    if (!GetFileVersionInfoW(path.c_str(), 0, bytes, data.data())) return {};
    struct Translation { WORD language, codepage; };
    Translation *translations = nullptr; UINT count = 0;
    if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", (void **)&translations, &count) || !translations) return {};
    for (UINT i = 0; i < count / sizeof(Translation); ++i) {
        wchar_t key[100];
        swprintf(key, 100, L"\\StringFileInfo\\%04x%04x\\ProductName", translations[i].language, translations[i].codepage);
        wchar_t *value = nullptr; UINT length = 0;
        if (VerQueryValueW(data.data(), key, (void **)&value, &length) && value && length > 1) return value;
    }
    return {};
}
std::wstring processPath(HWND window) {
    if (!window) return {};
    DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    wchar_t path[32768]; DWORD size = 32768;
    return process.h && QueryFullProcessImageNameW(process, 0, path, &size) ? std::wstring(path, size) : L"";
}
// Native file dialogs may be hosted in a helper process. Accept one only when
// Windows identifies the selected browser window as its owner.
bool belongsToBrowser(HWND window, const std::wstring &exe) {
    if (!window || exe.empty()) return false;
    if (lower(processPath(window)) == lower(exe)) return true;
    wchar_t cls[100]{}; GetClassNameW(window, cls, 100);
    if (std::wstring(cls) != L"#32770" || !GetWindow(window, GW_OWNER)) return false;
    HWND rootOwner = GetAncestor(window, GA_ROOTOWNER);
    return rootOwner && rootOwner != window && lower(processPath(rootOwner)) == lower(exe);
}
template<class T> struct Com {
    T *p = nullptr;
    ~Com() { if (p) p->Release(); }
    Com() = default;
    Com(const Com &) = delete;
    Com &operator=(const Com &) = delete;
    T *operator->() const { return p; }
    T **out() { return &p; }
};
struct Variant {
    VARIANT value;
    Variant() { VariantInit(&value); }
    ~Variant() { VariantClear(&value); }
};
using installer_detail::hasInterface;
bool contains(const std::wstring &text, std::initializer_list<const wchar_t *> names) {
    for (auto name : names) if (text.find(name) != std::wstring::npos) return true;
    return false;
}
std::wstring cachedString(IUIAutomationElement *element, PROPERTYID id) {
    Variant property;
    if (!element || FAILED(element->GetCachedPropertyValue(id, &property.value)) || property.value.vt != VT_BSTR || !property.value.bstrVal) return {};
    return lower(property.value.bstrVal);
}
std::optional<int> cachedInt(IUIAutomationElement *element, PROPERTYID id) {
    Variant property;
    if (!element || FAILED(element->GetCachedPropertyValue(id, &property.value))) return {};
    if (property.value.vt == VT_I4) return property.value.lVal;
    if (property.value.vt == VT_UI4) return int(property.value.ulVal);
    if (property.value.vt == VT_BOOL) return property.value.boolVal != VARIANT_FALSE ? 1 : 0;
    return {};
}
bool extensionUrl(const std::wstring &value) {
    for (auto prefix : {L"chrome://extensions", L"browser://extensions", L"about:addons"})
        if (guideInternalPage(value, prefix)) return true;
    return false;
}
std::wstring normalizedPath(std::wstring value) {
    std::replace(value.begin(), value.end(), L'/', L'\\');
    while (value.size() > 3 && value.back() == L'\\') value.pop_back();
    // Shell can display a long path even when the application was launched
    // through its 8.3 alias. Resolve the existing path before comparing it.
    DWORD needed = GetLongPathNameW(value.c_str(), nullptr, 0);
    if (needed && needed < 32768) {
        std::wstring expanded(needed, L'\0');
        DWORD written = GetLongPathNameW(value.c_str(), expanded.data(), needed);
        if (written && written < needed) {
            expanded.resize(written);
            value = std::move(expanded);
        }
    }
    return lower(value);
}
bool mentionsPath(const std::wstring &value, const std::wstring &path) {
    auto displayed = value;
    auto lowered = lower(displayed);
    for (auto prefix : {L"адрес: ", L"address: "}) {
        const std::wstring text(prefix);
        if (lowered.rfind(text, 0) == 0) { displayed.erase(0, text.size()); break; }
    }
    return normalizedPath(displayed) == normalizedPath(path);
}
struct Control { RECT rect{}; bool found = false; };
struct Scene {
    HWND window{}; RECT windowRect{};
    ULONGLONG at = 0;
    bool valid = false;
    std::wstring internalUrl, currentAddress;
    GuideObservation facts;
    Control developer, load, confirm, enable, connect, popupConnect, permissionAllow;
    Control toolbarAction, firefoxGear, firefoxMenu, firefoxAdd, firefoxOpen, firefoxFile, firefoxOk;
    bool firefoxAddedNotice = false, firefoxRemovedNotice = false;
    bool firefoxFileReady = false, firefoxFileSelected = false;
    bool connectorPopup = false;
    Json toolbarControls = Json::array();
    int enableCandidates = 0;
    bool liveCardFound = false, liveEnabledKnown = false;
    bool profileAbsenceKnown = false, enumerationComplete = false;
    int ownNamesSeen = 0, ownNamesOffscreen = 0;
#ifdef STREAMSHELF_INSTALLER_TESTING
    std::wstring diagnostics;
#endif
};
bool validRect(const RECT &rect) { return rect.right > rect.left && rect.bottom > rect.top; }
bool removeName(const std::wstring &name) {
    return name == L"удалить" || name == L"remove" || name.rfind(L"удалить ", 0) == 0 || name.rfind(L"remove ", 0) == 0;
}
bool ownExtensionName(const std::wstring &name) { return name.size() < 200 && name.find(L"streamshelf connector") != std::wstring::npos; }
bool toolbarAncestor(IUIAutomation *automation, IUIAutomationElement *element) {
    Com<IUIAutomationTreeWalker> walker;
    if (!hasInterface(automation->get_ControlViewWalker(walker.out()), walker.p)) return false;
    Com<IUIAutomationElement> parent;
    if (!hasInterface(walker->GetParentElement(element, parent.out()), parent.p)) return false;
    for (int depth = 0; depth < 6; ++depth) {
        CONTROLTYPEID type = 0; parent->get_CurrentControlType(&type);
        if (type == UIA_DocumentControlTypeId) return false;
        if (type == UIA_ToolBarControlTypeId) return true;
        Com<IUIAutomationElement> next;
        if (!hasInterface(walker->GetParentElement(parent.p, next.out()), next.p)) break;
        parent.p->Release(); parent.p = next.p; next.p = nullptr;
    }
    return false;
}
std::optional<bool> toggleState(IUIAutomationElement *element, int type) {
    auto toggle = cachedInt(element, UIA_ToggleToggleStatePropertyId);
    if (toggle && (*toggle == 0 || *toggle == 1)) return *toggle == 1;
    auto selected = cachedInt(element, UIA_SelectionItemIsSelectedPropertyId);
    if (selected) return *selected != 0;
    if (type == UIA_CheckBoxControlTypeId || type == UIA_CustomControlTypeId ||
        (type == UIA_ButtonControlTypeId && cachedInt(element, UIA_LegacyIAccessibleRolePropertyId).value_or(0) == 0x2c)) {
        auto legacy = cachedInt(element, UIA_LegacyIAccessibleStatePropertyId);
        if (legacy && !(*legacy & 0x20)) return (*legacy & 0x10) != 0;
    }
    return {};
}
bool enableControl(IUIAutomationElement *element, int type, const std::wstring &name) {
    if (contains(name, {L"разработчик", L"developer", L"инкогнито", L"incognito", L"файлам", L"file url"})) return false;
    if (type == UIA_CheckBoxControlTypeId || cachedInt(element, UIA_IsTogglePatternAvailablePropertyId).value_or(0)) return true;
    if (cachedInt(element, UIA_LegacyIAccessibleRolePropertyId).value_or(0) == 0x2c) return true;
    bool actionable = type == UIA_ButtonControlTypeId || type == UIA_HyperlinkControlTypeId || type == UIA_CustomControlTypeId || type == UIA_RadioButtonControlTypeId;
    return actionable && (contains(name, {L"включить", L"enable", L"turn on", L"отключить", L"выключить", L"disable", L"turn off"}) || name == L"вкл." || name == L"выкл." || name == L"включено");
}
std::optional<bool> enabledValue(IUIAutomationElement *element, int type, const std::wstring &name) {
    auto state = toggleState(element, type);
    if (state) return state;
    if (contains(name, {L"отключить", L"выключить", L"disable", L"turn off"}) || name == L"вкл." || name == L"включено") return true;
    if (contains(name, {L"включить", L"enable", L"turn on"}) || name == L"выкл.") return false;
    return {};
}
struct CardInfo { bool found = false; Control enable; std::optional<bool> enabled; int candidates = 0; };

// Inspect just the card's ancestors, never a name elsewhere on the page or an error message.
CardInfo installedCard(IUIAutomation *automation, IUIAutomationElement *nameElement, IUIAutomationElement *browserRoot, IUIAutomationCacheRequest *cache, IUIAutomationCondition *all, unsigned dpi) {
    RECT anchor{};
    if (FAILED(nameElement->get_CachedBoundingRectangle(&anchor)) || !validRect(anchor))
        if (FAILED(nameElement->get_CurrentBoundingRectangle(&anchor)) || !validRect(anchor)) return {};
    Com<IUIAutomationTreeWalker> walker;
    if (!hasInterface(automation->get_RawViewWalker(walker.out()), walker.p)) return {};
    Com<IUIAutomationElement> parent;
    if (!hasInterface(walker->GetParentElement(nameElement, parent.out()), parent.p)) return {};
    CardInfo partial;
    for (int depth = 0; depth < 12; ++depth) {
        {
            Com<IUIAutomationElementArray> children;
            if (hasInterface(parent->FindAllBuildCache(TreeScope_Descendants, all, cache, children.out()), children.p)) {
                int count = 0, remove = 0, name = 0;
                CardInfo card;
                RECT parentRect{}, removeRect{}; parent->get_CurrentBoundingRectangle(&parentRect);
                struct Candidate { RECT rect; std::optional<bool> enabled; bool explicitId; };
                std::vector<Candidate> footerCandidates;
                if (SUCCEEDED(children->get_Length(&count)) && count < 3500) {
                    for (int i = 0; i < count; ++i) {
                        Com<IUIAutomationElement> child;
                        if (!hasInterface(children->GetElement(i, child.out()), child.p)) continue;
                        auto n = cachedString(child.p, UIA_NamePropertyId);
                        auto type = cachedInt(child.p, UIA_ControlTypePropertyId).value_or(0);
                        auto id = cachedString(child.p, UIA_AutomationIdPropertyId);
                        bool explicitId = id == L"enable-toggle" || id == L"enabletoggle" || id == L"enable_toggle";
                        if (ownExtensionName(n)) ++name;
                        if ((type == UIA_ButtonControlTypeId || type == UIA_HyperlinkControlTypeId || type == UIA_MenuItemControlTypeId) && removeName(n)) {
                            ++remove; child->get_CachedBoundingRectangle(&removeRect);
                        }
                        bool isToggle = enableControl(child.p, type, n);
                        bool candidateType = type == UIA_ButtonControlTypeId || type == UIA_CheckBoxControlTypeId ||
                            type == UIA_CustomControlTypeId || type == UIA_RadioButtonControlTypeId;
                        if ((candidateType || explicitId) && (explicitId || isToggle || cachedInt(child.p, UIA_IsKeyboardFocusablePropertyId).value_or(0)) &&
                            !cachedInt(child.p, UIA_IsOffscreenPropertyId).value_or(1)) {
                            RECT candidate{};
                            if (SUCCEEDED(child->get_CachedBoundingRectangle(&candidate)) && validRect(candidate))
                                footerCandidates.push_back({candidate, enabledValue(child.p, type, n), explicitId});
                        }
                    }
                    if (name && remove == 1) {
                        card.found = true;
                        card.candidates = int(footerCandidates.size());
                        bool anchorInside = anchor.left >= parentRect.left && anchor.top >= parentRect.top &&
                            anchor.right <= parentRect.right && anchor.bottom <= parentRect.bottom;
                        if (anchorInside && validRect(parentRect) && validRect(removeRect)) {
                            std::vector<Candidate> explicitMatches;
                            for (const auto &c : footerCandidates) {
                                if (!c.explicitId || c.rect.left < parentRect.left || c.rect.right > parentRect.right ||
                                    c.rect.top < parentRect.top || c.rect.bottom > parentRect.bottom ||
                                    c.rect.right - c.rect.left > int(100 * (dpi ? dpi : 96) / 96) ||
                                    c.rect.bottom - c.rect.top > int(80 * (dpi ? dpi : 96) / 96)) continue;
                                if (std::none_of(explicitMatches.begin(), explicitMatches.end(), [&](const Candidate &other) { return EqualRect(&other.rect, &c.rect); })) explicitMatches.push_back(c);
                            }
                            if (explicitMatches.size() == 1) { card.enable = {explicitMatches[0].rect, true}; card.enabled = explicitMatches[0].enabled; }
                            std::vector<Candidate> matches;
                            for (const auto &c : footerCandidates) {
                                if (!cardFooterSwitch(c.rect.left, c.rect.top, c.rect.right, c.rect.bottom,
                                        parentRect.left, parentRect.top, parentRect.right, parentRect.bottom,
                                        removeRect.left, removeRect.top, removeRect.right, removeRect.bottom, dpi, true)) continue;
                                if (std::none_of(matches.begin(), matches.end(), [&](const Candidate &other) { return EqualRect(&other.rect, &c.rect); })) matches.push_back(c);
                            }
                            if (!card.enable.found && matches.size() == 1) { card.enable = {matches[0].rect, true}; card.enabled = matches[0].enabled; }
                        }
                        if (card.enable.found) return card;
                        if (!partial.found) partial = card;
                    }
                }
            }
        }
        BOOL atRoot = FALSE;
        if (SUCCEEDED(automation->CompareElements(parent.p, browserRoot, &atRoot)) && atRoot) break;
        Com<IUIAutomationElement> next;
        if (!hasInterface(walker->GetParentElement(parent.p, next.out()), next.p)) break;
        parent.p->Release(); parent.p = next.p; next.p = nullptr;
    }
    return partial;
}
bool browserChromeAddress(IUIAutomation *, IUIAutomationElement *, HWND, bool, bool verifyRoot = true);
Scene inspect(IUIAutomation *automation, HWND window, const std::wstring &folder, bool firefox = false, bool yandex = false) {
    Scene scene; scene.window = window; scene.at = GetTickCount64();
    if (!automation || !IsWindow(window) || !GetWindowRect(window, &scene.windowRect)) return scene;
    Com<IUIAutomationElement> root;
    // UIA providers can return S_OK and a null interface. Check BOTH for every COM out pointer.
    HRESULT result = automation->ElementFromHandle(window, root.out());
    if (!hasInterface(result, root.p)) return scene;
    Com<IUIAutomationCondition> all;
    result = automation->CreateTrueCondition(all.out());
    if (!hasInterface(result, all.p)) return scene;
    Com<IUIAutomationCacheRequest> cache;
    result = automation->CreateCacheRequest(cache.out());
    if (!hasInterface(result, cache.p)) return scene;
    cache->put_TreeScope(TreeScope_Element);
    for (auto id : {UIA_NamePropertyId, UIA_AutomationIdPropertyId, UIA_ControlTypePropertyId, UIA_BoundingRectanglePropertyId,
        UIA_IsOffscreenPropertyId, UIA_IsEnabledPropertyId, UIA_IsKeyboardFocusablePropertyId, UIA_ValueValuePropertyId,
        UIA_ToggleToggleStatePropertyId, UIA_SelectionItemIsSelectedPropertyId, UIA_LegacyIAccessibleValuePropertyId,
        UIA_LegacyIAccessibleStatePropertyId, UIA_LegacyIAccessibleRolePropertyId, UIA_IsTogglePatternAvailablePropertyId}) cache->AddProperty(id);
    Com<IUIAutomationElementArray> elements;
    result = root->FindAllBuildCache(TreeScope_Descendants, all.p, cache.p, elements.out());
    if (!hasInterface(result, elements.p)) return scene;
    int count = 0;
    if (FAILED(elements->get_Length(&count))) return scene;
    scene.enumerationComplete = count <= 3500;
    scene.valid = true;
    wchar_t cls[100]{}; GetClassNameW(window, cls, 100);
    bool dialog = std::wstring(cls) == L"#32770";
    bool addressEditing = false, browserAddressKnown = false, browserAddressIsExtensions = false;
    Control popupEnable, popupAllow; bool permissionRequest = false, firefoxAddedText = false;
    struct FlatCandidate { RECT rect{}; std::optional<bool> enabled; bool explicitId = false; bool toggle = false; };
    std::vector<RECT> flatTitles, flatRemoveButtons;
    std::vector<FlatCandidate> flatSwitches;
    std::vector<FlatCandidate> developerSwitches;
    std::vector<RECT> developerLabels, connectorTitles, developerWarnings;
    bool connectorWarningInTitle = false;
    std::vector<IUIAutomationElement *> titleElements;
    struct ReleaseTitles { std::vector<IUIAutomationElement *> &items; ~ReleaseTitles() { for (auto item : items) item->Release(); } } releaseTitles{titleElements};
    BSTR rootName = nullptr;
    if (SUCCEEDED(root->get_CurrentName(&rootName)) && rootName && lower(rootName).find(L"streamshelf connector") != std::wstring::npos) ++scene.ownNamesSeen;
    SysFreeString(rootName);
    for (int i = 0; i < std::min(count, 3500); ++i) {
        Com<IUIAutomationElement> element;
        result = elements->GetElement(i, element.out());
        if (!hasInterface(result, element.p)) continue;
        auto name = cachedString(element.p, UIA_NamePropertyId);
        bool offscreen = cachedInt(element.p, UIA_IsOffscreenPropertyId).value_or(1) != 0;
        if (name.find(L"streamshelf connector") != std::wstring::npos) { ++scene.ownNamesSeen; if (offscreen) ++scene.ownNamesOffscreen; }
        if (offscreen) continue;
        if (firefox && ss::firefoxRemovedNotice(name)) scene.firefoxRemovedNotice = true;
        if (firefox && contains(name, {L"было добавлено", L"was added", L"has been added"})) firefoxAddedText = true;
        if (contains(name, {L"просмотр и изменение ваших данных на всех сайтах", L"read and change all your data on all websites", L"дополнительные разрешения", L"additional permissions", L"доступ к вашим данным для всех веб-сайтов", L"access your data for all websites"})) permissionRequest = true;
        auto value = cachedString(element.p, UIA_ValueValuePropertyId);
        if (value.empty()) value = cachedString(element.p, UIA_LegacyIAccessibleValuePropertyId);
        int type = cachedInt(element.p, UIA_ControlTypePropertyId).value_or(0);
#ifdef STREAMSHELF_INSTALLER_TESTING
        if (dialog && !name.empty()) scene.diagnostics += std::to_wstring(type) + L" " + name + L" VALUE=" + value + L"\n";
#endif
        // Inactive tabs can remain in the accessibility tree. Only the browser's
        // visible address bar identifies the active page; a document's URL does not.
        RECT addressRect{};
        const bool smartBox = yandex && yandexOmniboxIdentity(cachedString(element.p, UIA_AutomationIdPropertyId), name);
        bool addressControl = !dialog && (type == UIA_EditControlTypeId || type == UIA_ComboBoxControlTypeId) &&
            (smartBox || contains(name, {L"адрес", L"address", L"поиск", L"search"}) || value.rfind(L"http://127.0.0.1:", 0) == 0 || extensionUrl(value)) &&
            SUCCEEDED(element->get_CachedBoundingRectangle(&addressRect)) && validRect(addressRect) &&
            (yandex ? guideOmniboxBounds(addressRect.left, addressRect.top, addressRect.right, addressRect.bottom,
                scene.windowRect.left, scene.windowRect.top, scene.windowRect.right, scene.windowRect.bottom, GetDpiForWindow(window), true) &&
                browserChromeAddress(automation, element.p, window, smartBox)
                : addressRect.top >= scene.windowRect.top && addressRect.bottom <= scene.windowRect.top + int(180 * GetDpiForWindow(window) / 96));
#ifdef STREAMSHELF_INSTALLER_TESTING
        if (!dialog && std::wstring(cls) == L"StreamShelfGuideFixture" && type == UIA_EditControlTypeId) addressControl = true;
#endif
        if (addressControl && !value.empty()) {
            scene.currentAddress = value;
            browserAddressKnown = true; browserAddressIsExtensions = extensionUrl(value);
            if (browserAddressIsExtensions) { scene.facts.page = true; scene.internalUrl = value; }
        }
        // The EDIT can contain a path which has merely been typed. Only the address
        // breadcrumb toolbar reports the folder Shell has actually navigated to.
        if (dialog && type == UIA_ToolBarControlTypeId && mentionsPath(name, firefox ? fs::path(folder).parent_path().wstring() : folder)) scene.facts.folderReady = true;
        if (dialog && type == UIA_EditControlTypeId && mentionsPath(value, folder)) addressEditing = true;
        RECT rect{};
        if (FAILED(element->get_CachedBoundingRectangle(&rect)) || !validRect(rect)) continue;
        bool button = type == UIA_ButtonControlTypeId;
        if (firefox) {
            auto id = cachedString(element.p, UIA_AutomationIdPropertyId);
            if (!dialog && button && (id == L"addon-page-options-button" || contains(name, {L"инструменты для всех дополнений", L"tools for all add-ons"}))) scene.firefoxGear = {rect, true};
            if (!dialog && (button || type == UIA_MenuItemControlTypeId || type == UIA_HyperlinkControlTypeId) && contains(name, {L"установить дополнение из файла", L"install add-on from file"})) scene.firefoxMenu = {rect, true};
            if (dialog && (button || type == UIA_SplitButtonControlTypeId) && (id == L"1" || name == L"открыть" || name == L"open") &&
                rect.right - rect.left >= int(50 * GetDpiForWindow(window) / 96)) scene.firefoxOpen = {rect, true};
            if (dialog && (type == UIA_ListItemControlTypeId || type == UIA_DataItemControlTypeId) &&
                name == lower(fs::path(folder).filename().wstring())) {
                scene.firefoxFile = {rect, true};
                scene.firefoxFileSelected = cachedInt(element.p, UIA_SelectionItemIsSelectedPropertyId).value_or(0) != 0;
            }
            if (dialog && (type == UIA_EditControlTypeId || type == UIA_ComboBoxControlTypeId) &&
                contains(name, {L"имя файла", L"file name", L"filename"}) &&
                (mentionsPath(value, folder) || value == lower(fs::path(folder).filename().wstring()))) scene.firefoxFileReady = true;
            if (!dialog && button && (name == L"добавить" || name == L"add")) scene.firefoxAdd = {rect, true};
            if (!dialog && button && (name == L"ok" || name == L"ок")) scene.firefoxOk = {rect, true};
        }
        bool titleAnchor = name.rfind(L"streamshelf connector", 0) == 0 &&
            (!firefox || !contains(name, {L"удален", L"удалён", L"removed", L"добавлено", L"added"})) &&
            (type == UIA_TextControlTypeId || type == UIA_HeaderControlTypeId || type == UIA_CustomControlTypeId);
        if (titleAnchor && std::none_of(flatTitles.begin(), flatTitles.end(), [&](const RECT &other) { return EqualRect(&other, &rect); })) flatTitles.push_back(rect);
        if ((button || type == UIA_HyperlinkControlTypeId || type == UIA_MenuItemControlTypeId) && removeName(name) &&
            std::none_of(flatRemoveButtons.begin(), flatRemoveButtons.end(), [&](const RECT &other) { return EqualRect(&other, &rect); })) flatRemoveButtons.push_back(rect);
        auto automationId = cachedString(element.p, UIA_AutomationIdPropertyId);
        const bool enabled = cachedInt(element.p, UIA_IsEnabledPropertyId).value_or(0) != 0;
        bool switchId = automationId == L"enable-toggle" || automationId == L"enabletoggle" || automationId == L"enable_toggle";
        bool actionable = button || type == UIA_CheckBoxControlTypeId || type == UIA_CustomControlTypeId || type == UIA_RadioButtonControlTypeId;
        if (!firefox && !dialog) {
            if (guideDeveloperModeLabel(name)) developerLabels.push_back(rect);
            if (titleAnchor && name == L"streamshelf connector") connectorTitles.push_back(rect);
            if (guideDeveloperModeWarning(name)) {
                developerWarnings.push_back(rect);
                if (titleAnchor) connectorWarningInTitle = true;
            }
            if (enabled && actionable && (cachedInt(element.p, UIA_IsKeyboardFocusablePropertyId).value_or(0) ||
                enableControl(element.p, type, name)))
                developerSwitches.push_back({rect, toggleState(element.p, type)});
        }
        bool forbiddenName = contains(name, {L"разработчик", L"developer", L"удалить", L"remove", L"ошибк", L"error", L"сведения", L"details", L"инкогнито", L"incognito"});
        if (!forbiddenName && (switchId || (actionable && (enableControl(element.p, type, name) || cachedInt(element.p, UIA_IsKeyboardFocusablePropertyId).value_or(0)))))
            flatSwitches.push_back({rect, enabledValue(element.p, type, name), switchId, enableControl(element.p, type, name)});
        if ((button || type == UIA_MenuItemControlTypeId) &&
            (ownExtensionName(name) || (firefox && (name == L"streamshelf" || name == L"streamshelf connector"))) && !removeName(name)) {
            if (!contains(name, {L"закрепить", L"открепить", L"pin to", L"unpin", L"управление", L"manage"}) &&
                (type == UIA_MenuItemControlTypeId || toolbarAncestor(automation, element.p))) scene.toolbarAction = {rect, true};
        }
        if (button && connectorPopupButton(name)) { scene.connectorPopup = true; scene.popupConnect = {rect, true}; }
        if (button && (name == L"разрешить" || name == L"allow")) popupAllow = {rect, true};
        if (button && contains(name, {L"включить", L"enable", L"turn on", L"добавить расширение", L"add extension"}) && !contains(name, {L"разработчик", L"developer"})) popupEnable = {rect, true};
        const bool developerId = !firefox && (automationId == L"devmode" || automationId == L"dev-mode" || automationId == L"developer-mode");
        if (!dialog && (button || type == UIA_CheckBoxControlTypeId || type == UIA_CustomControlTypeId) &&
            (developerId || (contains(name, {L"режим разработчика", L"developer mode"}) && !guideDeveloperModeWarning(name)))) {
            scene.developer = {rect, true};
            auto state = toggleState(element.p, type);
            if (state) { scene.facts.developerKnown = true; scene.facts.developerOn = *state; }
        }
        if (!dialog && button && enabled && contains(name, {L"загрузить распакован", L"load unpacked"})) scene.load = {rect, true};
        if (dialog && button && enabled && contains(name, {L"выбор папки", L"выбрать папку", L"select folder"})) scene.confirm = {rect, true};
        if (!dialog && titleAnchor && titleElements.size() < 8) { element.p->AddRef(); titleElements.push_back(element.p); }
        if (button && (ownExtensionName(name) || name == L"расширения" || name == L"extensions" || contains(name, {L"закрепить", L"открепить", L"pin to", L"unpin"})))
            scene.toolbarControls.push_back({{"name", utf8(name)}, {"id", utf8(cachedString(element.p, UIA_AutomationIdPropertyId))},
                {"rect", {rect.left, rect.top, rect.right, rect.bottom}}});
        if (!dialog && button && (name == L"расширения" || name == L"extensions" || name == L"дополнения") &&
            browserToolbarButton(rect.left, rect.top, rect.right, rect.bottom, scene.windowRect.left, scene.windowRect.top,
                                 scene.windowRect.right, scene.windowRect.bottom, GetDpiForWindow(window),
                                 yandex ? browserChromeAddress(automation, element.p, window, false) : toolbarAncestor(automation, element.p), yandex) &&
            (!scene.connect.found || rect.left > scene.connect.rect.left)) scene.connect = {rect, true};
    }
    if (firefox && dialog && !scene.firefoxOpen.found) {
        // The Windows file picker exposes its split Open button as the standard
        // IDOK control on systems where UI Automation omits the split button.
        HWND open = GetDlgItem(window, IDOK); RECT rect{}; wchar_t label[128]{};
        if (open && IsWindowVisible(open) && GetWindowTextW(open, label, 128) &&
            (lower(label) == L"открыть" || lower(label) == L"open") && GetWindowRect(open, &rect) && validRect(rect))
            scene.firefoxOpen = {rect, true};
    }
    scene.facts.folderDialog = dialog && scene.confirm.found;
    if (firefox && dialog && scene.firefoxOpen.found) scene.facts.folderDialog = true;
    if (firefox && !dialog && scene.firefoxGear.found) { scene.facts.page = true; scene.internalUrl = L"about:addons"; }
    if (firefox && scene.ownNamesSeen == 0) scene.firefoxAdd.found = false;
    scene.firefoxAddedNotice = firefox && firefoxAddedText && scene.ownNamesSeen && scene.firefoxOk.found;
    if (!scene.firefoxAddedNotice) scene.firefoxOk.found = false;
    if (!scene.facts.folderDialog && GetWindow(window, GW_OWNER) && scene.ownNamesSeen && popupEnable.found) {
        scene.facts.confirmationDialog = true; scene.facts.installedCard = true;
        scene.facts.enabledKnown = true; scene.facts.enabled = false; scene.enable = popupEnable;
    }
    if (permissionRequest && scene.ownNamesSeen && popupAllow.found) scene.permissionAllow = popupAllow;
    if (addressEditing) scene.facts.folderReady = false;
    if (!firefox && !dialog && !scene.developer.found) {
        std::vector<FlatCandidate> matches;
        const auto dpi = GetDpiForWindow(window);
        for (const auto &label : developerLabels) for (const auto &candidate : developerSwitches) {
            if (!guideDeveloperModeSwitch(label.left, label.top, label.right, label.bottom,
                    candidate.rect.left, candidate.rect.top, candidate.rect.right, candidate.rect.bottom, dpi)) continue;
            if (std::none_of(matches.begin(), matches.end(), [&](const FlatCandidate &other) {
                return EqualRect(&other.rect, &candidate.rect);
            })) matches.push_back(candidate);
        }
        if (matches.size() == 1) {
            scene.developer = {matches.front().rect, true};
            if (matches.front().enabled.has_value()) {
                scene.facts.developerKnown = true;
                scene.facts.developerOn = *matches.front().enabled;
            }
        }
    }
    if (!dialog && !scene.facts.page && (!browserAddressKnown || browserAddressIsExtensions) && (scene.developer.found || scene.load.found)) {
        // Some Chromium builds omit the address-bar value from UI Automation.
        // Use the browser window title only together with an actual page control.
        wchar_t title[512]{}; GetWindowTextW(window, title, 512);
        auto name = lower(title);
        bool extensionsTitle = name.starts_with(L"расширения") || name.starts_with(L"extensions") ||
            (yandex && extensionsTabTitle(name, false, true));
        bool browserTitle = contains(name, {L"google chrome", L"яндекс", L"yandex"});
        if (extensionsTitle && browserTitle) scene.facts.page = true;
    }
    bool pageLike = scene.facts.page;
#ifdef STREAMSHELF_INSTALLER_TESTING
    wchar_t fixtureTitle[512]{}; GetWindowTextW(window, fixtureTitle, 512);
    if (std::wstring(fixtureTitle).find(L"StreamShelf accessibility fixture") != std::wstring::npos) pageLike = true;
#endif
    if (!dialog && pageLike && !scene.enable.found) {
        std::vector<FlatCandidate> matches;
        auto dpi = GetDpiForWindow(window);
#ifdef STREAMSHELF_INSTALLER_TESTING
        scene.diagnostics += L"flattened titles=" + std::to_wstring(flatTitles.size()) + L" remove=" + std::to_wstring(flatRemoveButtons.size()) + L" switches=" + std::to_wstring(flatSwitches.size()) + L" dpi=" + std::to_wstring(dpi) + L"\n";
#endif
        for (const auto &title : flatTitles) for (const auto &remove : flatRemoveButtons) for (const auto &candidate : flatSwitches) {
            if (!flattenedCardSwitch(title.left, title.top, title.right, title.bottom,
                    remove.left, remove.top, remove.right, remove.bottom,
                    candidate.rect.left, candidate.rect.top, candidate.rect.right, candidate.rect.bottom, dpi)) continue;
            // A neighbouring card can be on the same row. Its own Remove
            // button lies between our Remove button and its switch.
            bool interveningCard = std::any_of(flatRemoveButtons.begin(), flatRemoveButtons.end(), [&](const RECT &other) {
                int rowDelta = (other.top + other.bottom - remove.top - remove.bottom) / 2;
                return other.left > remove.left && other.right <= candidate.rect.left &&
                    std::abs(rowDelta) <= int(20 * (dpi ? dpi : 96) / 96);
            });
            if (interveningCard) continue;
            auto duplicate = std::find_if(matches.begin(), matches.end(), [&](const FlatCandidate &other) { return EqualRect(&other.rect, &candidate.rect); });
            if (duplicate == matches.end()) matches.push_back(candidate);
            else if (candidate.explicitId) duplicate->explicitId = true;
        }
        auto explicitCount = std::count_if(matches.begin(), matches.end(), [](const FlatCandidate &c) { return c.explicitId; });
#ifdef STREAMSHELF_INSTALLER_TESTING
        scene.diagnostics += L"matches=" + std::to_wstring(matches.size()) + L" explicit=" + std::to_wstring(explicitCount) + L"\n";
#endif
        if (explicitCount == 1) matches.erase(std::remove_if(matches.begin(), matches.end(), [](const FlatCandidate &c) { return !c.explicitId; }), matches.end());
        if (matches.size() == 1) {
            scene.enable = {matches[0].rect, true}; scene.facts.installedCard = true;
            scene.liveCardFound = true; scene.enableCandidates = int(flatSwitches.size());
            if (matches[0].enabled.has_value()) { scene.facts.enabledKnown = true; scene.facts.enabled = *matches[0].enabled; scene.liveEnabledKnown = true; }
        }
    }
    // Firefox's add-on card has a switch and a three-dot menu, not a visible
    // Remove button. Restrict matching to a single toggle on the title's row.
    if (firefox && !dialog && pageLike && !scene.enable.found) {
        std::vector<FlatCandidate> matches;
        const auto dpi = GetDpiForWindow(window);
        for (const auto &title : flatTitles) for (const auto &candidate : flatSwitches) {
            if (!candidate.toggle || !firefoxCardSwitch(title.left, title.top, title.right, title.bottom,
                    candidate.rect.left, candidate.rect.top, candidate.rect.right, candidate.rect.bottom, dpi)) continue;
            if (std::none_of(matches.begin(), matches.end(), [&](const FlatCandidate &other) { return EqualRect(&other.rect, &candidate.rect); })) matches.push_back(candidate);
        }
        if (matches.size() == 1) {
            scene.enable = {matches[0].rect, true}; scene.facts.installedCard = true;
            scene.liveCardFound = true; scene.enableCandidates = 1;
            if (matches[0].enabled.has_value()) { scene.facts.enabledKnown = true; scene.facts.enabled = *matches[0].enabled; scene.liveEnabledKnown = true; }
        }
    }
    // Only traverse the more expensive nested card tree when the flattened
    // browser accessibility tree did not yield a unique, scoped switch.
    if (!dialog && pageLike && !scene.enable.found) {
        for (auto *anchor : titleElements) {
            auto card = installedCard(automation, anchor, root.p, cache.p, all.p, GetDpiForWindow(window));
            if (!card.found) continue;
            scene.facts.installedCard = true; scene.liveCardFound = true; scene.enableCandidates = card.candidates;
            if (card.enable.found) scene.enable = card.enable;
            if (card.enabled.has_value()) { scene.facts.enabledKnown = true; scene.facts.enabled = *card.enabled; scene.liveEnabledKnown = true; }
            if (scene.enable.found) break;
        }
    }
    if (!dialog && scene.facts.page && scene.load.found) { scene.facts.developerKnown = true; scene.facts.developerOn = true; }
    if (!firefox && scene.facts.page && !scene.facts.developerOn) {
        const auto dpi = GetDpiForWindow(window);
        for (const auto &title : connectorTitles) for (const auto &warning : developerWarnings)
            connectorWarningInTitle |= guideConnectorWarningPosition(title.left, title.top, title.bottom, warning.left, warning.top, dpi);
        if (connectorWarningInTitle) {
            scene.facts.developerRequired = scene.facts.developerKnown = scene.facts.installedCard = scene.facts.enabledKnown = true;
            scene.facts.enabled = false;
        }
    }
    scene.at = GetTickCount64();
    return scene;
}
bool modifiersUp() {
    return GetAsyncKeyState(VK_CONTROL) >= 0 && GetAsyncKeyState(VK_MENU) >= 0 && GetAsyncKeyState(VK_SHIFT) >= 0;
}
bool focusChord(WORD modifier, WORD key) {
    INPUT input[4]{};
    for (auto &item : input) item.type = INPUT_KEYBOARD;
    input[0].ki.wVk = modifier; input[1].ki.wVk = key;
    input[2].ki.wVk = key; input[2].ki.dwFlags = KEYEVENTF_KEYUP;
    input[3].ki.wVk = modifier; input[3].ki.dwFlags = KEYEVENTF_KEYUP;
    UINT sent = SendInput(4, input, sizeof(INPUT));
    if (sent != 4) SendInput(2, input + 2, sizeof(INPUT));
    return sent == 4;
}
bool enterText(HWND window, const std::wstring &exe, const std::wstring &text, WORD modifier, WORD key) {
    if (GetForegroundWindow() != window || lower(processPath(window)) != lower(exe) || !modifiersUp()) return false;
    if (!focusChord(modifier, key)) return false;
    Sleep(70);
    if (GetForegroundWindow() != window || lower(processPath(window)) != lower(exe)) return false;
    if (!focusChord(VK_CONTROL, 'A')) return false;
    Sleep(40);
    if (GetForegroundWindow() != window) return false;
    std::vector<INPUT> input;
    for (wchar_t c : text) {
        INPUT item{}; item.type = INPUT_KEYBOARD; item.ki.wScan = c; item.ki.dwFlags = KEYEVENTF_UNICODE;
        input.push_back(item); item.ki.dwFlags |= KEYEVENTF_KEYUP; input.push_back(item);
    }
    INPUT enter{}; enter.type = INPUT_KEYBOARD; enter.ki.wVk = VK_RETURN; input.push_back(enter);
    enter.ki.dwFlags = KEYEVENTF_KEYUP; input.push_back(enter);
    return SendInput(UINT(input.size()), input.data(), sizeof(INPUT)) == input.size();
}
void showIfChanged(HWND window, bool show) {
    if (window && bool(IsWindowVisible(window)) != show) ShowWindow(window, show ? SW_SHOWNOACTIVATE : SW_HIDE);
}
HWND findBrowserWindow(const std::wstring &exe, const std::set<HWND> &old, bool requireNew, DWORD expectedPid = 0, DWORD exactPid = 0) {
    std::map<uint32_t,uint32_t> parents;
    if (expectedPid) {
        Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
        PROCESSENTRY32W process{sizeof(process)};
        if (snapshot.h != INVALID_HANDLE_VALUE && Process32FirstW(snapshot, &process)) do {
            parents[process.th32ProcessID] = process.th32ParentProcessID;
        } while (Process32NextW(snapshot, &process));
    }
    struct Search { const std::wstring &exe; const std::set<HWND> &old; bool fresh; DWORD expectedPid, exactPid; const std::map<uint32_t,uint32_t> &parents; HWND found{}; long long score = -1; } search{exe, old, requireNew, expectedPid, exactPid, parents};
    EnumWindows([](HWND window, LPARAM context) -> BOOL {
        auto &s = *reinterpret_cast<Search *>(context);
        if (!IsWindowVisible(window) || (s.fresh && s.old.count(window))) return TRUE;
        wchar_t cls[100]{}; GetClassNameW(window, cls, 100);
        const std::wstring windowClass = cls;
        if ((windowClass != L"Chrome_WidgetWin_1" && windowClass != L"Chrome_Yandex_WidgetWin_1" && windowClass != L"MozillaWindowClass") || GetWindow(window, GW_OWNER) ||
            lower(processPath(window)) != lower(s.exe)) return TRUE;
        DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
        if (s.exactPid && pid != s.exactPid) return TRUE;
        if (s.expectedPid && !guideProcessDescendsFrom(pid, s.expectedPid, s.parents)) return TRUE;
        RECT rect{}; GetWindowRect(window, &rect);
        long long area = (long long)(rect.right - rect.left) * (rect.bottom - rect.top);
        if (area < 40000 && !IsIconic(window)) return TRUE;
        long long score = area + (s.old.count(window) ? 0 : 1000000000LL);
        if (score > s.score) { s.score = score; s.found = window; }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));
    return search.found;
}
// Query just edit controls in the browser chrome. Full extension-card scans are
// expensive and must not delay identifying a forwarded window or closing a probe.
std::wstring readActiveBrowserAddress(IUIAutomation *automation, HWND window, bool yandex = false) {
    if (!automation || !IsWindow(window)) return {};
    RECT frame{}; if (!GetWindowRect(window, &frame)) return {};
    Com<IUIAutomationElement> root;
    if (!hasInterface(automation->ElementFromHandle(window, root.out()), root.p)) return {};
    Variant edit, combo; edit.value.vt = combo.value.vt = VT_I4;
    edit.value.lVal = UIA_EditControlTypeId; combo.value.lVal = UIA_ComboBoxControlTypeId;
    Com<IUIAutomationCondition> editCondition, comboCondition, condition;
    if (FAILED(automation->CreatePropertyCondition(UIA_ControlTypePropertyId, edit.value, editCondition.out())) ||
        FAILED(automation->CreatePropertyCondition(UIA_ControlTypePropertyId, combo.value, comboCondition.out())) ||
        FAILED(automation->CreateOrCondition(editCondition.p, comboCondition.p, condition.out()))) return {};
    Com<IUIAutomationCacheRequest> cache;
    if (!hasInterface(automation->CreateCacheRequest(cache.out()), cache.p)) return {};
    for (auto id : {UIA_NamePropertyId, UIA_AutomationIdPropertyId, UIA_IsOffscreenPropertyId,
        UIA_BoundingRectanglePropertyId, UIA_ValueValuePropertyId, UIA_LegacyIAccessibleValuePropertyId}) cache->AddProperty(id);
    Com<IUIAutomationElementArray> elements;
    if (!hasInterface(root->FindAllBuildCache(TreeScope_Descendants, condition.p, cache.p, elements.out()), elements.p)) return {};
    int count = 0; elements->get_Length(&count);
    for (int i = 0; i < std::min(count, 100); ++i) {
        Com<IUIAutomationElement> element;
        if (!hasInterface(elements->GetElement(i, element.out()), element.p) || cachedInt(element.p, UIA_IsOffscreenPropertyId).value_or(1)) continue;
        RECT bounds{};
        const auto dpi = GetDpiForWindow(window);
        if (FAILED(element->get_CachedBoundingRectangle(&bounds)) || !validRect(bounds) ||
            (yandex ? !guideOmniboxBounds(bounds.left, bounds.top, bounds.right, bounds.bottom, frame.left, frame.top, frame.right, frame.bottom, dpi, true)
                    : bounds.top < frame.top || bounds.bottom > frame.top + int(180 * dpi / 96) || bounds.right - bounds.left < int(120 * dpi / 96))) continue;
        auto value = cachedString(element.p, UIA_ValueValuePropertyId);
        if (value.empty()) value = cachedString(element.p, UIA_LegacyIAccessibleValuePropertyId);
        const auto name = cachedString(element.p, UIA_NamePropertyId), id = cachedString(element.p, UIA_AutomationIdPropertyId);
        const bool smartBox = yandex && yandexOmniboxIdentity(id, name);
        if (!value.empty() && (smartBox || id == L"view_1012" || id == L"urlbar-input" ||
            contains(name, {L"адрес", L"address", L"поиск", L"search"}) || extensionUrl(value) ||
            value.rfind(L"http://127.0.0.1:", 0) == 0 || value.rfind(L"127.0.0.1:", 0) == 0) &&
            (!yandex || browserChromeAddress(automation, element.p, window, smartBox))) return value;
    }
    return {};
}
bool activeBrowserAddressMatches(const std::wstring &actual, const std::wstring &expected) {
    if (actual.empty() || expected.empty()) return false;
    if (lower(actual) == lower(expected)) return true;
    // Chromium may omit only the http scheme from its omnibox display value.
    return expected.rfind(L"http://127.0.0.1:", 0) == 0 && lower(actual) == lower(expected.substr(7));
}
bool browserChromeAddress(IUIAutomation *automation, IUIAutomationElement *element, HWND window, bool smartBox, bool verifyRoot) {
    Com<IUIAutomationTreeWalker> walker;
    if (!hasInterface(automation->get_ControlViewWalker(walker.out()), walker.p)) return false;
    Com<IUIAutomationElement> current;
    if (!hasInterface(walker->GetParentElement(element, current.out()), current.p)) return false;
    bool toolbar = false;
    for (int depth = 0; depth < 16; ++depth) {
        CONTROLTYPEID type = 0;
        if (FAILED(current->get_CurrentControlType(&type)) || type == UIA_DocumentControlTypeId) return false;
        if (type == UIA_ToolBarControlTypeId) toolbar = true;
        if (type == UIA_WindowControlTypeId) {
            if (!toolbar && !smartBox) return false;
            if (!verifyRoot) return true;
            Com<IUIAutomationElement> root;
            BOOL same = FALSE;
            return hasInterface(automation->ElementFromHandle(window, root.out()), root.p) &&
                SUCCEEDED(automation->CompareElements(current.p, root.p, &same)) && same;
        }
        Com<IUIAutomationElement> next;
        if (!hasInterface(walker->GetParentElement(current.p, next.out()), next.p)) return false;
        current.p->Release(); current.p = next.p; next.p = nullptr;
    }
    return false;
}
// Called only for a requested, focused browser address bar; never for page controls.
HRESULT setFocusedAddress(IUIAutomation *automation, HWND window, const std::wstring &url, bool yandex = false) {
    if (!automation || GetForegroundWindow() != window) return E_ACCESSDENIED;
    Com<IUIAutomationElement> element;
    auto result = automation->GetFocusedElement(element.out());
    if (!hasInterface(result, element.p)) return E_NOINTERFACE;
    int pid = 0; element->get_CurrentProcessId(&pid);
    DWORD expected = 0; GetWindowThreadProcessId(window, &expected);
    if (DWORD(pid) != expected) return E_ACCESSDENIED;
    // Names such as Search also occur inside the extensions page. Validate
    // the chrome hierarchy for every language, even when the label is known.
    CONTROLTYPEID type = 0; RECT bounds{}, frame{};
    bool smartBox = false;
    if (yandex) {
        BSTR id = nullptr, name = nullptr;
        element->get_CurrentAutomationId(&id); element->get_CurrentName(&name);
        smartBox = yandexOmniboxIdentity(id ? lower(id) : L"", name ? lower(name) : L"");
        SysFreeString(id); SysFreeString(name);
    }
    if (FAILED(element->get_CurrentControlType(&type)) || (type != UIA_EditControlTypeId && type != UIA_ComboBoxControlTypeId) ||
        FAILED(element->get_CurrentBoundingRectangle(&bounds)) || !GetWindowRect(window, &frame) ||
        !guideOmniboxBounds(bounds.left, bounds.top, bounds.right, bounds.bottom, frame.left, frame.top, frame.right, frame.bottom,
            GetDpiForWindow(window), yandex) ||
        !browserChromeAddress(automation, element.p, window, smartBox, yandex)) return E_ACCESSDENIED;
    BSTR text = SysAllocString(url.c_str());
    if (!text) return E_OUTOFMEMORY;
    Com<IUIAutomationValuePattern> value;
    result = element->GetCurrentPatternAs(UIA_ValuePatternId, __uuidof(IUIAutomationValuePattern), (void **)value.out());
    if (hasInterface(result, value.p)) result = value->SetValue(text);
    else result = E_NOINTERFACE;
    if (FAILED(result)) {
        Com<IUIAutomationLegacyIAccessiblePattern> legacy;
        auto legacyResult = element->GetCurrentPatternAs(UIA_LegacyIAccessiblePatternId, __uuidof(IUIAutomationLegacyIAccessiblePattern), (void **)legacy.out());
        if (hasInterface(legacyResult, legacy.p)) result = legacy->SetValue(text);
    }
    SysFreeString(text); return result;
}
bool matchesActiveProbe(IUIAutomation *automation, HWND window, const std::wstring &url, bool yandex) {
    const auto expectedTitle = yandex ? lower(wide(guideProfilePageTitle(utf8(url)))) : std::wstring{};
    if (!expectedTitle.empty()) {
        wchar_t caption[512]{};
        if (GetWindowTextW(window, caption, 512) && guideYandexProbeCaption(lower(caption), expectedTitle))
            return true;
    }
    if (activeBrowserAddressMatches(readActiveBrowserAddress(automation, window, yandex), url)) return true;
    if (expectedTitle.empty() || !automation) return false;
    Com<IUIAutomationElement> root;
    if (!hasInterface(automation->ElementFromHandle(window, root.out()), root.p)) return false;
    Variant type; type.value.vt = VT_I4; type.value.lVal = UIA_TabItemControlTypeId;
    Com<IUIAutomationCondition> condition;
    if (!hasInterface(automation->CreatePropertyCondition(UIA_ControlTypePropertyId, type.value, condition.out()), condition.p)) return false;
    Com<IUIAutomationElementArray> tabs;
    if (!hasInterface(root->FindAll(TreeScope_Descendants, condition.p, tabs.out()), tabs.p)) return false;
    int count = 0;
    if (FAILED(tabs->get_Length(&count))) return false;
    for (int i = 0; i < std::min(count, 200); ++i) {
        Com<IUIAutomationElement> tab;
        if (!hasInterface(tabs->GetElement(i, tab.out()), tab.p)) continue;
        BSTR raw = nullptr;
        tab->get_CurrentName(&raw);
        auto title = raw ? lower(raw) : L"";
        SysFreeString(raw);
        if (!guideYandexProbeCaption(title, expectedTitle)) continue;
        Com<IUIAutomationSelectionItemPattern> selection;
        const auto hr = tab->GetCurrentPatternAs(UIA_SelectionItemPatternId, __uuidof(IUIAutomationSelectionItemPattern), (void **)selection.out());
        BOOL selected = FALSE;
        if (hasInterface(hr, selection.p) && SUCCEEDED(selection->get_CurrentIsSelected(&selected)) && selected &&
            browserChromeAddress(automation, tab.p, window, true)) return true;
    }
    return false;
}
// Tab titles are only candidates: the actual internal page is verified after selection.
int selectExtensionsTab(IUIAutomation *automation, HWND window, ULONGLONG deadline, bool firefox, bool yandex) {
    if (!automation || GetForegroundWindow() != window) return 2;
    Com<IUIAutomationElement> root;
    if (FAILED(automation->ElementFromHandle(window, root.out())) || !root.p) return 2;
    Variant type; type.value.vt = VT_I4; type.value.lVal = UIA_TabItemControlTypeId;
    Com<IUIAutomationCondition> condition;
    if (FAILED(automation->CreatePropertyCondition(UIA_ControlTypePropertyId, type.value, condition.out())) || !condition.p) return 2;
    Com<IUIAutomationElementArray> tabs;
    if (FAILED(root->FindAll(TreeScope_Descendants, condition.p, tabs.out())) || !tabs.p) return 2;
    int count = 0; tabs->get_Length(&count); bool candidate = false;
    for (int i = 0; i < count && i < 200; ++i) {
        Com<IUIAutomationElement> tab;
        if (FAILED(tabs->GetElement(i, tab.out())) || !tab.p) continue;
        BSTR raw = nullptr; tab->get_CurrentName(&raw);
        std::wstring name = raw ? lower(raw) : L""; SysFreeString(raw);
        if (!extensionsTabTitle(name, firefox, yandex)) continue;
        candidate = true;
        Com<IUIAutomationSelectionItemPattern> selection;
        auto hr = tab->GetCurrentPatternAs(UIA_SelectionItemPatternId, __uuidof(IUIAutomationSelectionItemPattern), (void **)selection.out());
        if (GetTickCount64() >= deadline || GetForegroundWindow() != window) return 2;
        if (hasInterface(hr, selection.p)) {
            BOOL selected = FALSE;
            if (SUCCEEDED(selection->get_CurrentIsSelected(&selected)) && selected) return true;
            if (SUCCEEDED(selection->Select())) return true;
        }
        Com<IUIAutomationInvokePattern> invoke;
        hr = tab->GetCurrentPatternAs(UIA_InvokePatternId, __uuidof(IUIAutomationInvokePattern), (void **)invoke.out());
        if (hasInterface(hr, invoke.p) && GetTickCount64() < deadline && GetForegroundWindow() == window && SUCCEEDED(invoke->Invoke())) return 1;
        Com<IUIAutomationLegacyIAccessiblePattern> legacy;
        hr = tab->GetCurrentPatternAs(UIA_LegacyIAccessiblePatternId, __uuidof(IUIAutomationLegacyIAccessiblePattern), (void **)legacy.out());
        if (hasInterface(hr, legacy.p) && GetTickCount64() < deadline && GetForegroundWindow() == window && SUCCEEDED(legacy->DoDefaultAction())) return 1;
    }
    return candidate ? 2 : 0;
}
bool closeForwardingDuplicate(IUIAutomation *automation, HWND window, const std::wstring &url, bool yandex) {
    if (!automation || GetForegroundWindow() != window || !modifiersUp()) return false;
    Com<IUIAutomationElement> root;
    if (!hasInterface(automation->ElementFromHandle(window, root.out()), root.p)) return false;
    Variant type; type.value.vt = VT_I4; type.value.lVal = UIA_TabItemControlTypeId;
    Com<IUIAutomationCondition> condition;
    if (!hasInterface(automation->CreatePropertyCondition(UIA_ControlTypePropertyId, type.value, condition.out()), condition.p)) return false;
    Com<IUIAutomationElementArray> tabs;
    if (!hasInterface(root->FindAll(TreeScope_Descendants, condition.p, tabs.out()), tabs.p)) return false;
    int count = 0, matches = 0; tabs->get_Length(&count);
    for (int i = 0; i < std::min(count, 200); ++i) {
        Com<IUIAutomationElement> tab;
        if (!hasInterface(tabs->GetElement(i, tab.out()), tab.p)) continue;
        BSTR raw = nullptr; tab->get_CurrentName(&raw);
        const auto name = raw ? lower(raw) : L""; SysFreeString(raw);
        if (extensionsTabTitle(name, false, yandex)) ++matches;
    }
    // Close only our active nonce page, only while another candidate tab keeps
    // the window alive. The candidate's internal URL is verified after selection.
    return count >= 2 && matches >= 1 && matchesActiveProbe(automation, window, url, yandex) &&
        GetForegroundWindow() == window && modifiersUp() && focusChord(VK_CONTROL, 'W');
}
bool closeExactProbeTab(IUIAutomation *automation, HWND window, const std::wstring &url, const std::atomic<ULONGLONG> &deadline, bool yandex) {
    if (!automation || GetForegroundWindow() != window || !modifiersUp() || !deadline || GetTickCount64() >= deadline) return false;
    Com<IUIAutomationElement> root;
    if (!hasInterface(automation->ElementFromHandle(window, root.out()), root.p)) return false;
    Variant type; type.value.vt = VT_I4; type.value.lVal = UIA_TabItemControlTypeId;
    Com<IUIAutomationCondition> condition;
    if (!hasInterface(automation->CreatePropertyCondition(UIA_ControlTypePropertyId, type.value, condition.out()), condition.p)) return false;
    Com<IUIAutomationElementArray> tabs;
    if (!hasInterface(root->FindAll(TreeScope_Descendants, condition.p, tabs.out()), tabs.p)) return false;
    int count = 0;
    if (FAILED(tabs->get_Length(&count)) || count < 2) return false;
    return matchesActiveProbe(automation, window, url, yandex) && deadline && GetTickCount64() < deadline &&
        GetForegroundWindow() == window && modifiersUp() && focusChord(VK_CONTROL, 'W');
}
} // namespace

GuidedBrowser guidedBrowserProduct(const std::wstring &product, const std::wstring &filename) {
    auto p = lower(product), f = lower(filename);
    if (p == L"google chrome" && f == L"chrome.exe") return GuidedBrowser::Chrome;
    if ((p == L"firefox" || p == L"mozilla firefox") && f == L"firefox.exe") return GuidedBrowser::Firefox;
    if ((p == L"yandex" || p == L"yandex browser" || p == L"яндекс браузер" || p == L"яндекс") && f == L"browser.exe") return GuidedBrowser::Yandex;
    return GuidedBrowser::None;
}
GuidedBrowser guidedBrowser(const Browser &browser) {
    static std::map<std::string, GuidedBrowser> cache;
    auto found = cache.find(browser.exe);
    if (found != cache.end()) return found->second;
    fs::path exe = wide(browser.exe);
    return cache[browser.exe] = guidedBrowserProduct(productName(exe), exe.filename().wstring());
}
std::vector<std::wstring> extensionLaunchArguments(GuidedBrowser kind, const Profile *profile) {
    if (kind == GuidedBrowser::None) throw std::runtime_error("Для этого браузера доступна ручная установка");
    if (kind == GuidedBrowser::Firefox) {
        std::vector<std::wstring> args;
        if (profile && !profile->path.empty()) {
            // Never forward a selected profile to an unrelated running instance.
            // Firefox may refuse a profile that is already locked; in that case
            // the guide waits rather than attaching to a different profile.
            args.push_back(L"--new-instance");
            args.push_back(L"-profile"); args.push_back(wide(profile->path));
        }
        args.push_back(L"-new-window"); args.push_back(L"about:blank"); return args;
    }
    std::vector<std::wstring> args{L"--new-window", L"--force-renderer-accessibility"};
    if (profile) {
        fs::path path = wide(profile->path); auto name = path.filename().wstring();
        if (!path.is_absolute() || path.parent_path().empty() || name.empty() || name == L"." || name == L".." ||
            name.find_first_of(L"<>:\"|?*\r\n") != std::wstring::npos || path.lexically_normal() != path)
            throw std::runtime_error("Для мастера выберите полный путь к папке профиля браузера");
        // The profile path may be in a custom user-data directory. Both values
        // are passed as individually quoted arguments, never as a shell command.
        args.push_back(L"--profile-directory=" + name); args.push_back(L"--user-data-dir=" + path.parent_path().wstring());
    }
    // Internal URLs are ignored by some running Chromium instances. First create a
    // normal browser window, then explicitly navigate and verify the extensions URL.
    args.push_back(L"about:blank"); return args;
}
std::wstring extensionPageUrl(GuidedBrowser kind) {
    if (kind == GuidedBrowser::None) throw std::runtime_error("Для этого браузера мастер недоступен");
    return kind == GuidedBrowser::Chrome ? L"chrome://extensions/" : kind == GuidedBrowser::Firefox ? L"about:addons" : L"browser://extensions/";
}

struct ExtensionInstaller::Impl {
    bool firefox = false;
    bool yandex = false;
    bool firefoxWaitingForInstallResult = false;
    FirefoxPickerGuard firefoxPickerGuard;
    FirefoxInstallProgress firefoxInstallProgress;
    bool firefoxAcknowledgement = false;
    ULONGLONG firefoxInstallWaitAt = 0;
    HWND panel{}, marker{}, dimmer{}, owner{}, browserWindow{}, picker{};
    std::function<void()> retryConnection;
    std::function<bool()> openSetup;
    bool setupRequested = false;
    ULONGLONG setupRequestedAt = 0, connectionCheckAt = 0;
    bool autoCloseOnEnabled = true, reducedMotion = false;
    bool connectionConfirmed = false, extensionReloadRequired = false, extensionRunning = false;
    RECT dimTarget{}, dimPanel{}; bool dimPositioned = false;
    struct Binding { Impl *self; bool marker; } panelBinding{this, false}, markerBinding{this, true};
    fs::path extension;
    fs::path profileDirectory;
    std::wstring exe, browserName, pageUrl, notice;
    std::wstring lastInternalUrl;
    std::wstring browserWindowClass;
    std::set<HWND> existingWindows;
    ULONGLONG opened = GetTickCount64(), pickerAttemptAt = 0, connectionRetryAt = 0;
    bool navigationPending = true, markerPositioned = false;
    bool reuseBrowserWindow = false, newTabPending = false;
    bool launchInitiated = false, navigationInitiated = false;
    std::wstring forwardingUrl;
    std::unique_ptr<GuideProfileChallenge> forwardingProbe;
    std::atomic_bool identifyForwarded{false};
    std::atomic<HWND> forwardedWindow{nullptr};
    std::atomic_bool forwardingDuplicateRemoved{false};
    std::atomic<HWND> forwardingDuplicateRequest{nullptr};
    std::atomic<int> forwardingDuplicateResult{0};
    std::atomic_bool forwardingPreparation{false};
    bool forwardingTabHandled = false;
    bool forwardingProofSeen = false;
    bool explicitProfile = false, profileWindowBound = false;
    std::optional<FirefoxProfileOwner> firefoxProfileOwner;
    std::vector<std::wstring> launchArguments;
    std::vector<HWND> searchWindows;
    size_t searchWindowIndex = 0;
    HWND initialBrowserWindow{};
    DWORD launchPid = 0, launchError = 0;
    HANDLE launchHandle = nullptr;
    int navigationPhase = 0, navigationAttempts = 0;
    ULONGLONG navigationAt = 0, navigationStarted = 0;
    RECT stableBrowserRect{};
    ULONGLONG browserRectChangedAt = 0;
    bool pageVerified = false;
    std::atomic<HWND> addressRequest{nullptr};
    std::atomic<LONG> addressResult{E_PENDING};
    std::atomic<HWND> closeProbeRequest{nullptr};
    std::atomic<int> closeProbeResult{0};
    std::atomic<ULONGLONG> closeProbeDeadline{0};
    std::mutex addressMutex;
    std::wstring requestedAddress, challengeUrl, closeProbeUrl;
    int challengePhase = 0;
    ULONGLONG challengeAt = 0, challengeStarted = 0;
    std::atomic<HWND> tabRequest{nullptr};
    std::atomic<int> tabResult{-1};
    std::atomic<ULONGLONG> tabDeadline{0};
    bool tabSearchPending = false;
    int pickerAttempts = 0;
    int folderPhase = 0;
    HWND folderEdit{};
    ULONGLONG folderPhaseAt = 0, folderDeadline = 0;
    bool sawExtensionPage = false;
    ULONGLONG absenceSince = 0, absenceFirstScene = 0;
    GuideFlow flow;
    std::atomic_bool stop{false};
    std::atomic<HWND> scanWindow{nullptr};
    std::thread scanner;
    std::mutex mutex;
    Scene latest, displayed;
    RECT markerRect{}, markerTarget{};
    GuideOverlayBox markerStart{};
    ULONGLONG markerMoveAt = 0, markerDuration = 0, markerLostAt = 0, markerPaintAt = 0, markerRevealAt = 0;
    BYTE markerAlpha = 0, markerRevealFrom = 0, markerFadeFrom = 0, dimAlpha = 155;
    bool markerActive = false, markerShade = false;
    HDC panelDc{};
    HGLRC panelContext{};
    Canvas canvas;
    bool canvasReady = false, lightTheme = false, english = false;
    float mouseX = -1, mouseY = -1;
    int pressedButton = -1, focusedButton = 0;
    FocusVisual keyboardFocusVisual;
    std::array<Spring, 4> buttonHover{}, buttonPress{};
    Spring stageMotion{1};
    int paintedStage = -1;
    ULONGLONG lastFrame = 0;
    float scale = 1;
    enum class WaitTask { None, Page, Folder, Installation, Setup, Connection, Profile, CloseProbe };
    struct PendingTask {
        WaitTask task = WaitTask::None;
        ULONGLONG deadline = 0;
    };
    WaitTask shownWaitTask = WaitTask::None;
    ULONGLONG waitChangedAt = 0;
    static constexpr ULONGLONG navigationLimit = 30'000, tabSelectionLimit = 10'500, tabConfirmationLimit = 8'000;
    static constexpr ULONGLONG profileProbeLimit = 15'000, profileCleanupLimit = 3'000, folderNavigationLimit = 8'000;
    static constexpr ULONGLONG firefoxInstallLimit = 15'000, installFeedbackLimit = 5'000, connectionFeedbackLimit = 4'000;
#ifdef STREAMSHELF_INSTALLER_TESTING
    unsigned markerUpdates = 0, panelPaints = 0;
#endif
    static constexpr const wchar_t *panelClass = L"StreamShelfExtensionGuideV2";
    static constexpr const wchar_t *markerClass = L"StreamShelfExtensionMarkerV2";
    static constexpr const wchar_t *dimmerClass = L"StreamShelfExtensionDimmerV2";
    int px(int value) const { return int(std::lround(value * scale)); }
    ~Impl() {
        stop = true;
        if (scanner.joinable()) scanner.join();
        destroyPanelRenderer();
        if (IsWindow(panel)) DestroyWindow(panel);
        if (IsWindow(marker)) DestroyWindow(marker);
        if (IsWindow(dimmer)) DestroyWindow(dimmer);
        if (launchHandle) CloseHandle(launchHandle);
    }
    void setNotice(const std::wstring &value) {
        if (notice != value) { notice = value; if (panel) InvalidateRect(panel, nullptr, FALSE); }
    }
    std::wstring displayNotice(const std::wstring &value) const {
        if (!english) return value;
        static const std::map<std::wstring, std::wstring> translations = {
            {L"Открываю выбранный браузер и страницу расширений…", L"Opening the selected browser and its extensions page…"},
            {L"Страница не подтверждена. Нажмите «Открыть страницу» для повтора.", L"The page could not be verified. Click Open page to try again."},
            {L"Не удалось открыть выбранный профиль. Если он уже открыт в Firefox, закройте его и повторите.", L"Could not open the selected profile. If it is already open in Firefox, close it and try again."},
            {L"Не удалось проверить открытые вкладки. Переключитесь на страницу расширений вручную.", L"Could not check open tabs. Switch to the extensions page manually."},
            {L"Вкладка расширений найдена. Переключитесь на неё, чтобы продолжить.", L"The extensions tab was found. Switch to it to continue."},
            {L"Переключитесь на открытую вкладку расширений, чтобы продолжить.", L"Switch to the open extensions tab to continue."},
            {L"Браузер открыт, но страница расширений не подтверждена. Повторите открытие.", L"The browser is open, but the extensions page could not be verified. Open it again."},
            {L"Ожидаю адресную строку окна выбора папки…", L"Waiting for the address bar in the folder picker…"},
            {L"Включите «Режим разработчика» в правом верхнем углу страницы расширений.", L"Turn on Developer mode in the top right of the extensions page."},
            {L"Перетащите окно мастера, чтобы открыть нужную кнопку.", L"Move this guide window to reveal the highlighted button."},
            {L"Подключите расширение и подтвердите доступ к сайтам, если согласны.", L"Connect the extension and allow website access if you agree."},
            {L"Ожидаю соединение. Если оно не появилось, нажмите значок StreamShelf в панели Firefox и подключите расширение.", L"Waiting for a connection. If it does not appear, click the StreamShelf icon in Firefox and connect the extension."},
            {L"Проверяю связь. Если окно расширения не открылось, нажмите его значок в панели Firefox.", L"Checking the connection. If the extension did not open, click its icon in Firefox."},
            {L"Выберите файл StreamShelf Connector в списке.", L"Select the StreamShelf Connector file in the list."},
            {L"Файл указан. Нажмите кнопку «Открыть».", L"The file is selected. Click Open."},
            {L"Откройте папку с XPI вручную или вставьте путь в поле имени файла.", L"Open the XPI folder manually or paste its path into the file name field."},
            {L"Открываю папку с подписанным файлом расширения…", L"Opening the folder containing the signed extension…"},
            {L"Выберите подписанный XPI вручную; путь можно скопировать кнопкой мастера.", L"Select the signed XPI manually; use the guide to copy its path."},
            {L"Ожидаю подтверждение Firefox. Если выбор отменён, откройте меню установки снова.", L"Waiting for Firefox confirmation. If you cancelled, open the installation menu again."},
            {L"Включите StreamShelf Connector в списке дополнений Firefox.", L"Enable StreamShelf Connector in the Firefox add-ons list."},
            {L"Страница расширений открыта и проверена.", L"The extensions page is open and verified."},
            {L"Папка расширения открыта. Нажмите обведённую кнопку «Выбор папки».", L"The extension folder is open. Click the highlighted Select Folder button."},
            {L"Вставьте путь вручную в адресную строку окна папки (Alt+D), затем Enter.", L"Paste the path into the folder picker address bar (Alt+D), then press Enter."},
            {L"Обновите StreamShelf Connector на странице расширений (значок обновления на карточке), затем подключите расширение.", L"Reload StreamShelf Connector on the extensions page, then connect it."},
            {L"Открываю окно расширения автоматически. Если оно не появилось, нажмите «Проверить связь».", L"Opening the extension automatically. If it does not appear, click Check connection."},
            {L"Открыта другая страница. Нажмите «Открыть страницу», чтобы продолжить.", L"A different page is open. Click Open page to continue."},
            {L"Найдите карточку StreamShelf Connector и прокрутите её в видимую область: мастер ищет её переключатель.", L"Find the StreamShelf Connector card and scroll it into view so the guide can find its switch."},
            {L"Перетащите окно мастера, чтобы открыть доступ к переключателю браузера.", L"Move the guide window to reveal the browser switch."},
            {L"Не удалось скопировать путь. Повторите попытку.", L"Could not copy the path. Try again."},
            {L"Путь скопирован.", L"Path copied."},
            {L"Проверяю связь и открываю окно StreamShelf Connector. Подтвердите доступ к сайтам, если согласны.", L"Checking the connection and opening StreamShelf Connector. Allow website access if you agree."},
            {L"Не удалось прочитать интерфейс. Повторите действие в браузере.", L"Could not read the browser interface. Try the action again."},
            {L"Не удалось автоматически выбрать вкладку расширений. Переключитесь на неё, чтобы продолжить.", L"Could not automatically select the extensions tab. Switch to it to continue."},
            {L"Следуйте подсказкам в браузере — шаги меняются автоматически.", L"Follow the browser instructions. The steps advance automatically."}
        };
        if (auto it = translations.find(value); it != translations.end()) return it->second;
        const std::wstring prefix = L"Не удалось заполнить адресную строку. Нажмите «Открыть страницу». Код: ";
        if (value.rfind(prefix, 0) == 0) return L"Could not fill the address bar. Click Open page. Code: " + value.substr(prefix.size());
        return value;
    }
    void launchBrowser() {
        std::wstring command = quoteWide(exe);
        for (const auto &argument : launchArguments) command += L" " + quoteWide(argument);
        STARTUPINFOW startup{sizeof(startup)}; PROCESS_INFORMATION process{};
        startup.dwFlags = STARTF_USESHOWWINDOW; startup.wShowWindow = SW_SHOWNORMAL;
        if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
            launchError = GetLastError();
            throw std::runtime_error("Не удалось запустить браузер. Код Windows: " + std::to_string(launchError));
        }
        if (launchHandle) CloseHandle(launchHandle);
        launchPid = process.dwProcessId; launchHandle = process.hProcess;
        AllowSetForegroundWindow(launchPid);
        CloseHandle(process.hThread);
    }
    void openExtensionsPage(bool relaunch = false) {
        if (navigationPending && navigationInitiated && !relaunch) return;
        navigationInitiated = true;
        if (!IsWindow(browserWindow)) profileWindowBound = false;
        if (firefox && explicitProfile && !profileWindowBound && !relaunch) {
            firefoxProfileOwner = firefoxProfileLockOwner(profileDirectory.wstring());
            if (firefoxProfileOwner) {
                HWND selected = findBrowserWindow(exe, existingWindows, false, 0, firefoxProfileOwner->processId);
                DWORD pid = 0; if (selected) GetWindowThreadProcessId(selected, &pid);
                if (selected && firefoxProfileOwnerMatches(*firefoxProfileOwner, pid)) {
                    browserWindow = selected;
                    profileWindowBound = true;
                }
            }
        }
        if (!IsWindow(browserWindow) && !relaunch && guideCanReuseBrowserWindow(explicitProfile, profileWindowBound))
            browserWindow = findBrowserWindow(exe, existingWindows, false);
        reuseBrowserWindow = !relaunch && IsWindow(browserWindow) && guideCanReuseBrowserWindow(explicitProfile, profileWindowBound);
        newTabPending = reuseBrowserWindow;
        tabSearchPending = reuseBrowserWindow;
        searchWindows.clear(); searchWindowIndex = 0; initialBrowserWindow = browserWindow;
        if (reuseBrowserWindow && firefox) {
            searchWindows.push_back(browserWindow);
            for (HWND window : existingWindows) {
                wchar_t cls[100]{}; GetClassNameW(window, cls, 100);
                DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
                if (window != browserWindow && IsWindowVisible(window) && !GetWindow(window, GW_OWNER) &&
                    std::wstring(cls) == L"MozillaWindowClass" && lower(processPath(window)) == lower(exe) &&
                    (!explicitProfile || (firefoxProfileOwner && firefoxProfileOwnerMatches(*firefoxProfileOwner, pid)))) searchWindows.push_back(window);
            }
        }
        if (!reuseBrowserWindow && !launchInitiated) {
            // Snapshot immediately before the profile-specific launch. Never
            // fall back to a pre-existing window belonging to another profile.
            existingWindows.clear();
            EnumWindows([](HWND h, LPARAM context) -> BOOL {
                auto self = reinterpret_cast<Impl *>(context);
                if (lower(processPath(h)) == lower(self->exe)) self->existingWindows.insert(h);
                return TRUE;
            }, reinterpret_cast<LPARAM>(this));
            profileWindowBound = false;
            launchBrowser();
            launchInitiated = true;
        }
        navigationPending = true; pageVerified = false; navigationPhase = 1;
        navigationAttempts = 0; navigationStarted = navigationAt = GetTickCount64();
        setNotice(L"Открываю выбранный браузер и страницу расширений…");
    }
    void navigateBrowser() {
        if (!navigationPending) return;
        auto now = GetTickCount64();
        if (now - navigationStarted >= navigationLimit) {
            navigationPending = false; forwardingPreparation = false;
            setNotice(firefox && explicitProfile ? L"Не удалось открыть выбранный профиль. Если он уже открыт в Firefox, закройте его и повторите." : L"Страница не подтверждена. Нажмите «Открыть страницу» для повтора."); return;
        }
        if (navigationPhase == 1) {
            auto found = !forwardingUrl.empty() && !profileWindowBound ? forwardedWindow.load() :
                (reuseBrowserWindow || (explicitProfile && profileWindowBound)) && IsWindow(browserWindow) ? browserWindow :
                findBrowserWindow(exe, existingWindows, explicitProfile || (!reuseBrowserWindow && now - navigationStarted < 1700),
                                  firefox && explicitProfile ? launchPid : 0);
            if (!found) return;
            browserWindow = found; scanWindow = found;
            if (explicitProfile) profileWindowBound = true;
            if (forwardingProbe && !forwardingTabHandled) forwardingProbe->advance(GuideProbeProgress::matched);
            wchar_t cls[100]{}; GetClassNameW(found, cls, 100); browserWindowClass = cls;
            DWORD pid = 0; GetWindowThreadProcessId(found, &pid); AllowSetForegroundWindow(pid);
            if (!IsZoomed(found)) { ShowWindowAsync(found, SW_MAXIMIZE); browserRectChangedAt = now; return; }
            RECT current{};
            if (!GetWindowRect(found, &current)) return;
            if (!EqualRect(&current, &stableBrowserRect)) { stableBrowserRect = current; browserRectChangedAt = now; return; }
            if (now - browserRectChangedAt < 350) return;
            SetForegroundWindow(found);
            if (GetForegroundWindow() == found) {
                if (!forwardingUrl.empty() && !forwardingTabHandled) {
                    forwardingTabHandled = true;
                    navigationPhase = 9; navigationAt = now; return;
                }
                if (forwardingDuplicateRemoved) { tabSearchPending = true; newTabPending = false; }
                // The external probe is only for profile/window identity.
                // Reuse its tab for extensions unless an existing extensions
                // tab allowed the probe to be closed safely.
                navigationPhase = 2; navigationAt = now;
            }
            return;
        }
        if (navigationPhase == 9) {
            // The extension announces active tabs asynchronously. Keep this
            // exact HTTP probe alive briefly so its first announcement can
            // identify the session before we select/reuse an internal page.
            if (guideWaitForProbePage(now - navigationAt, forwardingProofSeen,
                                      !forwardingProbe || forwardingProbe->pageReady())) return;
            forwardingDuplicateResult = 0; forwardingDuplicateRequest = browserWindow;
            navigationPhase = 8; navigationAt = now; return;
        }
        if (navigationPhase == 8) {
            const auto result = forwardingDuplicateResult.load();
            if (!result) {
                if (now - navigationAt >= tabConfirmationLimit) {
                    forwardingPreparation = false;
                    navigationPending = false;
                    setNotice(L"Не удалось проверить открытые вкладки. Переключитесь на страницу расширений вручную.");
                }
                return;
            }
            forwardingDuplicateRemoved = result == 1;
            forwardingPreparation = false;
            if (forwardingProbe) forwardingProbe->advance(GuideProbeProgress::proceeding);
            tabSearchPending = result == 1; newTabPending = false;
            navigationPhase = 2; navigationAt = now; return;
        }
        if (GetForegroundWindow() != browserWindow || !modifiersUp()) return;
        if (now - navigationAt < 250) return;
        if (navigationPhase == 2) {
            if (tabSearchPending) {
                tabSearchPending = false; tabResult = -1; tabDeadline = now + 10000; tabRequest = browserWindow;
                navigationPhase = 6; navigationAt = now; return;
            }
            if (newTabPending) {
                if (!focusChord(VK_CONTROL, 'T')) return;
                newTabPending = false; navigationAt = now; return;
            }
            if (!focusChord(VK_CONTROL, 'L')) return;
            navigationPhase = 3; navigationAt = now; return;
        }
        if (navigationPhase == 6) {
            int result = tabResult;
            if (result < 0 && now - navigationAt < tabSelectionLimit) return;
            if (result < 0) {
                navigationPending = false;
                setNotice(L"Не удалось проверить открытые вкладки. Переключитесь на страницу расширений вручную."); return;
            }
            if (result == 1) { navigationPhase = 7; navigationAt = now; }
            else if (result == 2) {
                navigationPending = false;
                setNotice(L"Не удалось автоматически выбрать вкладку расширений. Переключитесь на неё, чтобы продолжить.");
            } else if (firefox && searchWindowIndex + 1 < searchWindows.size()) {
                browserWindow = searchWindows[++searchWindowIndex]; tabSearchPending = true;
                navigationPhase = 1; navigationAt = now;
            } else if (firefox && !searchWindows.empty() && browserWindow != initialBrowserWindow && IsWindow(initialBrowserWindow)) {
                browserWindow = initialBrowserWindow; tabSearchPending = false;
                navigationPhase = 1; navigationAt = now;
            } else { navigationPhase = 2; navigationAt = now; }
            return;
        }
        if (navigationPhase == 7 && now - navigationAt >= tabConfirmationLimit) {
            navigationPending = false;
            setNotice(L"Переключитесь на открытую вкладку расширений, чтобы продолжить."); return;
        }
        if (navigationPhase == 3) {
            { std::lock_guard lock(addressMutex); requestedAddress = pageUrl; }
            addressResult = E_PENDING; addressRequest = browserWindow;
            navigationPhase = 4; navigationAt = now; return;
        }
        if (navigationPhase == 4) {
            LONG result = addressResult;
            if (result == E_PENDING) return;
            if (FAILED(result)) {
                if (++navigationAttempts < 3) { navigationPhase = 2; navigationAt = now; }
                else {
                    navigationPending = false;
                    setNotice(L"Не удалось заполнить адресную строку. Нажмите «Открыть страницу». Код: " + std::to_wstring(result));
                }
                return;
            }
            INPUT enter[2]{}; enter[0].type = enter[1].type = INPUT_KEYBOARD;
            enter[0].ki.wVk = enter[1].ki.wVk = VK_RETURN; enter[1].ki.dwFlags = KEYEVENTF_KEYUP;
            if (GetForegroundWindow() != browserWindow || !modifiersUp()) return;
            if (SendInput(2, enter, sizeof(INPUT)) != 2) return;
            navigationPhase = 5; navigationAt = now; return;
        }
        if (navigationPhase == 5 && now - navigationAt > 4500) {
            if (++navigationAttempts < 3) { navigationPhase = 2; navigationAt = now; }
            else { navigationPending = false; setNotice(L"Браузер открыт, но страница расширений не подтверждена. Повторите открытие."); }
        }
    }
    void navigateFolder(HWND window) {
        if (GetForegroundWindow() != window || !modifiersUp() || !belongsToBrowser(window, exe)) return;
        auto now = GetTickCount64();
        if (folderDeadline && now >= folderDeadline) { folderPhase = 0; return; }
        if (folderPhase == 0) {
            if (pickerAttempts >= 2 || now - pickerAttemptAt < 1500) return;
            if (!folderDeadline) folderDeadline = now + folderNavigationLimit;
            ++pickerAttempts; pickerAttemptAt = now;
            if (focusChord(VK_CONTROL, 'L')) { folderPhase = 1; folderPhaseAt = now; }
            return;
        }
        if (now - folderPhaseAt < 150) return;
        if (folderPhase == 1) {
            GUITHREADINFO info{sizeof(info)};
            bool address = false;
            if (GetGUIThreadInfo(GetWindowThreadProcessId(window, nullptr), &info) && info.hwndFocus) {
                wchar_t cls[100]{}; GetClassNameW(info.hwndFocus, cls, 100);
                if (std::wstring(cls) == L"Edit") {
                    for (HWND parent = GetParent(info.hwndFocus); parent && parent != window; parent = GetParent(parent)) {
                        wchar_t name[100]{}; GetClassNameW(parent, name, 100);
#ifdef STREAMSHELF_INSTALLER_TESTING
                        if (now - folderPhaseAt > 950) std::cerr << "Address parent=" << utf8(name) << std::endl;
#endif
                        if (contains(lower(name), {L"address", L"breadcrumb"})) address = true;
                    }
                }
#ifdef STREAMSHELF_INSTALLER_TESTING
                if (!address && now - folderPhaseAt > 950) std::cerr << "Address focus class=" << utf8(cls) << std::endl;
#endif
            }
            if (!address) {
                if (now - folderPhaseAt > 1200) { folderPhase = 0; setNotice(L"Ожидаю адресную строку окна выбора папки…"); }
                return;
            }
            // The native address edit is replaced as a single value, after Ctrl+L has
            // actually been processed. No Sleep or Unicode keystroke stream races autocomplete.
            DWORD_PTR result = 0;
            const auto destination = firefox ? extension.parent_path().wstring() : extension.wstring();
            if (SendMessageTimeoutW(info.hwndFocus, WM_SETTEXT, 0, (LPARAM)destination.c_str(), SMTO_ABORTIFHUNG | SMTO_BLOCK, 700, &result)) {
                folderEdit = info.hwndFocus; folderPhase = 2; folderPhaseAt = now;
            } else folderPhase = 0;
            return;
        }
        if (folderPhase == 2) {
            GUITHREADINFO info{sizeof(info)};
            if (!GetGUIThreadInfo(GetWindowThreadProcessId(window, nullptr), &info) || info.hwndFocus != folderEdit) { folderPhase = 0; return; }
            INPUT enter[2]{}; enter[0].type = enter[1].type = INPUT_KEYBOARD;
            enter[0].ki.wVk = enter[1].ki.wVk = VK_RETURN; enter[1].ki.dwFlags = KEYEVENTF_KEYUP;
            if (GetForegroundWindow() != window || !modifiersUp()) { folderPhase = 0; return; }
            if (SendInput(2, enter, sizeof(INPUT)) == 2) { folderPhase = 3; folderPhaseAt = now; }
            else folderPhase = 0;
            return;
        }
        if (folderPhase == 3 && now - folderPhaseAt > 2500) folderPhase = 0;
    }
    bool folderAutomationFinished(ULONGLONG now) const {
        return (folderDeadline && now >= folderDeadline) || (pickerAttempts >= 2 && folderPhase == 0);
    }
    void updatePanelRegion(HWND window) {
        RECT client{};
        if (!window || !GetClientRect(window, &client)) return;
        const int inset = px(8), diameter = px(40);
        if (client.right <= inset * 2 || client.bottom <= inset * 2) return;
        // Outside the rendered card is not part of the window, so the browser
        // stays visible and mouse input is not intercepted in the corners.
        HRGN region = CreateRoundRectRgn(inset, inset, client.right - inset + 1,
                                         client.bottom - inset + 1, diameter, diameter);
        if (region && !SetWindowRgn(window, region, TRUE)) DeleteObject(region);
    }
    void initPanelRenderer() {
        HDC previousDc = wglGetCurrentDC(); HGLRC previousContext = wglGetCurrentContext();
        panelDc = GetDC(panel);
        if (!panelDc) throw std::runtime_error("Не удалось подготовить окно мастера");
        PIXELFORMATDESCRIPTOR pfd{}; pfd.nSize = sizeof(pfd); pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cAlphaBits = 8; pfd.iLayerType = PFD_MAIN_PLANE;
        int format = ChoosePixelFormat(panelDc, &pfd);
        if (!format || !SetPixelFormat(panelDc, format, &pfd)) throw std::runtime_error("Формат OpenGL окна мастера");
        HGLRC legacy = wglCreateContext(panelDc);
        if (!legacy || !wglMakeCurrent(panelDc, legacy)) {
            if (legacy) wglDeleteContext(legacy);
            throw std::runtime_error("Контекст OpenGL окна мастера");
        }
        using Create = HGLRC(WINAPI *)(HDC, HGLRC, const int *);
        auto create = reinterpret_cast<Create>(wglGetProcAddress("wglCreateContextAttribsARB"));
        int attributes[] = {0x2091, 3, 0x2092, 3, 0x9126, 1, 0};
        panelContext = create ? create(panelDc, nullptr, attributes) : nullptr;
        wglMakeCurrent(nullptr, nullptr); wglDeleteContext(legacy);
        if (!panelContext || !wglMakeCurrent(panelDc, panelContext)) {
            wglMakeCurrent(previousDc, previousContext);
            throw std::runtime_error("Не удалось создать OpenGL 3.3 для мастера");
        }
        try { canvas.init(executable().parent_path() / L"Assets"); canvasReady = true; }
        catch (...) { wglMakeCurrent(previousDc, previousContext); throw; }
        wglMakeCurrent(previousDc, previousContext);
    }
    void destroyPanelRenderer() {
        HDC previousDc = wglGetCurrentDC(); HGLRC previousContext = wglGetCurrentContext();
        if (panelContext && panelDc) {
            HGLRC oldPanelContext = panelContext;
            if (wglMakeCurrent(panelDc, panelContext) && canvasReady) canvas.destroy();
            canvasReady = false;
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(panelContext); panelContext = nullptr;
            if (previousContext && previousContext != oldPanelContext) wglMakeCurrent(previousDc, previousContext);
        }
        if (panelDc && panel) { ReleaseDC(panel, panelDc); panelDc = nullptr; }
    }
    Rect buttonRect(int index, float width, float height) const {
        float gap = 8, half = (width - 40 - gap) / 2;
        return {20 + (index % 2) * (half + gap), height - 94 + (index / 2) * 41, half, 35};
    }
    int buttonAt(float x, float y) const {
        RECT r{}; GetClientRect(panel, &r);
        for (int i = 0; i < 4; ++i) if (buttonRect(i, r.right / scale, r.bottom / scale).contains(x, y)) return i;
        return -1;
    }
    void wrapped(const std::string &value, float x, float y, float width, float size, int maxLines, Ink color) {
        std::istringstream input(value); std::string word, line; int count = 0;
        while (input >> word) {
            auto next = line.empty() ? word : line + " " + word;
            if (!line.empty() && canvas.measure(next, size) > width) {
                canvas.text(line, x, y + count * size * 1.5f, size, color, width);
                if (++count >= maxLines) return;
                line = word;
            } else line = std::move(next);
        }
        if (!line.empty() && count < maxLines) canvas.text(line, x, y + count * size * 1.5f, size, color, width);
    }
    PendingTask pendingTask(ULONGLONG now) {
        const auto recent = [now](ULONGLONG since, ULONGLONG span) {
            return since && now >= since && now - since < span;
        };
        if (connectionConfirmed || flow.stage == GuideStage::Done) return {};
        if (challengePhase) {
            if (challengePhase != 6 && GetForegroundWindow() != browserWindow) return {};
            if (challengePhase == 6) return {WaitTask::CloseProbe, challengeAt + profileCleanupLimit};
            return {WaitTask::Profile, challengeStarted + profileProbeLimit};
        }
        const bool connecting = flow.stage == GuideStage::Connect;
        const bool verifying = flow.stage == GuideStage::Verify;
        if (!navigationPending && !picker && !connecting && !verifying) return {};
        std::lock_guard lock(mutex);
        const auto &scene = latest;
        const bool current = scene.valid && recent(scene.at, 3000);
        if (current && (scene.permissionAllow.found || scene.popupConnect.found || scene.connectorPopup ||
                        scene.firefoxAdd.found || scene.firefoxAddedNotice || scene.facts.confirmationDialog))
            return {};
        if (picker && current && scene.window == picker && scene.facts.folderDialog) {
            if (firefox && firefoxPickerAction(scene.facts.folderReady, scene.firefoxFile.found,
                                   scene.firefoxFileSelected, scene.firefoxFileReady) != FirefoxPickerAction::Navigate)
                return {};
            if (!firefox && (flow.stage != GuideStage::Folder || scene.facts.folderReady))
                return {};
            if (GetForegroundWindow() != picker || folderAutomationFinished(now))
                return {};
            return {WaitTask::Folder, folderDeadline};
        }
        if (navigationPending && navigationInitiated) {
            const bool needsBrowserFocus = navigationPhase >= 2 && navigationPhase <= 4;
            if (!needsBrowserFocus || GetForegroundWindow() == browserWindow)
                return {WaitTask::Page, navigationStarted + navigationLimit};
        }
        const auto installAt = firefox ? firefoxInstallWaitAt : flow.pickerClosedAt;
        const auto installLimit = firefox ? firefoxInstallLimit : installFeedbackLimit;
        if (verifying && recent(installAt, installLimit))
            return {WaitTask::Installation, installAt + installLimit};
        if (connecting && !extensionReloadRequired) {
            if (recent(connectionCheckAt, connectionFeedbackLimit)) return {WaitTask::Connection, connectionCheckAt + connectionFeedbackLimit};
            if (setupRequested && recent(setupRequestedAt, connectionFeedbackLimit)) return {WaitTask::Setup, setupRequestedAt + connectionFeedbackLimit};
        }
        return {};
    }
    void paintPendingTask(ULONGLONG now, float width, float height, Ink ink, Ink accent, Ink edge) {
        const auto pending = pendingTask(now);
        const auto seconds = guideWaitSeconds(now, pending.deadline);
        const auto task = seconds ? pending.task : WaitTask::None;
        if (task != shownWaitTask) { shownWaitTask = task; waitChangedAt = now; }
        if (task == WaitTask::None) return;
        const char *labelsRu[] = {"", "Открываю страницу расширений", "Открываю нужную папку", "Проверяю установку",
                                   "Открываю окно расширения", "Проверяю соединение", "Проверяю профиль браузера", "Закрываю служебную вкладку"};
        const char *labelsEn[] = {"", "Opening the extensions page", "Opening the extension folder", "Checking installation",
                                   "Opening the extension", "Checking connection", "Checking the browser profile", "Closing the verification tab"};
        const auto label = (english ? labelsEn : labelsRu)[int(task)];
        const auto countdown = std::to_string(seconds) + (english ? " s" : " с");
        const float countdownWidth = canvas.measure(english ? "30 s" : "30 с", 11) + 20;
        const float opacity = reducedMotion ? 1.f : std::clamp(float(now - waitChangedAt + 32) / 150.f, 0.f, 1.f);
        const float top = height - 140, center = top + 11;
        const float badgeWidth = std::min(width - 48, canvas.measure(label, 11) + 40 + countdownWidth);
        const Rect badge{24, top, badgeWidth, 22};
        canvas.rect(badge, 11, accent.alpha(.09f * opacity));
        canvas.outline(badge, 11, mix(edge, accent, .28f).alpha(opacity), 1);
        if (reducedMotion) {
            canvas.outline({33, center - 5, 10, 10}, 5, accent.alpha(opacity), 1.4f);
            canvas.circle(38, center, 1.45f, accent.alpha(opacity));
        } else {
            constexpr float turn = 6.2831853f;
            const float head = float((now - waitChangedAt) % 1050) * turn / 1050;
            for (int dot = 0; dot < 8; ++dot) {
                const float angle = dot * turn / 8;
                const float highlight = (1 + std::cos(angle - head)) * .5f;
                const float strength = .22f + .78f * highlight * highlight * highlight;
                canvas.circle(38 + std::cos(angle) * 5, center + std::sin(angle) * 5, 1.1f + .4f * strength,
                              accent.alpha(opacity * strength));
            }
        }
        canvas.text(label, 50, center + 4, 11, ink.alpha(opacity), badgeWidth - 32 - countdownWidth);
        const float boundary = badge.x + badge.w - countdownWidth;
        canvas.line(boundary, center - 5, boundary, center + 5, 1, edge.alpha(opacity));
        canvas.text(countdown, boundary + (countdownWidth - canvas.measure(countdown, 11)) / 2, center + 4,
                    11, ink.alpha(opacity), countdownWidth);
    }
    void paintPanel(HWND window) {
#ifdef STREAMSHELF_INSTALLER_TESTING
        ++panelPaints;
#endif
        PAINTSTRUCT ps; BeginPaint(window, &ps); RECT r{}; GetClientRect(window, &r);
        if (!canvasReady || !validRect(r)) { EndPaint(window, &ps); return; }
        HDC previousDc = wglGetCurrentDC(); HGLRC previousContext = wglGetCurrentContext();
        if (!wglMakeCurrent(panelDc, panelContext)) { EndPaint(window, &ps); return; }
        const wchar_t *titles[] = {L"Включите режим разработчика", L"Загрузите расширение", L"Открываю папку расширения", L"Подтвердите выбор папки", L"Проверяю установку", L"Включите расширение", L"Подключите расширение", L"Браузер подключён"};
        const wchar_t *bodies[] = {
            L"Нажмите обведённый переключатель «Режим разработчика» / Developer mode. После включения следующий шаг появится автоматически.",
            L"Нажмите обведённую кнопку «Загрузить распакованное расширение» / Load unpacked. Мастер сам откроет нужную папку в появившемся диалоге.",
            L"В окне выбора папки автоматически открывается папка StreamShelf Connector. Не закрывайте окно: дождитесь обводки кнопки «Выбор папки».",
            L"Нажмите обведённую кнопку «Выбор папки» / Select folder. Нужная папка уже открыта. После появления карточки расширения шаг сменится автоматически.",
            L"Ожидаю StreamShelf Connector. Если расширение добавлено, найдите его карточку. Если вы отменили выбор папки или браузер показал ошибку, нажмите «Повторить загрузку».",
            L"На карточке StreamShelf Connector включите переключатель. Если браузер открыл отдельное окно подтверждения, проверьте разрешения и нажмите обведённую кнопку «Включить». Далее мастер перейдёт автоматически.",
            L"Окно StreamShelf Connector откроется автоматически. Нажмите «Подключить расширение», затем прочитайте запрос браузера и разрешите доступ к сайтам, если согласны.",
            L"Связь с расширением подтверждена."
        };
        const wchar_t *firefoxTitles[] = {L"Откройте дополнения Firefox", L"Откройте меню настроек", L"Установите дополнение из файла", L"Выберите подписанный файл", L"Подтвердите установку", L"Включите расширение", L"Подключите расширение", L"Браузер подключён"};
        const wchar_t *firefoxBodies[] = {
            L"Откройте страницу about:addons и выберите раздел «Расширения».",
            L"Нажмите шестерёнку «Инструменты для всех дополнений» на странице расширений.",
            L"В открытом меню нажмите «Установить дополнение из файла».",
            L"Выберите подписанный XPI. Можно скопировать путь кнопкой ниже и вставить его в поле имени файла.",
            L"Прочитайте разрешения Firefox и нажмите «Добавить», если согласны.",
            L"Включите StreamShelf Connector на странице дополнений Firefox.",
            L"Окно расширения откроется автоматически. Нажмите «Подключить расширение», затем прочитайте запрос Firefox и подтвердите доступ к сайтам, если согласны.",
            L"Связь с расширением подтверждена."
        };
        const wchar_t *titlesEn[] = {L"Enable developer mode", L"Load the extension", L"Opening the extension folder", L"Confirm the folder", L"Checking installation", L"Enable the extension", L"Connect the extension", L"Browser connected"};
        const wchar_t *bodiesEn[] = {
            L"Click the highlighted Developer mode switch. The next step appears automatically.",
            L"Click the highlighted Load unpacked button. The guide will open the correct folder in the dialog.",
            L"The StreamShelf Connector folder will open in the folder picker. Keep it open until Select Folder is highlighted.",
            L"Click the highlighted Select Folder button. The next step appears when the extension card is found.",
            L"Waiting for StreamShelf Connector. Find its card if installed. If you cancelled or saw an error, click Retry loading.",
            L"Enable the switch on the StreamShelf Connector card. If a confirmation opens, review permissions and click the highlighted Enable button.",
            L"StreamShelf Connector will open automatically. Click Connect extension, then review and allow website access if you agree.",
            L"The extension connection is confirmed."
        };
        const wchar_t *firefoxTitlesEn[] = {L"Open Firefox add-ons", L"Open the settings menu", L"Install add-on from file", L"Select the signed file", L"Confirm installation", L"Enable the extension", L"Connect the extension", L"Browser connected"};
        const wchar_t *firefoxBodiesEn[] = {
            L"Open about:addons and select Extensions.",
            L"Click the Tools for all add-ons gear on the extensions page.",
            L"Choose Install Add-on From File in the menu.",
            L"Select the signed XPI. You can copy its path below and paste it into the file name field.",
            L"Review Firefox permissions and click Add if you agree.",
            L"Enable StreamShelf Connector on the Firefox add-ons page.",
            L"The extension will open automatically. Click Connect extension, then review and allow website access if you agree.",
            L"The extension connection is confirmed."
        };
        int s = std::clamp(int(flow.stage), 0, 7);
        auto now = GetTickCount64(); float dt = lastFrame ? std::min(.1f, float(now - lastFrame) / 1000.f) : .016f; lastFrame = now;
        if (paintedStage != s) { paintedStage = s; stageMotion.value = reducedMotion ? 1 : 0; stageMotion.velocity = 0; }
        stageMotion.target = 1; stageMotion.step(dt, reducedMotion ? 14 : 5, 1);
        float t = std::clamp(stageMotion.value, 0.f, 1.f), width = r.right / scale, height = r.bottom / scale;
        auto bg = lightTheme ? hex(0xeff3f9) : hex(0x080e19), panelInk = lightTheme ? hex(0xffffff) : hex(0x141d2c);
        auto edge = lightTheme ? hex(0xd9e2ef) : hex(0x2a374b), ink = lightTheme ? hex(0x192b43) : hex(0xeef3fc);
        auto muted = lightTheme ? hex(0x61748c) : hex(0x8999b4), accent = lightTheme ? hex(0x278c78) : hex(0x8be2d1);
        canvas.begin(width, height, r.right, r.bottom);
        canvas.rect({0, 0, width, height}, 0, bg);
        canvas.border({8, 8, width - 16, height - 16}, 20, edge, panelInk);
        canvas.text(english ? "STREAMSHELF  /  EXTENSION SETUP" : "STREAMSHELF  /  УСТАНОВКА РАСШИРЕНИЯ", 24, 32, 11, accent, width - 48);
        canvas.text(utf8(browserName), 24, 57, 12, muted, width - 48);
        const bool opening = navigationPending && !pageVerified && s == 0;
        const wchar_t *title = opening ? (english ? L"Opening the extensions page" : L"Открываю страницу расширений")
            : firefox && firefoxAcknowledgement && s == int(FirefoxGuideStage::ConfirmInstall)
                ? (english ? L"Extension added" : L"Расширение добавлено")
                : firefox ? (english ? firefoxTitlesEn[s] : firefoxTitles[s]) : (english ? titlesEn[s] : titles[s]);
        canvas.text(utf8(title), 24, 94 + (1 - t) * 4, 21, ink.alpha(t), width - 48);
        const wchar_t *body = opening ? (english ? L"Wait while the guide finds the browser window and opens the extensions page."
                                               : L"Мастер найдёт окно браузера и откроет страницу расширений.")
            : explicitProfile && s == int(GuideStage::Connect)
            ? (english ? L"Connect the extension in the selected profile. This guide closes automatically once the connection is confirmed." : L"Подключите расширение в выбранном профиле. Мастер закроется автоматически после подтверждения связи.")
            : firefox && firefoxAcknowledgement && s == int(FirefoxGuideStage::ConfirmInstall)
                ? (english ? L"Click OK in the Firefox notification. Then enable the extension on its card if needed." : L"Нажмите «OK» в уведомлении Firefox. Затем при необходимости включите расширение переключателем в его карточке.")
                : firefox ? (english ? firefoxBodiesEn[s] : firefoxBodies[s]) : (english ? bodiesEn[s] : bodies[s]);
        wrapped(utf8(body), 24, 128 + (1 - t) * 4, width - 48, 13, 4, ink.alpha(t));
        canvas.border({20, 218, width - 40, 45}, 12, edge, bg.alpha(.7f));
        canvas.text(firefox ? (english ? "Signed extension file" : "Подписанный файл расширения") : (english ? "Extension folder" : "Папка расширения"), 32, 235, 10, muted, width - 64);
        canvas.text(utf8(extension.wstring()), 32, 253, 11, ink, width - 64);
        wrapped(utf8(displayNotice(notice.empty() ? L"Следуйте подсказкам в браузере — шаги меняются автоматически." : notice)), 24, 286, width - 48, 12, 3, muted);
        paintPendingTask(now, width, height, ink, accent, edge);
        for (int i = 0; i < 7; ++i) canvas.rect({24 + i * (width - 48) / 7, height - 112, (width - 48) / 7 - 5, 3}, 2, i <= s ? accent : edge);
        const std::string names[] = {english ? "Copy path" : "Копировать путь", english ? "Open folder" : "Открыть папку", !firefox && s == int(GuideStage::Verify) ? (english ? "Retry loading" : "Повторить загрузку") : s >= int(GuideStage::Connect) ? (english ? "Check connection" : "Проверить связь") : (english ? "Open page" : "Открыть страницу"), english ? "Close" : "Закрыть"};
        if (GetFocus() != panel) keyboardFocusVisual.hide();
        const float focusOpacity = keyboardFocusVisual.step(double(now) / 1000., dt, reducedMotion);
        for (int i = 0; i < 4; ++i) {
            auto box = buttonRect(i, width, height); auto &hover = buttonHover[i]; auto &press = buttonPress[i];
            hover.target = box.contains(mouseX, mouseY) ? 1 : 0; press.target = pressedButton == i ? 1 : 0;
            hover.step(dt, reducedMotion ? 14 : 4.5f, .86f); press.step(dt, reducedMotion ? 14 : 4.5f, .86f);
            float h = std::clamp(hover.value, 0.f, 1.f), p = std::clamp(press.value, 0.f, 1.f);
            box = box.scale(1 - .035f * p).move(0, -h + p * .6f);
            bool primary = i == 2;
            auto fill = primary ? mix(accent, hex(0xb6f5e8), h * .3f) : mix(panelInk, lightTheme ? hex(0xe8f1f5) : hex(0x2a3b52), h * .5f);
            canvas.glow({box.x - 10, box.y - 8, box.w + 20, box.h + 22}, accent.alpha(h * (primary ? .12f : .04f)));
            canvas.border(box, 12, primary ? accent : mix(edge, accent, h * .4f), fill);
            if (focusedButton == i && focusOpacity > 0) {
                const auto ring = box.inset(-4);
                canvas.outline(ring, 16, (lightTheme ? hex(0xffffff) : hex(0x08101c)).alpha(focusOpacity), 5);
                canvas.outline(ring, 16, accent.alpha(focusOpacity), 2);
            }
            float x = box.x + std::max(9.f, (box.w - canvas.measure(names[i], 12.5f)) / 2);
            canvas.text(names[i], x, box.y + box.h / 2 + 4.5f, 12.5f, primary ? hex(0x102821) : ink, box.w - 18);
        }
        canvas.render(); SwapBuffers(panelDc); wglMakeCurrent(previousDc, previousContext); EndPaint(window, &ps);
    }
    static GuideOverlayBox markerBox(const RECT &bounds) {
        return {double(bounds.left), double(bounds.top), double(bounds.right), double(bounds.bottom)};
    }
    GuideOverlayBox markerPose(ULONGLONG now) const {
        const double time = reducedMotion || !markerDuration ? 1. : double(now - markerMoveAt) / double(markerDuration);
        return guideOverlayInterpolate(markerStart, markerBox(markerTarget), time);
    }
    void outline(const RECT &control, bool shaded = false) {
        RECT target = control; InflateRect(&target, px(9), px(9));
        const auto now = GetTickCount64();
        if (!markerActive) {
            markerRevealAt = now;
            markerRevealFrom = markerAlpha;
        }
        if (!markerPositioned && !markerActive) {
            markerStart = guideOverlayOpeningBox(markerBox(target), px(4));
            markerTarget = target;
            markerMoveAt = now; markerDuration = guideOverlayRevealMs;
        } else if (!markerActive || !EqualRect(&target, &markerTarget)) {
            markerStart = markerActive ? markerPose(now) : markerBox(markerRect);
            markerTarget = target;
            markerMoveAt = now; markerDuration = guideOverlayTravel(markerStart, markerBox(target), scale);
        }
        markerActive = true; markerShade = shaded; markerLostAt = 0;
        if (!markerPositioned) paintOverlay(now);
    }
    void presentMarker(const RECT &rect, BYTE opacity, ULONGLONG frameAt) {
        int width = rect.right - rect.left, height = rect.bottom - rect.top;
        if (width <= 0 || height <= 0 || width > 5000 || height > 2000) return;
        BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
        HDC dc = GetDC(nullptr), memory = CreateCompatibleDC(dc); void *pixels = nullptr;
        HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!memory || !bitmap || !pixels) { if (bitmap) DeleteObject(bitmap); if (memory) DeleteDC(memory); ReleaseDC(nullptr, dc); return; }
        auto old = SelectObject(memory, bitmap); auto data = static_cast<uint32_t *>(pixels);
        double boxWidth = width;
        double radius = std::min(double(px(10)), std::min(boxWidth, double(height)) / 2.0 - 2), thickness = std::max(4, px(7));
        double phase = reducedMotion ? 0 : frameAt / 180.;
        double pulse = reducedMotion ? 0 : (1. - std::cos(frameAt * 2. * 3.141592653589793 / 1800.)) / 2.;
        std::fill_n(data, size_t(width) * height, 0u);
        const auto drawPixel = [&](int x, int y) {
            double qx = std::abs(x + .5 - boxWidth / 2.) - (boxWidth / 2. - radius - thickness / 2. - 1);
            double qy = std::abs(y + .5 - height / 2.) - (height / 2. - radius - thickness / 2. - 1);
            double distance = std::hypot(std::max(qx, 0.), std::max(qy, 0.)) + std::min(std::max(qx, qy), 0.) - radius + pulse * px(3);
            unsigned alpha = unsigned(255 * std::clamp(thickness / 2. + .5 - std::abs(distance), 0., 1.));
            if (!alpha) return;
            double trail = reducedMotion ? 0 : std::pow((1 + std::cos(std::atan2(y + .5 - height / 2., x + .5 - boxWidth / 2.) - phase)) / 2., 12);
            unsigned red = unsigned(68 + 140 * trail), green = unsigned(201 + 45 * trail);
            data[size_t(y) * width + x] = (alpha << 24) | ((red * alpha / 255) << 16) | ((green * alpha / 255) << 8) | alpha;
        };
        const int band = std::max(24, px(24));
        for (int y = 0; y < height; ++y) {
            if (y < band || y >= height - band || width <= 2 * band) {
                for (int x = 0; x < width; ++x) drawPixel(x, y);
            } else {
                for (int x = 0; x < band; ++x) drawPixel(x, y);
                for (int x = width - band; x < width; ++x) drawPixel(x, y);
            }
        }
        POINT position{rect.left, rect.top}, source{}; SIZE size{width, height};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, opacity, AC_SRC_ALPHA};
        if (UpdateLayeredWindow(marker, dc, &position, &size, memory, &source, 0, &blend, ULW_ALPHA)) {
#ifdef STREAMSHELF_INSTALLER_TESTING
            ++markerUpdates;
#endif
            markerRect = rect; markerPositioned = true; markerAlpha = opacity; markerPaintAt = frameAt;
            showIfChanged(marker, true);
        }
        SelectObject(memory, old); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(nullptr, dc);
    }
    void hideFocus(bool immediate = false) {
        markerActive = false; markerShade = false;
        showIfChanged(dimmer, false);
        if (immediate || reducedMotion) {
            showIfChanged(marker, false);
            markerPositioned = false; markerLostAt = 0; markerAlpha = 0;
        } else if (markerPositioned && !markerLostAt) {
            markerLostAt = GetTickCount64();
            markerFadeFrom = markerAlpha;
        }
    }
    void paintOverlay(ULONGLONG now) {
        if (!markerActive) {
            if (!markerPositioned || !markerLostAt) return;
            const auto alpha = reducedMotion ? BYTE(0) : BYTE(guideOverlayFade(now - markerLostAt, markerFadeFrom));
            if (!alpha) hideFocus(true);
            else if (alpha != markerAlpha) presentMarker(markerRect, alpha, markerPaintAt);
            return;
        }
        const BYTE alpha = reducedMotion ? 255 : BYTE(guideOverlayReveal(now - markerRevealAt, markerRevealFrom));
        if (!alpha) return;
        const auto pose = markerPose(now);
        const RECT rect{LONG(std::lround(pose.left)), LONG(std::lround(pose.top)),
                        LONG(std::lround(pose.right)), LONG(std::lround(pose.bottom))};
        if (!markerPositioned || markerAlpha != alpha || !EqualRect(&rect, &markerRect) ||
            (!reducedMotion && now - markerPaintAt >= 32))
            presentMarker(rect, alpha, now);
        if (markerShade && markerPositioned) shade(markerRect, BYTE((155u * markerAlpha + 127) / 255));
        else showIfChanged(dimmer, false);
    }
    void shade(const RECT &target, BYTE opacity) {
        if (!opacity) { showIfChanged(dimmer, false); return; }
        if (dimAlpha != opacity) {
            if (!SetLayeredWindowAttributes(dimmer, 0, opacity, LWA_ALPHA)) return;
            dimAlpha = opacity;
        }
        // Expose only the control and the inside edge of the stroke. A larger
        // cutout exposes a rectangular patch around small rounded controls.
        RECT hole = target; InflateRect(&hole, px(2) - px(9), px(2) - px(9));
        RECT card{}; GetWindowRect(panel, &card);
        if (!dimPositioned || !EqualRect(&hole, &dimTarget) || !EqualRect(&card, &dimPanel)) {
            int x = GetSystemMetrics(SM_XVIRTUALSCREEN), y = GetSystemMetrics(SM_YVIRTUALSCREEN);
            int w = GetSystemMetrics(SM_CXVIRTUALSCREEN), h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
            HRGN region = CreateRectRgn(0, 0, w, h);
            HRGN cut = CreateRoundRectRgn(hole.left-x, hole.top-y, hole.right-x, hole.bottom-y, px(12), px(12));
            CombineRgn(region, region, cut, RGN_DIFF); DeleteObject(cut);
            // The dimmer is ordered behind the panel. Keep it continuous behind
            // the panel's transparent corners instead of cutting a rectangle.
            if (!SetWindowRgn(dimmer, region, FALSE)) DeleteObject(region);
            SetWindowPos(dimmer, panel, x, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
            dimTarget = hole; dimPanel = card; dimPositioned = true;
        }
        showIfChanged(dimmer, true);
    }
    void highlightTarget(const Scene &scene, Control control) {
        if (!control.found) { hideFocus(); return; }
        RECT windowRect{}; GetWindowRect(scene.window, &windowRect);
        if (windowRect.right - windowRect.left != scene.windowRect.right - scene.windowRect.left ||
            windowRect.bottom - windowRect.top != scene.windowRect.bottom - scene.windowRect.top) { hideFocus(); return; }
        OffsetRect(&control.rect, windowRect.left - scene.windowRect.left, windowRect.top - scene.windowRect.top);
        RECT panelRect{}, overlap{}; GetWindowRect(panel, &panelRect);
        if (IntersectRect(&overlap, &control.rect, &panelRect)) {
            MONITORINFO monitor{sizeof(monitor)};
            if (GetMonitorInfoW(MonitorFromRect(&control.rect, MONITOR_DEFAULTTONEAREST), &monitor)) {
                int width = panelRect.right - panelRect.left, height = panelRect.bottom - panelRect.top;
                int left = control.rect.left - monitor.rcWork.left > monitor.rcWork.right - control.rect.right
                    ? monitor.rcWork.left + px(12) : monitor.rcWork.right - width - px(12);
                int top = std::clamp(int(panelRect.top), int(monitor.rcWork.top), int(std::max(monitor.rcWork.top, monitor.rcWork.bottom - height)));
                SetWindowPos(panel, nullptr, left, top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                GetWindowRect(panel, &panelRect);
            }
            if (IntersectRect(&overlap, &control.rect, &panelRect)) { setNotice(L"Перетащите окно мастера, чтобы открыть нужную кнопку."); hideFocus(); return; }
        }
        RECT intersection{};
        if (IntersectRect(&intersection, &control.rect, &windowRect)) {
            outline(control.rect, !scene.permissionAllow.found && !scene.firefoxAdd.found && !scene.firefoxAddedNotice);
        } else hideFocus();
    }
    void tickFirefox(const Scene &scene, bool current, bool browser, bool own) {
        if (!extensionRunning) setupRequested = false;
        FirefoxGuideObservation observation;
        const auto now = GetTickCount64();
        if (current) {
            observation.manager = scene.facts.page;
            observation.menu = scene.firefoxMenu.found;
            if (browser) firefoxPickerGuard.observeMenu(observation.manager, observation.menu, now);
            const bool sameOwner = scene.window && browserWindow &&
                GetAncestor(scene.window, GA_ROOTOWNER) == GetAncestor(browserWindow, GA_ROOTOWNER);
            observation.picker = browser && firefoxPickerGuard.observePicker(scene.facts.folderDialog, sameOwner, now);
            observation.installPrompt = scene.firefoxAdd.found;
            observation.addedNotice = scene.firefoxAddedNotice;
            observation.installedKnown = scene.facts.installedCard || scene.profileAbsenceKnown;
            observation.installed = scene.facts.installedCard;
            observation.enabledKnown = scene.facts.enabledKnown;
            observation.enabled = scene.facts.enabled;
            observation.removedNotice = scene.firefoxRemovedNotice && !scene.liveCardFound;
            if (scene.facts.page && scene.firefoxGear.found && navigationPending &&
                (navigationPhase != 7 || scene.at >= navigationAt)) {
                navigationPending = false; pageVerified = true;
            }
        }
        observation.extensionRunning = extensionRunning;
        if (current && (observation.picker || observation.installPrompt)) {
            firefoxWaitingForInstallResult = true;
            firefoxInstallWaitAt = 0;
        } else if (current && firefoxWaitingForInstallResult && !firefoxInstallWaitAt) {
            firefoxInstallWaitAt = now;
        }
        if (observation.installed || observation.extensionRunning || observation.addedNotice || observation.removedNotice ||
            (firefoxInstallWaitAt && now - firefoxInstallWaitAt >= firefoxInstallLimit)) {
            firefoxWaitingForInstallResult = false;
            firefoxInstallWaitAt = 0;
        }
        observation.awaitingInstall = firefoxInstallWaitAt != 0;
        const bool profileAbsent = current && scene.profileAbsenceKnown && scene.enumerationComplete && scene.ownNamesSeen == 0;
        firefoxInstallProgress.apply(observation, profileAbsent, scene.at);
        if (observation.picker && browser) {
            navigationPending = false;
            if (picker != scene.window) { picker = scene.window; pickerAttempts = 0; pickerAttemptAt = 0; folderPhase = 0; folderDeadline = 0; }
            // The guard above requires the installation menu in this browser
            // and an owned picker. Restore 2.5's automatic folder navigation
            // only after that proof, never for an unrelated browser Open dialog.
            if (firefoxPickerAction(scene.facts.folderReady, scene.firefoxFile.found,
                                   scene.firefoxFileSelected, scene.firefoxFileReady) == FirefoxPickerAction::Navigate)
                navigateFolder(scene.window);
        } else if (current && !observation.picker) { picker = nullptr; pickerAttempts = 0; folderPhase = 0; }
        auto stage = firefoxGuideStage(observation);
        auto previous = flow.stage;
        if (firefoxAcknowledgement != observation.addedNotice) { firefoxAcknowledgement = observation.addedNotice; InvalidateRect(panel, nullptr, FALSE); }
        flow.stage = static_cast<GuideStage>(int(stage));
        if (flow.stage != previous) { setNotice(L""); InvalidateRect(panel, nullptr, FALSE); }
        if (guideConnectionRetryDue(flow.stage, GetTickCount64(), connectionRetryAt) && retryConnection) {
            connectionRetryAt = GetTickCount64();
            retryConnection();
            if (stop || !IsWindow(panel) || challengePhase) return;
        }
        if (stage == FirefoxGuideStage::Connect && browser && extensionRunning && !setupRequested && openSetup && openSetup()) {
            setupRequested = true;
            setupRequestedAt = GetTickCount64();
        }
        if (stage == FirefoxGuideStage::Connect) {
            if (scene.permissionAllow.found || scene.popupConnect.found)
                setNotice(L"Подключите расширение и подтвердите доступ к сайтам, если согласны.");
            else if (!extensionRunning)
                setNotice(L"Ожидаю соединение. Если оно не появилось, нажмите значок StreamShelf в панели Firefox и подключите расширение.");
            else
                setNotice(L"Проверяю связь. Если окно расширения не открылось, нажмите его значок в панели Firefox.");
        }
        showIfChanged(panel, true);
        if ((!own && !browser) || !current) { hideFocus(); return; }
        Control control;
        if (stage == FirefoxGuideStage::OpenMenu) control = scene.firefoxGear;
        else if (stage == FirefoxGuideStage::InstallFile) control = scene.firefoxMenu;
        else if (stage == FirefoxGuideStage::ConfirmInstall) control = scene.firefoxAddedNotice ? scene.firefoxOk : scene.firefoxAdd;
        else if (stage == FirefoxGuideStage::SelectFile) {
            auto action = firefoxPickerAction(scene.facts.folderReady, scene.firefoxFile.found,
                                              scene.firefoxFileSelected, scene.firefoxFileReady);
            if (action == FirefoxPickerAction::SelectFile) control = scene.firefoxFile;
            else if (action == FirefoxPickerAction::OpenFile) control = scene.firefoxOpen;
        }
        else if (stage == FirefoxGuideStage::Enable) control = scene.enable;
        else if (stage == FirefoxGuideStage::Connect) {
            if (scene.permissionAllow.found) control = scene.permissionAllow;
            else if (scene.popupConnect.found) control = scene.popupConnect;
        }
        if (stage == FirefoxGuideStage::SelectFile) setNotice(scene.firefoxFile.found && !scene.firefoxFileSelected ? L"Выберите файл StreamShelf Connector в списке." : (scene.firefoxFileReady || scene.firefoxFileSelected) ? L"Файл указан. Нажмите кнопку «Открыть»." : folderAutomationFinished(GetTickCount64()) ? L"Откройте папку с XPI вручную или вставьте путь в поле имени файла." : L"Открываю папку с подписанным файлом расширения…");
        if (stage == FirefoxGuideStage::ConfirmInstall && !scene.firefoxAdd.found && !scene.firefoxAddedNotice) setNotice(L"Ожидаю подтверждение Firefox. Если выбор отменён, откройте меню установки снова.");
        if (stage == FirefoxGuideStage::Enable && !control.found) setNotice(L"Включите StreamShelf Connector в списке дополнений Firefox.");
        highlightTarget(scene, control);
    }
    void tick() {
        if (challengePhase) {
            hideFocus(true);
            const auto now = GetTickCount64();
            if (challengePhase == 6) {
                if (closeProbeResult == 1 || now - challengeAt >= profileCleanupLimit || !IsWindow(browserWindow)) {
                    closeProbeDeadline = 0; closeProbeRequest = nullptr; challengePhase = 0; challengeUrl.clear();
                    return;
                }
                if (closeProbeResult == 2 && GetForegroundWindow() == browserWindow && modifiersUp()) {
                    closeProbeResult = 0; closeProbeRequest = browserWindow;
                }
                return;
            }
            if (now - challengeStarted >= profileProbeLimit || !IsWindow(browserWindow)) { challengePhase = 0; return; }
            if (GetForegroundWindow() != browserWindow || !modifiersUp()) return;
            if (challengePhase == 1) {
                if (focusChord(VK_CONTROL, 'T')) { challengePhase = 2; challengeAt = now; }
            } else if (challengePhase == 2 && now - challengeAt >= 180) {
                if (focusChord(VK_CONTROL, 'L')) { challengePhase = 3; challengeAt = now; }
            } else if (challengePhase == 3 && now - challengeAt >= 120) {
                { std::lock_guard lock(addressMutex); requestedAddress = challengeUrl; }
                addressResult = E_PENDING; addressRequest = browserWindow; challengePhase = 4;
            } else if (challengePhase == 4) {
                LONG result = addressResult;
                if (result == E_PENDING) return;
                if (FAILED(result)) { challengePhase = 0; return; }
                INPUT enter[2]{}; enter[0].type = enter[1].type = INPUT_KEYBOARD;
                enter[0].ki.wVk = enter[1].ki.wVk = VK_RETURN; enter[1].ki.dwFlags = KEYEVENTF_KEYUP;
                if (SendInput(2, enter, sizeof(INPUT)) == 2) challengePhase = 5;
            }
            return;
        }
        HWND beforeNavigation = GetForegroundWindow();
        wchar_t activeClass[100]{}; if (beforeNavigation) GetClassNameW(beforeNavigation, activeClass, 100);
        bool pickerOwnedByTarget = browserWindow && beforeNavigation &&
            GetAncestor(beforeNavigation, GA_ROOTOWNER) == GetAncestor(browserWindow, GA_ROOTOWNER);
        bool firefoxPickerActive = firefox && std::wstring(activeClass) == L"#32770" &&
            belongsToBrowser(beforeNavigation, exe) && (!explicitProfile || pickerOwnedByTarget);
        if (!firefoxPickerActive) navigateBrowser();
        HWND foreground = GetForegroundWindow();
        bool own = foreground == panel || (foreground && IsChild(panel, foreground));
        wchar_t foregroundClass[100]{}; if (foreground) GetClassNameW(foreground, foregroundClass, 100);
        bool ownedBrowserPopup = foreground && browserWindow && GetAncestor(foreground, GA_ROOTOWNER) == GetAncestor(browserWindow, GA_ROOTOWNER);
        bool browserClass = (!browserWindowClass.empty() && std::wstring(foregroundClass) == browserWindowClass) || std::wstring(foregroundClass) == L"#32770" || ownedBrowserPopup;
#ifdef STREAMSHELF_INSTALLER_TESTING
        browserClass = browserClass || std::wstring(foregroundClass) == L"StreamShelfGuideFixture";
#endif
        bool browser = !own && browserClass && foreground && !IsIconic(foreground) && belongsToBrowser(foreground, exe) &&
            (!explicitProfile || (profileWindowBound && (foreground == browserWindow || ownedBrowserPopup)));
        if (browser && foreground == browserWindow) {
            RECT current{}; auto now = GetTickCount64();
            if (GetWindowRect(foreground, &current)) {
                if (!EqualRect(&current, &stableBrowserRect)) { stableBrowserRect = current; browserRectChangedAt = now; }
                if (now - browserRectChangedAt < 350) { hideFocus(); return; }
            }
        }
        if (browser) {
            scanWindow = foreground;
            wchar_t cls[100]{}; GetClassNameW(foreground, cls, 100);
            if (std::wstring(cls) != L"#32770" && !GetWindow(foreground, GW_OWNER)) browserWindow = foreground;
        }
        // The guide remains available after a wrong click, a tab change or a switch
        // to another program. Only the target outline follows foreground visibility.
        Scene scene;
        { std::lock_guard lock(mutex); scene = latest; }
        bool current = scene.valid && scene.window == scanWindow && GetTickCount64() - scene.at < 3000;
        if (firefox) { tickFirefox(scene, current, browser, own); return; }
        if (current) {
            auto facts = scene.facts;
            if (yandex && navigationPending && navigationPhase == 7 && scene.at >= navigationAt &&
                guideInternalPage(scene.currentAddress, L"browser://tune")) {
                navigationPhase = 2; navigationAt = GetTickCount64();
            }
            // A verified live session proves our extension is loaded and enabled.
            // Developer mode remains the first prerequisite in GuideFlow.
            if (facts.page && extensionRunning && !facts.developerRequired && !(scene.liveEnabledKnown && !facts.enabled)) {
                facts.installedCard = true; facts.enabledKnown = true; facts.enabled = true;
            }
            if (yandex && facts.page && facts.developerKnown && !facts.developerOn &&
                (facts.installedCard || flow.installedSeen) && !(facts.enabledKnown && facts.enabled))
                facts.developerRequired = true;
            bool possibleRemoval = facts.page && (flow.installedSeen || flow.stage == GuideStage::Verify) && scene.profileAbsenceKnown &&
                scene.enumerationComplete && scene.ownNamesSeen == 0 && !facts.installedCard && !extensionRunning &&
                flow.stage != GuideStage::Folder && flow.stage != GuideStage::Confirm;
            if (possibleRemoval) {
                if (!absenceSince) { absenceSince = GetTickCount64(); absenceFirstScene = scene.at; }
                if (scene.at > absenceFirstScene && GetTickCount64() - absenceSince >= 2000) facts.installedAbsent = true;
            } else { absenceSince = 0; absenceFirstScene = 0; }
            if (facts.page) {
                lastInternalUrl = scene.internalUrl;
                if (!navigationPending && (scene.developer.found || scene.load.found || scene.liveCardFound)) pageVerified = true;
            }
            if (navigationPending && scene.at >= navigationStarted && (navigationPhase != 7 || scene.at >= navigationAt) && verifiedExtensionPage((navigationPhase == 1 || (navigationPhase == 2 && newTabPending) || navigationPhase == 7) ? 5 : navigationPhase, facts.page,
                scene.developer.found || scene.load.found || scene.liveCardFound)) {
                navigationPending = false; pageVerified = true; setNotice(L"Страница расширений открыта и проверена.");
            }
            const auto now = GetTickCount64();
            if (facts.folderDialog && pickerAttempts && !folderAutomationFinished(now) &&
                (folderPhase < 3 || now - folderPhaseAt < 400)) facts.folderReady = false;
            if (facts.page) sawExtensionPage = true;
            if (facts.folderDialog && (!sawExtensionPage || flow.stage == GuideStage::Enable || flow.stage == GuideStage::Connect || flow.stage == GuideStage::Done)) facts.folderDialog = false;
            auto previous = flow.stage;
            flow.observe(facts, GetTickCount64());
            if (flow.stage != previous) {
                setNotice(L"");
                InvalidateRect(panel, nullptr, FALSE);
            }
            if (facts.developerRequired && flow.stage == GuideStage::Developer)
                setNotice(L"Включите «Режим разработчика» в правом верхнем углу страницы расширений.");
            if (facts.folderDialog && flow.expectsPicker()) {
                if (picker != scene.window) { picker = scene.window; pickerAttempts = 0; pickerAttemptAt = 0; folderPhase = 0; folderDeadline = 0; }
                if (!facts.folderReady && browser) navigateFolder(scene.window);
                if (facts.folderReady) setNotice(L"Папка расширения открыта. Нажмите обведённую кнопку «Выбор папки».");
                else if (folderAutomationFinished(GetTickCount64()))
                    setNotice(L"Вставьте путь вручную в адресную строку окна папки (Alt+D), затем Enter.");
            } else if (!scene.facts.folderDialog) { picker = nullptr; pickerAttempts = 0; folderPhase = 0; }
            displayed = scene;
        }
        if (guideConnectionRetryDue(flow.stage, GetTickCount64(), connectionRetryAt) && retryConnection) {
            connectionRetryAt = GetTickCount64();
            retryConnection();
            if (stop || !IsWindow(panel) || challengePhase) return;
        }
        // A verified Native Messaging response from this browser already proves
        // that our extension is installed and running. Once the developer-mode
        // step and folder dialog are behind us, don't wait for another expensive
        // accessibility scan of the extension card before opening setup.
        bool afterFolder = flow.stage == GuideStage::Confirm || flow.stage == GuideStage::Verify ||
                           flow.stage == GuideStage::Enable || flow.stage == GuideStage::Connect;
        bool browserMain = browser && foreground == browserWindow && std::wstring(foregroundClass) != L"#32770";
        bool installedAndEnabled = flow.installedSeen && flow.installedEnabledKnown && flow.installedEnabled;
        if (autoCloseOnEnabled && pageVerified && (flow.developerVerified || installedAndEnabled) && afterFolder && browserMain &&
            !navigationPending && !extensionReloadRequired && !setupRequested && openSetup && openSetup()) {
            setupRequested = true;
            setupRequestedAt = GetTickCount64();
            flow.installedSeen = true;
            flow.stage = GuideStage::Connect;
            InvalidateRect(panel, nullptr, FALSE);
        }
        if (autoCloseOnEnabled && pageVerified && (flow.stage == GuideStage::Connect || flow.stage == GuideStage::Done)) {
            setNotice(extensionReloadRequired ? L"Обновите StreamShelf Connector на странице расширений (значок обновления на карточке), затем подключите расширение." : L"Открываю окно расширения автоматически. Если оно не появилось, нажмите «Проверить связь».");
        }
        showIfChanged(panel, true);
        if ((!own && !browser) || !current) { hideFocus(); return; }
        if (!scene.facts.page && !scene.facts.folderDialog && !scene.facts.confirmationDialog && flow.stage != GuideStage::Connect && flow.stage != GuideStage::Done && !navigationPending)
            setNotice(L"Открыта другая страница. Нажмите «Открыть страницу», чтобы продолжить.");
        // Never darken or point at page controls while navigation is unverified.
        if (navigationPending && !scene.facts.folderDialog && !scene.facts.confirmationDialog) { hideFocus(); return; }
        Control control;
        if (flow.stage == GuideStage::Developer && scene.facts.page) control = scene.developer;
        if (flow.stage == GuideStage::Load && scene.facts.page) control = scene.load;
        if (flow.stage == GuideStage::Confirm && scene.facts.folderDialog) control = scene.confirm;
        if (flow.stage == GuideStage::Enable && (scene.facts.page || scene.facts.confirmationDialog)) control = scene.enable;
        if (flow.stage == GuideStage::Connect || flow.stage == GuideStage::Done)
            control = scene.permissionAllow.found ? scene.permissionAllow : scene.popupConnect;
        if (!control.found) {
            if (flow.stage == GuideStage::Developer && scene.facts.page)
                setNotice(L"Включите «Режим разработчика» в правом верхнем углу страницы расширений.");
            if (flow.stage == GuideStage::Enable && scene.facts.page)
                setNotice(L"Найдите карточку StreamShelf Connector и прокрутите её в видимую область: мастер ищет её переключатель.");
            hideFocus(); return;
        }
        RECT windowRect{}; GetWindowRect(scene.window, &windowRect);
        // Translate a stable target during window movement instead of blinking it off every frame.
        if (windowRect.right - windowRect.left != scene.windowRect.right - scene.windowRect.left ||
            windowRect.bottom - windowRect.top != scene.windowRect.bottom - scene.windowRect.top) { hideFocus(); return; }
        OffsetRect(&control.rect, windowRect.left - scene.windowRect.left, windowRect.top - scene.windowRect.top);
        RECT panelRect{}, overlap{}; GetWindowRect(panel, &panelRect);
        if (IntersectRect(&overlap, &control.rect, &panelRect)) {
            MONITORINFO monitor{sizeof(monitor)};
            if (GetMonitorInfoW(MonitorFromRect(&control.rect, MONITOR_DEFAULTTONEAREST), &monitor)) {
                int width = panelRect.right - panelRect.left, height = panelRect.bottom - panelRect.top;
                int left = control.rect.left - monitor.rcWork.left > monitor.rcWork.right - control.rect.right
                    ? monitor.rcWork.left + px(12) : monitor.rcWork.right - width - px(12);
                int top = std::clamp(int(panelRect.top), int(monitor.rcWork.top), int(std::max(monitor.rcWork.top, monitor.rcWork.bottom - height)));
                SetWindowPos(panel, nullptr, left, top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                GetWindowRect(panel, &panelRect);
            }
            if (IntersectRect(&overlap, &control.rect, &panelRect)) {
                setNotice(L"Перетащите окно мастера, чтобы открыть доступ к переключателю браузера.");
                hideFocus(); return;
            }
        }
        RECT intersection{};
        if (IntersectRect(&intersection, &control.rect, &windowRect)) {
            // Keep the entire browser permission prompt readable, including Deny.
            outline(control.rect, !scene.permissionAllow.found);
        } else hideFocus();
    }
    void copyPath() {
        setNotice(L"Не удалось скопировать путь. Повторите попытку.");
        auto path = extension.wstring(); HGLOBAL block = GlobalAlloc(GMEM_MOVEABLE, (path.size() + 1) * sizeof(wchar_t));
        if (!block) return; void *memory = GlobalLock(block);
        if (!memory) { GlobalFree(block); return; }
        memcpy(memory, path.c_str(), (path.size() + 1) * sizeof(wchar_t)); GlobalUnlock(block);
        if (OpenClipboard(panel)) {
            if (EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, block)) { block = nullptr; setNotice(L"Путь скопирован."); }
            CloseClipboard();
        }
        if (block) GlobalFree(block);
    }
    static LRESULT CALLBACK dimmerProcedure(HWND h, UINT message, WPARAM w, LPARAM l) {
        if (message == WM_NCHITTEST) return HTTRANSPARENT;
        if (message == WM_ERASEBKGND) return 1;
        if (message == WM_PAINT) {
            PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
            RECT r{}; GetClientRect(h, &r);
            FillRect(dc, &r, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            EndPaint(h, &ps); return 0;
        }
        return DefWindowProcW(h, message, w, l);
    }
    static LRESULT CALLBACK procedure(HWND h, UINT message, WPARAM w, LPARAM l) {
        auto binding = reinterpret_cast<Binding *>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            binding = static_cast<Binding *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)binding);
        }
        if (!binding) return DefWindowProcW(h, message, w, l);
        auto self = binding->self;
        try {
            if (binding->marker) {
                if (message == WM_NCHITTEST) return HTTRANSPARENT;
                if (message == WM_PAINT) { ValidateRect(h, nullptr); return 0; }
                if (message == WM_ERASEBKGND) return 1;
                return DefWindowProcW(h, message, w, l);
            }
            switch (message) {
            case WM_ERASEBKGND: return 1;
            case WM_PAINT: self->paintPanel(h); return 0;
            case WM_SIZE: self->updatePanelRegion(h); return 0;
            case WM_TIMER:
                if (w == 1) self->tick();
                else if (w == 2) { self->paintOverlay(GetTickCount64()); InvalidateRect(h, nullptr, FALSE); }
                return 0;
            case WM_DPICHANGED: {
                self->scale = HIWORD(w) / 96.f; auto r = reinterpret_cast<RECT *>(l);
                SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
                self->updatePanelRegion(h); self->markerPositioned = false; InvalidateRect(h, nullptr, FALSE); return 0;
            }
            case WM_NCHITTEST: {
                POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)}; ScreenToClient(h, &p);
                if (p.y < self->px(40)) return HTCAPTION; break;
            }
            case WM_MOUSEMOVE: {
                self->mouseX = float(GET_X_LPARAM(l)) / self->scale; self->mouseY = float(GET_Y_LPARAM(l)) / self->scale;
                TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, h, 0}; TrackMouseEvent(&track);
                InvalidateRect(h, nullptr, FALSE); return 0;
            }
            case WM_MOUSELEAVE: self->mouseX = self->mouseY = -1; InvalidateRect(h, nullptr, FALSE); return 0;
            case WM_LBUTTONDOWN: {
                self->keyboardFocusVisual.hide();
                self->mouseX = float(GET_X_LPARAM(l)) / self->scale; self->mouseY = float(GET_Y_LPARAM(l)) / self->scale;
                self->pressedButton = self->buttonAt(self->mouseX, self->mouseY);
                if (self->pressedButton >= 0) { self->focusedButton = self->pressedButton; SetFocus(h); SetCapture(h); }
                InvalidateRect(h, nullptr, FALSE); return 0;
            }
            case WM_LBUTTONUP: {
                int pressed = self->pressedButton; self->pressedButton = -1;
                float x = float(GET_X_LPARAM(l)) / self->scale, y = float(GET_Y_LPARAM(l)) / self->scale;
                if (GetCapture() == h) ReleaseCapture();
                if (pressed >= 0 && self->buttonAt(x, y) == pressed) SendMessageW(h, WM_COMMAND, 101 + pressed, 0);
                if (IsWindow(h)) InvalidateRect(h, nullptr, FALSE); return 0;
            }
            case WM_CAPTURECHANGED: self->pressedButton = -1; return 0;
            case WM_COMMAND:
                switch (LOWORD(w)) {
                case IDCANCEL: case 104: SendMessageW(h, WM_CLOSE, 0, 0); break;
                case 101: self->copyPath(); break;
                case 102: openPath(self->firefox ? self->extension.parent_path() : self->extension); break;
                case 103:
                    if (self->flow.stage == GuideStage::Connect || self->flow.stage == GuideStage::Done) {
                        self->setupRequested = false;
                        if (self->retryConnection) {
                            self->connectionCheckAt = GetTickCount64();
                            self->retryConnection();
                        }
                        self->setNotice(L"Проверяю связь и открываю окно StreamShelf Connector. Подтвердите доступ к сайтам, если согласны.");
                        break;
                    }
                    if (!self->firefox && self->flow.stage == GuideStage::Verify) { self->flow.retryInstallation(); InvalidateRect(h, nullptr, FALSE); }
                    self->openExtensionsPage();
                    break;
                } return 0;
            case WM_GETDLGCODE: return DLGC_WANTALLKEYS;
            case WM_KEYDOWN:
                if (w == VK_ESCAPE) SendMessageW(h, WM_CLOSE, 0, 0);
                else if (w == VK_TAB) {
                    self->focusedButton = (self->focusedButton + ((GetKeyState(VK_SHIFT) & 0x8000) ? 3 : 1)) % 4;
                    self->keyboardFocusVisual.show(double(GetTickCount64()) / 1000.);
                    InvalidateRect(h, nullptr, FALSE);
                }
                else if (w == VK_RETURN || w == VK_SPACE) SendMessageW(h, WM_COMMAND, 101 + self->focusedButton, 0);
                return 0;
            case WM_CLOSE:
                self->stop = true; KillTimer(h, 1); KillTimer(h, 2); self->hideFocus(true); self->destroyPanelRenderer(); DestroyWindow(h); return 0;
            case WM_DESTROY: self->stop = true; return 0;
            }
        } catch (...) { self->setNotice(L"Не удалось прочитать интерфейс. Повторите действие в браузере."); }
        return DefWindowProcW(h, message, w, l);
    }
    void create(HWND parent, const Browser &browser, const fs::path &folder) {
        owner = parent; exe = wide(browser.exe); browserName = wide(browser.name); extension = folder;
        auto kind = guidedBrowser(browser);
        firefox = kind == GuidedBrowser::Firefox;
        yandex = kind == GuidedBrowser::Yandex;
        pageUrl = extensionPageUrl(kind == GuidedBrowser::None ? GuidedBrowser::Chrome : kind);
        EnumWindows([](HWND h, LPARAM context) -> BOOL {
            auto self = reinterpret_cast<Impl *>(context);
            if (lower(processPath(h)) == lower(self->exe)) self->existingWindows.insert(h); return TRUE;
        }, reinterpret_cast<LPARAM>(this));
        WNDCLASSEXW cls{sizeof(cls)}; cls.lpfnWndProc = procedure; cls.hInstance = GetModuleHandleW(nullptr); cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        cls.style = CS_OWNDC; cls.lpszClassName = panelClass;
        if (!RegisterClassExW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) throw std::runtime_error("Не удалось создать мастер установки");
        cls.style = 0; cls.lpszClassName = markerClass;
        if (!RegisterClassExW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) throw std::runtime_error("Не удалось создать подсветку");
        cls.lpfnWndProc = dimmerProcedure; cls.lpszClassName = dimmerClass;
        if (!RegisterClassExW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) throw std::runtime_error("Не удалось создать затемнение");
        MONITORINFO monitor{sizeof(monitor)}; GetMonitorInfoW(MonitorFromWindow(parent, MONITOR_DEFAULTTONEAREST), &monitor);
        UINT dpi = parent ? GetDpiForWindow(parent) : GetDpiForSystem(); scale = std::max(96u, dpi) / 96.f;
        scale = std::min(scale, std::min((monitor.rcWork.right - monitor.rcWork.left) / 510.f, (monitor.rcWork.bottom - monitor.rcWork.top) / 510.f));
        panel = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, panelClass, english ? L"StreamShelf · Extension setup" : L"StreamShelf · Установка расширения",
            WS_POPUP | WS_CLIPCHILDREN, monitor.rcWork.right - px(500), monitor.rcWork.bottom - px(490), px(480), px(470), nullptr, nullptr, cls.hInstance, &panelBinding);
        if (!panel) throw std::runtime_error("Не удалось открыть мастер установки");
        updatePanelRegion(panel);
        marker = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
            markerClass, L"StreamShelf · Обводка", WS_POPUP, 0, 0, 1, 1, panel, nullptr, cls.hInstance, &markerBinding);
        if (!marker) throw std::runtime_error("Не удалось открыть обводку");
        dimmer = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
            dimmerClass, L"StreamShelf · Затемнение", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, cls.hInstance, nullptr);
        if (!dimmer) throw std::runtime_error("Не удалось создать затемнение");
        SetLayeredWindowAttributes(dimmer, 0, 155, LWA_ALPHA);
        initPanelRenderer(); SetTimer(panel, 1, 40, nullptr); SetTimer(panel, 2, 16, nullptr);
        scanner = std::thread([this] {
            HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            {
                Com<IUIAutomation> automation;
                HRESULT result = CoCreateInstance(CLSID_CUIAutomation8, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IUIAutomation), (void **)automation.out());
                if (hasInterface(result, automation.p)) {
                    Com<IUIAutomation2> options;
                    result = automation->QueryInterface(__uuidof(IUIAutomation2), (void **)options.out());
                    if (hasInterface(result, options.p)) { options->put_ConnectionTimeout(700); options->put_TransactionTimeout(500); }
                    while (!stop) {
                        if (HWND requested = forwardingDuplicateRequest.exchange(nullptr))
                            forwardingDuplicateResult = closeForwardingDuplicate(automation.p, requested, forwardingUrl, yandex) ? 1 : 2;
                        if (HWND requested = closeProbeRequest.exchange(nullptr)) {
                            std::wstring url; { std::lock_guard lock(addressMutex); url = closeProbeUrl; }
                            closeProbeResult = closeExactProbeTab(automation.p, requested, url, closeProbeDeadline, yandex) ? 1 : 2;
                        }
                        if (identifyForwarded && !forwardingUrl.empty()) {
                            struct ForwardSearch { Impl *self; IUIAutomation *automation; HWND found{}; } search{this, automation.p};
                            EnumWindows([](HWND window, LPARAM param) -> BOOL {
                                auto &s = *reinterpret_cast<ForwardSearch *>(param);
                                if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER) || lower(processPath(window)) != lower(s.self->exe)) return TRUE;
                                try {
                                    if (matchesActiveProbe(s.automation, window, s.self->forwardingUrl, s.self->yandex)) {
                                        s.found = window; return FALSE;
                                    }
                                } catch (...) {}
                                return TRUE;
                            }, reinterpret_cast<LPARAM>(&search));
                            if (search.found) { forwardedWindow = search.found; identifyForwarded = false; }
                        }
                        if (HWND requested = tabRequest.exchange(nullptr)) tabResult = selectExtensionsTab(automation.p, requested, tabDeadline, firefox, yandex);
                        if (HWND requested = addressRequest.exchange(nullptr)) {
                            std::wstring text; { std::lock_guard lock(addressMutex); text = requestedAddress; }
                            addressResult = setFocusedAddress(automation.p, requested, text, yandex);
                        }
                        HWND window = scanWindow;
                        if (!forwardingPreparation && window && IsWindow(window) && belongsToBrowser(window, exe)) {
                            try {
                                auto scene = inspect(automation.p, window, extension.wstring(), firefox, yandex);
                                if (firefox && !profileDirectory.empty()) {
                                    try {
                                        auto path = profileDirectory / L"extensions.json";
                                        if (fs::is_regular_file(path)) {
                                            auto json = Json::parse(readFile(path, 64 * 1024 * 1024));
                                            if (json.contains("addons") && json["addons"].is_array()) {
                                                bool found = false;
                                                for (const auto &addon : json["addons"]) if (str(addon, "id") == FirefoxId) {
                                                    found = true; scene.facts.installedCard = true;
                                                    if (addon.contains("active") && addon["active"].is_boolean()) { scene.facts.enabledKnown = true; scene.facts.enabled = addon["active"].get<bool>(); }
                                                    break;
                                                }
                                                scene.profileAbsenceKnown = !found;
                                            }
                                        }
                                    } catch (...) { /* Profile state can be temporarily unavailable. */ }
                                }
                                if (!firefox && (scene.facts.page || scene.facts.confirmationDialog) && !profileDirectory.empty()) {
                                    bool readAny = false, complete = true, settingsKnown = false, profileInstalled = false;
                                    for (auto filename : {L"Secure Preferences", L"Preferences"}) {
                                        try {
                                            auto file = profileDirectory / filename;
                                            if (!fs::exists(file)) continue;
                                            auto json = Json::parse(readFile(file, 64 * 1024 * 1024));
                                            if (!json.is_object()) { complete = false; continue; }
                                            readAny = true;
                                            auto extensions = json.find("extensions");
                                            if (extensions != json.end() && extensions->is_object()) {
                                                auto settings = extensions->find("settings");
                                                if (settings != extensions->end() && settings->is_object()) settingsKnown = true;
                                            }
                                            auto present = extensionPresence(json);
                                            // Live UIA state has priority; preferences can lag behind a click.
                                            if (present.developerKnown && !scene.facts.developerKnown) {
                                                scene.facts.developerKnown = true; scene.facts.developerOn = present.developerOn;
                                            }
                                            if (present.installed) {
                                                profileInstalled = true;
                                                scene.facts.installedCard = true;
                                                if (!scene.facts.enabledKnown && present.enabledKnown) { scene.facts.enabledKnown = true; scene.facts.enabled = present.enabled; }
                                            }
                                        } catch (...) { complete = false; /* Changing or unreadable data is unknown. */ }
                                    }
                                    scene.profileAbsenceKnown = readAny && complete && settingsKnown && !profileInstalled;
                                }
                                if (scene.valid) { std::lock_guard lock(mutex); latest = std::move(scene); }
                            } catch (...) { /* An unavailable provider must not terminate the application. */ }
                        }
                        if (forwardingPreparation) std::this_thread::sleep_for(std::chrono::milliseconds(20));
                        else for (int i = 0; i < 5 && !stop; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                }
            }
            if (SUCCEEDED(initialized)) CoUninitialize();
        });
        ShowWindow(panel, SW_SHOWNORMAL);
    }
};

ExtensionInstaller::ExtensionInstaller() = default;
ExtensionInstaller::~ExtensionInstaller() = default;
bool ExtensionInstaller::active() const { return impl && IsWindow(impl->panel) && !impl->stop; }
bool ExtensionInstaller::beginProfileVerification(const std::wstring &url) {
    if (!active() || !impl->explicitProfile || !impl->profileWindowBound || impl->navigationPending ||
        impl->challengePhase || !IsWindow(impl->browserWindow) || GetForegroundWindow() != impl->browserWindow ||
        url.rfind(L"http://127.0.0.1:", 0) != 0) return false;
    impl->challengeUrl = url; impl->challengePhase = 1;
    impl->challengeStarted = impl->challengeAt = GetTickCount64();
    impl->hideFocus(true);
    return true;
}
void ExtensionInstaller::finishProfileVerification(const std::wstring &url) {
    if (impl && !impl->forwardingUrl.empty() && activeBrowserAddressMatches(url, impl->forwardingUrl)) {
        impl->forwardingProofSeen = true;
        return;
    }
    if (!impl || impl->challengeUrl != url) return;
    impl->hideFocus(true);
    if (impl->challengePhase != 6) {
        impl->challengePhase = 6; impl->challengeAt = GetTickCount64(); impl->closeProbeDeadline = impl->challengeAt + Impl::profileCleanupLimit;
        { std::lock_guard lock(impl->addressMutex); impl->closeProbeUrl = url; }
        impl->closeProbeResult = 0; impl->closeProbeRequest = impl->browserWindow;
    }
}
bool ExtensionInstaller::profileVerificationFinishing() const {
    return active() && (impl->challengePhase == 6 || (!impl->forwardingUrl.empty() && impl->navigationPending));
}
bool ExtensionInstaller::translate(MSG &message) {
    (void)message;
    return false;
}
void ExtensionInstaller::connectionEstablished() {
    if (active()) {
        auto owner = impl->owner;
        impl->connectionConfirmed = true;
        impl->flow.stage = GuideStage::Done; impl->navigationPending = false;
        SendMessageW(impl->panel, WM_CLOSE, 0, 0);
        if (IsWindow(owner)) { if (IsIconic(owner)) ShowWindow(owner, SW_RESTORE); SetForegroundWindow(owner); }
    }
}
void ExtensionInstaller::setExtensionReloadRequired(bool required) { if (impl) { impl->extensionReloadRequired = required; InvalidateRect(impl->panel, nullptr, FALSE); } }
void ExtensionInstaller::setRetryConnection(std::function<void()> callback) { if (impl) impl->retryConnection = std::move(callback); }
void ExtensionInstaller::setOpenSetup(std::function<bool()> callback) { if (impl) impl->openSetup = std::move(callback); }
void ExtensionInstaller::setExtensionRunning(bool running) {
    if (!impl) return;
    impl->extensionRunning = running;
    // A disconnected/replaced session may need its permissions window opened
    // again. Do not leave Chromium stuck behind the previous session's flag.
    if (!running) impl->setupRequested = false;
}
void ExtensionInstaller::setLanguage(const std::string &language) {
    english = language == "en";
    if (impl) { impl->english = english; if (IsWindow(impl->panel)) { SetWindowTextW(impl->panel, english ? L"StreamShelf · Extension setup" : L"StreamShelf · Установка расширения"); InvalidateRect(impl->panel, nullptr, FALSE); } }
}
void ExtensionInstaller::close() { if (active()) SendMessageW(impl->panel, WM_CLOSE, 0, 0); }
Json ExtensionInstaller::diagnostics() const {
    if (!impl) return {{"active", false}};
    std::lock_guard lock(impl->mutex);
    const auto &scene = impl->latest;
    Json windows = Json::array();
    struct WindowList { const std::wstring &exe; Json &windows; } context{impl->exe, windows};
    EnumWindows([](HWND window, LPARAM data) -> BOOL {
        auto &context = *reinterpret_cast<WindowList *>(data);
        if (lower(processPath(window)) != lower(context.exe)) return TRUE;
        wchar_t cls[100]{}; GetClassNameW(window, cls, 100); RECT rect{}; GetWindowRect(window, &rect);
        context.windows.push_back({{"class", utf8(cls)}, {"visible", bool(IsWindowVisible(window))}, {"owned", bool(GetWindow(window, GW_OWNER))}, {"width", rect.right - rect.left}, {"height", rect.bottom - rect.top}});
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context));
    return {{"active", active()}, {"browser", utf8(impl->browserName)}, {"launched_pid", impl->launchPid}, {"browser_windows", windows},
        {"launch_error", impl->launchError}, {"connection_confirmed", impl->connectionConfirmed},
        {"toolbar_icon_found", scene.toolbarAction.found}, {"connector_popup", scene.connectorPopup}, {"toolbar_controls", scene.toolbarControls},
        {"browser_window", (uint64_t)(uintptr_t)impl->browserWindow},
        {"forwarding_profile", !impl->forwardingUrl.empty()}, {"forwarded_window_found", bool(impl->forwardedWindow.load())},
        {"browser_window_visible", bool(IsWindowVisible(impl->browserWindow))}, {"panel_visible", bool(IsWindowVisible(impl->panel))},
        {"page_verified", impl->pageVerified}, {"page_observed", scene.facts.page}, {"internal_url", utf8(scene.internalUrl.empty() ? impl->lastInternalUrl : scene.internalUrl)},
        {"developer_found", scene.developer.found}, {"load_found", scene.load.found}, {"outline_visible", bool(IsWindowVisible(impl->marker))},
        {"extension_present", scene.facts.installedCard}, {"extension_enabled_known", scene.facts.enabledKnown}, {"extension_enabled", scene.facts.enabled}, {"enable_target_found", scene.enable.found},
        {"live_card_found", scene.liveCardFound}, {"enable_candidates", scene.enableCandidates},
        {"own_names_seen", scene.ownNamesSeen}, {"own_names_offscreen", scene.ownNamesOffscreen},
        {"enable_confirmation_dialog", scene.facts.confirmationDialog},
        {"navigation_phase", impl->navigationPhase}, {"navigation_pending", impl->navigationPending}, {"address_result", impl->addressResult.load()},
        {"stage", int(impl->flow.stage)}, {"notice", utf8(impl->notice)}};
}
bool ExtensionInstaller::hasExplicitProfile() const { return impl && impl->explicitProfile; }
bool ExtensionInstaller::isVerificationUrl(const std::string &url) const {
    return impl && (activeBrowserAddressMatches(wide(url), impl->forwardingUrl) || activeBrowserAddressMatches(wide(url), impl->challengeUrl));
}
void ExtensionInstaller::start(HWND owner, const Browser &browser, const Profile *profile, const fs::path &extension, bool reduced, bool light) {
    if (active()) { showIfChanged(impl->panel, true); SetForegroundWindow(impl->panel); return; }
    auto kind = guidedBrowser(browser);
    auto arguments = extensionLaunchArguments(kind, profile);
    if (kind == GuidedBrowser::Firefox ? !fs::is_regular_file(extension) : !fs::is_regular_file(extension / L"manifest.json")) throw std::runtime_error("Сначала подготовьте расширение");
    auto next = std::make_unique<Impl>(); next->reducedMotion = reduced; next->lightTheme = light; next->english = english;
    if (profile) { next->profileDirectory = wide(profile->path); next->explicitProfile = !profile->path.empty(); }
    else if (kind != GuidedBrowser::Firefox) {
        PWSTR local = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local)) && local) {
            next->profileDirectory = fs::path(local) / (guidedBrowser(browser) == GuidedBrowser::Chrome ? L"Google/Chrome/User Data/Default" : L"Yandex/YandexBrowser/User Data/Default");
            CoTaskMemFree(local);
        }
    }
    if (next->explicitProfile && kind != GuidedBrowser::Firefox) {
        next->forwardingProbe = std::make_unique<GuideProfileChallenge>(GuideProbePurpose::openExtensions, english, light, reduced);
        next->forwardingUrl = wide(next->forwardingProbe->url);
        arguments.erase(std::remove(arguments.begin(), arguments.end(), L"--new-window"), arguments.end());
        if (!arguments.empty()) arguments.back() = next->forwardingUrl;
        next->identifyForwarded = true;
        next->forwardingPreparation = true;
    }
    // Publish all immutable forwarding state before create starts the scanner.
    next->create(owner, browser, extension);
    next->launchArguments = std::move(arguments);
    next->openExtensionsPage(); impl = std::move(next);
}
#ifdef STREAMSHELF_INSTALLER_TESTING
int ExtensionInstaller::runBrowserCardRegression(const std::wstring &browserExe) {
    HWND window = nullptr;
    for (int i = 0; i < 100 && !window; ++i) {
        struct Search { const std::wstring &exe; HWND result{}; } search{browserExe};
        EnumWindows([](HWND h, LPARAM value) -> BOOL {
            auto &s = *reinterpret_cast<Search *>(value);
            wchar_t title[512]{}; GetWindowTextW(h, title, 512);
            if (IsWindowVisible(h) && lower(processPath(h)) == lower(s.exe) &&
                std::wstring(title).find(L"StreamShelf accessibility fixture") != std::wstring::npos) {
                s.result = h; return FALSE;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&search));
        window = search.result;
        if (!window) Sleep(100);
    }
    if (!window) throw std::runtime_error("Окно локальной тестовой страницы Chrome не найдено");
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Com<IUIAutomation> automation;
    HRESULT result = CoCreateInstance(CLSID_CUIAutomation8, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IUIAutomation), (void **)automation.out());
    if (!hasInterface(result, automation.p)) throw std::runtime_error("UI Automation недоступна");
    Com<IUIAutomation2> options;
    result = automation->QueryInterface(__uuidof(IUIAutomation2), (void **)options.out());
    if (hasInterface(result, options.p)) { options->put_ConnectionTimeout(700); options->put_TransactionTimeout(500); }
    Scene scene;
    for (int i = 0; i < 2; ++i) {
        scene = inspect(automation.p, window, L"");
        if (scene.enable.found) break;
        Sleep(200);
    }
    Com<IUIAutomationElement> root, expected;
    result = automation->ElementFromHandle(window, root.out());
    if (!hasInterface(result, root.p)) throw std::runtime_error("Дерево тестового окна недоступно");
    Variant value; value.value.vt = VT_BSTR; value.value.bstrVal = SysAllocString(L"fixture Connector toggle");
    Com<IUIAutomationCondition> condition;
    result = automation->CreatePropertyCondition(UIA_NamePropertyId, value.value, condition.out());
    if (!hasInterface(result, condition.p)) throw std::runtime_error("Поиск тестового переключателя недоступен");
    result = root->FindFirst(TreeScope_Descendants, condition.p, expected.out());
    if (!hasInterface(result, expected.p)) throw std::runtime_error("Тестовый переключатель не найден в UIA");
    RECT target{}; expected->get_CurrentBoundingRectangle(&target);
    std::cout << "fixture: card=" << scene.liveCardFound << " target=" << scene.enable.found
        << " expected=" << target.left << ',' << target.top << ',' << target.right << ',' << target.bottom
        << " actual=" << scene.enable.rect.left << ',' << scene.enable.rect.top << ',' << scene.enable.rect.right << ',' << scene.enable.rect.bottom << std::endl;
    if (!scene.diagnostics.empty()) std::cout << utf8(scene.diagnostics);
    if (!scene.liveCardFound || !scene.enable.found || !EqualRect(&scene.enable.rect, &target)) {
        Com<IUIAutomationCondition> all; automation->CreateTrueCondition(all.out());
        Com<IUIAutomationCacheRequest> cache; automation->CreateCacheRequest(cache.out());
        if (all.p && cache.p) {
            cache->put_TreeScope(TreeScope_Element);
            for (auto id : {UIA_NamePropertyId, UIA_AutomationIdPropertyId, UIA_ControlTypePropertyId, UIA_BoundingRectanglePropertyId, UIA_IsOffscreenPropertyId, UIA_IsKeyboardFocusablePropertyId, UIA_IsTogglePatternAvailablePropertyId}) cache->AddProperty(id);
            Com<IUIAutomationElementArray> elements;
            if (hasInterface(root->FindAllBuildCache(TreeScope_Descendants, all.p, cache.p, elements.out()), elements.p)) {
                int count = 0; elements->get_Length(&count);
                for (int i = 0; i < std::min(count, 3500); ++i) {
                    Com<IUIAutomationElement> e;
                    if (!hasInterface(elements->GetElement(i, e.out()), e.p)) continue;
                    auto n = cachedString(e.p, UIA_NamePropertyId), id = cachedString(e.p, UIA_AutomationIdPropertyId);
                    if (!ownExtensionName(n) && !removeName(n) && n.find(L"fixture") == n.npos && id.find(L"enable") == id.npos) continue;
                    RECT r{}; e->get_CachedBoundingRectangle(&r);
                    std::cout << "node name=" << utf8(n) << " id=" << utf8(id) << " type=" << cachedInt(e.p, UIA_ControlTypePropertyId).value_or(-1)
                        << " off=" << cachedInt(e.p, UIA_IsOffscreenPropertyId).value_or(-1)
                        << " focus=" << cachedInt(e.p, UIA_IsKeyboardFocusablePropertyId).value_or(-1)
                        << " toggle=" << cachedInt(e.p, UIA_IsTogglePatternAvailablePropertyId).value_or(-1)
                        << " rect=" << r.left << ',' << r.top << ',' << r.right << ',' << r.bottom << std::endl;
                }
            }
        }
        throw std::runtime_error("Мастер не нашёл точный переключатель в браузерной тестовой карточке");
    }
    std::cout << "PASS real Chromium accessibility tree targets only Connector switch" << std::endl;
    CoUninitialize(); return 1;
}
namespace {
struct NativeFolderEvents : IFileDialogEvents {
    LONG refs = 1;
    std::wstring &folder;
    explicit NativeFolderEvents(std::wstring &value) : folder(value) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (id == __uuidof(IUnknown) || id == __uuidof(IFileDialogEvents)) { *out = static_cast<IFileDialogEvents *>(this); AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&refs); }
    ULONG STDMETHODCALLTYPE Release() override { return InterlockedDecrement(&refs); }
    HRESULT STDMETHODCALLTYPE OnFileOk(IFileDialog *) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnFolderChanging(IFileDialog *, IShellItem *) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnFolderChange(IFileDialog *dialog) override {
        Com<IShellItem> item; HRESULT result = dialog->GetFolder(item.out());
        if (hasInterface(result, item.p)) {
            PWSTR value = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &value)) && value) { folder = value; CoTaskMemFree(value); }
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnSelectionChange(IFileDialog *) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnShareViolation(IFileDialog *, IShellItem *, FDE_SHAREVIOLATION_RESPONSE *result) override { *result = FDESVR_DEFAULT; return S_OK; }
    HRESULT STDMETHODCALLTYPE OnTypeChange(IFileDialog *) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnOverwrite(IFileDialog *, IShellItem *, FDE_OVERWRITE_RESPONSE *result) override { *result = FDEOR_DEFAULT; return S_OK; }
};
}
int ExtensionInstaller::runNativeRegression(const fs::path &folder) {
    int passed = 0;
    auto check = [&](bool ok, const char *label) {
        if (!ok) throw std::runtime_error(label);
        ++passed; std::cout << "PASS native guide: " << label << std::endl;
    };
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    WNDCLASSEXW cls{sizeof(cls)}; cls.lpfnWndProc = DefWindowProcW; cls.hInstance = GetModuleHandleW(nullptr); cls.lpszClassName = L"StreamShelfGuideFixture";
    RegisterClassExW(&cls);
    HWND fixture = CreateWindowExW(0, cls.lpszClassName, L"StreamShelf installer regression fixture", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        30, 30, 800, 600, nullptr, nullptr, cls.hInstance, nullptr);
    HWND edit = CreateWindowExW(0, L"EDIT", L"chrome://extensions/", WS_CHILD | WS_VISIBLE, 20, 20, 500, 30, fixture, nullptr, cls.hInstance, nullptr);
    HWND toggle = CreateWindowExW(0, L"BUTTON", L"Developer mode", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 450, 75, 220, 35, fixture, nullptr, cls.hInstance, nullptr);
    HWND load = CreateWindowExW(0, L"BUTTON", L"Load unpacked", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 125, 230, 35, fixture, nullptr, cls.hInstance, nullptr);
    ShowWindow(load, SW_HIDE);
    ExtensionInstaller guide; std::thread dialogThread; std::atomic<DWORD> dialogThreadId{0}; std::atomic_bool dialogDone{false};
    std::promise<std::wstring> actualFolder; auto actual = actualFolder.get_future();
    auto pump = [&](auto condition, DWORD timeout = 8000) {
        auto end = GetTickCount64() + timeout;
        do {
            MSG message;
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (!guide.translate(message)) { TranslateMessage(&message); DispatchMessageW(&message); }
            }
            if (condition()) return true;
            MsgWaitForMultipleObjects(0, nullptr, FALSE, 20, QS_ALLINPUT);
        } while (GetTickCount64() < end);
        return false;
    };
    auto cancelDialog = [&] {
        if (dialogThreadId) EnumThreadWindows(dialogThreadId, [](HWND window, LPARAM) -> BOOL {
            wchar_t name[100]{}; GetClassNameW(window, name, 100);
            if (std::wstring(name) == L"#32770") PostMessageW(window, WM_CLOSE, 0, 0); return TRUE;
        }, 0);
        if (dialogThread.joinable()) { pump([&] { return dialogDone.load(); }, 2500); dialogThread.join(); }
    };
    try {
        Browser browser{"Regression fixture", "chromium", utf8(executable().wstring())};
        guide.impl = std::make_unique<Impl>(); guide.impl->create(fixture, browser, folder);
        guide.impl->autoCloseOnEnabled = false; guide.impl->reducedMotion = true;
        guide.impl->navigationPending = false; guide.impl->scanWindow = fixture;
        check(fixture && edit && toggle && load, "fixture created");
        check(activeBrowserAddressMatches(L"127.0.0.1:43123/streamshelf-profile-abc", L"http://127.0.0.1:43123/streamshelf-profile-abc"),
            "probe address accepts Chromium's omitted http scheme");
        check(!activeBrowserAddressMatches(L"127.0.0.1:43123/streamshelf-profile-other", L"http://127.0.0.1:43123/streamshelf-profile-abc") &&
              !activeBrowserAddressMatches(L"example.test/streamshelf-profile-abc", L"http://127.0.0.1:43123/streamshelf-profile-abc"),
            "probe address rejects a different nonce or host");
        Com<IUIAutomation> addressAutomation;
        auto addressAutomationResult = CoCreateInstance(CLSID_CUIAutomation8, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IUIAutomation), (void **)addressAutomation.out());
        check(hasInterface(addressAutomationResult, addressAutomation.p) && readActiveBrowserAddress(addressAutomation.p, fixture) == L"chrome://extensions/",
            "lightweight address scan identifies the visible browser chrome");
        SetForegroundWindow(guide.impl->panel);
        std::cout << "READY: activate the local regression fixture if Windows denied foreground focus" << std::endl;
        check(pump([&] { return lower(processPath(GetForegroundWindow())) == lower(browser.exe.empty() ? L"" : wide(browser.exe)); }, 30000), "fixture foreground granted");
        guide.impl->scanWindow = fixture;
        check(pump([&] { return guide.impl->displayed.facts.page && IsWindowVisible(guide.impl->marker); }), "cached UIA discovers extension page and checkbox");
        check(guide.impl->flow.stage == GuideStage::Developer, "unchecked mode waits");
        RECT dimRect{}; GetWindowRect(guide.impl->dimmer, &dimRect);
        RECT targetRect{}; GetWindowRect(toggle, &targetRect);
        HRGN shadeRegion = CreateRectRgn(0, 0, 0, 0);
        check(GetWindowRgn(guide.impl->dimmer, shadeRegion) != ERROR, "screen shade has an explicit focus region");
        check(!PtInRegion(shadeRegion, (targetRect.left + targetRect.right) / 2 - dimRect.left,
            (targetRect.top + targetRect.bottom) / 2 - dimRect.top), "target button remains outside darkening region");
        check(PtInRegion(shadeRegion, targetRect.left - guide.impl->px(7) - dimRect.left,
            (targetRect.top + targetRect.bottom) / 2 - dimRect.top), "no rectangular bright patch outside the outline");
        DeleteObject(shadeRegion);
        BYTE opacity = 0; DWORD shadeFlags = 0;
        check(GetLayeredWindowAttributes(guide.impl->dimmer, nullptr, &opacity, &shadeFlags) && opacity == 155,
            "surrounding screen uses translucent darkening");
        RECT buttonRect{}; GetWindowRect(toggle, &buttonRect); RECT expected = buttonRect; InflateRect(&expected, guide.impl->px(9), guide.impl->px(9));
        check(EqualRect(&expected, &guide.impl->markerRect), "outline follows actual checkbox bounds");
        auto updates = guide.impl->markerUpdates, paints = guide.impl->panelPaints;
        pump([] { return false; }, 850);
        check(updates == guide.impl->markerUpdates, "stationary outline is not repainted or repositioned every timer tick");
        check(guide.impl->panelPaints > paints, "OpenGL panel animation timer renders while the target remains stable");
        SendMessageW(toggle, BM_SETCHECK, BST_CHECKED, 0); ShowWindow(load, SW_SHOW);
        NotifyWinEvent(EVENT_OBJECT_STATECHANGE, toggle, OBJID_CLIENT, CHILDID_SELF);
        check(pump([&] { return guide.impl->flow.stage == GuideStage::Load; }), "checkbox automatically advances to load");
        GetWindowRect(load, &expected); InflateRect(&expected, guide.impl->px(9), guide.impl->px(9));
        check(pump([&] { return EqualRect(&expected, &guide.impl->markerRect); }), "outline moves to actual load button");
        SetWindowTextW(edit, L"about:blank"); ShowWindow(load, SW_HIDE); ShowWindow(toggle, SW_HIDE);
        check(pump([&] { return !guide.impl->displayed.facts.page; }), "leaving extensions page is observed");
        check(IsWindowVisible(guide.impl->panel), "wrong browser page does not hide the guide panel");
        check(!IsWindowVisible(guide.impl->marker), "wrong browser page does not retain a misleading target outline");
        HWND unrelated = CreateWindowExW(0, L"STATIC", L"Local unrelated window", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 380, 200, nullptr, nullptr, cls.hInstance, nullptr);
        SetForegroundWindow(unrelated);
        pump([] { return false; }, 400);
        check(IsWindowVisible(guide.impl->panel), "switching to another window does not dismiss the guide");
        DestroyWindow(unrelated); SetForegroundWindow(fixture);
        SetWindowTextW(edit, L"chrome://extensions/"); ShowWindow(load, SW_SHOW); ShowWindow(toggle, SW_SHOW);
        check(pump([&] { return guide.impl->displayed.facts.page && IsWindowVisible(guide.impl->marker); }), "returning to extensions restores target outline");
        dialogThread = std::thread([&] {
            dialogThreadId = GetCurrentThreadId(); CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            std::wstring observed;
            {
                Com<IFileOpenDialog> dialog;
                HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IFileOpenDialog), (void **)dialog.out());
                if (hasInterface(result, dialog.p)) {
                    NativeFolderEvents events(observed); DWORD cookie = 0; dialog->Advise(&events, &cookie);
                    dialog->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR);
                    dialog->SetTitle(L"StreamShelf folder navigation regression — no extension installation");
                    Com<IShellItem> initial;
                    result = SHCreateItemFromParsingName(folder.parent_path().c_str(), nullptr, __uuidof(IShellItem), (void **)initial.out());
                    if (hasInterface(result, initial.p)) dialog->SetFolder(initial.p);
                    dialog->Show(fixture);
                    dialog->Unadvise(cookie);
                }
            }
            actualFolder.set_value(observed); dialogDone = true; CoUninitialize();
        });
        bool reachedFolder = pump([&] { return guide.impl->flow.stage == GuideStage::Confirm; }, 13000);
        if (!reachedFolder) {
            std::lock_guard lock(guide.impl->mutex);
            std::cerr << "STATE=" << int(guide.impl->flow.stage) << " VALID=" << guide.impl->latest.valid
                << " PICKER=" << guide.impl->latest.facts.folderDialog << " READY=" << guide.impl->latest.facts.folderReady
                << " ATTEMPTS=" << guide.impl->pickerAttempts << "\n" << utf8(guide.impl->latest.diagnostics) << std::endl;
        }
        check(reachedFolder, "real Windows folder picker navigates automatically and advances on observed path");
        check(!dialogDone, "folder confirmation stays open for the user");
        check(IsWindowVisible(guide.impl->marker), "select-folder button is outlined");
        cancelDialog();
        auto observed = actual.get();
        if (normalizedPath(observed) != normalizedPath(folder.wstring())) std::cerr << "ACTUAL FOLDER=" << utf8(observed) << std::endl;
        check(normalizedPath(observed) == normalizedPath(folder.wstring()), "actual IFileDialog folder equals extension directory");
        SetForegroundWindow(guide.impl->panel); guide.impl->scanWindow = fixture;
        check(pump([&] { return guide.impl->flow.stage == GuideStage::Verify; }, 7000), "closed picker waits for observed install state instead of restarting");
        pump([] { return false; }, 1900);
        check(guide.impl->flow.stage == GuideStage::Verify, "missing card after 1.6 seconds does not create a loading loop");
        HWND card = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 10, 190, 450, 230, fixture, nullptr, cls.hInstance, nullptr);
        HWND cardName = CreateWindowExW(0, L"STATIC", L"StreamShelf Connector 1.0.0", WS_CHILD | WS_VISIBLE, 20, 20, 280, 30, card, nullptr, cls.hInstance, nullptr);
        HWND remove = CreateWindowExW(0, L"BUTTON", L"Remove", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 160, 140, 35, card, nullptr, cls.hInstance, nullptr);
        HWND enable = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 375, 166, 30, 22, card, nullptr, cls.hInstance, nullptr);
        HWND otherCard = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 480, 190, 280, 230, fixture, nullptr, cls.hInstance, nullptr);
        HWND otherName = CreateWindowExW(0, L"STATIC", L"Unrelated extension", WS_CHILD | WS_VISIBLE, 10, 20, 220, 30, otherCard, nullptr, cls.hInstance, nullptr);
        HWND otherRemove = CreateWindowExW(0, L"BUTTON", L"Remove", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 160, 100, 35, otherCard, nullptr, cls.hInstance, nullptr);
        HWND otherEnable = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 225, 166, 30, 22, otherCard, nullptr, cls.hInstance, nullptr);
        check(card && cardName && remove && enable && otherCard && otherName && otherRemove && otherEnable, "two extension cards with distinct compact toggles created");
        check(pump([&] { return guide.impl->flow.stage == GuideStage::Enable; }), "newly installed but disabled extension advances to enable, not load");
        SetForegroundWindow(fixture);
        GetWindowRect(enable, &expected); InflateRect(&expected, guide.impl->px(9), guide.impl->px(9));
        check(pump([&] { return EqualRect(&expected, &guide.impl->markerRect) && IsWindowVisible(guide.impl->marker); }), "outline targets extension toggle rather than developer mode");
        SendMessageW(enable, BM_SETCHECK, BST_CHECKED, 0); NotifyWinEvent(EVENT_OBJECT_STATECHANGE, enable, OBJID_CLIENT, CHILDID_SELF);
        check(pump([&] { return guide.impl->flow.stage == GuideStage::Connect; }), "enabling extension automatically advances to connection");
        ShowWindow(card, SW_HIDE); pump([] { return false; }, 1900);
        check(guide.impl->flow.stage == GuideStage::Connect, "temporary missing installed card cannot send guide back to loading");
        ShowWindow(card, SW_SHOW); SendMessageW(enable, BM_SETCHECK, BST_UNCHECKED, 0);
        NotifyWinEvent(EVENT_OBJECT_STATECHANGE, enable, OBJID_CLIENT, CHILDID_SELF);
        check(pump([&] { return guide.impl->flow.stage == GuideStage::Enable; }), "switching extension off returns to enable step");
        GetWindowRect(enable, &expected); InflateRect(&expected, guide.impl->px(9), guide.impl->px(9));
        check(pump([&] { return EqualRect(&expected, &guide.impl->markerRect) && IsWindowVisible(guide.impl->marker); }), "reopened disabled extension outlines its own switch, not the neighbouring card");
        SendMessageW(toggle, BM_SETCHECK, BST_UNCHECKED, 0); ShowWindow(load, SW_HIDE);
        NotifyWinEvent(EVENT_OBJECT_STATECHANGE, toggle, OBJID_CLIENT, CHILDID_SELF);
        check(pump([&] { return guide.impl->flow.stage == GuideStage::Enable && guide.impl->displayed.facts.developerKnown && !guide.impl->displayed.facts.developerOn; }), "installed extension remains the priority when developer mode is disabled");
        SendMessageW(toggle, BM_SETCHECK, BST_CHECKED, 0); ShowWindow(load, SW_SHOW);
        NotifyWinEvent(EVENT_OBJECT_STATECHANGE, toggle, OBJID_CLIENT, CHILDID_SELF);
        check(pump([&] { return guide.impl->flow.stage == GuideStage::Enable; }), "enabling developer mode resumes at the existing disabled extension");
        check(pump([&] { return EqualRect(&expected, &guide.impl->markerRect) && IsWindowVisible(guide.impl->marker); }), "resuming after developer mode outlines the extension switch");
        // Start a genuinely new guide instance against an already populated page.
        guide.impl.reset();
        guide.impl = std::make_unique<Impl>(); guide.impl->reducedMotion = true;
        guide.impl->create(fixture, browser, folder);
        guide.impl->autoCloseOnEnabled = false; guide.impl->navigationPending = false;
        guide.impl->scanWindow = fixture; SetForegroundWindow(fixture);
        check(pump([&] { return guide.impl->flow.stage == GuideStage::Enable; }), "fresh guide starts at an already installed disabled extension");
        check(pump([&] { return EqualRect(&expected, &guide.impl->markerRect) && IsWindowVisible(guide.impl->marker); }), "fresh guide outlines the already installed extension switch");
        SendMessageW(enable, BM_CLICK, 0, 0); NotifyWinEvent(EVENT_OBJECT_STATECHANGE, enable, OBJID_CLIENT, CHILDID_SELF);
        check(pump([&] { return guide.impl->flow.stage == GuideStage::Connect; }), "clicking the existing extension switch advances the fresh guide");
        SendMessageW(enable, BM_CLICK, 0, 0); NotifyWinEvent(EVENT_OBJECT_STATECHANGE, enable, OBJID_CLIENT, CHILDID_SELF);
        check(pump([&] { return guide.impl->flow.stage == GuideStage::Enable; }), "disabling the existing extension again restores enable");
        SendMessageW(toggle, BM_SETCHECK, BST_UNCHECKED, 0); ShowWindow(load, SW_HIDE);
        guide.impl.reset();
        guide.impl = std::make_unique<Impl>(); guide.impl->reducedMotion = true;
        guide.impl->create(fixture, browser, folder);
        guide.impl->autoCloseOnEnabled = false; guide.impl->navigationPending = false;
        guide.impl->scanWindow = fixture; SetForegroundWindow(fixture);
        check(pump([&] { return guide.impl->flow.stage == GuideStage::Enable && guide.impl->displayed.facts.installedCard && guide.impl->displayed.facts.developerKnown && !guide.impl->displayed.facts.developerOn; }), "fresh guide prioritizes installed extension even with developer mode disabled");
        GetWindowRect(enable, &expected); InflateRect(&expected, guide.impl->px(9), guide.impl->px(9));
        check(pump([&] { return EqualRect(&expected, &guide.impl->markerRect) && IsWindowVisible(guide.impl->marker); }), "fresh guide outlines the installed extension switch");
        SendMessageW(toggle, BM_CLICK, 0, 0); ShowWindow(load, SW_SHOW);
        NotifyWinEvent(EVENT_OBJECT_STATECHANGE, toggle, OBJID_CLIENT, CHILDID_SELF);
        check(pump([&] { return guide.impl->flow.stage == GuideStage::Enable; }), "clicking developer mode resumes at already disabled extension");
        GetWindowRect(enable, &expected); InflateRect(&expected, guide.impl->px(9), guide.impl->px(9));
        check(pump([&] { return EqualRect(&expected, &guide.impl->markerRect) && IsWindowVisible(guide.impl->marker); }), "fresh guide outlines extension switch after developer click");
        SendMessageW(enable, BM_CLICK, 0, 0); NotifyWinEvent(EVENT_OBJECT_STATECHANGE, enable, OBJID_CLIENT, CHILDID_SELF);
        check(pump([&] { return guide.impl->flow.stage == GuideStage::Connect; }), "enable click advances to connection");
        auto fixtureProfile = executable().parent_path() / (L"fixture-profile-" + std::to_wstring(GetCurrentProcessId()));
        fs::create_directories(fixtureProfile);
        Json fixturePrefs = {{"extensions", {{"ui", {{"developer_mode", true}}},
            {"settings", {{ExtensionId, {{"path", "fixture/connector"}, {"disable_reasons", Json::array()}}}}}}}};
        writeFile(fixtureProfile / L"Secure Preferences", fixturePrefs.dump());
        guide.impl.reset();
        guide.impl = std::make_unique<Impl>(); guide.impl->reducedMotion = true;
        guide.impl->profileDirectory = fixtureProfile;
        guide.impl->create(fixture, browser, folder);
        guide.impl->autoCloseOnEnabled = false; guide.impl->navigationPending = false;
        guide.impl->scanWindow = fixture; SetForegroundWindow(fixture);
        check(pump([&] { return guide.impl->flow.stage == GuideStage::Connect; }), "fresh guide starts at connection for already enabled extension");
        SendMessageW(enable, BM_CLICK, 0, 0); NotifyWinEvent(EVENT_OBJECT_STATECHANGE, enable, OBJID_CLIENT, CHILDID_SELF);
        check(pump([&] { return guide.impl->flow.stage == GuideStage::Enable; }), "turning the extension off after restart restores enable");
        DestroyWindow(card); card = nullptr;
        fixturePrefs["extensions"]["settings"] = Json::object();
        writeFile(fixtureProfile / L"Secure Preferences", fixturePrefs.dump());
        check(pump([&] { return guide.impl->flow.stage == GuideStage::Load; }, 10000), "confirmed removal from page and profile returns to load step");
        GetWindowRect(load, &expected); InflateRect(&expected, guide.impl->px(9), guide.impl->px(9));
        check(pump([&] { return EqualRect(&expected, &guide.impl->markerRect) && IsWindowVisible(guide.impl->marker); }), "after removal the unpacked-load control is outlined");
        HWND confirmPopup = CreateWindowExW(0, L"STATIC", L"StreamShelf Connector", WS_POPUP | WS_BORDER | WS_VISIBLE,
            150, 150, 480, 210, fixture, nullptr, cls.hInstance, nullptr);
        HWND confirmLabel = CreateWindowExW(0, L"STATIC", L"StreamShelf Connector requests permission", WS_CHILD | WS_VISIBLE,
            20, 20, 420, 40, confirmPopup, nullptr, cls.hInstance, nullptr);
        HWND confirmEnable = CreateWindowExW(0, L"BUTTON", L"Enable", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            280, 120, 150, 35, confirmPopup, nullptr, cls.hInstance, nullptr);
        check(confirmPopup && confirmLabel && confirmEnable, "browser-style owned enable confirmation created");
        SetForegroundWindow(confirmPopup);
        GetWindowRect(confirmEnable, &expected); InflateRect(&expected, guide.impl->px(9), guide.impl->px(9));
        check(pump([&] { return guide.impl->displayed.facts.confirmationDialog && EqualRect(&expected, &guide.impl->markerRect) && IsWindowVisible(guide.impl->marker); }),
            "owned browser confirmation stays in enable step and outlines its enable button");
        check(guide.impl->browserWindow == fixture && guide.impl->flow.stage == GuideStage::Enable, "permission popup does not replace the main browser or restart loading");
        DestroyWindow(confirmPopup); SetForegroundWindow(fixture);
        guide.impl->flow.stage = GuideStage::Connect;
        ShowWindow(fixture, SW_MINIMIZE); pump([] { return false; }, 350);
        check(IsWindowVisible(guide.impl->panel), "minimizing the application does not hide the independent guide");
        guide.impl->reducedMotion = false;
        auto before = guide.impl->markerUpdates;
        guide.impl->outline(buttonRect, true);
        const auto revealAt = guide.impl->markerRevealAt;
        guide.impl->paintOverlay(revealAt + 65);
        guide.impl->paintOverlay(revealAt + 140);
        BYTE dimmerOpacity = 0;
        check(guide.impl->markerAlpha == 128 &&
              GetLayeredWindowAttributes(guide.impl->dimmer, nullptr, &dimmerOpacity, nullptr) && dimmerOpacity == 78,
              "the border and the dimmer appear together");
        check(guide.impl->markerUpdates >= before + 2, "animated border redraws without recreating its window");
        guide.impl->paintOverlay(revealAt + guideOverlayRevealMs);
        auto drawn = guide.impl->markerRect;
        guide.impl->hideFocus();
        guide.impl->paintOverlay(guide.impl->markerLostAt + 120);
        BYTE markerGlobalAlpha = 0;
        check(guide.impl->markerAlpha == 128 && EqualRect(&drawn, &guide.impl->markerRect) &&
              !GetLayeredWindowAttributes(guide.impl->marker, nullptr, &markerGlobalAlpha, nullptr),
              "fading preserves position and uses the per-pixel layered surface");
        guide.impl->paintOverlay(guide.impl->markerLostAt + 240);
        check(!IsWindowVisible(guide.impl->marker) && !IsWindowVisible(guide.impl->dimmer), "fade ends with both layers hidden");
        guide.impl->outline(buttonRect, true);
        guide.impl->paintOverlay(guide.impl->markerRevealAt + guideOverlayRevealMs);
        guide.impl->challengeUrl = L"http://127.0.0.1:8765/test";
        guide.finishProfileVerification(guide.impl->challengeUrl);
        check(!IsWindowVisible(guide.impl->marker) && !IsWindowVisible(guide.impl->dimmer),
              "closing the service tab removes both highlighting layers before automation");
        guide.impl->closeProbeDeadline = 0; guide.impl->closeProbeRequest = nullptr;
        guide.impl->challengePhase = 0; guide.impl->challengeUrl.clear();
        check((GetWindowLongPtrW(guide.impl->dimmer, GWL_EXSTYLE) & WS_EX_TRANSPARENT) != 0, "dimmer passes mouse input through");
        guide.impl->autoCloseOnEnabled = true; guide.impl->pageVerified = true;
        guide.impl->setupRequested = true;
        guide.setExtensionRunning(false);
        check(!guide.impl->setupRequested, "connection loss permits the replacement Chromium session to reopen setup");
        guide.impl->tick();
        check(guide.active(), "enabled extension alone does not close the guide before connection confirmation");
        guide.connectionEstablished();
        check(!guide.active() && guide.impl->connectionConfirmed, "confirmed connection closes installation guide");
        check(!IsWindowVisible(guide.impl->dimmer), "completion removes screen dimming");
        guide.impl.reset(); DestroyWindow(fixture); CoUninitialize(); return passed;
    } catch (...) {
        cancelDialog(); guide.impl.reset(); if (IsWindow(fixture)) DestroyWindow(fixture); CoUninitialize(); throw;
    }
}
#endif
} // namespace ss
