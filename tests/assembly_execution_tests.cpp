#include "app.hpp"
#include "download_validation.hpp"
#include <iostream>

// Exercise the real assembly implementation without starting a window,
// connector, worker pool, or touching the user's StreamShelf data.
namespace ss {
struct ExtensionInstaller::Impl {};
ExtensionInstaller::ExtensionInstaller() = default;
ExtensionInstaller::~ExtensionInstaller() = default;
Connector::~Connector() = default;
App::App() = default;
App::~App() = default;
void App::post(std::function<void()> f) { f(); }
static fs::path fixtureTools;
fs::path App::tool(const wchar_t *name) { return fixtureTools / name; }
}

struct AssemblyAccess {
    using type = void (ss::App::*)(const std::shared_ptr<ss::Job> &);
    friend type get(AssemblyAccess);
};
template<class T, typename T::type member> struct PrivateAccess {
    friend typename T::type get(T) { return member; }
};
template struct PrivateAccess<AssemblyAccess, &ss::App::downloadAssembly>;

int main() {
    using namespace ss;
    wchar_t value[32768]{};
    DWORD count=GetEnvironmentVariableW(L"STREAMSHELF_TEST_TOOLS",value,DWORD(std::size(value)));
    if(count && count<std::size(value)) fixtureTools=fs::path(value);
    else {
        count=GetEnvironmentVariableW(L"LOCALAPPDATA",value,DWORD(std::size(value)));
        if(count && count<std::size(value))fixtureTools=fs::path(value)/L"StreamShelfNative"/L"tools";
    }
    if(!fs::is_regular_file(fixtureTools/L"ffmpeg.exe") || !fs::is_regular_file(fixtureTools/L"ffprobe.exe")) {
        std::cout << "SKIP: set STREAMSHELF_TEST_TOOLS to a directory with ffmpeg.exe and ffprobe.exe\n";
        return 77;
    }

    const auto root=fs::temp_directory_path()/(L"StreamShelf-assembly-regression-"+
        std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    try {
        if(!fs::create_directory(root))throw std::runtime_error("Could not create isolated test directory");
        struct Cleanup {
            fs::path root;
            ~Cleanup() {
                std::error_code ec;
                auto temp=fs::canonical(fs::temp_directory_path(ec),ec);
                if(ec)return;
                auto attributes=GetFileAttributesW(root.c_str());
                if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_REPARSE_POINT))return;
                auto resolved=fs::canonical(root,ec);
                if(ec || resolved.parent_path()!=temp || resolved.filename().wstring().rfind(L"StreamShelf-assembly-regression-",0)!=0)return;
                fs::remove_all(resolved,ec);
            }
        } cleanup{root};
        auto check=[](bool ok,const char *message){if(!ok)throw std::runtime_error(message);};
        App app;
        auto execute=[&](const std::shared_ptr<Job> &job){(app.*get(AssemblyAccess{}))(job);return fs::path(wide(job->workerOutput));};
        std::atomic_bool cancel{false};
        auto run=[&](const wchar_t *tool,const std::vector<std::wstring> &args) {
            auto result=runProcess(fixtureTools/tool,args,nullptr,cancel,true);
            if(result.code)throw std::runtime_error("Fixture tool failed: "+result.error);
            return result;
        };
        auto probe=[&](const fs::path &path) {
            auto result=run(L"ffprobe.exe",{L"-v",L"error",L"-show_streams",L"-show_chapters",L"-show_format",L"-of",L"json",path.wstring()});
            return Json::parse(result.output);
        };
        auto makeJob=[&](const wchar_t *name) {
            auto job=std::make_shared<Job>();job->folder=utf8((root/name).wstring());job->title="Test";return job;
        };

        auto jsonInput=root/L"chapters.json";
        writeFile(jsonInput,R"({"note":"keep","chapters":[{"start_time":0,"end_time":2,"title":"OLD","custom":"preserved"}]})");
        AssemblySource chapters;chapters.role=4;chapters.path=utf8(jsonInput.wstring());
        chapters.chaptersReady=true;chapters.chapterEdits={{"0","2","NEW"}};
        for(int format=0;format<=2;++format) {
            auto job=makeJob((L"chapter-json-"+std::to_wstring(format)).c_str());
            job->singleExport=true;job->exportFormat=format;job->sources={chapters};
            auto output=execute(job);auto content=readFile(output);
            check(content.find("NEW")!=content.npos && content.find("OLD")==content.npos,"External chapter edits missing");
            if(format==0) {
                auto original=Json::parse(content);
                check(original["note"]=="keep" && original["chapters"][0]["custom"]=="preserved","Unrelated JSON fields discarded");
            }
        }
        auto ffmetadata=root/L"chapters.ffmetadata";
        writeFile(ffmetadata,";FFMETADATA1\nalbum=ALBUM_KEEP\ncomment=literal\\\n[CHAPTER]\n[CHAPTER]\nTIMEBASE=1/1000\nSTART=0\nEND=2000\ntitle=OLD\n");
        chapters.path=utf8(ffmetadata.wstring());
        auto originalFormat=makeJob(L"chapter-original-ffmetadata");originalFormat->singleExport=true;originalFormat->sources={chapters};
        auto originalContent=readFile(execute(originalFormat));
        check(originalContent.find("NEW")!=originalContent.npos && originalContent.find("OLD")==originalContent.npos,"Original ffmetadata edits missing");
        check(originalContent.find("album=ALBUM_KEEP")!=originalContent.npos,"Original ffmetadata global tags discarded");
        check(originalContent.find("comment=literal\\\n[CHAPTER]")!=originalContent.npos,"Escaped ffmetadata content mistaken for chapter marker");
        chapters.chaptersReady=false;
        auto unchanged=makeJob(L"chapter-unchanged");unchanged->singleExport=true;unchanged->sources={chapters};
        check(readFile(execute(unchanged))==readFile(ffmetadata),"Unedited original chapter file changed");

        auto audio=root/L"audio.mka";
        run(L"ffmpeg.exe",{L"-hide_banner",L"-loglevel",L"error",L"-f",L"lavfi",L"-i",L"sine=frequency=440:duration=2",L"-c:a",L"flac",L"-y",audio.wstring()});
        auto combined=root/L"combined.mkv";
        run(L"ffmpeg.exe",{L"-hide_banner",L"-loglevel",L"error",L"-f",L"lavfi",L"-i",L"color=c=black:s=64x64:r=10:d=1",L"-f",L"lavfi",L"-i",L"sine=frequency=440:duration=1",L"-c:v",L"mpeg4",L"-c:a",L"aac",L"-shortest",L"-y",combined.wstring()});
        for(int role=0;role<2;++role) {
            AssemblySource unknownTrack;unknownTrack.role=role;unknownTrack.stream=-1;
            unknownTrack.path=utf8(combined.wstring());unknownTrack.derivedAudio=role==1;
            auto job=makeJob(role==0?L"single-video-unknown-stream":L"single-audio-unknown-stream");
            job->singleExport=true;job->exportFormat=0;job->sources={unknownTrack};
            auto streams=probe(execute(job)).value("streams",Json::array());
            check(streams.size()==1 && str(streams[0],"codec_type")==std::string(role==0?"video":"audio"),
                  "Separate original export must contain only the requested audio or video track");
        }
        auto cover=root/L"cover.jpg", album=root/L"audio-with-cover.m4a";
        run(L"ffmpeg.exe",{L"-hide_banner",L"-loglevel",L"error",L"-f",L"lavfi",L"-i",L"color=c=black:s=32x32",L"-frames:v",L"1",L"-update",L"1",L"-y",cover.wstring()});
        run(L"ffmpeg.exe",{L"-hide_banner",L"-loglevel",L"error",L"-f",L"lavfi",L"-i",L"sine=frequency=440:duration=1",L"-i",cover.wstring(),L"-map",L"0:a:0",L"-map",L"1:v:0",L"-c:a",L"aac",L"-c:v",L"copy",L"-disposition:v:0",L"attached_pic",L"-y",album.wstring()});
        auto coverProbe=run(L"ffprobe.exe",{L"-v",L"error",L"-show_entries",L"stream=codec_type:stream_disposition=attached_pic",L"-of",L"json",album.wstring()});
        auto coverData=Json::parse(coverProbe.output);
        check(probeMatches(0,coverData,ExpectedStreams::Audio),"Audio with an attached image must validate as audio");
        check(!probeMatches(0,coverData,ExpectedStreams::Video) && !probeMatches(0,coverData,ExpectedStreams::VideoAndAudio),
              "An attached image from real FFprobe output must not validate as playable video");
        Media media;media.title="Audio";media.category="Аудио";
        auto audioSources=assemblySourcesFromProbe(media,utf8(audio.wstring()),probe(audio));
        check(!audioSources.empty() && audioSources[0].role==1,"Fixture audio missing");
        auto source=audioSources[0];source.originalKnownTags={{"title",source.name}};

        auto metadataInput=root/L"metadata.json";
        writeFile(metadataInput,R"({"title":"Title","artist":"Old artist","genre":"OLD","album":"REMOVE","node":{"label":"old"}})");
        AssemblySource metadata;metadata.role=5;metadata.path=utf8(metadataInput.wstring());metadata.metadataEdit=true;metadata.metadataEditTitle="Edited Title";metadata.metadataEditArtist="Edited Artist";
        metadata.extraTags={{"genre","OLD"},{"album","REMOVE"}};metadata.originalExtraTags=metadata.extraTags;
        metadata.extraTags["genre"]="Edited in builder";metadata.extraTags["composer"]="Composer";
        stageMetadataField(metadata,"genre",R"("NEW")",false);
        stageMetadataField(metadata,"album","",true);
        auto metadataJob=makeJob(L"metadata-mux");metadataJob->sources={source,metadata};
        metadataJob->assemblyMetadataTitle="Auto Title";metadataJob->assemblyArtist="Auto Artist";
        metadataJob->globalTags={{"genre","OLD"},{"album","REMOVE"}};metadataJob->originalGlobalTags=metadataJob->globalTags;
        auto tags=probe(execute(metadataJob))["format"]["tags"];
        check(metadataTagMatches(tags,"genre","NEW"),"Edited genre missing in mux");
        check(metadataTagMatches(tags,"album",""),"Removed album still present in mux");
        check(metadataTagMatches(tags,"composer","Composer"),"Additional builder tag missing in mux");
        check(metadataTagMatchesAny(tags,{"title","name"},"Edited Title") && metadataTagMatchesAny(tags,{"artist","performer"},"Edited Artist"),"Explicit JSON title/artist edits were overridden");
        auto separateMetadata=makeJob(L"metadata-export");separateMetadata->singleExport=true;separateMetadata->sources={metadata};
        auto exportedMetadata=Json::parse(readFile(execute(separateMetadata)));
        check(exportedMetadata["genre"]=="NEW" && !exportedMetadata.contains("album") && exportedMetadata["composer"]=="Composer","Separate export and mux apply different edits");
        auto pathMetadata=metadata;stageMetadataField(pathMetadata,"/genre",R"("PATH")",false);
        auto pathJob=makeJob(L"metadata-pointer-mux");pathJob->sources={source,pathMetadata};
        check(metadataTagMatches(probe(execute(pathJob))["format"]["tags"],"genre","PATH"),"Top-level JSON pointer edit missing");
        auto nested=metadata;stageMetadataField(nested,"/node/label",R"("new")",false);
        auto nestedJob=makeJob(L"metadata-nested-mux");nestedJob->sources={source,nested};
        bool rejected=false;
        try{(void)execute(nestedJob);}catch(const std::exception &e){rejected=std::string(e.what()).find("JSON")!=std::string::npos;}
        check(rejected,"Unrepresentable nested edit must fail explicitly");
        auto nonString=metadata;stageMetadataField(nonString,"rating","42",false);
        auto numberJob=makeJob(L"metadata-number-mux");numberJob->sources={source,nonString};
        rejected=false;
        try{(void)execute(numberJob);}catch(const std::exception &e){rejected=std::string(e.what()).find("JSON")!=std::string::npos;}
        check(rejected,"Unrepresentable non-text edit must fail explicitly");

        auto chapterAudio=root/L"chapter-audio.mka";
        run(L"ffmpeg.exe",{L"-hide_banner",L"-loglevel",L"error",L"-i",audio.wstring(),L"-f",L"ffmetadata",L"-i",ffmetadata.wstring(),L"-map",L"0:a",L"-map_chapters",L"1",L"-c:a",L"copy",L"-y",chapterAudio.wstring()});
        auto chapterSources=assemblySourcesFromProbe(media,utf8(chapterAudio.wstring()),probe(chapterAudio));
        auto audioIt=std::find_if(chapterSources.begin(),chapterSources.end(),[](const auto &s){return s.role==1;});
        auto chapterIt=std::find_if(chapterSources.begin(),chapterSources.end(),[](const auto &s){return s.role==4;});
        check(audioIt!=chapterSources.end() && chapterIt!=chapterSources.end(),"Fixture chapters missing");
        auto audioOnly=*audioIt;audioOnly.originalKnownTags={{"title",audioOnly.name}};
        auto noChapters=makeJob(L"excluded-chapters");noChapters->sources={audioOnly};
        check(probe(execute(noChapters)).value("chapters",Json::array()).empty(),"Unselected chapters copied");
        auto withChapters=makeJob(L"included-chapters");withChapters->sources={audioOnly,*chapterIt};
        check(probe(execute(withChapters)).value("chapters",Json::array()).size()==1,"Selected chapters missing");
        auto flac=makeJob(L"flac-default");auto primary=source;primary.primary=true;primary.originalDisposition["default"]=1;
        flac->sources={primary};flac->muxOptions.container=6;
        auto flacProbe=probe(execute(flac));
        check(!flacProbe.value("streams",Json::array()).empty(),"Default FLAC output missing");
        auto forcedFlac=makeJob(L"flac-forced");primary.forced=true;forcedFlac->sources={primary};forcedFlac->muxOptions.container=6;
        rejected=false;
        try{(void)execute(forcedFlac);}catch(const std::exception &e){rejected=std::string(e.what()).find("FLAC")!=std::string::npos;}
        check(rejected,"Unsupported FLAC forced flag must be rejected");
        check(fs::is_empty(root/L"flac-forced"),"Unsupported FLAC flag should fail before writing output");
        auto flacChapters=makeJob(L"flac-included-chapters");flacChapters->sources={audioOnly,*chapterIt};flacChapters->muxOptions.container=6;
        check(probe(execute(flacChapters)).value("chapters",Json::array()).size()==1,"Selected FLAC chapters missing");
        std::cout<<"Assembly execution regression checks passed\n";
        return 0;
    } catch(const std::exception &e) {
        std::cerr << "Assembly regression failed: " << e.what() << '\n';
        return 1;
    }
}
