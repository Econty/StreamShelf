#include "core.hpp"
#include "media_merge.hpp"
#include "media_display.hpp"
#include "mux_options.hpp"
#include "assembly_source.hpp"
#include "firefox_guide_state.hpp"
#include "terminal_selection.hpp"
#include <iostream>
#include <stdexcept>
using namespace ss;
static int passed = 0, failed = 0;
void check(bool v, const std::string &s) {
    if (!v)
        throw std::runtime_error(s);
}
template <typename F> void test(const char *name, F f) {
    try {
        f();
        std::cout << "PASS " << name << '\n';
        passed++;
    } catch (const std::exception &e) {
        std::cout << "FAIL " << name << ": " << e.what() << '\n';
        failed++;
    }
}
template <typename F> void rejects(F f) {
    bool threw = false;
    try {
        f();
    } catch (...) {
        threw = true;
    }
    check(threw, "Expected rejection");
}
int main() {
    test("Terminal selection survives scrolling and follows the pointer", [] {
        check(preserveTerminalSelectionOnWheel(true,true,true), "wheel preserves active log selection");
        check(!preserveTerminalSelectionOnWheel(true,false,true) && !preserveTerminalSelectionOnWheel(false,true,true), "ordinary wheel input releases capture");
        check(terminalSelectionScroll(105,100,400,.05f)<0, "drag near top scrolls up");
        check(terminalSelectionScroll(495,100,400,.05f)>0, "drag near bottom scrolls down");
        check(terminalSelectionScroll(300,100,400,.05f)==0, "drag in middle does not scroll");
    });
    test("Data sources retain their own analysis snapshot", [] {
        auto first = parseAnalysis(Json{{"title", "First"}, {"description", "first description"}, {"chapters", Json::array({Json{{"title", "first chapter"}}})}}, "https://example.com/first");
        auto second = parseAnalysis(Json{{"title", "Second"}, {"description", "second description"}, {"chapters", Json::array({Json{{"title", "second chapter"}}})}}, "https://example.com/second");
        for (const auto &item : first.items) if (item.category == "Данные") {
            check(str(item.data, "description") == "first description", "description belongs to first analysis");
            check(str(item.data["chapters"][0], "title") == "first chapter", "chapters belong to first analysis");
            if (item.id == "description") check(dataSourceContent(item) == "first description", "description exported as plain text");
            if (item.id == "chapters") check(str(Json::parse(dataSourceContent(item))[0], "title") == "first chapter", "correct chapters exported");
            if (item.id == "metadata") check(str(Json::parse(dataSourceContent(item)), "title") == "First", "correct metadata exported");
        }
        check(str(second.items[0].data, "description") == "second description", "second analysis has independent snapshot");
    });
    test("Firefox guide follows current evidence and requires a verified connection", [] {
        FirefoxGuideObservation o;
        check(firefoxGuideStage(o) == FirefoxGuideStage::OpenManager, "unknown page");
        o.manager = true; check(firefoxGuideStage(o) == FirefoxGuideStage::OpenMenu, "open menu");
        o.menu = true; check(firefoxGuideStage(o) == FirefoxGuideStage::InstallFile, "select menu item");
        o.picker = true; check(firefoxGuideStage(o) == FirefoxGuideStage::SelectFile, "select signed file");
        o.installPrompt = true; check(firefoxGuideStage(o) == FirefoxGuideStage::ConfirmInstall, "confirm browser prompt");
        o.installPrompt = o.picker = false; o.awaitingInstall = true;
        check(firefoxGuideStage(o) == FirefoxGuideStage::ConfirmInstall, "picker closing waits for the browser instead of restarting");
        o.awaitingInstall = false; o.addedNotice = true; o.installedKnown = o.installed = o.enabledKnown = true;
        check(firefoxGuideStage(o) == FirefoxGuideStage::ConfirmInstall, "acknowledge added notification before enabling");
        o.addedNotice = false; o.enabledKnown = false;
        check(firefoxGuideStage(o) == FirefoxGuideStage::Enable, "unknown switch state asks to check enablement");
        o.installPrompt = o.picker = o.menu = false; o.installedKnown = o.installed = o.enabledKnown = true;
        check(firefoxGuideStage(o) == FirefoxGuideStage::Enable, "resume disabled add-on");
        check(firefoxCardSwitch(458, 413, 700, 435, 1109, 412, 1149, 432, 96), "Firefox card switch beside the title");
        check(!firefoxCardSwitch(458, 413, 700, 435, 1109, 520, 1149, 540, 96), "neighbouring row is not the switch");
        o.enabled = true; check(firefoxGuideStage(o) == FirefoxGuideStage::Connect, "enabled alone is not success");
        o.connectionConfirmed = true; check(firefoxGuideStage(o) == FirefoxGuideStage::Done, "verified connection");
        o.connectionConfirmed = o.installed = o.enabled = false;
        check(firefoxGuideStage(o) == FirefoxGuideStage::OpenMenu, "removed add-on returns to installation");
    });
    test("Mux settings and chapters preserve safe arguments and chapter timing", [] {
        MuxOptions o; check(muxContainer(o) == "mkv", "default container");
        auto args = muxCodecArgs(o); check(args.size() == 6 && args[1] == L"copy", "copy streams by default");
        o.container = 3; o.video = 3; o.audio = 2; o.subtitles = 3;
        args = muxCodecArgs(o); check(muxContainer(o) == "webm" && args[1] == L"libvpx-vp9" && args[3] == L"libopus" && args[5] == L"webvtt", "explicit codec selection");
        o.audio = 100; rejects([&] { muxCodecArgs(o); });
        auto chapters = Json::array({{{"start_time", 0.25}, {"end_time", 1.5}, {"title", "a=b;#\\c\nnext"}}});
        auto text = chaptersToFfmetadata(chapters);
        check(text.find("START=250\nEND=1500") != std::string::npos, "chapter milliseconds");
        check(text.find("a\\=b\\;\\#\\\\c next") != std::string::npos, "escape chapter title");
        rejects([&] { chaptersToFfmetadata(Json::array({{{"start_time", 2}, {"end_time", 1}}})); });
        auto flacChapters=Json::array({{{"start_time",0.0},{"end_time",1.0},{"title","First"}},{{"start_time",1.0},{"end_time",2.0},{"title","Second"}}});
        auto tags=flacChapterTags(flacChapters,2.0);
        check(tags.size()==4 && tags[0].first=="CHAPTER000" && tags[0].second=="00:00:00.000" && tags[3].second=="Second","FLAC chapter times and names");
        auto gap=flacChapters;gap[1]["start_time"]=1.1;rejects([&]{flacChapterTags(gap,2.0);});
        rejects([&]{flacChapterTags(flacChapters,3.0);});
        auto normalized=chaptersFromProbe(Json{{"chapters",Json::array({{{"start_time","0.250"},{"end_time","1.500"},{"tags",{{"title","Intro"}}}}})}});
        check(normalized[0]["start_time"]==0.25 && normalized[0]["title"]=="Intro", "probe chapters converted to JSON");
        check(chaptersToFfmetadata(normalized).find("START=250\nEND=1500")!=std::string::npos,"probe chapter round trip");
        rejects([&] { chaptersFromProbe(Json{{"chapters",Json::array({{{"start_time","broken"},{"end_time","1"}}})}}); });

    });
    test("Constructor rejects known incompatible codec choices", [] {
        AssemblySource track;track.role=0;track.codec=1;
        check(!assemblyCodecConflict(3,track).empty(),"WebM rejects H.264");
        track.codec=3;check(assemblyCodecConflict(3,track).empty(),"WebM accepts VP9");
        track.role=1;track.codec=1;check(!assemblyCodecConflict(3,track).empty(),"WebM rejects AAC");
        track.codec=2;check(assemblyCodecConflict(3,track).empty(),"WebM accepts Opus");
        track.role=2;track.codec=1;check(!assemblyCodecConflict(1,track).empty(),"MP4 rejects SRT");
        track.codec=2;check(assemblyCodecConflict(1,track).empty(),"MP4 accepts mov_text");
        track.codec=0;check(assemblyCodecConflict(3,track).empty(),"unknown copied stream requires probing");
        AssemblySource image;image.role=3;image.exportFormat=2;image.exportQuality=0;
        AssemblySource subtitles;subtitles.role=2;subtitles.exportFormat=1;subtitles.exportQuality=2;
        auto restoredImage=assemblyFrom(assemblyJson(image)), restoredSubs=assemblyFrom(assemblyJson(subtitles));
        check(restoredImage.exportFormat==2 && restoredImage.exportQuality==0 && restoredSubs.exportFormat==1 && restoredSubs.exportQuality==2,"each source retains its own export settings");
        AssemblySource metadata;metadata.metadataEdit=true;metadata.metadataEditTitle="New";metadata.metadataEditArtist="Author";metadata.metadataEditDescription="Notes";
        auto changed=Json::parse(editedMetadataJson(R"({"title":"Old","other":{"keep":7}})",metadata));
        check(changed["title"]=="New" && changed["artist"]=="Author" && changed["other"]["keep"]==7,"metadata edits preserve other fields");
        auto restoredMetadata=assemblyFrom(assemblyJson(metadata));
        check(restoredMetadata.metadataEdit && restoredMetadata.metadataEditTitle=="New", "metadata edits persist with source");
        rejects([&] {editedMetadataJson("not json",metadata);});
        stageMetadataField(metadata,"extra",R"({"nested":[1,true,null]})",false);
        stageMetadataField(metadata,"other","",true);
        auto advanced=Json::parse(editedMetadataJson(R"({"title":"Old","other":5})",metadata));
        check(!advanced.contains("other") && advanced["extra"]["nested"][1]==true,"arbitrary JSON values and deletion");
        auto persisted=assemblyFrom(assemblyJson(metadata));
        check(persisted.metadataOverrides.at("extra")["nested"][0]==1 && persisted.metadataRemoved.size()==1,"metadata changes persist");
        stageMetadataField(metadata,"other","42",false);
        check(metadata.metadataRemoved.empty() && metadata.metadataOverrides.at("other")==42,"setting a removed field cancels deletion");
        rejects([&] {stageMetadataField(metadata,"bad","not-json",false);});
        rejects([&] {stageMetadataField(metadata,"title","42",false);});
        AssemblySource unchanged;auto noFields=Json::parse(editedMetadataJson(R"({"keep":1})",unchanged));
        check(noFields.size()==1 && !noFields.contains("title"),"unedited missing fields remain absent");
        AssemblySource nested;
        stageMetadataField(nested,"/obj/a~1b/1","42",false);
        stageMetadataField(nested,"/obj/a~1b/0","",true);
        auto nestedResult=Json::parse(editedMetadataJson(R"({"obj":{"a/b":[1,2]}})",nested));
        check(nestedResult["obj"]["a/b"].size()==1 && nestedResult["obj"]["a/b"][0]==42,"nested array edits preserve operation order");
        auto nestedRestored=assemblyFrom(assemblyJson(nested));
        check(nestedRestored.metadataPathEdits.size()==2,"nested edits survive history serialization");
        auto paths=makeMetadataTree(nestedResult);
        check(std::any_of(paths.begin(),paths.end(),[](const auto &v){return v.path=="/obj/a~1b/0" && v.value=="42";}),"tree escapes JSON pointer keys and reaches array values");
        stageMetadataField(nested,"/obj/missing/child","1",false);
        rejects([&] {editedMetadataJson(R"({"obj":{"a/b":[1,2]}})",nested);});
        check(!assemblyCopiedCodecConflict(3,0,"h264").empty(),"copied H.264 rejected by WebM");
        check(assemblyCopiedCodecConflict(3,0,"vp9").empty(),"copied VP9 accepted by WebM");
        check(!assemblyCopiedCodecConflict(1,2,"subrip").empty(),"copied SRT rejected by MP4");
        check(!assemblyCopiedCodecConflict(5,1,"mp3").empty(),"M4A rejects MP3 copy under AAC policy");
        check(assemblyCopiedCodecConflict(5,1,"aac").empty(),"M4A accepts AAC copy");
        check(assemblyCopiedCodecConflict(5,1,"mp4a.40.2").empty(),"unrecognized codec alias waits for probing");
        check(isBitmapSubtitleCodec("hdmv_pgs_subtitle") && !isBitmapSubtitleCodec("subrip"),"bitmap subtitles need recognition before text conversion");
    });
    test("Original subtitle export selects a compatible format", [] {
        check(originalSubtitleExtension("subrip")==L".srt", "SubRip remains SubRip");
        check(originalSubtitleExtension("ass")==L".ass", "ASS remains ASS");
        check(originalSubtitleExtension("webvtt")==L".vtt", "WebVTT remains WebVTT");
        check(originalSubtitleExtension("hdmv_pgs_subtitle")==L".sup", "PGS remains bitmap subtitles");
        check(originalSubtitleExtension("dvd_subtitle")==L".mkv", "DVD subtitles use a compatible container");
        check(originalSubtitleExtension("mov_text")==L".mp4", "mov_text uses a compatible container");
        rejects([] { originalSubtitleExtension("unknown_codec"); });
        Json probe={{"streams",Json::array({
            {{"index",0},{"codec_type","audio"},{"codec_name","flac"}},
            {{"index",1},{"codec_type","subtitle"},{"codec_name","subrip"}},
            {{"index",4},{"codec_type","subtitle"},{"codec_name","ass"}}
        })}};
        auto selected=originalSubtitlePlan(probe,4);
        check(selected.stream==4 && selected.codec=="ass" && selected.extension==L".ass", "select the requested subtitle, not another stream");
        check(originalSubtitlePlan(probe,-1).stream==1, "choose the first subtitle when no stream index is specified");
        rejects([&] { originalSubtitlePlan(probe,0); });
        rejects([&] { originalSubtitlePlan(probe,2); });
    });
    test("Video filter includes silent and uncertain video without mixing audio", [] {
        Media m; m.category = "Видео без звука"; m.videoOnly = true;
        check(matchesMediaCategory(m, "Видео") && videoSoundLabel(m) == "Без звука", "silent video is visible and labelled");
        m.category = "Видео · звук не проверен"; m.videoOnly = false;
        check(matchesMediaCategory(m, "Видео") && videoSoundLabel(m) == "Звук не проверен", "uncertain audio stays uncertain");
        m.category = "Видео"; m.acodec = "aac";
        check(matchesMediaCategory(m, "Видео") && videoSoundLabel(m) == "Со звуком", "confirmed audio is labelled");
        m.category = "Аудио";
        check(!matchesMediaCategory(m, "Видео") && matchesMediaCategory(m, "Все категории"), "audio remains separate");
    });
    test("Resource merging retains format metadata and full URL identity", [] {
        Media format;
        format.url = "https://cdn.example/video?token=one&quality=1080";
        format.page = "https://site.example/watch/1";
        format.format = true;
        format.title = "1080p English";
        format.quality = "1080p";
        format.language = "en";
        format.acodec = "aac";
        Media network;
        network.url = format.url;
        network.page = format.page;
        network.title = "Video element";
        network.quality = "Network";
        network.thumbnail = "https://cdn.example/cover.jpg";
        network.size = 100;
        check(sameDiscoveredResource(format, network), "Same page and complete URL must match");
        mergeNetworkMetadata(format, network);
        check(format.title == "1080p English" && format.quality == "1080p" && format.language == "en" && format.acodec == "aac", "Preserve rendition data");
        check(format.thumbnail == network.thumbnail && format.size == 100, "Retain useful discovery data");
        network.url = "https://cdn.example/video?token=two&quality=1080";
        check(!sameDiscoveredResource(format, network), "Query parameters affect identity");
        network.url = format.url;
        network.page = "https://site.example/watch/2";
        check(!sameDiscoveredResource(format, network), "Different page context must not merge");
        network.page.clear();
        check(!sameDiscoveredResource(format, network), "Unknown page context must not merge");
    });
    test("Analysis keys retain page context", [] {
        Json data = {{"formats", Json::array({{{"format_id", "1"}, {"url", "https://cdn.example/stream"}, {"vcodec", "h264"}, {"acodec", "aac"}}})}};
        auto first = parseAnalysis(data, "https://site.example/one");
        auto second = parseAnalysis(data, "https://site.example/two");
        check(first.items[0].key != second.items[0].key, "Pages require distinct keys");
    });
    test("Spring settles at 30/60/120 FPS", [] {
        for (int fps : {30, 60, 120}) {
            Spring s;
            s.target = 1;
            for (int i = 0; i < fps * 3; i++)
                s.step(1.f / fps);
            check(std::abs(s.value - 1) < .001f, "Settlement");
        }
    });
    test("Spring reversal preserves value and velocity", [] {
        Spring s;
        s.target = 1;
        s.step(.05f);
        auto v = s.value, speed = s.velocity;
        s.target = 0;
        check(s.value == v && s.velocity == speed, "Discontinuity");
        s.step(.01f);
        check(std::isfinite(s.value), "Finite");
    });
    test("Long and invalid frame intervals do not explode", [] {
        Spring s;
        s.target = 100;
        s.step(300);
        s.step(NAN);
        check(std::isfinite(s.value) && s.value < 1000, "Finite");
    });
    test("Scroll bounds and content shrink", [] {
        Scroll s;
        s.bounds(1000, 300);
        s.wheel(-100);
        for (int i = 0; i < 300; i++)
            s.step(.016f);
        check(std::abs(s.p.value - 700) < .1f, "Bottom");
        s.bounds(10, 300);
        for (int i = 0; i < 300; i++)
            s.step(.016f);
        check(std::abs(s.p.value) < .1f, "Shrink");
    });
    test("Clip intersection rejects covered input", [] {
        auto r = Rect{0, 0, 100, 100}.intersect({90, 90, 100, 100});
        check(r.w == 10 && !r.contains(80, 80), "Clip");
    });
    test("Windows quote escapes empty argument", [] { check(quoteWide(L"") == L"\"\"", "Empty"); });
    test("Windows quote doubles trailing backslash",
         [] { check(quoteWide(L"C:\\my folder\\") == L"\"C:\\my folder\\\\\"", "Trailing slash"); });
    test("Windows quote escapes embedded quote",
         [] { check(quoteWide(L"a\"b") == L"\"a\\\"b\"", "Embedded quote"); });
    test("URLs accept HTTPS signed resources",
         [] { check(httpUrl("https://example.com/a.m3u8?token=x"), "Valid URL"); });
    test("URLs reject unsupported scheme and injected whitespace", [] {
        check(!httpUrl("file:///C:/x") && !httpUrl("https://a.com/\nx") &&
                  !httpUrl("https://user:pass@a.com/"),
              "Unsafe URL");
    });
    test("Secrets are removed from log text", [] {
        auto s = redact("Authorization: Bearer ABC\nhttps://x.test/video?token=SECRET");
        check(s.find("ABC") == s.npos && s.find("SECRET") == s.npos, "Leak");
        auto longLine=redact(std::string(65536,'x')+" https://example.test/file?token=LONGSECRET");
        check(longLine.find("LONGSECRET")==std::string::npos && longLine.size()>65536,"Long line redaction");
    });
    Json sample = {
        {"title", "Тест"},
        {"formats",
         Json::array({{{"format_id", "137"},
                       {"vcodec", "avc1"},
                       {"acodec", "none"},
                       {"filesize", 1200},
                       {"has_drm", nullptr}},
                      {{"format_id", "140"}, {"vcodec", "none"}, {"acodec", "aac"}},
                      {{"format_id", "m"}, {"vcodec", "none"}, {"acodec", "aac"}, {"has_drm", true}}})},
        {"subtitles", {{"ru", Json::array({{{"ext", "vtt"}, {"url", "https://a.test/sub"}}})}}},
        {"automatic_captions", {{"ru", Json::array({{{"ext", "vtt"}, {"url", "https://a.test/auto"}}})}}},
        {"thumbnails", Json::array({{{"url", "https://a.test/image"}, {"width", 100}, {"height", 50}}})},
        {"description", "Text"},
        {"chapters", Json::array({{{"title", "Chapter"}}})}};
    test("Formats distinguish mute video and audio", [&] {
        auto a = parseAnalysis(sample, "https://x.test");
        check(a.items[0].videoOnly && a.items[1].category == "Аудио", "Codecs");
        check(std::none_of(a.items.begin(), a.items.end(), [](const Media &m) { return m.selected; }), "Analysis must not select media automatically");
        auto unknown = parseAnalysis(Json{{"formats", Json::array({Json{{"format_id", "unknown"}, {"url", "https://example.com/item"}, {"acodec", "none"}}, Json{{"format_id", "video"}, {"url", "https://example.com/video"}, {"vcodec", "h264"}}})}}, "https://example.com/");
        check(!unknown.items[0].videoOnly && unknown.items[0].category == "Требуют проверки" && unknown.items[1].category == "Видео · звук не проверен", "Unknown codecs retain uncertainty");
    });
    test("Null DRM flag accepted; true flag preserved", [&] {
        auto a = parseAnalysis(sample, "https://x.test");
        check(!a.items[0].drm && a.items[2].drm, "DRM");
    });
    test("Author and automatic subtitles remain distinct", [&] {
        auto a = parseAnalysis(sample, "https://x.test");
        check(a.items[3].key != a.items[4].key && a.items[4].automatic, "Subtitle collision");
    });
    test("Thumbnails, chapters, metadata and description listed", [&] {
        auto a = parseAnalysis(sample, "https://x.test");
        check(a.items.size() == 9 && a.items[5].category == "Превью" && a.items.back().id == "chapters",
              "Extras");
    });
    test("Playlists are rejected",
         [] { rejects([] { parseAnalysis(Json{{"entries", Json::array()}}, "https://x.test"); }); });
    test("Direct media without format array parsed", [] {
        auto a = parseAnalysis(Json{{"url", "https://a.test/v"}, {"acodec", "aac"}}, "https://x.test");
        check(a.items.size() == 2 && a.items[0].format, "Direct");
    });
    test("Netscape HttpOnly cookie export", [] {
        auto text = cookies(Json::array({{{"domain", ".example.com"},
                                          {"path", "/"},
                                          {"name", "session"},
                                          {"value", "secret"},
                                          {"httpOnly", true},
                                          {"secure", true}}}));
        check(text.find("#HttpOnly_.example.com\tTRUE\t/\tTRUE") != text.npos, "HttpOnly");
        validateCookies(text);
    });
    test("Partitioned cookie export rejected",
         [] { rejects([] { cookies(Json::array({{{"domain", "a.test"}, {"partitioned", true}}})); }); });
    test("Cookie delimiter injection rejected",
         [] { rejects([] { cookies(Json::array({{{"domain", "a.test"}, {"value", "bad\tvalue"}}})); }); });
    test("Malformed imported cookies rejected", [] { rejects([] { validateCookies("a\tb\tc\n"); }); });
    test("Metadata export strips URL and nested credentials", [] {
        auto s =
            sanitizeMetadata(Json{{"url", "https://x.test/?token=secret"},
                                  {"nested", {{"http_headers", {{"Cookie", "secret"}}}, {"title", "Good"}}}});
        check(s.find("secret") == s.npos && s.find("Good") != s.npos, "Metadata leak");
    });
    test("UTF-8 Cyrillic and supplementary plane decode", [] {
        auto r = codepoints("Тест😀");
        check(r.size() == 5 && r.back() == 0x1f600, "UTF-8");
    });
    test("Unknown size stays unknown", [] {
        check(sizeText(0) == "Неизвестен" && sizeText(1024).find("КБ") != std::string::npos, "Size");
    });
    std::cout << passed << " passed, " << failed << " failed\n";
    return failed ? 1 : 0;
}
