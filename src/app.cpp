#include "guide_profile_challenge.hpp"
#include "extension_guide_state.hpp"
#include "app.hpp"
#include "probe_result.hpp"
#include "media_merge.hpp"
#include "browser_identity.hpp"
#include "download_validation.hpp"
#include "http_resume.hpp"
#include "media_display.hpp"
#include "preview_policy.hpp"
#include "media_preview.hpp"
#include <fstream>
#include <shlobj.h>
#include <sddl.h>
#include <iomanip>
#include <wincrypt.h>
namespace ss {
// The same bound applies to encrypted history on both read and write.  Other
// DPAPI files retain the smaller default read limit.
static constexpr size_t QueueHistoryLimit = size_t(512) * 1024 * 1024;
static std::wstring guideIdentityPath(const std::string &path) {
    if (path.empty()) return {};
    auto value = fs::path(wide(path)).lexically_normal().wstring();
    DWORD needed = GetLongPathNameW(value.c_str(), nullptr, 0);
    if (needed && needed < 32768) {
        std::wstring expanded(needed, L'\0');
        DWORD written = GetLongPathNameW(value.c_str(), expanded.data(), needed);
        if (written && written < needed) { expanded.resize(written); value = std::move(expanded); }
    }
    if (!value.empty()) CharLowerBuffW(value.data(), DWORD(value.size()));
    return value;
}
static Json mediaJson(const Media &m) {
    return {{"id", m.id},
            {"key", m.key},
            {"title", m.title},
            {"sourceTitle", m.sourceTitle},
            {"thumbnail", m.thumbnail},
            {"category", m.category},
            {"ext", m.ext},
            {"quality", m.quality}, {"size", m.size}, {"approx", m.approx},
            {"fps", m.fps}, {"rate", m.rate},
            {"url", m.url},
            {"page", m.page},
            {"protocol", m.protocol},
            {"credentialUrl", m.credentialUrl},
            {"format", m.format},
            {"videoOnly", m.videoOnly},
            {"automatic", m.automatic},
            {"drm", m.drm},
            {"language", m.language},
            {"vcodec",m.vcodec},{"acodec",m.acodec},
            {"data", m.data}};
}
static Media mediaFrom(const Json &j) {
    Media m;
    m.id = str(j, "id");
    m.key = str(j, "key");
    m.title = str(j, "title");
    m.sourceTitle = str(j, "sourceTitle");
    m.thumbnail = str(j,"thumbnail");
    m.category = str(j, "category");
    m.ext = str(j, "ext");
    m.quality = str(j, "quality");
    m.size = number(j, "size");
    m.approx = j.value("approx", false);
    m.fps = number(j, "fps");
    m.rate = number(j, "rate");
    m.url = str(j, "url");
    m.page = str(j, "page");
    m.protocol = str(j, "protocol");
    m.credentialUrl = str(j, "credentialUrl");
    m.format = j.value("format", false);
    m.videoOnly = j.value("videoOnly", false);
    m.automatic = j.value("automatic", false);
    m.drm = j.value("drm", false);
    m.language = str(j, "language");
    m.vcodec = str(j,"vcodec"); m.acodec = str(j,"acodec");
    m.data = j.value("data", Json());
    return m;
}
App::App() {
    interfaceLanguage = defaultInterfaceLanguage(GetUserDefaultUILanguage());
    auto root = dataRoot();
    fs::create_directories(root / L"tools");
    fs::create_directories(root / L"temp");
    for (auto &p : fs::directory_iterator(root / L"temp")) {
        std::error_code ec;
        fs::remove(p.path(), ec);
    }
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &path))) {
        destination = utf8((fs::path(path) / L"StreamShelf").wstring());
        CoTaskMemFree(path);
    }
    try {
        if (fs::exists(root / L"settings.dpapi")) {
            auto j = Json::parse(unprotectFile(root / L"settings.dpapi"));
            const auto savedLanguage = str(j, "interfaceLanguage");
            if (savedLanguage == "en" || savedLanguage == "ru") interfaceLanguage = savedLanguage;
            destination = str(j, "destination", destination);
            autoUpdate = j.value("autoUpdate", true);
            light = j.value("light", false);
            reduced = j.value("reduced", false);
            keepResults = j.value("keepResults", false);
            splashEnabled = j.value("splashEnabled", true);
            splashSeconds = std::clamp(j.value("splashSeconds", 3), 1, 10);
            parallel = std::clamp(j.value("parallel", 2), 1, 4);
            auto savedFragments = j.value("fragments", 4);
            fragments = savedFragments == 1 || savedFragments == 2 || savedFragments == 4 || savedFragments == 8 ? savedFragments : 4;
        }
    } catch (...) {
    }
    title = interfaceLanguage == "ru" ? "Медиа" : "Media";
    assemblyName = interfaceLanguage == "ru" ? "Мой контейнер" : "My container";
    extensionInstaller.setLanguage(interfaceLanguage);
    try {
        if (fs::exists(root / L"queue.dpapi")) {
            auto j = Json::parse(unprotectFile(root / L"queue.dpapi", QueueHistoryLimit));
            if (!j.is_array()) throw std::runtime_error("Invalid queue history");
            for (auto &v : j) {
                try {
                auto job = std::make_shared<Job>();
                job->id = str(v, "id");
                job->title = str(v, "title");
                job->page = str(v, "page");
                job->folder = str(v, "folder");
                job->pair = str(v, "pair");
                job->local = str(v, "local");
                job->output = str(v, "output");
                job->outputDirectory = str(v, "outputDirectory");
                job->item = mediaFrom(v.at("item"));
                // Old histories did not record their browser identity. They must
                // not silently adopt whichever account happens to be selected now.
                job->browserSourceRequired = v.value("browserSourceRequired", !v.value("localFile", false) && !v.value("assembly", false) && httpUrl(job->page));
                const auto accessKey = str(v, "resumeAccessKey");
                if (job->browserSourceRequired && validJobAccessKey(accessKey)) job->resumeAccessKey = accessKey;
                job->assemblyInput = v.value("assemblyInput",false);
                job->localFile=v.value("localFile",false);job->conversionSource=v.value("conversionSource",false);
                job->assembly = v.value("assembly",false);
                job->singleExport=v.value("singleExport",false);job->exportFormat=v.value("exportFormat",0);job->exportQuality=v.value("exportQuality",1);
                job->assemblyMetadataTitle = str(v,"assemblyMetadataTitle"); job->assemblyArtist = str(v,"assemblyArtist"); job->assemblyDescription = str(v,"assemblyDescription");
                if(v.contains("globalTags") && v["globalTags"].is_object())job->globalTags=v["globalTags"].get<std::map<std::string,std::string>>();
                if(v.contains("originalGlobalTags") && v["originalGlobalTags"].is_object())job->originalGlobalTags=v["originalGlobalTags"].get<std::map<std::string,std::string>>();
                for (const auto &source : v.value("sources",Json::array())) { auto entry=assemblyFrom(source); if(source.contains("media"))entry.media=mediaFrom(source["media"]); job->sources.push_back(entry); }
                job->mux = v.value("mux", false);
                if (job->mux) {
                    job->muxInputs = v.value("muxInputs", std::vector<std::string>{});
                    job->muxChapters = str(v, "muxChapters");
                    auto options = v.value("muxOptions", Json::object());
                    job->muxOptions = {options.value("container", 0), options.value("video", 0), options.value("audio", 0), options.value("subtitles", 0)};
                }
                job->state = str(v, "state") == "Готово" ? "Готово" : "Ожидание повтора";
                job->percent = job->state == "Готово" ? 100 : 0;
                job->claimed = true;
                job->done = true;
                jobs.push_back(job);
                } catch (const std::exception &) {
                    // Restore subsequent valid entries, while retaining the
                    // original encrypted file for recovery of the bad one.
                    historyRecoveryRequired = true;
                }
            }
            // Older histories predate the assembly-input flag; recover it from the saved parent links.
            for (const auto &parent : jobs) if (parent->assembly)
                for (const auto &source : parent->sources) if (!source.dependency.empty())
                    for (const auto &child : jobs) if (child->id == source.dependency) {child->assemblyInput = true;if(source.codec>0)child->conversionSource=true;}
        }
    } catch (...) {
        historyRecoveryRequired = true;
    }
    if (!historyRecoveryRequired) {
        std::set<std::string> retainedAccess;
        for (const auto &job : jobs) if (!job->resumeAccessKey.empty()) retainedAccess.insert(job->resumeAccessKey);
        removeOrphanJobAccess(root, retainedAccess);
    }
    if (historyRecoveryRequired) {
        logs.push_back({"Историю очереди не удалось восстановить", true});
        // Surface partial recovery immediately, rather than hiding it in the
        // journal until the user discovers missing tasks.
        modalTitle = "Не удалось выполнить действие";
        modalBody = "Историю очереди не удалось восстановить";
        modalConfirm = nullptr;
    }
}
App::~App() {
    stopping = true;
    if(terminalCancel) *terminalCancel=true;
    *analysisCancel = true;
    connector.stop();
    {
        std::lock_guard l(jobsMutex);
        for (auto &j : jobs)
            j->cancel = true;
    }
    jobsCv.notify_all();
    probeCv.notify_all();
    readyCv.notify_all();
    for (auto &t : probeWorkers)
        if (t.joinable()) t.join();
    for (auto &t : downloadWorkers)
        if (t.joinable())
            t.join();
    for (auto &t : workers)
        if (t.thread.joinable())
            t.thread.join();
    try {
        saveHistory();
        saveSettings();
    } catch (...) {
    }
    connector.message = nullptr;
}
void App::saveSettings() {
    protectFile(dataRoot() / L"settings.dpapi", Json{{"destination", destination},
        {"interfaceLanguage", interfaceLanguage}, {"autoUpdate", autoUpdate}, {"light", light},
        {"reduced", reduced}, {"keepResults", keepResults},
        {"splashEnabled", splashEnabled}, {"splashSeconds", splashSeconds},
        {"parallel", parallel.load()}, {"fragments", fragments.load()}}.dump());
}
void App::setInterfaceLanguage(const std::string &language) {
    if ((language != "ru" && language != "en") || interfaceLanguage == language) return;
    interfaceLanguage = language;
    if (analysis.title.empty()) title = language == "ru" ? "Медиа" : "Media";
    extensionInstaller.setLanguage(language);
    refreshSessions();
    const auto revision = ++languageRevision;
    try { saveSettings(); } catch (...) { log("Could not save language settings"); }
    for (const auto &s : connector.list()) if (s && s->alive && s->languageSupported) {
        work([this, s, language, revision] { std::lock_guard lock(languageDispatchMutex); if (revision != languageRevision) return; try { s->send({{"type", "language"}, {"language", language}}); } catch (...) {} });
    }
}
void App::post(std::function<void()> f) {
    std::lock_guard l(eventsMutex);
    events.push_back(std::move(f));
}
bool App::drain() {
    std::deque<std::function<void()>> v;
    {
        std::lock_guard l(eventsMutex);
        v.swap(events);
    }
    bool changed = !v.empty();
    for (auto &f : v)
        f();
    if (!extensionInstaller.active() && (installerProfileChallenge || !installerProfilePing.empty() || installerConnectionPending)) {
        extensionInstaller.finishProfileVerification(wide(installerProfileUrl));
        installerProfileChallenge.reset();
        installerProfileSession.reset();
        installerProfilePing.clear();
        installerProfileUrl.clear();
        installerConnectionPending = false;
    } else if (extensionInstaller.active() && installerConnectionPending &&
               !extensionInstaller.profileVerificationFinishing()) {
        auto bound = installerProfileSession.lock();
        const bool connected = bound && bound->alive && bound->verified && bound->setupReady && bound->languageSupported;
        installerConnectionPending = false;
        installerProfileChallenge.reset();
        installerProfileSession.reset();
        installerProfilePing.clear();
        installerProfileUrl.clear();
        if (connected) {
            extensionInstaller.connectionEstablished();
            notify("Расширение установлено и подключено");
            requestedPage = 0;
        }
    }
    changed = updateAssemblies() || changed;
    for (auto it=workers.begin(); it!=workers.end();) {
        if (it->finished->load()) { if (it->thread.joinable()) it->thread.join(); it=workers.erase(it); }
        else ++it;
    }
    return changed;
}
void App::work(std::function<void()> f) {
    auto finished = std::make_shared<std::atomic_bool>(false);
    std::thread thread([this, f, finished] {
        try {
            f();
        } catch (const std::exception &e) {
            auto s = redact(e.what());
            post([this, s] { error(s); });
        } catch (...) {}
        *finished = true;
    });
    workers.push_back({std::move(thread), finished});
}
// Command-line tools may use the Windows OEM code page even when the shell
// requests UTF-8. Keep the journal UTF-8 without weakening path/URL validation.
static std::string logTextUtf8(const std::string &s) {
    if (s.empty()) return {};
    int length = int(std::min<size_t>(s.size(), size_t(INT_MAX)));
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), length, nullptr, 0)) return s;
    UINT codepage = GetOEMCP();
    int count = MultiByteToWideChar(codepage, 0, s.data(), length, nullptr, 0);
    if (count <= 0) return "[Не удалось прочитать вывод программы]";
    std::wstring decoded(size_t(count), L'\0');
    if (!MultiByteToWideChar(codepage, 0, s.data(), length, decoded.data(), count))
        return "[Не удалось прочитать вывод программы]";
    return utf8(decoded);
}
void App::log(const std::string &s, bool localizable) {
    auto clean = redact(logTextUtf8(s));
    post([this, clean, localizable] {
        SYSTEMTIME t;
        GetLocalTime(&t);
        char time[32];
        snprintf(time, sizeof(time), "%02d:%02d:%02d  ", t.wHour, t.wMinute, t.wSecond);
        logs.push_back({std::string(time) + clean, localizable});
        if (logs.size() > 4000)
            logs.erase(logs.begin(), logs.begin() + 500);
    });
}
void App::notify(const std::string &s) {
    toast = redact(s);
    toastSerial++;
}
void App::error(const std::string &s) {
    log(s);
    modalTitle = "Не удалось выполнить действие";
    modalBody = redact(s);
    modalConfirm = nullptr;
}
fs::path App::tool(const wchar_t *n) {
    auto p = dataRoot() / L"tools" / n;
    if (!componentFilePresent(p))
        throw std::runtime_error("Инструмент отсутствует в папке компонентов или является ссылкой: " + utf8(n));
    return p;
}
void App::start() {
    requestedPage = 1;
    connector.message = [this](std::shared_ptr<Session> s, const Json &j) {
        post([this, s, j] {
            auto type = str(j, "type");
            if (type == "connectorError") { error(str(j, "message")); return; }
            if (!s) return;
            if (type == "pong") {
                std::string actual; { std::lock_guard lock(s->mutex); actual = s->browserExecutable; }
                const bool profileMatches = !extensionInstaller.hasExplicitProfile() ||
                    (installerProfileSession.lock() == s &&
                     !installerProfilePing.empty() && str(j, "requestId") == installerProfilePing);
                bool matching = sameBrowserExecutable(actual, installerBrowserExecutable) && profileMatches;
                bool wasInstalling = extensionInstaller.active() && matching;
                bool profileProof = installerProbeSession.lock() == s;
                bool currentSetup = j.value("setupRevision", std::string()) == "2026-10-31-1";
                const auto granted = j.find("hasSiteAccess");
                if (granted != j.end() && granted->is_boolean()) {
                    if (granted->get<bool>()) sessionsWithSiteAccess.insert(s->instanceSerial);
                    else if (sessionsWithSiteAccess.erase(s->instanceSerial)) revokeSessionAccess(s);
                }
                s->setupReady = connectorSetupReady(currentSetup, j.value("hasSiteAccess", false));
                s->setupApiSupported = currentSetup && j.value("setupApi", false);
                if (extensionInstaller.active() && matching) {
                    extensionInstaller.setExtensionRunning(s->alive && s->verified);
                    extensionInstaller.setExtensionReloadRequired(!currentSetup || !s->languageSupported);
                }
                if (extensionInstaller.active() && s->verified && matching && s->setupReady && s->languageSupported) {
                    if (installerProfileSession.lock() == s && !installerProfileUrl.empty()) {
                        if (!installerProfileKey.first.empty() && !installerProfileKey.second.empty())
                            installerVerifiedProfiles[installerProfileKey] = s;
                        if (installerProfileChallenge && installerProfileChallenge->url == installerProfileUrl)
                            installerProfileChallenge->advance(GuideProbeProgress::proceeding);
                        extensionInstaller.finishProfileVerification(wide(installerProfileUrl));
                        installerConnectionPending = true;
                    } else {
                        extensionInstaller.connectionEstablished();
                        notify("Расширение установлено и подключено");
                        requestedPage = 0;
                    }
                }
                refreshSessions();
                if (s->verified && wasInstalling && !profileProof) {
                    auto it = std::find(sessions.begin(), sessions.end(), s);
                    if (it != sessions.end()) { sessionIndex = int(it - sessions.begin()); { std::lock_guard lock(s->mutex); if (!s->url.empty()) url = s->url; } refreshSessions(); }
                }
                return;
            }
            if (type == "setupChanged") {
                s->setupReady = false;
                refreshSessions();
                auto ping = installerProfileSession.lock() == s ? installerProfilePing : std::string();
                if (s->alive) work([s, ping] { try { s->verifyConnection(ping); } catch (...) {} });
                return;
            }
            if (type == "hello" || type == "tabSelected" || type == "tabClosed" || type == "disconnected") {
                if (type == "disconnected") sessionsWithSiteAccess.erase(s->instanceSerial);
                if (type == "hello" && s->languageSupported) {
                    const auto language = interfaceLanguage;
                    const auto revision = languageRevision.load();
                    work([this, s, language, revision] { std::lock_guard lock(languageDispatchMutex); if (revision != languageRevision) return; try { s->send({{"type", "language"}, {"language", language}}); } catch (...) {} });
                }
                refreshSessions();
                if (type == "disconnected" && extensionInstaller.active()) {
                    bool running = false;
                    for (auto &other : sessions) {
                        std::string actual; { std::lock_guard lock(other->mutex); actual = other->browserExecutable; }
                        if (other->alive && other->verified &&
                            (!extensionInstaller.hasExplicitProfile() || installerProfileSession.lock() == other) &&
                            sameBrowserExecutable(actual, installerBrowserExecutable)) { running = true; break; }
                    }
                    extensionInstaller.setExtensionRunning(running);
                }
                if (type == "tabSelected") {
                    const auto observedUrl = str(j, "url");
                    const bool initialProof = extensionInstaller.isVerificationUrl(observedUrl);
                    if (initialProof) {
                        if (installerProbeUrls.size() >= 16) installerProbeUrls.erase(installerProbeUrls.begin());
                        installerProbeUrls.insert(observedUrl);
                        installerProbeSession = s;
                    }
                    const bool profileProof = installerProbeUrls.contains(observedUrl);
                    const bool currentProfileProof = initialProof ||
                        (installerProfileChallenge && observedUrl == installerProfileChallenge->url);
                    if (extensionInstaller.active() && extensionInstaller.hasExplicitProfile() && currentProfileProof &&
                        !installerProfileSession.lock()) {
                        std::string actual; { std::lock_guard lock(s->mutex); actual = s->browserExecutable; }
                        if (s->alive && sameBrowserExecutable(actual, installerBrowserExecutable)) {
                            installerProfileSession = s;
                            installerProbeSession = s;
                            installerProfileUrl = observedUrl;
                            installerProfilePing = initialProof ? "profile-initial-" + observedUrl : installerProfileChallenge->pingToken;
                            if (installerProfileChallenge && installerProfileChallenge->url == observedUrl)
                                installerProfileChallenge->advance(GuideProbeProgress::matched);
                            extensionInstaller.finishProfileVerification(wide(observedUrl));
                            auto ping = installerProfilePing;
                            work([s, ping] { try { s->verifyConnection(ping); } catch (...) {} });
                        }
                    }
                    // This temporary page identifies a browser profile; it is
                    // not a user-selected source for downloads or scans.
                    if (profileProof) { refreshSessions(); return; }
                    if (installerProbeSession.lock() == s) installerProbeSession.reset();
                    auto it = std::find(sessions.begin(), sessions.end(), s);
                    if (it != sessions.end()) {
                        sessionIndex = int(it - sessions.begin());
                        // Session may already contain a newer queued browser
                        // event. Use this event's URL so a later probe cannot
                        // leak through an earlier ordinary-tab notification.
                        url = observedUrl;
                        refreshSessions();
                    }
                } else if (type == "tabClosed" && session() == s &&
                           installerProbeSession.lock() != s) {
                    url.clear();
                    connection = "Вкладка закрыта — выберите страницу в браузере";
                }
                return;
            }
            // Older signed extensions may send unsolicited snapshots; do not retain them.
            if (type == "scanCredentials") return;
            if (type == "credentialsChanged") return;
            if (session() != s)
                return;
            if (type == "scanState" || type == "resource") {
                std::lock_guard lock(s->mutex);
                bool legacyFirefox = s->browser.find("Firefox/") != std::string::npos && s->browser.find("Chrome/") == std::string::npos;
                if ((!j.contains("scanId") && !legacyFirefox) || (j.contains("scanId") && j.value("scanId", uint64_t(0)) != scanGeneration)) return;
                if (type == "resource" && int(number(j, "tabId", -1)) != s->tab) return;
            }
            if (type == "scanState") {
                scanning = j.value("scanning", false);
                const bool expectedStop = !scanning && j.value("scanId", uint64_t(0)) == silentScanStop && str(j, "message").empty();
                if (expectedStop) silentScanStop = 0;
                else notify(str(j, "message", scanning ? "Поиск включён. Воспроизведите медиа." : "Поиск остановлен"));
            } else if (type == "resource" && scanning && j.contains("item")) {
                const auto eventSource = s->source();
                if (j.contains("sourceId") && (!eventSource.extensionSource || !j["sourceId"].is_number_unsigned() || j["sourceId"].get<uint64_t>() != *eventSource.extensionSource)) return;
                if (eventSource.credentialRevision && (!j.contains("credentialRevision") ||
                    !j["credentialRevision"].is_number_unsigned() || j["credentialRevision"].get<uint64_t>() != *eventSource.credentialRevision)) return;
                auto &i = j["item"];
                auto u = str(i, "url");
                if (!httpUrl(u) || media.size() > 5000)
                    return;
                Media discovery;
                discovery.url = u;
                discovery.page = str(j, "pageUrl");
                discovery.size = number(i, "size");
                discovery.quality = str(i, "source");
                discovery.thumbnail = str(i, "thumbnail");
                discovery.title = str(i, "title");
                const auto capturedKind = str(i,"kind","Ресурс");
                const bool captureOnly = eventSource.valid() && !i.value("drm",false) &&
                    (capturedKind == "Изображения" || capturedKind == "Превью" || capturedKind == "Субтитры");
                auto existing = std::find_if(media.begin(), media.end(), [&](const auto &m) {
                    auto source = mediaSessions.find(m.key);
                    auto identity = mediaSources.find(m.key);
                    return source != mediaSessions.end() && source->second == s && identity != mediaSources.end() && identity->second == eventSource && sameDiscoveredResource(m, discovery);
                });
                if (existing != media.end()) {
                    mergeNetworkMetadata(*existing, discovery);
                    return;
                }
                Media m;
                m.id = "network:" + std::to_string(media.size());
                m.key = m.id + "|" + u;
                m.category = str(i,"kind","Ресурс");
                m.title = str(i, "title");
                if(m.title.empty()) {
                    auto path=u.substr(0,u.find('?'));auto slash=path.find_last_of('/');
                    m.title=slash!=path.npos&&slash+1<path.size()?path.substr(slash+1):m.category;
                }
                m.url = u;
                m.page = str(j, "pageUrl");
                { std::lock_guard lock(s->mutex); m.sourceTitle = s->title; }
                m.credentialUrl = u;
                m.size = number(i, "size");
                m.quality = str(i, "source");
                m.thumbnail=str(i,"thumbnail");
                if(m.category=="Изображения"||m.category=="Превью")m.thumbnail=u;
                m.data=i;
                m.protocol = m.category == "HLS" ? "m3u8" : m.category == "DASH" ? "dash" : "https";
                m.drm = i.value("drm", false);
                bool needsProbe = !m.drm && (m.category == "Видео" || m.category == "Аудио" || m.category == "HLS" || m.category == "DASH");
                if (needsProbe) { m.category = "Требуют проверки"; m.quality = "Уточняем параметры…"; }
                m.ext = "bin";
                std::string path = u.substr(0, u.find('?'));
                auto dot = path.find_last_of('.');
                if (dot != path.npos && path.size() - dot <= 8)
                    m.ext = path.substr(dot + 1);
                if (m.ext.empty() || m.ext.size() > 12 ||
                    !std::all_of(m.ext.begin(), m.ext.end(), [](unsigned char c) { return std::isalnum(c); }))
                    m.ext = "bin";
                media.push_back(m);
                mediaSessions[m.key] = s;
                mediaSources[m.key] = eventSource;
                if (needsProbe) enqueueProbe(m, s, scanGeneration, int(number(j, "tabId", -1)));
                else if (captureOnly) enqueueProbe(m, s, scanGeneration, eventSource.tab, true);
            } else if (type == "notice" || type == "error") {
                log(str(j, "message"), false);
                notify(str(j, "message"));
            }
        });
    };
    connector.start();
    for (int i = 0; i < 2; ++i) probeWorkers.emplace_back([this] { probeLoop(); });
    for (int i = 0; i < 4; i++)
        downloadWorkers.emplace_back([this] { downloadLoop(); });
    bool startupUpdate = autoUpdate;
    work([this, startupUpdate] {
        try {
            seedAndUpdate(startupUpdate);
        } catch (const std::exception &e) {
            log(e.what());
            post([this] { status = "Ошибка подготовки инструментов"; });
        }
        {
            std::lock_guard l(readyMutex);
            ready = true;
        }
        readyCv.notify_all();
    });
    detectVideoEncoders();
    work([this] {
        auto fingerprint = componentFingerprint(dataRoot() / L"tools");
        while (!stopping) {
            std::unique_lock lock(readyMutex);
            readyCv.wait_for(lock, std::chrono::seconds(1), [this] { return stopping.load(); });
            lock.unlock();
            if (stopping) break;
            auto current = componentFingerprint(dataRoot() / L"tools");
            if (current == fingerprint) continue;
            fingerprint = std::move(current);
            post([this] {
                if (!componentFilePresent(dataRoot() / L"tools" / L"yt-dlp.exe")) status = "yt-dlp отсутствует · установите в настройках";
                if (componentsBusy) { componentsDirty = true; return; }
                if (componentsScanned) refreshToolFiles();
            });
        }
    });
    work([this] {
        while (!stopping) {
            for (auto &s : connector.list()) {
                bool awaiting;
                { std::lock_guard lock(s->mutex); awaiting = !s->verificationToken.empty(); }
                auto sent = s->verificationSent.load();
                if (awaiting && sent && GetTickCount64() - sent < 5000) continue;
                if (awaiting && s->verified.exchange(false))
                    post([this] { refreshSessions(); });
                try { s->verifyConnection(); }
                catch (...) { post([this] { refreshSessions(); }); }
            }
            std::unique_lock lock(readyMutex);
            readyCv.wait_for(lock, std::chrono::seconds(1), [this] { return stopping.load(); });
        }
    });
    work([this] {
        while (!stopping) {
            try {
                auto found = discoverBrowsers();
                post([this, found = std::move(found)]() mutable {
                    std::string selectedExe, selectedProfile;
                    if (browserIndex >= 0 && browserIndex < int(browsers.size())) {
                        auto &selected = browsers[browserIndex]; selectedExe = selected.exe;
                        if (profileIndex >= 0 && profileIndex < int(selected.profiles.size())) selectedProfile = selected.profiles[profileIndex].path;
                    }
                    for (auto &old : browsers) {
                        auto it = std::find_if(found.begin(), found.end(), [&](const Browser &b) { return b.exe == old.exe; });
                        if (it == found.end() && customBrowserPaths.count(old.exe)) { found.push_back(old); continue; }
                        if (it != found.end()) for (auto &p : old.profiles)
                            if (p.status == "Подключение подтверждается расширением" && std::none_of(it->profiles.begin(), it->profiles.end(), [&](const Profile &v) { return v.path == p.path; })) it->profiles.push_back(p);
                    }
                    browsers = std::move(found); browserIndex = profileIndex = 0;
                    for (size_t i = 0; i < browsers.size(); ++i) if (browsers[i].exe == selectedExe) {
                        browserIndex = int(i);
                        for (size_t j = 0; j < browsers[i].profiles.size(); ++j) if (browsers[i].profiles[j].path == selectedProfile) { profileIndex = int(j); break; }
                        break;
                    }
                });
            } catch (...) { /* Retry on the next refresh. */ }
            std::unique_lock lock(readyMutex);
            readyCv.wait_for(lock, std::chrono::seconds(3), [this] { return stopping.load(); });
        }
    });
}
void App::seedAndUpdate(bool update) {
    std::unique_lock lock(toolsMutex);
    auto root = dataRoot() / L"tools";
    if (!componentFilePresent(root / L"yt-dlp.exe")) {
        post([this] { status = "yt-dlp отсутствует · установите в настройках"; });
        return;
    }
    if (update) {
        post([this] { status = "Проверка официального выпуска yt-dlp…"; });
        try {
            auto j = Json::parse(
                httpGet("https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest", 4 * 1024 * 1024));
            std::string tag = str(j, "tag_name"), downloadUrl, checksums;
            for (auto &a : j.at("assets")) {
                if (str(a, "name") == "yt-dlp.exe")
                    downloadUrl = str(a, "browser_download_url");
                if (str(a, "name") == "SHA2-256SUMS")
                    checksums = str(a, "browser_download_url");
            }
            if (downloadUrl.rfind("https://github.com/yt-dlp/yt-dlp/releases/", 0) != 0 ||
                checksums.rfind("https://github.com/yt-dlp/yt-dlp/releases/", 0) != 0)
                throw std::runtime_error("Неверный источник обновления");
            const auto installed = componentInstalledVersion(root / L"yt-dlp.exe", stopping);
            if (installed.empty() || componentVersion(installed).empty() || componentVersion(tag).empty()) {
                log("Не удалось сравнить версии yt-dlp; автоматическое обновление пропущено");
                post([this] { status = "yt-dlp · локальная версия готова"; });
                return;
            }
            if (componentNewer(tag, installed)) {
                auto sums = httpGet(checksums, 1024 * 1024);
                std::istringstream in(sums);
                std::string hash, name, expected;
                while (in >> hash >> name)
                    if (name == "yt-dlp.exe" || name == "*yt-dlp.exe")
                        expected = hash;
                if (expected.size() != 64)
                    throw std::runtime_error("Нет SHA-256 yt-dlp.exe");
                auto temp = root / L"yt-dlp.new.exe";
                httpDownload(downloadUrl, temp, stopping, [](double) {});
                if (sha256(temp) != expected) {
                    fs::remove(temp);
                    throw std::runtime_error("SHA-256 обновления не совпал");
                }
                std::atomic_bool cancel{false};
                auto test = runProcess(temp, {L"--version"}, nullptr, cancel, true);
                if (test.code != 0 || componentVersion(test.output) != componentVersion(tag))
                    throw std::runtime_error("Новая версия yt-dlp не запускается");
                auto current = root / L"yt-dlp.exe", backup = root / L"yt-dlp.previous.exe";
                if (fs::exists(current))
                    fs::copy_file(current, backup, fs::copy_options::overwrite_existing);
                if (!MoveFileExW(temp.c_str(), current.c_str(),
                                 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                    throw std::runtime_error("Не удалось заменить yt-dlp");
                writeFile(root / L"version.txt", tag);
                log("Обновлён yt-dlp " + tag);
            }
            post([this, installed, tag] { status = "yt-dlp " + (componentNewer(tag, installed) ? tag : installed) + " · движок готов"; });
        } catch (const std::exception &e) {
            if (!fs::exists(root / L"yt-dlp.exe"))
                throw;
            log(std::string("Обновление недоступно. Используется локальная версия: ") + e.what());
            post([this] { status = "yt-dlp · локальная версия готова"; });
        }
    } else
        post([this] { status = "yt-dlp · движок готов"; });
}
void App::scanToolComponents(bool install) {
    if (componentsBusy) return;
    componentsBusy = true;
    ++componentsRevision;
    componentsActivity = install ? "Подготовка загрузки…" : "Проверяем версии…";
    componentProgressVisible = false;
    auto pending = components;
    log(install ? "Установка компонентов запущена" : "Проверка компонентов запущена");
    work([this, install, pending] {
        std::string problem;
        try {
            if (install) {
                std::unique_lock lock(toolsMutex);
                auto current = refreshInstalledComponents(dataRoot() / L"tools", pending, stopping);
                ULONGLONG lastProgressAt = 0;
                std::string lastProgressName;
                auto lastPhase = ComponentPhase::Downloading;
                installComponents(dataRoot() / L"tools", current, stopping,
                    [this](const std::string &message) { log(message); post([this, message] { componentsActivity = message; }); },
                    [this, &lastProgressAt, &lastProgressName, &lastPhase](const ComponentProgress &p) {
                        const auto now = GetTickCount64();
                        const bool transition = p.name != lastProgressName || p.phase != lastPhase;
                        const bool complete = p.total && p.received >= p.total;
                        if (!transition && !complete && now - lastProgressAt < 100) return;
                        lastProgressAt = now; lastProgressName = p.name; lastPhase = p.phase;
                        post([this, p] {
                            componentProgressVisible = true;
                            componentProgress = p;
                            const auto name = p.name == "FFmpeg" ? "FFmpeg и FFprobe" : p.name;
                            switch (p.phase) {
                            case ComponentPhase::Downloading: componentsActivity = "Загрузка: " + name; break;
                            case ComponentPhase::Verifying: componentsActivity = "Проверка SHA-256: " + name; break;
                            case ComponentPhase::Extracting: componentsActivity = "Распаковка: " + name; break;
                            case ComponentPhase::Installing: componentsActivity = "Установка: " + name; break;
                            }
                        });
                    });
            }
        } catch (const std::exception &e) { problem = e.what(); }
        if (install) post([this] { componentProgressVisible = false; componentsActivity = "Проверяем установленные версии…"; });
        std::vector<Component> result;
        try { std::shared_lock lock(toolsMutex); result = scanComponents(dataRoot() / L"tools", stopping, [this](const std::string &line) { log(line); }); }
        catch (const std::exception &e) { if (problem.empty()) problem = e.what(); }
        post([this, result = std::move(result), problem, install] {
            componentsBusy = false;
            componentProgressVisible = false;
            if (!result.empty()) { components = result; componentsScanned = true; }
            if (install) detectVideoEncoders();
            if (componentsDirty) { componentsDirty = false; if (componentsScanned) refreshToolFiles(); }
            if (!problem.empty()) { log("Компоненты: " + problem); error("Не удалось установить все выбранные компоненты: " + problem); }
            else { log(install ? "Установка и проверка компонентов завершены" : "Проверка компонентов завершена"); notify(install ? "Установка завершена. Версии компонентов проверены." : "Проверка компонентов завершена"); }
        });
    });
}
void App::refreshToolFiles() {
    detectVideoEncoders();
    const auto revision = ++componentsRevision;
    const auto snapshot = components;
    work([this, revision, snapshot] {
        std::vector<Component> refreshed;
        { std::shared_lock lock(toolsMutex); refreshed = refreshInstalledComponents(dataRoot() / L"tools", snapshot, stopping); }
        post([this, revision, refreshed = std::move(refreshed)]() mutable {
            if (revision != componentsRevision || componentsBusy) return;
            components = std::move(refreshed);
            if (!components.empty() && components[0].installed.empty()) status = "yt-dlp отсутствует · установите в настройках";
            else if (status == "yt-dlp отсутствует · установите в настройках") status = "yt-dlp · движок готов";
        });
    });
}
void App::refreshSessions() {
    auto previous = session();
    sessions = connector.list();
    auto it = std::find(sessions.begin(), sessions.end(), previous);
    if (previous && it == sessions.end()) { sessions.push_back(previous); it = sessions.end() - 1; }
    sessionIndex = it != sessions.end() ? int(it - sessions.begin()) : sessions.empty() ? -1 : 0;
    auto s = session();
    if (s) {
        std::lock_guard l(s->mutex);
        std::string browser = s->browser.find("YaBrowser/") != std::string::npos ? (interfaceLanguage == "ru" ? "Яндекс Браузер" : "Yandex Browser") : s->browser.find("Chrome/") != std::string::npos ? "Chrome" : (interfaceLanguage == "ru" ? "Браузер" : "Browser");
        const bool probe = installerProbeSession.lock() == s || installerProbeUrls.contains(s->url);
        connection = !s->alive ? "Расширение отключено · " + browser : !s->verified ? "Нет подтверждения связи · " + browser : !s->setupReady ? "Требуется разрешение · " + browser : "Расширение подключено · " + browser + (s->title.empty() || probe ? "" : " · " + s->title);
    } else
        connection = "Браузер не подключён";
}
std::shared_ptr<Session> App::session() {
    return sessionIndex >= 0 && sessionIndex < int(sessions.size()) ? sessions[sessionIndex] : nullptr;
}
std::pair<std::string, Json> App::credentials(std::shared_ptr<Session> s, const std::string &u,
                                              std::atomic_bool &cancel, const BrowserSource *source, Session::CredentialSnapshot *pinned) {
    if (s) {
        try {
            auto j = source ? s->credentials(u, *source, cancel, pinned) : s->credentials(u, cancel);
            return {cookies(j.value("cookies", Json::array())), j.value("headers", Json::object())};
        } catch (const BrowserSourceChanged &) {
            throw;
        } catch (const BrowserCredentialsChanged &) {
            throw;
        } catch (...) {
            if (cancel) throw;
            // A rejected or unrepresentable browser cookie snapshot must not
            // make the caller silently retry the request without credentials.
            throw BrowserSourceChanged();
        }
    }
    auto p = dataRoot() / L"cookies.dpapi";
    return {fs::exists(p) ? validateCookies(unprotectFile(p)) : "", Json::object()};
}
void App::bindJobSource(Job &job, const Media &item) {
    if (item.category == "Данные") return;
    auto it = mediaSessions.find(item.key);
    if (it == mediaSessions.end()) {
        if (item.id.rfind("network:", 0) == 0) throw BrowserSourceChanged();
        return; // Explicitly analysed without a browser session.
    }
    job.session = it->second;
    job.browserSourceRequired = bool(job.session);
    if (job.session) {
        auto source = mediaSources.find(item.key);
        if (source == mediaSources.end() || !source->second.valid()) throw BrowserSourceChanged();
        job.browserSource = source->second;
        const auto current = job.session->source();
        const bool canQuery = job.session->sourceMatches(job.browserSource) &&
            current.credentialRevision == job.browserSource.credentialRevision;
        job.resumeAccessKey = newJobAccessKey();
        auto captured = capturedDownloadAccess(job);
        if (!captured && isExtractedThumbnail(item))
            captured = JobDownloadAccess{{"", Json::object()}, std::nullopt};
        const bool checkpointed = captured && saveDownloadAccess(job, *captured);
        if (!canQuery && !checkpointed) {
            if (current == job.browserSource && current.credentialRevision != job.browserSource.credentialRevision)
                throw BrowserCredentialsChanged();
            throw BrowserSourceChanged();
        }
        job.preparingAccess = canQuery && !checkpointed;
    }
}

static bool subtitleNeedsPageAccess(const Job &job) {
    return (job.item.category == "Субтитры" || job.item.category == "Автосубтитры") && httpUrl(job.page) &&
        !job.item.language.empty() && (job.item.id.starts_with("subtitles:") || job.item.id.starts_with("automatic_captions:"));
}

static Json downloadAccessScope(const Job &job) {
    return Json::array({job.id, job.item.id, job.page, jobCredentialTarget(job), job.item.url, job.pair});
}

bool App::saveDownloadAccess(Job &job, const JobDownloadAccess &access) {
    if (job.resumeAccessKey.empty()) return false;
    std::lock_guard lock(job.resumeAccessMutex);
    if (job.resumeAccessRemoved) return false;
    if (access.target.second.contains("Authorization") || (access.page && access.page->second.contains("Authorization"))) {
        eraseJobAccess(dataRoot(), job.resumeAccessKey);
        return false;
    }
    saveJobAccess(dataRoot(), job.resumeAccessKey, downloadAccessScope(job), access);
    return true;
}

void App::discardDownloadAccess(Job &job, bool removed) {
    if (job.resumeAccessKey.empty()) return;
    std::lock_guard lock(job.resumeAccessMutex);
    job.resumeAccessRemoved = job.resumeAccessRemoved || removed;
    eraseJobAccess(dataRoot(), job.resumeAccessKey);
}

void App::revokeSessionAccess(const std::shared_ptr<Session> &session) {
    siteAccessWithdrawnAt[session->instanceSerial] = GetTickCount64();
    for (const auto &[key, owner] : mediaSessions) if (owner == session) mediaSources.erase(key);
    std::erase_if(mediaCredentialSnapshots, [&](const auto &entry) {
        const auto &snapshot = entry.second;
        if (snapshot.owner != session->instanceSerial) return false;
        mediaCredentialBytes -= snapshot.protectedData->size() + std::get<3>(entry.first).size() + snapshot.source.page.size();
        return true;
    });
    std::lock_guard lock(jobsMutex);
    for (const auto &job : jobs) {
        if (job->session != session) continue;
        try { discardDownloadAccess(*job, true); }
        catch (...) { log("Не удалось удалить отозванный доступ загрузки."); }
    }
}

JobDownloadAccess App::getDownloadAccess(Job &job, bool prepareOnly) {
    if (job.cancel || stopping) throw std::runtime_error("Отменено");
    const auto target = jobCredentialTarget(job);
    const bool needsPage = subtitleNeedsPageAccess(job);
    if (!job.browserSourceRequired) {
        JobDownloadAccess access{{"", Json::object()}, std::nullopt};
        if (isExtractedThumbnail(job.item)) return access;
        try { access.target = credentials(nullptr, needsPage ? job.page : target, job.cancel); }
        catch (...) { if (job.cancel || stopping) throw; log("Не удалось прочитать импортированные cookies. Пробуем без них."); }
        if (needsPage) {
            access.page = std::move(access.target);
            access.target = subtitleDownloadAccess(job.page, target, *access.page);
        }
        return access;
    }
    {
        std::lock_guard lock(job.resumeAccessMutex);
        if (job.resumeAccessRemoved) throw BrowserCredentialsChanged();
        if (!prepareOnly) {
            auto saved = readJobAccess(dataRoot(), job.resumeAccessKey, downloadAccessScope(job));
            if (saved && (!needsPage || saved->page)) return std::move(*saved);
        }
    }
    if (job.session && job.session->sourceMatches(job.browserSource)) {
        if (job.session->source().credentialRevision != job.browserSource.credentialRevision)
            throw BrowserCredentialsChanged();
        try {
            JobDownloadAccess access{{"", Json::object()}, std::nullopt};
            if (needsPage) {
                access.page = credentials(job.session, job.page, job.cancel, &job.browserSource);
                access.target = subtitleDownloadAccess(job.page, target, *access.page);
            } else {
                access.target = credentials(job.session, target, job.cancel, &job.browserSource);
            }
            if (job.cancel || stopping) throw std::runtime_error("Отменено");
            const auto current = job.session->source();
            if (current != job.browserSource) throw BrowserSourceChanged();
            if (current.credentialRevision != job.browserSource.credentialRevision) throw BrowserCredentialsChanged();
            try { saveDownloadAccess(job, access); }
            catch (...) { log("Не удалось сохранить защищённый доступ для повтора загрузки."); }
            return access;
        } catch (const BrowserCredentialsChanged &) {
            discardDownloadAccess(job);
            throw;
        } catch (const BrowserSourceChanged &) {
            if (job.session->sourceMatches(job.browserSource)) discardDownloadAccess(job);
            throw;
        }
    }
    throw BrowserSourceChanged();
}

void App::stageDownloadAccess(const std::vector<std::shared_ptr<Job>> &pending) {
    const auto clear = [this, &pending] {
        for (const auto &job : pending) job->preparingAccess = false;
        jobsCv.notify_all();
    };
    try {
        saveHistory();
        if (std::none_of(pending.begin(), pending.end(), [](const auto &job) { return job->preparingAccess.load(); })) return;
        work([this, pending] {
            for (const auto &job : pending) if (job->preparingAccess) {
                if (!job->cancel && !stopping) {
                    try { (void)getDownloadAccess(*job, true); } catch (...) {}
                }
                job->preparingAccess = false;
                jobsCv.notify_all();
            }
        });
    } catch (...) { clear(); throw; }
}

// Temporary plaintext credentials are needed by external tools, but must not
// appear on their command lines or inherit a permissive directory ACL.
static void writePrivateCredentialFile(const fs::path &path, const std::string &contents) {
    HANDLE rawToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &rawToken)) throw std::runtime_error("Не удалось защитить временный файл cookies");
    Handle token(rawToken);
    DWORD needed = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &needed);
    std::vector<BYTE> info(needed);
    if (!needed || !GetTokenInformation(token, TokenUser, info.data(), needed, &needed)) throw std::runtime_error("Не удалось определить владельца cookies");
    PWSTR sid = nullptr;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(info.data())->User.Sid, &sid)) throw std::runtime_error("Не удалось определить владельца cookies");
    std::wstring sddl = L"D:P(A;;FA;;;" + std::wstring(sid) + L")(A;;FA;;;SY)";
    LocalFree(sid);
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr)) throw std::runtime_error("Не удалось защитить временный файл cookies");
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), descriptor, FALSE};
    fs::create_directories(path.parent_path());
    Handle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, &attributes, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr));
    LocalFree(descriptor);
    if (file.h == INVALID_HANDLE_VALUE) throw std::runtime_error("Не удалось создать защищённый файл cookies");
    size_t offset = 0;
    while (offset < contents.size()) {
        DWORD written = 0;
        const auto chunk = DWORD(std::min<size_t>(contents.size() - offset, 1024 * 1024));
        if (!WriteFile(file, contents.data() + offset, chunk, &written, nullptr) || !written) {
            CloseHandle(file.release());
            std::error_code ec; fs::remove(path, ec);
            throw std::runtime_error("Не удалось записать защищённый файл cookies");
        }
        offset += written;
    }
}
struct TempCookie {
    fs::path path;
    explicit TempCookie(const std::string &s) {
        if (!s.empty()) {
            static std::atomic_uint64_t id{0};
            path = dataRoot() / L"temp" /
                (L"cookies-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++id) + L".txt");
            writePrivateCredentialFile(path, s);
        }
    }
    ~TempCookie() { if (!path.empty()) { std::error_code ec; fs::remove(path, ec); } }
};
static constexpr size_t MediaAccessLimit = 128 * 1024;
static constexpr size_t MediaAccessBudget = 32 * 1024 * 1024;
static Session::CredentialSnapshot freezeMediaAccess(const std::shared_ptr<Session> &session,
                                                    Session::CredentialSnapshot snapshot) {
    if (!session || !snapshot.protectedData) return {};
    std::lock_guard lock(session->mutex);
    if (snapshot.protectedData->empty() || snapshot.protectedData->size() > MediaAccessLimit) return {};
    snapshot.protectedData = std::make_shared<std::string>(*snapshot.protectedData);
    return snapshot;
}
void App::rememberMediaAccess(const std::shared_ptr<Session> &owner, const std::string &target,
                             Session::CredentialSnapshot snapshot) {
    if (!owner || !snapshot.protectedData || snapshot.owner != owner->instanceSerial || !snapshot.source.valid())
        return;
    if (const auto withdrawal = siteAccessWithdrawnAt.find(snapshot.owner);
        withdrawal != siteAccessWithdrawnAt.end() && snapshot.captured <= withdrawal->second) return;
    MediaCredentialKey key{snapshot.owner, snapshot.source.generation, snapshot.source.credentialRevision, target};
    const auto existing = mediaCredentialSnapshots.find(key);
    const auto recordBytes = snapshot.protectedData->size() + target.size() + snapshot.source.page.size();
    size_t freed = 0;
    if (existing != mediaCredentialSnapshots.end()) {
        if (existing->second.captured > snapshot.captured) return;
        freed = existing->second.protectedData->size() + std::get<3>(existing->first).size() + existing->second.source.page.size();
    } else if (mediaCredentialSnapshots.size() >= 5000) return;
    if (recordBytes > MediaAccessBudget - (mediaCredentialBytes - freed)) return;
    mediaCredentialSnapshots.insert_or_assign(std::move(key), std::move(snapshot));
    mediaCredentialBytes = mediaCredentialBytes - freed + recordBytes;
}
std::optional<JobDownloadAccess> App::capturedDownloadAccess(const Job &job) {
    if (!job.session || !job.browserSource.valid()) return {};
    const auto read = [&](const std::string &target) -> std::optional<DownloadAccess> {
        const auto found = mediaCredentialSnapshots.find({job.session->instanceSerial, job.browserSource.generation, job.browserSource.credentialRevision, target});
        if (found == mediaCredentialSnapshots.end()) return {};
        const auto &snapshot = found->second;
        if (snapshot.source != job.browserSource || snapshot.source.credentialRevision != job.browserSource.credentialRevision ||
            !snapshot.protectedData || snapshot.protectedData->size() > MediaAccessLimit) return {};
        const auto &blob = *snapshot.protectedData;
        DATA_BLOB input{DWORD(blob.size()), reinterpret_cast<BYTE *>(const_cast<char *>(blob.data()))}, output{};
        if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) return {};
        struct ClearOutput {
            DATA_BLOB &blob;
            ~ClearOutput() { SecureZeroMemory(blob.pbData, blob.cbData); LocalFree(blob.pbData); }
        } clear{output};
        try {
            const auto parsed = Json::parse(output.pbData, output.pbData + output.cbData);
            if (str(parsed,"url") != target || number(parsed,"tabId",-1) != snapshot.source.tab ||
                str(parsed,"storeId") != snapshot.source.store ||
                parsed.at("sourceId") != Json(*snapshot.source.extensionSource) ||
                (snapshot.source.credentialRevision && parsed.at("credentialRevision") != Json(*snapshot.source.credentialRevision))) return {};
            DownloadAccess access{cookies(parsed.at("cookies")), parsed.at("headers")};
            checkDownloadAccess(access);
            return access;
        } catch (...) { return {}; }
    };
    const auto targetUrl = jobCredentialTarget(job);
    auto target = read(targetUrl);
    if (subtitleNeedsPageAccess(job)) {
        auto page = read(job.page);
        if (!page) return {};
        if (!target) target = subtitleDownloadAccess(job.page, targetUrl, *page);
        return JobDownloadAccess{std::move(*target), std::move(page)};
    }
    if (!target) return {};
    return JobDownloadAccess{std::move(*target), std::nullopt};
}
void App::enqueueProbe(const Media &item, const std::shared_ptr<Session> &source, uint64_t generation, int tab, bool captureOnly) {
    const auto kind = str(item.data, "kind");
    auto identity = mediaSources.find(item.key);
    ProbeTask task{item, source, identity == mediaSources.end() ? BrowserSource{} : identity->second, probeCancel, generation, tab,
        captureOnly ? 0 : kind == "HLS" || kind == "DASH" ? 3 : kind == "Видео" ? 2 : 1, captureOnly};
    {
        std::lock_guard lock(probeMutex);
        probes.erase(std::remove_if(probes.begin(), probes.end(), [](const ProbeTask &p) { return p.cancelled->load(); }), probes.end());
        if (probes.size() >= 256) {
            auto it = std::find_if(media.begin(), media.end(), [&](const Media &m) { return m.key == item.key; });
            if (captureOnly) return;
            if (probes.back().captureOnly) probes.pop_back();
            else {
                if (it != media.end()) it->quality = "Проверка отложена";
                return;
            }
        }
        auto position = std::find_if(probes.begin(), probes.end(), [&](const ProbeTask &p) { return p.priority < task.priority; });
        probes.insert(position, std::move(task));
    }
    probeCv.notify_one();
}
void App::probeLoop() {
    while (!stopping) {
        ProbeTask task;
        {
            std::unique_lock lock(probeMutex);
            probeCv.wait(lock, [&] { return stopping || !probes.empty(); });
            if (stopping) return;
            task = std::move(probes.front()); probes.pop_front();
        }
        if (*task.cancelled) continue;
        if (!task.captureOnly) {
            std::unique_lock lock(readyMutex);
            readyCv.wait(lock, [&] { return ready || stopping || *task.cancelled; });
        }
        if (stopping || *task.cancelled) continue;
        std::atomic_bool cancel{false}, finished{false};
        std::thread watchdog([&] {
            for (int i = 0; i < 120 && !finished && !stopping && !*task.cancelled; ++i) Sleep(100);
            if (!finished) cancel = true;
        });
        Json result;
        try {
            std::pair<std::string, Json> cred;
            const auto target = task.item.format ? task.item.page : task.item.url;
            Session::CredentialSnapshot snapshot;
            try { cred = credentials(task.session, target, cancel, task.session ? &task.source : nullptr, &snapshot); }
            catch (const BrowserSourceChanged &) { throw; }
            catch (const BrowserCredentialsChanged &) { throw; }
            catch (...) {
                if (cancel || stopping || *task.cancelled) throw;
                std::lock_guard guard(task.session->mutex);
                if (task.session->tab != task.tab || task.session->url != task.item.page) throw;
                // A public resource may still be probed without optional site permissions.
                cred = {"", Json::object()};
            }
            if (auto frozen = freezeMediaAccess(task.session, std::move(snapshot)); frozen.protectedData) {
                post([this, owner = task.session, key = task.item.key, target, frozen = std::move(frozen)]() mutable {
                    const auto linked = mediaSources.find(key);
                    const auto ownerEntry = mediaSessions.find(key);
                    if (linked != mediaSources.end() && ownerEntry != mediaSessions.end() && ownerEntry->second == owner &&
                        linked->second == frozen.source && linked->second.credentialRevision == frozen.source.credentialRevision)
                        rememberMediaAccess(owner, target, std::move(frozen));
                });
            }
            if (!task.captureOnly) {
                std::shared_lock lock(toolsMutex);
                if (cred.second.contains("Authorization")) throw std::runtime_error("Authorization requires a scoped request");
                std::vector<std::wstring> args{L"-v", L"error", L"-rw_timeout", L"6000000", L"-protocol_whitelist", L"http,https,tcp,tls,crypto", L"-analyzeduration", L"2000000", L"-probesize", L"2000000"};
                if (auto value = str(cred.second,"User-Agent"); !value.empty() && value.find_first_of("\r\n") == value.npos)
                    args.insert(args.end(), {L"-user_agent", wide(value)});
                args.insert(args.end(), {L"-show_entries", L"stream=codec_name,codec_type,width,height:stream_disposition=attached_pic:format=duration,format_name", L"-of", L"json", wide(task.item.url)});
                auto process = runProcess(tool(L"ffprobe.exe"), args, nullptr, cancel, true);
                if (!process.code && !cancel) result = Json::parse(process.output, nullptr, false);
                else if (!cancel && !stopping && !*task.cancelled) log("Не удалось уточнить параметры: " + redact(process.error.substr(0, 500)));
            }
        } catch (const std::exception &e) { if (!task.captureOnly && !cancel && !stopping && !*task.cancelled) log("Не удалось уточнить параметры: " + redact(e.what())); }
        finished = true; watchdog.join();
        if (task.captureOnly || stopping || *task.cancelled) continue;
        post([this, task = std::move(task), result = std::move(result)]() mutable {
            if (*task.cancelled || task.generation != scanGeneration || session() != task.session) return;
            const auto current = task.session->source();
            if (current != task.source || current.credentialRevision != task.source.credentialRevision) return;
            auto it = std::find_if(media.begin(), media.end(), [&](const Media &m) { return m.key == task.item.key && m.url == task.item.url; });
            if (it == media.end()) return;
            if (!applyMediaProbe(*it, result)) { it->category = "Требуют проверки"; it->quality = "Не удалось уточнить параметры"; }
            else {
                auto key = previewCacheKey(*it);
                auto old = previews.find(key);
                // An earlier request may have started while the item was still unclassified.
                // Drop that entry even if its worker has not returned yet: it cannot produce
                // a video frame using the old classification, and owns its Preview separately.
                if (old != previews.end() && old->second->image.rgba.empty()) previews.erase(old);
            }
        });
    }
}
static std::vector<std::wstring> headersArgs(const Json &h) {
    std::vector<std::wstring> args;
    for (auto &[k, v] : h.items()) {
        if (k == "Authorization")
            throw std::runtime_error("Этот поток требует Authorization: глобальный заголовок yt-dlp не "
                                     "применяется к сторонним CDN");
        if (k != "User-Agent" && k != "Referer")
            continue;
        auto s = v.is_string() ? v.get<std::string>() : "";
        if (s.find_first_of("\r\n") != s.npos)
            continue;
        args.push_back(k == "User-Agent" ? L"--user-agent" : L"--referer");
        args.push_back(wide(s));
    }
    return args;
}
void App::resetCredentialSnapshots() {
    std::set<std::shared_ptr<Session>> sources(sessions.begin(), sessions.end());
    for (const auto &[key, source] : mediaSessions) if(source) sources.insert(source);
    if(analysisSession) sources.insert(analysisSession);
    {
        std::lock_guard lock(jobsMutex);
        for (const auto &job : jobs) if(job->session) sources.insert(job->session);
    }
    for (const auto &source : sources) {
        std::lock_guard lock(source->mutex);
        source->credentialSnapshots.clear();
        ++source->credentialEpoch;
        source->credentialScanId=0;
    }
}
void App::prepareAnalysisAccess(const std::shared_ptr<Session> &session, const std::string &url, std::atomic_bool &cancel,
                                BrowserSource &source, Session::CredentialSnapshot &snapshot,
                                const std::function<void(const DownloadAccess &)> &prepare) {
    if (!session) { prepare(credentials(nullptr, url, cancel)); return; }
    const auto original = source;
    const auto currentSource = [&] {
        auto current = session->source();
        if (!session->alive || !session->scopedCredentialsSupported || !current.valid() || current != original)
            throw BrowserSourceChanged();
        return current;
    };
    constexpr int attempts = 3;
    for (int attempt = 0; attempt < attempts; ++attempt) {
        if (stopping || cancel) throw std::runtime_error("Анализ отменён");
        auto current = currentSource();
        Session::CredentialSnapshot fresh;
        try {
            auto access = credentials(session, url, cancel, &current, &fresh);
            if (currentSource().credentialRevision != current.credentialRevision)
                throw BrowserCredentialsRefreshPending();
            prepare(access);
            if (stopping || cancel) throw std::runtime_error("Анализ отменён");
            if (currentSource().credentialRevision != current.credentialRevision)
                throw BrowserCredentialsRefreshPending();
            source = std::move(current);
            snapshot = std::move(fresh);
            return;
        } catch (const BrowserCredentialsRefreshPending &) {
            if (attempt + 1 == attempts) throw;
            for (int waited = 0; waited < (attempt + 1) * 40 && !stopping && !cancel; waited += 5) Sleep(5);
        }
    }
    throw BrowserCredentialsRefreshPending();
}

void App::analyze(const std::string &u, bool deep) {
    if (!httpUrl(u)) {
        error("Введите HTTP/HTTPS-ссылку");
        return;
    }
    scanning = false;
    const auto stopId = nextScanGeneration();
    silentScanStop = stopId;
    if (auto previous = session()) work([previous, stopId] { try { previous->send({{"type", "scanStop"}, {"scanId", stopId}}); } catch (...) {} });
    *analysisCancel = true;
    auto cancel = analysisCancel = std::make_shared<std::atomic_bool>(false);
    auto generation = ++analysisGeneration;
    // Session snapshots stay bound to the current source generation. Clearing
    // them here would discard the authentication baseline of queued jobs.
    if (!keepResults) { media.clear(); mediaSessions.clear(); mediaSources.clear(); clearMediaAccess(); previews.clear(); ++previewGeneration; analysis = {}; selectedMedia.clear(); }
    busy = true;
    auto s = manualAnalysis ? nullptr : session();
    auto source = s ? s->source() : BrowserSource{};
    work([this, u, s, source = std::move(source), cancel, generation, deep]() mutable {
        try {
            {
                std::unique_lock l(readyMutex);
                readyCv.wait(l, [this] { return ready || stopping; });
            }
            if (stopping || *cancel)
                return;
            std::shared_lock tools(toolsMutex);
            Session::CredentialSnapshot snapshot;
            std::unique_ptr<TempCookie> file;
            std::vector<std::wstring> args;
            prepareAnalysisAccess(s, u, *cancel, source, snapshot, [&](const DownloadAccess &access) {
                file = std::make_unique<TempCookie>(access.first);
                args = headersArgs(access.second);
                args.insert(args.end(),
                            {L"--ignore-config", L"--encoding", L"utf-8", L"--no-playlist",
                             L"--quiet", L"--dump-single-json", L"--skip-download", L"--allow-unplayable-formats",
                             L"--js-runtimes", L"deno:" + (dataRoot() / L"tools" / L"deno.exe").wstring()});
                if (!file->path.empty()) args.insert(args.end(), {L"--cookies", file->path.wstring()});
                args.insert(args.end(), {L"--", wide(u)});
            });
            auto frozen = freezeMediaAccess(s, std::move(snapshot));
            const auto analyzedAt = frozen.protectedData ? frozen.captured : GetTickCount64();
            if (s) {
                const auto current = s->source();
                if (current != source || !s->alive) throw BrowserSourceChanged();
                if (current.credentialRevision != source.credentialRevision) throw BrowserCredentialsRefreshPending();
            }
            log("yt-dlp --dump-single-json --skip-download " + redact(u));
            auto result = runProcess(tool(L"yt-dlp.exe"), args, nullptr, *cancel, true);
            if (*cancel)
                throw std::runtime_error("Анализ отменён");
            if (result.code) {
                auto details = result.error.empty() ? result.output : result.error;
                if (s && details.find("Unsupported URL") != std::string::npos)
                    throw std::runtime_error("yt-dlp не поддерживает эту страницу. Откройте «Подключение», "
                        "нажмите «Обновить вкладку и искать» и запустите видео или аудио в выбранной вкладке. "
                        "Найденные прямые ссылки появятся в «Обзоре медиа». Поиск расширением работает отдельно от анализа yt-dlp.");
                if (details.find("Requested format is not available") != std::string::npos) {
                    std::string advice = "yt-dlp не получил доступные форматы для этой страницы.";
                    if (!fs::is_regular_file(dataRoot() / L"tools" / L"deno.exe"))
                        advice += " Не найден Deno, необходимый для обработки многих страниц YouTube.";
                    advice += " Проверьте, что ролик воспроизводится в подключённом браузере, затем повторите анализ. Доступность форматов также зависит от YouTube и учётной записи.";
                    throw std::runtime_error(advice + "\n\n" + redact(details));
                }
                if (details.find("Sign in to confirm") != std::string::npos || details.find("HTTP Error 429") != std::string::npos)
                    throw std::runtime_error("YouTube ограничил запрос или запросил подтверждение входа. Проверьте воспроизведение в подключённом браузере и повторите попытку позже.\n\n" + redact(details));
                throw std::runtime_error(redact(details));
            }
            if (!result.error.empty())
                log(result.error, false);
            auto a = parseAnalysis(Json::parse(result.output), u);
            post([this, a = std::move(a), s, source, u, frozen = std::move(frozen), cancel, generation, analyzedAt, deep]() mutable {
                if (*cancel || generation != analysisGeneration) return;
                if (s) {
                    const auto withdrawal = siteAccessWithdrawnAt.find(s->instanceSerial);
                    if (withdrawal != siteAccessWithdrawnAt.end() && analyzedAt <= withdrawal->second) {
                        busy = false;
                        if (!deep) error("Доступ к сайту изменился. Повторите анализ перед новой попыткой скачивания.");
                        return;
                    }
                }
                rememberMediaAccess(s, u, std::move(frozen));
                analysis = std::move(a);
                analysisSession = s;
                for (auto item : analysis.items) {
                    // The same resource may be authorized differently in two browser profiles.
                    item.key += "|session:" + (s ? std::to_string(s->instanceSerial) + ":" +
                        std::to_string(source.generation) + ":" +
                        (source.credentialRevision ? std::to_string(*source.credentialRevision) : "-") : std::string("manual"));
                    auto network = std::find_if(media.begin(), media.end(), [&](const Media &m) {
                        auto owner = mediaSessions.find(m.key);
                        auto identity = mediaSources.find(m.key);
                        return owner != mediaSessions.end() && owner->second == s &&
                            (!s || (identity != mediaSources.end() && identity->second == source &&
                                     identity->second.credentialRevision == source.credentialRevision)) &&
                            m.id.rfind("network:", 0) == 0 && sameDiscoveredResource(m, item);
                    });
                    if (network != media.end()) {
                        auto old = *network;
                        bool wasSelected = selectedMedia == old.key;
                        mediaSessions.erase(old.key);
                        mediaSources.erase(old.key);
                        *network = item;
                        mergeNetworkMetadata(*network, old);
                        network->selected = network->selected || old.selected;
                        if (wasSelected) selectedMedia = item.key;
                    } else {
                        auto previous = std::find_if(media.begin(), media.end(), [&](const Media &m) { return m.key == item.key; });
                        if (previous == media.end()) media.push_back(item);
                        else {
                            auto old = *previous;
                            *previous = item;
                            mergeNetworkMetadata(*previous, old);
                            previous->selected = previous->selected || old.selected;
                        }
                    }
                    mediaSessions[item.key] = s;
                    if (s) mediaSources[item.key] = source;
                    if (s && (item.category == "Требуют проверки" || item.category == "Видео · звук не проверен") && httpUrl(item.url)) {
                        int selectedTab = -1; std::string selectedUrl;
                        { std::lock_guard lock(s->mutex); selectedTab = s->tab; selectedUrl = s->url; }
                        if (selectedTab >= 0 && selectedUrl == item.page) {
                            auto found = std::find_if(media.begin(), media.end(), [&](const Media &m) { return m.key == item.key; });
                            if (found != media.end()) { found->quality = "Уточняем параметры…"; enqueueProbe(*found, s, scanGeneration, selectedTab); }
                        }
                    }
                }
                title = analysis.title;
                audioIndex = -1;
                busy = false;
                notify("Анализ завершён");
            });
        } catch (const BrowserCredentialsRefreshPending &) {
            post([this, cancel, generation, deep] {
                if (!*cancel && generation == analysisGeneration) {
                    busy = false;
                    if (!deep) error("Доступ к странице обновляется. Подождите немного и повторите анализ.");
                }
            });
        } catch (const BrowserCookieFormatUnsupported &) {
            post([this, cancel, generation, deep] {
                if (!*cancel && generation == analysisGeneration) {
                    busy = false;
                    if (!deep) error("Браузер передал cookies, которые нельзя безопасно использовать для анализа. Для общедоступной страницы можно выбрать режим «Без браузера».");
                }
            });
        } catch (const std::exception &e) {
            auto message = redact(e.what());
            post([this, cancel, generation, message, deep] { if (!*cancel && generation == analysisGeneration) { busy = false; if (!deep) error(message); } });
        }
    });
}
static std::string previewLocalPath(const Media &item, const Job *job) {
    if (job && job->done && job->state=="Готово" && !job->output.empty()) return job->output;
    if (item.key.starts_with("local:")) return item.key.substr(6);
    return {};
}

std::string App::previewCacheKey(const Media &item, const Job *job) {
    const auto foundSession = mediaSessions.find(item.key);
    auto selectedSession = job ? job->session : foundSession != mediaSessions.end() ? foundSession->second : nullptr;
    const auto foundSource = mediaSources.find(item.key);
    const auto source = job ? job->browserSource : foundSource != mediaSources.end() ? foundSource->second : BrowserSource{};
    if (!source.valid()) selectedSession.reset();
    return Json::array({item.key,item.thumbnail,previewLocalPath(item,job),
                        selectedSession ? selectedSession->instanceSerial : uint64_t(0),
                        source.tab, source.generation, source.page, source.store,
                        source.extensionSource ? Json(*source.extensionSource) : Json(),
                        source.credentialRevision ? Json(*source.credentialRevision) : Json()}).dump();
}
std::shared_ptr<Preview> App::requestPreview(const Media &item, const Job *job) {
    const bool still=item.category=="Изображения" || item.category=="Превью";
    const auto local=previewLocalPath(item,job);
    const auto thumbnail=local.empty() ? (item.thumbnail.empty() && still ? item.url : item.thumbnail) : std::string{};
    const auto publicKey=httpUrl(thumbnail) ? Json::array({"public-cover",thumbnail}).dump() : std::string{};
    if(!publicKey.empty()) {
        const auto shared=previews.find(publicKey);
        if(shared!=previews.end()) {
            shared->second->lastSeenFrame=previewFrame;
            return shared->second;
        }
    }
    auto key=previewCacheKey(item,job);
    if(auto it=previews.find(key);it!=previews.end()){
        it->second->lastSeenFrame=previewFrame;
        if (!it->second->done || !it->second->image.rgba.empty() || !it->second->retryAt || GetTickCount64() < it->second->retryAt)
            return it->second;
        previews.erase(it);
    }
    if(previewActive>=2)return nullptr;
    if(previews.size()>=64){
        auto old=previews.end();
        for(auto it=previews.begin();it!=previews.end();++it){
            const auto &candidate=it->second;
            if(!candidate->done || candidate->lastSeenFrame>=previewFrame || previewFrame-candidate->lastSeenFrame<=1)continue;
            if(old==previews.end() || candidate->lastSeenFrame<old->second->lastSeenFrame)old=it;
        }
        if(old!=previews.end())previews.erase(old);
        else if(previews.size()>=1024)return nullptr;
    }
    auto preview=std::make_shared<Preview>();
    preview->id=++previewSerial;preview->lastSeenFrame=previewFrame;
    previews[key]=preview;++previewActive;
    auto foundSession = mediaSessions.find(item.key);
    auto selectedSession = job ? job->session : foundSession != mediaSessions.end() ? foundSession->second : nullptr;
    auto foundSource = mediaSources.find(item.key);
    const auto selectedSource = job ? job->browserSource : foundSource != mediaSources.end() ? foundSource->second : BrowserSource{};
    if (!selectedSource.valid()) selectedSession.reset();
    const auto generation=previewGeneration;
    work([this,item,selectedSession,selectedSource,preview,key,publicKey,local,thumbnail,still,generation]{
        PreviewImage image;
        bool publicCover=false;
        const bool frame=previewFrameCandidate(item) && httpUrl(item.url);
        const bool retryable=!local.empty() || !publicKey.empty() || frame;
        if (retryable && !stopping) {
            std::atomic_bool cancel{false};
            std::jthread watchdog([this,&cancel](std::stop_token stop){
                const auto until=GetTickCount64()+18000;
                while(!stop.stop_requested() && !stopping && GetTickCount64()<until)Sleep(50);
                if(!stop.stop_requested())cancel=true;
            });
            static std::atomic_uint64_t serial{0};
            const auto scratch=dataRoot()/L"temp"/(L"preview-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++serial)+L".input");
            struct Cleanup {fs::path path;~Cleanup(){std::error_code ec;fs::remove(path,ec);}} cleanup{scratch};
            const auto kind=previewAudioCandidate(item) ? MediaPreviewKind::audio : MediaPreviewKind::video;
            const auto decode=[&](const std::wstring &input,bool remote,MediaPreviewKind mediaKind) {
                if(cancel || stopping)return PreviewImage{};
                std::shared_lock lock(toolsMutex);
                fs::path ffprobe;
                if(!remote && mediaKind!=MediaPreviewKind::image) {try{ffprobe=tool(L"ffprobe.exe");}catch(...){}}
                return decodeMediaPreview(tool(L"ffmpeg.exe"),ffprobe,input,remote,mediaKind,cancel);
            };
            const auto scopedFile=[&](const std::string &contents,MediaPreviewKind mediaKind) {
                if(cancel || stopping || contents.empty())return PreviewImage{};
                writePrivateCredentialFile(scratch,contents);
                struct Erase {const fs::path &path;~Erase(){std::error_code ec;fs::remove(path,ec);}} erase{scratch};
                return decode(scratch.wstring(),false,mediaKind);
            };
            const auto decodeCover=[&](const std::string &contents) {
                try {return decodePreview(contents);}
                catch (...) {return scopedFile(contents,MediaPreviewKind::image);}
            };
            const auto attempt=[&](auto &&fn) {
                if(image.rgba.empty() && !cancel && !stopping) {
                    try {image=fn();}
                    catch (...) {}
                }
            };
            if(!local.empty()) {
                attempt([&] {
                    const fs::path path=wide(local);
                    if(!path.is_absolute() || !fs::is_regular_file(path))return PreviewImage{};
                    if(still) {
                        try {return decodePreview(readFile(path,8*1024*1024));}
                        catch (...) {return decode(path.wstring(),false,MediaPreviewKind::image);}
                    }
                    return decode(path.wstring(),false,kind);
                });
            } else {
                if(!publicKey.empty()) {
                    attempt([&]{return decodeCover(httpPreview(thumbnail,cancel,Json::object(),""));});
                    publicCover=!image.rgba.empty();
                    if(selectedSession) attempt([&]{
                        auto cred=credentials(selectedSession,thumbnail,cancel,&selectedSource);
                        return decodeCover(httpPreview(thumbnail,cancel,cred.second,cred.first));
                    });
                }
                if(frame) {
                    const bool manifest=previewManifest(item);
                    const bool signedAddress=item.url.find('?')!=std::string::npos;
                    const auto anonymous=[&] {
                        if(signedAddress && !manifest)
                            return scopedFile(httpPreviewPrefix(item.url,cancel,Json::object(),"",32*1024*1024),kind);
                        if(signedAddress)return PreviewImage{};
                        return decode(wide(item.url),true,kind);
                    };
                    if(item.format || manifest || !selectedSession)attempt(anonymous);
                    if(!manifest) attempt([&] {
                        auto cred=credentials(selectedSession,item.url,cancel,selectedSession?&selectedSource:nullptr);
                        if(!previewHasCookies(cred.first) && cred.second.empty())
                            return (item.format || !selectedSession) ? PreviewImage{} : anonymous();
                        return scopedFile(httpPreviewPrefix(item.url,cancel,cred.second,cred.first,32*1024*1024),kind);
                    });
                    if(!item.format && !manifest && selectedSession)attempt(anonymous);
                }
            }
        }
        const std::string message=retryable ? "Превью недоступно" : "Обложка не предоставлена";
        post([this,preview,key,publicKey,publicCover,generation,retryable,image=std::move(image),message]()mutable{
            preview->image=std::move(image);
            preview->done=true;
            preview->message=message;
            if(preview->image.rgba.empty() && retryable)preview->retryAt=GetTickCount64()+45000;
            if(publicCover && generation==previewGeneration){
                const auto entry=previews.find(key);
                if(entry!=previews.end() && entry->second==preview){
                    previews.erase(entry);
                    previews.try_emplace(publicKey,preview);
                }
            }
            --previewActive;
        });
    });
    return preview;
}
void App::saveHistory() {
    Json v = Json::array();
    std::lock_guard l(jobsMutex);
    for (auto &j : jobs) {
        Json sources = Json::array(); for (const auto &source : j->sources) { auto entry=assemblyJson(source); entry["media"]=mediaJson(source.media); sources.push_back(entry); }
        v.push_back({{"id", j->id},
                     {"title", j->title},
                     {"state", j->state},
                     {"page", j->page},
                     {"folder", j->folder},
                     {"pair", j->pair},
                     {"local", j->local},
                     {"output", j->output},
                     {"outputDirectory", j->outputDirectory},
                     {"assembly", j->assembly}, {"singleExport",j->singleExport}, {"exportFormat",j->exportFormat}, {"exportQuality",j->exportQuality}, {"assemblyInput",j->assemblyInput}, {"localFile",j->localFile}, {"conversionSource",j->conversionSource}, {"sources", sources}, {"assemblyMetadataTitle",j->assemblyMetadataTitle}, {"assemblyArtist",j->assemblyArtist}, {"assemblyDescription",j->assemblyDescription}, {"globalTags",j->globalTags}, {"originalGlobalTags",j->originalGlobalTags},
                     {"item", mediaJson(j->item)}, {"browserSourceRequired", j->browserSourceRequired}, {"resumeAccessKey", j->resumeAccessKey}, {"mux", j->mux}, {"muxInputs", j->muxInputs}, {"muxChapters", j->muxChapters},
                     {"muxOptions", {{"container", j->muxOptions.container}, {"video", j->muxOptions.video}, {"audio", j->muxOptions.audio}, {"subtitles", j->muxOptions.subtitles}}}});
    }
    const auto serialized = v.dump();
    if (serialized.size() > QueueHistoryLimit) throw std::runtime_error("Файл слишком большой");
    const auto root = dataRoot();
    const auto history = root / L"queue.dpapi";
    if (historyRecoveryRequired) {
        // Do not overwrite unreadable or partly incompatible history without
        // first retaining a byte-for-byte copy.  On failure, keep the original
        // untouched and let the caller report the save error.
        if (fs::exists(history)) {
            bool preserved = false;
            for (unsigned attempt = 0; attempt < 100; ++attempt) {
                const auto name = L"queue-unreadable-" + std::to_wstring(GetTickCount64()) +
                    L"-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(attempt) + L".dpapi";
                const auto backup = root / name;
                std::error_code ec;
                if (fs::copy_file(history, backup, fs::copy_options::none, ec)) {
                    logs.push_back({"Исходная история очереди сохранена в резервной копии: " + utf8(backup.wstring()), true});
                    preserved = true;
                    break;
                }
                if (ec != std::errc::file_exists) break;
            }
            if (!preserved) throw std::runtime_error("Не удалось сохранить резервную копию истории очереди");
        }
        historyRecoveryRequired = false;
    }
    protectFile(history, serialized, QueueHistoryLimit);
}
void App::enqueue() {
    std::vector<Media> chosen;
    for (auto &m : media)
        if (m.selected)
            chosen.push_back(m);
    if (chosen.empty()) {
        notify("Отметьте нужные файлы галочками");
        return;
    }
    auto dest = fs::path(wide(destination));
    if (!dest.is_absolute()) {
        error("Выберите полную папку сохранения");
        return;
    }
    auto destinationPath = dest.wstring();
    if (destinationPath.find_first_of(L"<>\"|?*") != std::wstring::npos || destinationPath.find(L':', 2) != std::wstring::npos) {
        error("Путь к папке некорректен. Выберите папку заново кнопкой «Папка».");
        return;
    }
    for (auto &m : chosen)
        if (m.drm) {
            error("Защищённые потоки недоступны для скачивания");
            return;
        }
    std::vector<std::shared_ptr<Job>> pending;
    for (auto &m : chosen) {
        auto j = std::make_shared<Job>();
        j->id = std::to_string(GetTickCount64()) + "-" + std::to_string(++jobSerial);
        j->title = mediaJobTitle(m);
        j->item = m;
        j->page = m.page;
        bindJobSource(*j, m);
        j->folder = utf8((dest / wide(j->id)).wstring());
        j->outputDirectory = utf8(dest.wstring());
        if (m.category == "Данные") {
            j->local = dataSourceContent(m);
        }
        pending.push_back(std::move(j));
    }
    { std::lock_guard l(jobsMutex); jobs.insert(jobs.end(), pending.begin(), pending.end()); }
    selectedJob = pending.back()->id;
    stageDownloadAccess(pending);
    jobsCv.notify_all();
    requestedPage = 2;
    notify("Добавлено задач: " + std::to_string(chosen.size()));
}
void App::setParallel(int n) {
    parallel = std::clamp(n, 1, 4);
    jobsCv.notify_all();
}
static bool isSubtitleJob(const Job &job) {
    return job.item.category=="Субтитры" || job.item.category=="Автосубтитры";
}
struct SubtitleRateLimited : std::runtime_error {
    SubtitleRateLimited() : std::runtime_error("HTTP 429") {}
};
struct InvalidDownloadedMedia : std::runtime_error {
    using std::runtime_error::runtime_error;
};
void App::downloadLoop() {
    while (!stopping) {
        std::shared_ptr<Job> next;
        bool subtitle=false;
        {
            std::unique_lock lock(jobsMutex);
            auto eligible=[this](const auto &job) {
                return !job->waitingSources && !job->preparingAccess && !job->claimed && !job->done && !job->cancel &&
                    (!isSubtitleJob(*job) || (!subtitleActive && GetTickCount64()>=subtitleResumeAt));
            };
            jobsCv.wait_for(lock,std::chrono::seconds(1),[&] {
                return stopping || (active<parallel && std::any_of(jobs.begin(),jobs.end(),eligible));
            });
            if (stopping) return;
            if (active<parallel) {
                auto it=std::find_if(jobs.begin(),jobs.end(),eligible);
                if(it!=jobs.end()){
                    next=*it;
                    next->claimed=true;
                    subtitle=isSubtitleJob(*next);
                    if(subtitle)subtitleActive=true;
                    ++active;
                }
            }
        }
        if (!next)
            continue;
        next->workerOutput.clear();
        next->workerCategory.clear();
        post([next] { next->state = "Скачивание"; });
        bool succeeded=false;
        try {
            download(next);
            if(next->cancel || stopping)throw std::runtime_error("Отменено");
            if (!next->outputDirectory.empty() && !next->workerOutput.empty()) {
                fs::path source = wide(next->workerOutput), dest = wide(next->outputDirectory);
                const auto &name = (!next->assembly && !next->assemblyInput && !next->title.empty())
                    ? next->title : next->item.title;
                std::wstring base = wide(name.empty() ? "media" : name);
                for (auto &ch : base) if (ch < 32 || std::wstring(L"<>:\"/\\|?*").find(ch) != std::wstring::npos) ch = L'_';
                while (!base.empty() && (base.back() == L'.' || base.back() == L' ')) base.pop_back();
                if (base.empty()) base = L"media";
                if (base.size() > 100) base.resize(100);
                if(!base.empty() && IS_HIGH_SURROGATE(base.back()))base.pop_back();
                auto stem=base.substr(0,base.find(L'.'));std::transform(stem.begin(),stem.end(),stem.begin(),::towupper);
                if(stem==L"CON"||stem==L"PRN"||stem==L"AUX"||stem==L"NUL"||(stem.size()==4&&(stem.substr(0,3)==L"COM"||stem.substr(0,3)==L"LPT")&&stem[3]>=L'0'&&stem[3]<=L'9'))base=L"media_"+base;
                auto extension = source.extension().wstring();
                bool moved = false;
                for (int n = 0; n < 10000; ++n) {
                    auto name = base + (n ? L" (" + std::to_wstring(n + 1) + L")" : L"") + extension;
                    auto target = dest / name;
                    if (MoveFileW(source.c_str(), target.c_str())) { next->workerOutput = utf8(target.wstring()); moved = true; break; }
                    auto error = GetLastError();
                    if (error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS) throw std::runtime_error("Не удалось сохранить файл в выбранной папке.");
                }
                if (!moved) throw std::runtime_error("Не удалось подобрать свободное имя файла.");
                std::error_code ec; fs::remove(fs::path(wide(next->folder)), ec);
            }
            auto output = next->workerOutput, category = next->workerCategory;
            std::error_code sizeError;
            auto fileSize = output.empty() ? 0 : fs::file_size(wide(output), sizeError);
            post([this, next, output, category, fileSize, measured = !sizeError && !output.empty()] {
                if (measured) {
                    next->item.size = static_cast<double>(fileSize);
                    next->item.approx = false;
                }
                next->output = output; if (!category.empty()) next->item.category = category;
                next->done = true;
                next->state = next->cancel ? "Отменено" : "Готово";
                next->percent = next->cancel ? next->percent : 100;
                next->detail = next->cancel ? "Загрузка остановлена" : "Файл сохранён";
                saveHistory();
            });
            succeeded=true;
        } catch (const SubtitleRateLimited &) {
            unsigned seconds;
            {
                std::lock_guard lock(jobsMutex);
                seconds=subtitleBackoff;
                subtitleResumeAt=GetTickCount64()+ULONGLONG(seconds)*1000;
                subtitleBackoff=std::min(seconds*2,600u);
            }
            auto message="HTTP 429: субтитры отложены на "+std::to_string(seconds)+" с";
            post([this,next,message]{
                {
                    std::lock_guard lock(jobsMutex);
                    next->claimed=false;
                    next->done=next->cancel.load();
                    next->state=next->cancel?"Отменено":"Пауза сервера";
                    next->percent=0;
                    next->detail=message;
                }
                jobsCv.notify_all();
                saveHistory();
            });
            log(message);
        } catch (const std::exception &e) {
            auto msg = dynamic_cast<const fs::filesystem_error *>(&e)
                ? std::string("Не удалось создать папку или сохранить файл. Проверьте путь и права доступа.") : redact(e.what());
            auto output = next->workerOutput;
            post([this, next, msg, output] {
                next->output = output;
                next->done = true;
                next->state = next->cancel ? "Отменено" : "Ошибка";
                next->detail = msg;
                saveHistory();
            });
            log(msg);
        }
        {
            std::lock_guard l(jobsMutex);
            active--;
            if(subtitle) {
                subtitleActive=false;
                if(succeeded)subtitleBackoff=60;
            }
        }
        jobsCv.notify_all();
    }
}
void App::download(const std::shared_ptr<Job> &j) {
    {
        std::unique_lock l(readyMutex);
        readyCv.wait(l, [this] { return ready || stopping; });
    }
    if (stopping || j->cancel)
        throw std::runtime_error("Отменено");
    std::shared_lock tools(toolsMutex);
    if (j->assembly) { downloadAssembly(j); return; }
    auto folder = fs::path(wide(j->folder));
    fs::create_directories(folder);
    auto validate = [&](const fs::path &path, ExpectedStreams expected) {
        std::atomic_bool probeCancel{false}, finished{false}, timedOut{false};
        std::thread watchdog([&] { for (int i = 0; i < 100 && !finished && !j->cancel; ++i) Sleep(100); if (!finished) { timedOut = !j->cancel; probeCancel = true; } });
        ProcessResult probe{};
        try { probe = runProcess(tool(L"ffprobe.exe"), {L"-v", L"error", L"-show_entries", L"stream=codec_type:stream_disposition=attached_pic", L"-of", L"json", path.wstring()}, nullptr, probeCancel, true); }
        catch (...) { finished = true; watchdog.join(); throw; }
        finished = true; watchdog.join();
        if (j->cancel) throw std::runtime_error("Отменено");
        auto info = Json::parse(probe.output, nullptr, false);
        if (timedOut) throw std::runtime_error("Не удалось проверить файл вовремя. Скачанный файл сохранён в папке задачи.");
        if (probe.code != 0 || !info.is_object() || !info.contains("streams") || !info["streams"].is_array()) {
            auto details=redact(probe.error); if(details.size()>350)details=details.substr(0,350);
            throw InvalidDownloadedMedia("Не удалось прочитать сведения о скачанном файле. Код FFprobe: " + std::to_string(probe.code) + ". Файл сохранён в папке задачи." + (details.empty()?std::string():" "+details));
        }
        if (!probeMatches(probe.code, info, expected)) {
            bool video=false, audio=false;
            for(const auto &stream:info["streams"]) { video |= isPlayableVideoStream(stream); audio |= str(stream,"codec_type")=="audio"; }
            std::string wanted=expected==ExpectedStreams::Audio?"аудио":expected==ExpectedStreams::Video?"видео":expected==ExpectedStreams::VideoAndAudio?"видео и аудио":"аудио или видео";
            std::string found=video?(audio?"видео и аудио":"только видео"):(audio?"только аудио":"нет видео и аудио");
            throw InvalidDownloadedMedia("Ожидалось: "+wanted+"; обнаружено: "+found+". Скачанный файл сохранён в папке задачи.");
        }
    };
    auto quarantine = [&](const fs::path &path) {
        static std::atomic_uint64_t serial{0};
        auto name = L"invalid-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++serial) + L"-" + path.filename().wstring();
        auto backup = folder / name;
        if (!MoveFileW(path.c_str(), backup.c_str())) throw std::runtime_error("Не удалось сохранить повреждённый файл отдельно перед повтором.");
        j->workerOutput = utf8(backup.wstring());
    };
    // yt-dlp only gives its final filename after a completed download. If a later
    // verification failed, reuse that completed file on retry without downloading it again.
    if (j->assemblyInput && j->item.format) {
        static const std::set<std::wstring> mediaExtensions={L".mp4",L".webm",L".mkv",L".m4a",L".mp3",L".mov",L".ts",L".m4v",L".aac",L".ogg",L".opus",L".flv",L".wav",L".m4b",L".mka"};
        std::vector<fs::path> candidates;
        for (const auto &entry : fs::directory_iterator(folder)) {
            auto path=entry.path();auto ext=path.extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);
            if(entry.is_regular_file() && path.stem()==L"media" && mediaExtensions.count(ext))candidates.push_back(path);
        }
        if(candidates.size()==1){
            j->workerOutput=utf8(candidates[0].wstring());
            try {
                validate(candidates[0],ExpectedStreams::AnyMedia);
                return;
            } catch (const InvalidDownloadedMedia &) {
                quarantine(candidates[0]);
                // Continue below and re-download; keep the failed file under
                // its quarantine name for inspection or recovery.
            }
        }
    }
    if (j->mux) {
        const auto container = muxContainer(j->muxOptions);
        auto codecArgs = muxCodecArgs(j->muxOptions);
        auto saved = folder / (L"result." + wide(container));
        if (!j->output.empty() && fs::path(wide(j->output)) == saved && fs::is_regular_file(saved)) {
            validate(saved, ExpectedStreams::AnyMedia);
            j->workerOutput = utf8(saved.wstring());
            return;
        }
        if (j->muxInputs.empty() || j->muxInputs.size() > 64) throw std::runtime_error("Выберите от 1 до 64 медиафайлов");
        auto probeFile = [&](const fs::path &path) {
            std::atomic_bool cancel{false}, finished{false};
            std::thread watchdog([&] { for (int i = 0; i < 150 && !finished && !j->cancel; ++i) Sleep(100); if (!finished) cancel = true; });
            ProcessResult result{};
            try { result = runProcess(tool(L"ffprobe.exe"), {L"-v", L"error", L"-show_entries", L"stream=codec_type,duration:chapter=start_time,end_time:format=duration", L"-of", L"json", path.wstring()}, nullptr, cancel, true); }
            catch (...) { finished = true; watchdog.join(); throw; }
            finished = true; watchdog.join();
            if (j->cancel) throw std::runtime_error("Отменено");
            auto json = Json::parse(result.output, nullptr, false);
            if (result.code || !json.is_object()) throw std::runtime_error("Не удалось прочитать дорожки одного из файлов. Проверьте файл и повторите.");
            return json;
        };
        std::vector<std::wstring> args{L"-hide_banner", L"-loglevel", L"error", L"-nostdin", L"-n"};
        double totalDuration = 0;
        int expectedVideo = 0, expectedAudio = 0, expectedSubtitle = 0, chapterInput = -1, chapterCount = 0;
        for (size_t i = 0; i < j->muxInputs.size(); ++i) {
            fs::path path = wide(j->muxInputs[i]);
            if (!fs::is_regular_file(path)) throw std::runtime_error("Исходный файл отсутствует: выберите загрузки заново");
            auto probe = probeFile(path);
            totalDuration=std::max(totalDuration,muxDuration(probe.value("format",Json::object())));
            int streams = 0;
            for (const auto &stream : probe.value("streams", Json::array())) {
                auto type = str(stream, "codec_type");
                if (type == "video") { ++expectedVideo; ++streams; }
                else if (type == "audio") { ++expectedAudio; ++streams; }
                else if (type == "subtitle") { ++expectedSubtitle; ++streams; }
            }
            if (!streams) throw std::runtime_error("В выбранном файле нет поддерживаемых видео-, аудио- или субтитровых дорожек");
            if (chapterInput < 0 && probe.value("chapters", Json::array()).size() > 0) { chapterInput = int(i); chapterCount = int(probe["chapters"].size()); }
            args.insert(args.end(), {L"-i", path.wstring()});
        }
        if (!expectedVideo && !expectedAudio) throw std::runtime_error("Выберите хотя бы одну видео- или аудиодорожку");
        static std::atomic_uint64_t temporarySerial{0};
        auto suffix = std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++temporarySerial);
        auto chapterFile = folder / (L"chapters-" + suffix + L".ffmetadata");
        auto partial = folder / (L"result.part-" + suffix + L"." + wide(container));
        auto final = folder / (L"result." + wide(container));
        struct Cleanup { fs::path partial, chapters; ~Cleanup(){ std::error_code ec; fs::remove(chapters, ec); } } cleanup{partial, chapterFile};
        if (!j->muxChapters.empty()) {
            auto json = Json::parse(readFile(wide(j->muxChapters), 4 * 1024 * 1024));
            writeFile(chapterFile, chaptersToFfmetadata(json));
            chapterCount = int(json.size()); chapterInput = int(j->muxInputs.size());
            args.insert(args.end(), {L"-f", L"ffmetadata", L"-i", chapterFile.wstring()});
        }
        for (size_t i = 0; i < j->muxInputs.size(); ++i) {
            auto index = std::to_wstring(i);
            args.insert(args.end(), {L"-map", index + L":v?", L"-map", index + L":a?", L"-map", index + L":s?"});
        }
        args.insert(args.end(), {L"-map_metadata", L"0", L"-map_chapters", std::to_wstring(chapterInput)});
        args.insert(args.end(), codecArgs.begin(), codecArgs.end());
        args.push_back(partial.wstring());
        auto result = runFfmpegWithProgress(j,args,totalDuration);
        if (j->cancel) throw std::runtime_error("Отменено");
        if (result.code) {
            auto detail = redact(result.error);
            if (detail.size() > 500) detail = detail.substr(detail.size() - 500);
            throw std::runtime_error("FFmpeg не смог объединить дорожки. Проверьте совместимость контейнера и кодеков или выберите перекодирование." + (detail.empty() ? "" : " " + detail));
        }
        j->workerOutput = utf8(partial.wstring());
        auto output = probeFile(partial);
        int video = 0, audio = 0, subtitles = 0;
        for (const auto &stream : output.value("streams", Json::array())) {
            auto type = str(stream, "codec_type"); video += type == "video"; audio += type == "audio"; subtitles += type == "subtitle";
        }
        if (video < expectedVideo || audio < expectedAudio || subtitles < expectedSubtitle || int(output.value("chapters", Json::array()).size()) < chapterCount)
            throw std::runtime_error("Итоговый файл не содержит все выбранные дорожки или главы. Попробуйте другой контейнер.");
        fs::rename(partial, final); j->workerOutput = utf8(final.wstring());
        j->workerCategory = video ? "Видео" : "Аудио";
        return;
    }
    if (j->item.category == "Данные") {
        auto path = folder / (L"metadata." + wide(j->item.ext)); writeFile(path, j->local); j->workerOutput = utf8(path.wstring());
        return;
    }
    if (!j->item.format && j->item.protocol != "m3u8" && j->item.protocol != "dash" &&
        j->item.category != "HLS" && j->item.category != "DASH") {
        auto final = folder / (L"media." + wide(j->item.ext.empty() ? "bin" : j->item.ext));
        if (fs::is_regular_file(final)) {
            j->workerOutput = utf8(final.wstring());
            if (j->item.category == "Видео" || j->item.category == "Видео без звука" ||
                j->item.category == "Видео · звук не проверен" || j->item.category == "Аудио" || j->item.category == "Требуют проверки") {
                auto expected = j->assemblyInput ? ExpectedStreams::AnyMedia : j->item.category == "Аудио" ? ExpectedStreams::Audio : j->item.category == "Требуют проверки" ? ExpectedStreams::AnyMedia : ExpectedStreams::Video;
                try { validate(final, expected); }
                catch (const InvalidDownloadedMedia &) { quarantine(final); }
                if (!fs::exists(final)) {
                    // An explicitly retried invalid file was preserved in the
                    // task directory; obtain a fresh copy below.
                } else return;
            } else {
                return;
            }
        }
    }
    const bool subtitle=isSubtitleJob(*j);
    const bool extractedSubtitle = subtitleNeedsPageAccess(*j);
    auto access = getDownloadAccess(*j);
    auto &cred = access.target;
    const DownloadAccess emptyPage{"", Json::object()};
    const auto &pageCred = access.page ? *access.page : emptyPage;
    if (!j->item.format && j->item.protocol != "m3u8" && j->item.protocol != "dash" &&
        j->item.category != "HLS" && j->item.category != "DASH") {
        auto path = folder / (L"media." + wide(j->item.ext.empty() ? "bin" : j->item.ext));
        path += L".part";
        auto lastTick=GetTickCount64();uint64_t lastCount=0;
        post([j]{j->state="Скачивание";j->percent=-1;j->detail="Соединяемся с сервером…";});
        auto fetchFile=[&](const std::string &url){httpDownloadResumable(
            url, path, j->cancel, [](double){}, cred.second, cred.first,
            [this,j,&lastTick,&lastCount](uint64_t count,uint64_t total){
                auto now=GetTickCount64(),elapsed=now-lastTick;
                if(elapsed<250 && (!total||count<total))return;
                double speed=elapsed?double(count-lastCount)*1000./elapsed:0;
                lastTick=now;lastCount=count;
                std::ostringstream line;line<<"Скачано "<<std::fixed<<std::setprecision(2)<<double(count)/(1024.*1024.)<<" МиБ";
                if(total)line<<" из "<<double(total)/(1024.*1024.)<<" МиБ";
                if(speed>0)line<<" · "<<std::setprecision(2)<<speed/(1024.*1024.)<<" МиБ/с";
                auto detail=line.str();double percent=total?std::clamp(100.*count/total,0.,100.):-1.;
                post([j,detail,percent]{j->state="Скачивание";j->detail=detail;j->percent=percent;});
            });};
        auto downloadUrl=j->item.url;
        bool refreshed=false;
        for(;;) {
            try { fetchFile(downloadUrl); break; }
            catch(const std::runtime_error &e) {
                std::string reason=e.what();
                if(subtitle && reason=="HTTP 429" && !j->cancel && !stopping)throw SubtitleRateLimited();
                if(j->cancel || stopping)throw;
                if(!extractedSubtitle || refreshed || (reason!="HTTP 401" && reason!="HTTP 403" && reason!="HTTP 404" && reason!="HTTP 410")) {
                    if (reason == "HTTP 401" || reason == "HTTP 403") discardDownloadAccess(*j);
                    throw;
                }
                refreshed=true;
                post([j]{j->detail="Обновляем ссылку на субтитры…";});
                TempCookie pageCookie(pageCred.first);
                auto args=headersArgs(pageCred.second);
                args.insert(args.end(),{L"--ignore-config",L"--encoding",L"utf-8",L"--no-playlist",
                    L"--no-warnings",L"--dump-single-json",L"--skip-download",L"--socket-timeout",L"15",
                    L"--js-runtimes",L"deno:"+(dataRoot()/L"tools"/L"deno.exe").wstring()});
                if(!pageCookie.path.empty())args.insert(args.end(),{L"--cookies",pageCookie.path.wstring()});
                args.insert(args.end(),{L"--",wide(j->page)});
                auto result=runProcess(tool(L"yt-dlp.exe"),args,nullptr,j->cancel,true);
                if(j->cancel || stopping)throw std::runtime_error("Отменено");
                auto data=Json::parse(result.output,nullptr,false);
                if(result.code || !data.is_object())
                    throw std::runtime_error("Не удалось обновить ссылку на субтитры после "+reason);
                auto updated=parseAnalysis(data,j->page);
                auto track=std::find_if(updated.items.begin(),updated.items.end(),[&](const Media &item){
                    return item.id==j->item.id && httpUrl(item.url);
                });
                if(track==updated.items.end())
                    throw std::runtime_error("Субтитры выбранного языка и формата больше не доступны");
                downloadUrl=track->url;
                cred = subtitleDownloadAccess(j->page, downloadUrl, pageCred);
            }
        }
        if(subtitle) {
            std::error_code ec;
            if(fs::file_size(path,ec)==0 || ec)throw std::runtime_error("Сервер вернул пустые субтитры");
        }
        {
            std::ifstream file(path, std::ios::binary); char head[1024]{}; file.read(head, sizeof(head));
            if (looksLikeHtml(std::string(head, size_t(file.gcount())))) {
                file.close(); std::error_code ec; fs::remove(path, ec);
                throw std::runtime_error("Вместо файла сервер вернул HTML-страницу. Обновите страницу и повторите анализ.");
            }
        }
        auto final = path;
        final.replace_extension();
        j->workerOutput = utf8(path.wstring());
        if (j->item.category == "Видео" || j->item.category == "Видео без звука" ||
            j->item.category == "Видео · звук не проверен" || j->item.category == "Аудио" || j->item.category == "Требуют проверки") {
            auto expected = j->assemblyInput ? ExpectedStreams::AnyMedia : j->item.category == "Аудио" ? ExpectedStreams::Audio : j->item.category == "Требуют проверки" ? ExpectedStreams::AnyMedia : ExpectedStreams::Video;
            try { validate(path, expected); }
            catch (const InvalidDownloadedMedia &) { quarantine(path); throw; }
            catch (...) {
                // A missing FFprobe or timeout says nothing about validity. Keep
                // the completed file for validation on the next retry.
                fs::rename(path, final);
                j->workerOutput = utf8(final.wstring());
                throw;
            }
        }
        fs::rename(path, final);
        j->workerOutput = utf8(final.wstring());
        return;
    }
    TempCookie cookie(cred.first);
    auto args = headersArgs(cred.second);
    args.insert(args.end(),
                {L"--ignore-config", L"--encoding", L"utf-8", L"--no-playlist", L"--newline", L"--progress",
                 L"--progress-delta", L"0.2", L"--windows-filenames", L"--ffmpeg-location",
                 (dataRoot() / L"tools").wstring(), L"--js-runtimes",
                 L"deno:" + (dataRoot() / L"tools" / L"deno.exe").wstring(), L"--concurrent-fragments",
                 std::to_wstring(fragments), L"--no-simulate", L"--print", L"after_move:STREAMSHELF_FINAL:%(filepath)s", L"-o", (folder / L"media.%(ext)s").wstring()});
    if (j->item.format) {
        args.push_back(L"-f");
        args.push_back(wide(j->item.id + (j->pair.empty() ? "" : "+" + j->pair)));
        if (!j->pair.empty()) {
            args.push_back(L"--merge-output-format");
            args.push_back(L"mkv");
        }
    }
    if (!cookie.path.empty()) {
        args.push_back(L"--cookies");
        args.push_back(cookie.path.wstring());
    }
    args.push_back(L"--");
    args.push_back(wide(j->item.format ? j->page : j->item.url));
    auto result = runProcess(
        tool(L"yt-dlp.exe"), args,
        [this, j](const std::string &line) {
            if (line.rfind("STREAMSHELF_FINAL:", 0) == 0) return;
            log(line, false);
            static const std::regex percent(R"((\d+(?:\.\d+)?)%)");
            std::smatch m;
            double p = -1;
            if (std::regex_search(line, m, percent))
                try {
                    p = std::stod(m[1]);
                } catch (...) {
                }
            post([j, line, p] {
                if (p >= 0 || line.rfind("[download]",0)==0) j->detail = line;
                if (p >= 0)
                    j->percent = p;
            });
        },
        j->cancel, true);
    if (result.code && !j->cancel) {
        if (result.error.find("HTTP Error 401") != result.error.npos || result.error.find("HTTP Error 403") != result.error.npos)
            discardDownloadAccess(*j);
        throw std::runtime_error("yt-dlp завершился с кодом " + std::to_string(result.code));
    }
    if (j->cancel) throw std::runtime_error("Отменено");
    auto resultPath = finalDownloadedPath(result.output);
    if (!resultPath) throw std::runtime_error("Не удалось определить итоговый файл загрузки.");
    auto final = fs::weakly_canonical(fs::path(wide(*resultPath)));
    if (final.parent_path() != fs::weakly_canonical(folder) || !fs::is_regular_file(final)) throw std::runtime_error("Итоговый файл не найден в папке загрузки.");
    j->workerOutput = utf8(final.wstring());
    if (j->item.category == "Видео" || j->item.category == "Видео без звука" || j->item.category == "Видео · звук не проверен" || j->item.category == "Аудио" || j->item.category == "HLS" || j->item.category == "DASH" || j->item.category == "Требуют проверки") {
        try {
            validate(final, j->assemblyInput ? ExpectedStreams::AnyMedia : !j->pair.empty() ? ExpectedStreams::VideoAndAudio : j->item.category == "Аудио" ? ExpectedStreams::Audio : j->item.category == "HLS" || j->item.category == "DASH" || j->item.category == "Требуют проверки" ? ExpectedStreams::AnyMedia : ExpectedStreams::Video);
        } catch (const InvalidDownloadedMedia &) {
            quarantine(final);
            throw;
        }
    }
    j->workerOutput = utf8(final.wstring());
}
void App::closeRequest() {
    bool activeJob;
    {
        std::lock_guard l(jobsMutex);
        activeJob = active > 0 ||
                    std::any_of(jobs.begin(), jobs.end(), [](auto &j) { return !j->claimed && !j->done; });
    }
    if (activeJob) {
        modalTitle = "Остановить загрузки?";
        modalBody = "Текущие загрузки будут отменены. Частичные файлы останутся на диске. При повторе загрузка продолжится, если сервер поддерживает проверенное возобновление; иначе начнётся заново.";
        modalConfirmLabel = "Остановить и выйти";
        modalConfirm = [this] {
            for (auto &j : jobs)
                j->cancel = true;
            exitRequested = true;
        };
    } else
        exitRequested = true;
}
void App::action(const std::string &cmd) {
    try {
        auto requireAvailableCodecs = [this](const std::shared_ptr<Job> &job) {
            if(!job->assembly)return;
            for(const auto &source:job->sources) {
                if(source.codec==0 || source.role<0 || source.role>2)continue;
                int maximum=source.role==0?10:4;
                if(source.codec<0 || source.codec>maximum)throw std::runtime_error("Некорректный сохранённый кодек. Откройте конструктор и выберите доступный вариант.");
                bool available=source.role==0?videoEncoders[source.codec]:source.role==1?audioEncoders[source.codec]:subtitleEncoders[source.codec];
                if(!available)throw std::runtime_error(videoEncodersReady?"Сохранённый кодировщик недоступен. Откройте конструктор и выберите другой.":"Проверка кодировщиков ещё выполняется. Дождитесь её завершения.");
            }
        };
        if (cmd == "analyze")
            analyze(url);
        else if (cmd == "toggleManualAnalysis") {
            manualAnalysis = !manualAnalysis;
            notify(manualAnalysis ? "Режим без браузера включён" : "Режим без браузера выключен");
        }
        else if (cmd == "deepAnalyze") {
            if (manualAnalysis) throw std::runtime_error("Выключите режим без браузера для глубокого анализа");
            if (!httpUrl(url)) throw std::runtime_error("Выберите страницу для анализа");
            auto selected = session();
            if (!selected) throw std::runtime_error("Для глубокого анализа подключите расширение и откройте страницу с медиа в браузере");
            { std::lock_guard lock(selected->mutex); if (selected->tab < 0 || selected->url != url) throw std::runtime_error("Выберите открытую вкладку браузера и повторите анализ"); }
            analyze(url, true);
            action("scan");
        }
        else if (cmd == "cancelAnalysis")
            { *analysisCancel = true; ++analysisGeneration; busy = false; }
        else if (cmd == "clearMedia") {
            *analysisCancel = true; ++analysisGeneration; busy = false; scanning = false;
            media.clear(); mediaSessions.clear(); mediaSources.clear(); clearMediaAccess(); previews.clear(); ++previewGeneration; analysis = {}; selectedMedia.clear(); audioIndex = -1;
            auto id = nextScanGeneration();
            if (auto s = session()) work([s, id] { try { s->send({{"type", "scanStop"}, {"scanId", id}}); } catch (...) {} });
            notify("Список очищен");
        }
        else if (cmd == "enqueueAssembly") enqueueAssembly();
        else if (cmd == "importAssemblyFile") importAssemblyFile();
        else if (cmd == "assemblePartial" || cmd == "retryAssemblySources") {
            for (const auto &j : jobs) if (j->id == selectedJob && j->assembly) {
                requireAvailableCodecs(j);
                if (cmd == "assemblePartial") resolveAssembly(j,true);
                else {
                    std::lock_guard queueLock(jobsMutex);
                    for (const auto &s : j->sources) for (const auto &child : jobs)
                        if (child->id == s.dependency && child->done && child->state != "Готово") {
                            child->cancel=false; child->done=false; child->claimed=false; child->state="Ожидание";
                        }
                    for(auto &source:j->sources)source.enabled=true;
                    j->decisionNeeded=false; j->prompted=false; j->waitingSources=true; j->state="Ожидание источников"; jobsCv.notify_all();
                }
                break;
            }
            saveHistory();
        }
        else if (cmd == "enqueue")
            enqueue();
        else if (cmd == "exit")
            closeRequest();
        else if (cmd == "openBrowser") {
            if (browserIndex < int(browsers.size()))
                openPath(wide(browsers[browserIndex].exe));
        } else if (cmd == "customBrowser") {
            work([this] {
                auto path = pickFile(hwnd);
                if (path.empty())
                    return;
                auto p = fs::path(wide(path));
                if (p.extension() != L".exe")
                    throw std::runtime_error("Выберите EXE браузера");
                std::wstring file = p.filename();
                std::transform(file.begin(), file.end(), file.begin(), ::towlower);
                Browser b{utf8(p.stem()),
                          file.find(L"firefox") != std::wstring::npos ? "firefox" : "chromium",
                          path,
                          false,
                          {}};
                post([this, b = std::move(b)]() mutable {
                    customBrowserPaths.insert(b.exe);
                    browsers.push_back(std::move(b));
                    browserIndex = int(browsers.size()) - 1;
                    profileIndex = 0;
                    notify("Браузер добавлен. Укажите папку профиля при необходимости.");
                });
            });
        } else if (cmd == "customProfile") {
            if (browserIndex >= int(browsers.size()))
                throw std::runtime_error("Сначала выберите браузер");
            auto exe = browsers[browserIndex].exe;
            work([this, exe] {
                auto path = pickFile(hwnd, true);
                if (path.empty())
                    return;
                post([this, exe, path] {
                    for (auto &b : browsers)
                        if (b.exe == exe) {
                            b.profiles.push_back(
                                {"Выбран вручную", path, "Подключение подтверждается расширением"});
                            profileIndex = int(b.profiles.size()) - 1;
                            break;
                        }
                });
            });
        } else if (cmd == "prepareConnector") {
            if (preparingInstaller) return;
            if (extensionInstaller.active()) {
                extensionInstaller.start(hwnd, {}, nullptr, {}, reduced);
                return;
            }
            installerProfileChallenge.reset();
            installerProfileSession.reset();
            installerProfilePing.clear();
            installerProfileUrl.clear();
            installerConnectionPending = false;
            Browser selected;
            if (browserIndex >= 0 && browserIndex < int(browsers.size())) selected = browsers[browserIndex];
            installerBrowserExecutable = selected.exe;
            std::optional<Profile> profile;
            if (profileIndex >= 0 && profileIndex < int(selected.profiles.size())) profile = selected.profiles[profileIndex];
            installerProfileKey = {guideIdentityPath(selected.exe), profile ? guideIdentityPath(profile->path) : std::wstring()};
            if (!installerProfileKey.first.empty() && !installerProfileKey.second.empty()) {
                auto found = installerVerifiedProfiles.find(installerProfileKey);
                if (found != installerVerifiedProfiles.end()) {
                    auto existing = found->second.lock();
                    const auto now = GetTickCount64();
                    const auto pong = existing ? existing->lastPong.load() : 0;
                    std::string actual;
                    if (existing) { std::lock_guard lock(existing->mutex); actual = existing->browserExecutable; }
                    if (existing && existing->alive && existing->verified && existing->setupReady &&
                        existing->languageSupported && pong && now >= pong && now - pong <= 5000 &&
                        sameBrowserExecutable(actual, selected.exe)) {
                        notify("Расширение установлено и подключено");
                        return;
                    }
                    // Keep an established profile identity through a transient
                    // ping or permissions refresh. It is usable only after the
                    // readiness checks above pass again; a dead session cannot
                    // identify a future connection.
                    if (!existing || !existing->alive || !sameBrowserExecutable(actual, selected.exe))
                        installerVerifiedProfiles.erase(found);
                }
            }
            for (auto &existing : sessions) {
                std::string actual; { std::lock_guard lock(existing->mutex); actual = existing->browserExecutable; }
                if ((!profile || profile->path.empty()) && existing->alive && existing->verified && existing->setupReady && existing->languageSupported && sameBrowserExecutable(actual, selected.exe)) {
                    notify("Расширение установлено и подключено"); return;
                }
            }
            // Validate before replacing the native host or opening any browser.
            auto kind = guidedBrowser(selected);
            if (kind != GuidedBrowser::None) extensionLaunchArguments(kind, profile ? &*profile : nullptr);
            preparingInstaller = true;
            work([this, selected, profile] {
                try {
                    installConnector(executable());
                }
                catch (...) { post([this] { preparingInstaller = false; }); throw; }
                post([this, selected, profile] {
                    preparingInstaller = false;
                    try {
                    auto folder = executable().parent_path() / L"extensions" /
                        (selected.family == "firefox" ? L"firefox-signed" : L"chromium-development");
                    auto signedXpi = folder / SignedFirefoxConnectorFilename;
                    if (guidedBrowser(selected) != GuidedBrowser::None) {
                        auto guideTarget = guidedBrowser(selected) == GuidedBrowser::Firefox ? signedXpi : folder;
                        if (guidedBrowser(selected) == GuidedBrowser::Firefox && !fs::is_regular_file(guideTarget)) throw std::runtime_error("Подписанный файл расширения Firefox версии 1.0.6 не найден. После подписи Mozilla сохраните XPI как extensions/firefox-signed/StreamShelf-Connector-1.0.6.xpi рядом с программой.");
                        extensionInstaller.start(hwnd, selected, profile ? &*profile : nullptr, guideTarget, reduced, light);
                        auto checkConnection = [this, expected = selected.exe] {
                            if (installerConnectionPending) return;
                            if (extensionInstaller.hasExplicitProfile()) {
                                if (auto bound = installerProfileSession.lock()) {
                                    if (bound->alive && !installerProfilePing.empty()) {
                                        auto ping = installerProfilePing;
                                        work([bound, ping] { try { bound->verifyConnection(ping); } catch (...) {} });
                                        return;
                                    }
                                    installerProfileSession.reset();
                                    installerProfilePing.clear();
                                    installerProfileUrl.clear();
                                }
                                if (installerProfileChallenge && GetTickCount64() - installerProfileChallenge->started < 15000) return;
                                if (installerProfileChallenge)
                                    extensionInstaller.finishProfileVerification(wide(installerProfileChallenge->url));
                                installerProfileChallenge.reset();
                                bool candidateConnected = false;
                                for (const auto &candidate : sessions) {
                                    std::string actual; { std::lock_guard lock(candidate->mutex); actual = candidate->browserExecutable; }
                                    if (candidate->alive && sameBrowserExecutable(actual, expected)) { candidateConnected = true; break; }
                                }
                                if (!candidateConnected) return;
                                try {
                                    auto challenge = std::make_shared<GuideProfileChallenge>(GuideProbePurpose::verifyConnector,
                                        interfaceLanguage == "en", light, reduced);
                                    installerProfileChallenge = challenge;
                                    if (!extensionInstaller.beginProfileVerification(wide(challenge->url)))
                                        installerProfileChallenge.reset();
                                    else {
                                        // Keep exact URLs after cleanup: browser events can
                                        // arrive after the guide has already closed.
                                        if (installerProbeUrls.size() >= 16) installerProbeUrls.erase(installerProbeUrls.begin());
                                        installerProbeUrls.insert(challenge->url);
                                    }
                                } catch (const std::exception &e) { log(e.what()); }
                                return;
                            }
                            for (auto &existing : sessions) {
                                std::string actual; { std::lock_guard lock(existing->mutex); actual = existing->browserExecutable; }
                                if (existing->alive && sameBrowserExecutable(actual, expected)) {
                                    if (existing->verified && existing->setupReady && existing->languageSupported && extensionInstaller.active()) {
                                        extensionInstaller.connectionEstablished();
                                        notify("Расширение установлено и подключено");
                                        requestedPage = 0;
                                        return;
                                    }
                                    work([existing] { try { existing->verifyConnection(); } catch (...) {} });
                                }
                            }
                        };
                        extensionInstaller.setRetryConnection(checkConnection);
                        extensionInstaller.setOpenSetup([this, expected = selected.exe] {
                            if (extensionInstaller.hasExplicitProfile()) {
                                auto bound = installerProfileSession.lock();
                                if (bound && bound->alive && bound->verified && bound->setupApiSupported && !bound->setupReady) {
                                    work([bound] { try { bound->send({{"type", "openSetup"}}); } catch (...) {} });
                                    return true;
                                }
                                return false;
                            }
                            for (auto &existing : sessions) {
                                std::string actual; { std::lock_guard lock(existing->mutex); actual = existing->browserExecutable; }
                                if (existing->alive && existing->verified && existing->setupApiSupported && !existing->setupReady && sameBrowserExecutable(actual, expected)) {
                                    work([existing] { try { existing->send({{"type", "openSetup"}}); } catch (...) {} });
                                    return true;
                                }
                            }
                            return false;
                        });
                        checkConnection();
                        notify("Мастер установки открыт. Следуйте подсказкам поверх браузера.");
                    } else {
                        auto signedDir = executable().parent_path() / L"extensions" / L"firefox-signed";
                        bool signedAvailable = selected.family == "firefox" && fs::is_regular_file(signedXpi);
                        auto developmentFolder = selected.family == "firefox" ? executable().parent_path() / L"extensions" / L"firefox-development" : folder;
                        openPath(signedAvailable ? signedDir : developmentFolder);
                        modalTitle = "Ручная установка расширения";
                        const bool english = interfaceLanguage == "en";
                        modalBody = english ? "The highlighted guide is available for Google Chrome and Yandex Browser.\n\n" : "Мастер с подсветкой доступен только для Google Chrome и Яндекс Браузера.\n\n";
                        if (selected.family == "firefox") {
                            modalBody += signedAvailable
                                ? (english ? "Firefox: open about:addons, select the gear, choose Install Add-on From File, and select the signed XPI. Confirm installation and permissions in the browser.\n\n" : "Firefox: откройте about:addons, выберите шестерёнку → «Установить дополнение из файла» и укажите подписанный файл XPI. Подтвердите установку и разрешения в браузере.\n\n")
                                : (english ? "The signed Firefox file is not included. For temporary development installation, open about:debugging#/runtime/this-firefox, choose Load Temporary Add-on, and select manifest.json. This lasts until Firefox restarts.\n\n" : "Подписанный файл Firefox не найден в комплекте. Временная установка для разработки: about:debugging#/runtime/this-firefox → Загрузить временное дополнение → manifest.json; она действует до перезапуска.\n\n");
                        } else {
                            modalBody += english ? "Open the browser extensions page, enable developer mode, and choose Load unpacked.\n\n" : "Откройте страницу расширений выбранного браузера, включите режим разработчика и выберите «Загрузить распакованное расширение».\n\n";
                        }
                        modalBody += (english ? "Path: " : "Путь: ") + utf8((signedAvailable ? signedXpi : developmentFolder).wstring()) +
                            (english ? "\n\nAfter installation, open StreamShelf Connector from the extensions menu and connect it." : "\n\nПосле установки откройте StreamShelf Connector в меню расширений и подключите расширение.");
                        modalConfirm = nullptr;
                    }
                    } catch (const std::exception &e) { error(e.what()); }
                });
            });
        } else if (cmd == "extensionFolder")
            openPath(browserIndex < int(browsers.size()) && browsers[browserIndex].family == "firefox"
                         ? executable().parent_path() / L"extensions" / L"firefox-signed"
                         : executable().parent_path() / L"extensions" / L"chromium-development");
        else if (cmd == "scan" || cmd == "reloadScan" || cmd == "stopScan") {
            auto s = session();
            if (!s)
                throw std::runtime_error("Откройте страницу с медиа в браузере и вернитесь в StreamShelf");
            int tab;
            {
                std::lock_guard l(s->mutex);
                tab = s->tab;
            }
            auto id = nextScanGeneration();
            if(cmd!="stopScan") {
                std::lock_guard lock(s->mutex);
                s->credentialScanId=id;
            }
            scanning = false;
            auto command = cmd == "stopScan" ? Json{{"type", "scanStop"}, {"scanId", id}}
                : Json{{"type", "scanStart"}, {"scanId", id}, {"tabId", tab}, {"reload", cmd == "reloadScan"}};
            scanCommandPending = true;
            notify("Отправка команды расширению…");
            work([this, s, command = std::move(command)] {
                try { s->send(command); }
                catch (...) { post([this] { scanCommandPending = false; }); throw; }
                post([this] { scanCommandPending = false; });
            });
        } else if (cmd == "useTab") {
            if (auto s = session()) {
                std::lock_guard l(s->mutex);
                url = s->url;
                requestedPage = 0;
            }
        } else if (cmd == "json") {
            modalTitle = "JSON анализа · содержит временные адреса";
            modalBody = analysis.raw.is_null() ? "Сначала выполните анализ ссылки" : analysis.raw.dump(2);
            modalConfirm = nullptr;
        } else if (cmd == "analyzeResource") {
            for (auto &m : media)
                if (m.key == selectedMedia) {
                    url = m.url;
                    analyze(url);
                    break;
                }
        } else if (cmd == "folder" || cmd == "importCookies" || cmd == "exportLogs") {
            std::string text;
            for (const auto &line : logs)
                text += displayLog(line, interfaceLanguage) + "\r\n";
            work([this, cmd, text] {
                auto path = pickFile(hwnd, cmd == "folder", cmd == "exportLogs");
                if (path.empty())
                    return;
                if (cmd == "importCookies") {
                    auto content = validateCookies(readFile(wide(path), 4 * 1024 * 1024));
                    protectFile(dataRoot() / L"cookies.dpapi", content);
                    post([this] {
                        sessionIndex = -1;
                        analysisSession = nullptr;
                        media.clear();
                        mediaSessions.clear();
                        mediaSources.clear();
                        clearMediaAccess();
                        previews.clear(); ++previewGeneration;
                        notify("Cookies импортированы и защищены DPAPI");
                    });
                } else if (cmd == "exportLogs") {
                    writeFile(wide(path), text);
                    post([this] { notify("Журнал сохранён"); });
                } else
                    post([this, path] { destination = path; });
            });
        } else if (cmd == "deleteCookies") {
            fs::remove(dataRoot() / L"cookies.dpapi");
            notify("Импортированные cookies удалены");
        } else if (cmd == "update")
            scanToolComponents(false);
        else if (cmd == "installComponents")
            scanToolComponents(true);
        else if (cmd == "installBundledComponents") {
            if (componentsBusy) return;
            componentsBusy=true;
            ++componentsRevision;
            componentProgressVisible=false;
            componentsActivity="Проверяем компоненты из комплекта…";
            const auto pending=components;
            work([this,pending] {
                std::string problem;
                size_t installed=0;
                std::vector<Component> result;
                try {
                    std::unique_lock lock(toolsMutex);
                    const auto root=dataRoot()/L"tools";
                    installed=ss::installBundledComponents(root,executable().parent_path()/L"bundled-tools",stopping);
                    result=refreshInstalledComponents(root,pending,stopping);
                } catch(const std::exception &e) { problem=e.what(); }
                post([this,result=std::move(result),problem,installed] {
                    componentsBusy=false;
                    if(!result.empty()) { components=result; componentsScanned=true; }
                    detectVideoEncoders();
                    if(componentsDirty) { componentsDirty=false; if(componentsScanned)refreshToolFiles(); }
                    if(!problem.empty())error("Не удалось установить компоненты из комплекта: "+interfaceText(interfaceLanguage,problem));
                    else {
                        if(!components.empty() && !components[0].installed.empty())status="yt-dlp · локальная версия готова";
                        notify(installed?"Компоненты из комплекта установлены":"Все компоненты из комплекта уже установлены");
                    }
                });
            });
        }
        else if (cmd == "dataFolder") {
            auto folder = dataRoot() / L"tools";
            fs::create_directories(folder);
            openPath(folder);
        }
        else if (cmd == "copyLogs") {
            std::string text;
            for (const auto &line : logs) {
                auto displayed = displayLog(line, interfaceLanguage);
                if (logSearch.empty() || displayed.find(logSearch) != displayed.npos) text += displayed + "\r\n";
            }
            copyText(text);
            notify("Журнал скопирован");
        } else if (cmd == "addLocalDownloads") {
            work([this]{auto path=pickFile(hwnd);if(!path.empty())post([this,path]{addLocalFiles({path});});});
        } else if (cmd == "interruptTerminal") {
            if(terminalCancel) *terminalCancel=true;
        } else if (cmd == "runTerminal") {
            if(terminalCancel) {notify("Дождитесь завершения команды или нажмите Ctrl+C");return;}
            auto command=terminalCommand;terminalCommand.clear();
            auto normalized=command;
            auto firstNonspace=normalized.find_first_not_of(" \t");
            if(firstNonspace==std::string::npos)normalized.clear();
            else {normalized.erase(0,firstNonspace);auto last=normalized.find_last_not_of(" \t");normalized.resize(last+1);}
            std::transform(normalized.begin(),normalized.end(),normalized.begin(),[](unsigned char c){return char(std::tolower(c));});
            if(normalized=="clear"||normalized=="cls"||normalized=="/clear")logs.clear();
            else if(normalized=="/help") {
                log("> " + command, false);
                log("/versions — версии компонентов; /clear — очистить журнал. Отдельно: yt-dlp --version, ffmpeg -version, ffprobe -version, deno --version.");
            } else if(normalized=="/versions") {
                log("> " + command, false);
                work([this] {
                    std::vector<Component> versions;
                    { std::shared_lock lock(toolsMutex); versions=refreshInstalledComponents(dataRoot()/L"tools",{},stopping); }
                    for(const auto &c:versions) log(c.name + ": " + (c.installed.empty()?"не установлен или не запускается":c.installed));
                });
            } else if(!normalized.empty()){
                auto cancel=std::make_shared<std::atomic_bool>(false);
                terminalCancel=cancel;
                work([this,command,cancel]{
                    auto finished=[this]{post([this]{terminalCancel.reset();});};
                    try {
                        wchar_t shell[MAX_PATH]{};
                        if(!GetSystemDirectoryW(shell,MAX_PATH))throw std::runtime_error("Не найден системный командный процессор");
                        auto exe=fs::path(shell)/L"cmd.exe";log("> "+command, false);
                        auto resolved = terminalComponentCommand(wide(command), dataRoot()/L"tools");
                        auto result=runProcess(exe,{L"/d",L"/s",L"/c",L"chcp 65001>nul & " + resolved},
                            [this](const std::string &line){log(line, false);},
                            *cancel,true,true,dataRoot()/L"tools");
                        log(*cancel?"Команда прервана":"Команда завершена: код "+std::to_string(result.code));
                    } catch (const std::exception &e) { log(std::string("Ошибка команды: ") + e.what()); finished(); throw; }
                    catch (...) { finished(); throw; }
                    finished();
                });
            }
        } else if (cmd == "clearLogs")
            logs.clear();
        else if (cmd == "selectAllJobs") {
            selectedJobs.clear();
            for (const auto &j : jobs) selectedJobs.insert(j->id);
        } else if (cmd == "clearSelectedJobs" || cmd == "clearFinishedJobs" || cmd == "removeJob" || cmd == "cancelJob" || cmd == "cancelSelectedJobs") {
            size_t removed=0, cancelled=0;
            const bool cancelOnly=cmd=="cancelJob" || cmd=="cancelSelectedJobs";
            {
                std::lock_guard lock(jobsMutex);
                std::set<std::string> targets;
                for(const auto &job:jobs) {
                    const bool terminal=job->done && (job->state=="Готово" || job->state=="Ошибка" ||
                        job->state=="Отменено" || job->state=="Ожидание повтора");
                    const bool chosen=(cmd=="removeJob" || cmd=="cancelJob")?job->id==selectedJob:
                        cancelOnly?(selectedJobs.empty()?job->id==selectedJob:selectedJobs.count(job->id)!=0):
                        cmd=="clearFinishedJobs"?terminal:selectedJobs.count(job->id)!=0;
                    if(chosen)targets.insert(job->id);
                }
                std::set<std::string> retainedDependencies;
                for(const auto &job:jobs)if(!targets.count(job->id) && !job->done)
                    for(const auto &source:job->sources)if(!source.dependency.empty())
                        retainedDependencies.insert(source.dependency);
                std::set<std::string> stop=targets;
                for(const auto &job:jobs)if(targets.count(job->id) && job->assembly && !job->done)
                    for(const auto &source:job->sources)
                        if(!source.dependency.empty() && !retainedDependencies.count(source.dependency))
                            stop.insert(source.dependency);
                for(const auto &job:jobs)if(stop.count(job->id) && !job->done){
                    job->cancel=true;
                    ++cancelled;
                    if(!job->claimed){
                        job->done=true;
                        job->state="Отменено";
                    }
                }
                if(!cancelOnly){
                    for(const auto &child:jobs)if(targets.count(child->id)){
                        try { discardDownloadAccess(*child, true); }
                        catch (...) { log("Не удалось удалить сохранённый доступ загрузки."); }
                        for(const auto &parent:jobs)if(!targets.count(parent->id) && !parent->done && parent->waitingSources)
                            for(auto &source:parent->sources)if(source.dependency==child->id){
                                if(child->state=="Готово" && !child->output.empty() && fs::is_regular_file(wide(child->output)))
                                    source.path=child->output;
                                else source.path.clear();
                                source.dependency.clear();
                            }
                        selectedJobs.erase(child->id);
                        if(selectedJob==child->id)selectedJob.clear();
                    }
                    removed=std::erase_if(jobs,[&](const auto &job){return targets.count(job->id)!=0;});
                }
            }
            jobsCv.notify_all();
            saveHistory();
            if(cancelOnly)notify(cancelled?"Остановка задач: "+std::to_string(cancelled):"Нет активных или ожидающих задач");
            else notify(removed?"Удалено записей: "+std::to_string(removed)+". Файлы сохранены.":"Нет записей для удаления");
        } else if (cmd == "retryJob" || cmd == "openJobFolder" || cmd == "openJobFile") {
            for (auto &j : jobs)
                if (j->id == selectedJob) {
                    if (cmd == "retryJob" && j->assembly && j->done) {
                        auto saved=fs::path(wide(j->folder))/(L"result."+wide(muxContainer(j->muxOptions)));
                        bool reuse=j->state!="Готово" && !j->output.empty() && fs::path(wide(j->output))==saved && fs::is_regular_file(saved);
                        if (!reuse) requireAvailableCodecs(j);
                        std::lock_guard queueLock(jobsMutex);
                        j->waitingSources=!reuse; j->cancel=false; j->done=false; j->claimed=false; j->decisionNeeded=false; j->prompted=false;
                        j->state=reuse?"Ожидание":"Ожидание источников"; for(auto &source:j->sources)source.enabled=true;
                        jobsCv.notify_all();
                    } else if (cmd == "retryJob" && j->done &&
                               (j->state == "Ошибка" || j->state == "Отменено" ||
                                j->state == "Ожидание повтора")) {
                        std::lock_guard queueLock(jobsMutex);
                        j->cancel = false;
                        j->done = false;
                        j->claimed = false;
                        j->state = "Ожидание";
                        jobsCv.notify_all();
                    } else if (cmd == "openJobFolder") {
                        if (j->state != "Готово" && !j->output.empty() && fs::is_regular_file(wide(j->output)))
                            openPath(fs::path(wide(j->output)).parent_path().wstring());
                        else if(j->state != "Готово" && fs::is_directory(wide(j->folder))) openPath(wide(j->folder));
                        else openPath(wide(j->outputDirectory.empty() ? j->folder : j->outputDirectory));
                    }
                    else if (cmd == "openJobFile" && j->state == "Готово") {
                        if (!j->output.empty() && fs::is_regular_file(wide(j->output))) openPath(wide(j->output));
                        else notify("Итоговый файл не найден. Откройте папку загрузки.");
                    }
                    break;
                }
            saveHistory();
        } else if (cmd == "unregisterConnector") {
            unregisterConnector();
            notify("Регистрация Native Messaging удалена");
        }
    } catch (const std::exception &e) {
        error(e.what());
    }
}
} // namespace ss
