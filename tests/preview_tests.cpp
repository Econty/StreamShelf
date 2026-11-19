#include "platform.hpp"
#include "media_preview.hpp"
#include "preview_policy.hpp"
#include <iostream>
int main(){
    int passed=0;
    for(const auto* name:{L"preview-wide.png",L"preview-wide.jpg"}){
        auto preview=ss::decodePreview(ss::readFile(ss::fs::path(L"tests/fixtures")/name));
        if(preview.width!=480||preview.height!=120||preview.rgba.size()!=480*120*4){std::cerr<<"FAIL thumbnail size/aspect\n";return 1;}
        if(preview.rgba[0]<180||preview.rgba[1]>50||preview.rgba[2]>50){std::cerr<<"FAIL RGBA channel order\n";return 1;}
        passed+=2;
    }
    try{ss::decodePreview("not an image");std::cerr<<"FAIL corrupt input accepted\n";return 1;}catch(...){++passed;}
    try{ss::decodePreview(std::string(9*1024*1024,'x'));std::cerr<<"FAIL excessive input accepted\n";return 1;}catch(...){++passed;}
    ss::Media candidate; candidate.category="Требуют проверки";candidate.ext="MP4";
    if(!ss::previewFrameCandidate(candidate)){std::cerr<<"FAIL unclassified MP4 cannot request a frame\n";return 1;}++passed;
    candidate.drm=true;
    if(ss::previewFrameCandidate(candidate)){std::cerr<<"FAIL protected media frame extraction allowed\n";return 1;}++passed;
    candidate.drm=false;candidate.category="Данные";candidate.ext="json";
    if(ss::previewFrameCandidate(candidate)){std::cerr<<"FAIL JSON treated as video\n";return 1;}++passed;
    candidate.category="Аудио";
    if(!ss::previewFrameCandidate(candidate)){std::cerr<<"FAIL audio embedded cover excluded\n";return 1;}++passed;
    candidate.url="https://media.example/INDEX.M3U8?token=synthetic";
    if(!ss::previewManifest(candidate)){std::cerr<<"FAIL signed manifest treated as direct media\n";return 1;}++passed;
    candidate.url="https://media.example/video.mp4?description=.m3u8";
    if(ss::previewManifest(candidate)){std::cerr<<"FAIL MP4 query mistaken for manifest\n";return 1;}++passed;
    candidate.format=true;candidate.category="Видео";candidate.ext="mp4";
    candidate.url="https://page.example/watch?id=1#video";candidate.page="https://page.example/watch?id=1";
    if(ss::previewFrameCandidate(candidate)){std::cerr<<"FAIL extractor webpage treated as video input\n";return 1;}++passed;
    candidate.url="https://cdn.example/media.mp4";
    if(!ss::previewFrameCandidate(candidate)){std::cerr<<"FAIL extractor direct media input excluded\n";return 1;}++passed;
    candidate.protocol="m3u8_native";
    if(!ss::previewManifest(candidate)){std::cerr<<"FAIL extractor HLS protocol ignored\n";return 1;}++passed;
    const auto emptyJar=ss::cookies(ss::Json::array());
    if(ss::previewHasCookies(emptyJar) || !ss::previewHasCookies(emptyJar+"#HttpOnly_example.test\tFALSE\t/\tTRUE\t0\tsession\tfixture\n")){
        std::cerr<<"FAIL empty or HttpOnly cookie classification\n";return 1;
    }++passed;
    const auto *toolsEnv=_wgetenv(L"STREAMSHELF_TEST_TOOLS");
    const auto *localEnv=_wgetenv(L"LOCALAPPDATA");
    const auto tools=toolsEnv ? ss::fs::path(toolsEnv) : localEnv ? ss::fs::path(localEnv)/L"StreamShelfNative"/L"tools" : ss::fs::path{};
    const auto ffmpeg=tools/L"ffmpeg.exe", ffprobe=tools/L"ffprobe.exe";
    if(!tools.empty() && ss::fs::is_regular_file(ffmpeg)) {
        std::atomic_bool cancel{false};
        const auto probe=ss::fs::is_regular_file(ffprobe) ? ffprobe : ss::fs::path{};
        auto video=ss::decodeMediaPreview(ffmpeg,probe,L"tests/fixtures/preview-video.mp4",false,ss::MediaPreviewKind::video,cancel);
        if(video.rgba.empty()){std::cerr<<"FAIL local video contains no preview frame\n";return 1;}++passed;
        using ss::fs::path;
        const auto fixture=ss::fs::temp_directory_path()/(L"StreamShelf-waveform-test-"+std::to_wstring(GetCurrentProcessId())+L".wav");
        struct Cleanup {path file;~Cleanup(){std::error_code error;ss::fs::remove(file,error);}} cleanup{fixture};
        auto tone=ss::runProcess(ffmpeg,{L"-hide_banner",L"-loglevel",L"error",L"-f",L"lavfi",L"-i",L"sine=frequency=530:duration=1",L"-y",fixture.wstring()},nullptr,cancel,true);
        if(tone.code){std::cerr<<"FAIL could not generate an audio fixture: "<<tone.error<<'\n';return 1;}
        auto waveform=ss::decodeMediaPreview(ffmpeg,probe,fixture.wstring(),false,ss::MediaPreviewKind::audio,cancel);
        if(waveform.width!=480 || waveform.height!=270 || waveform.rgba.empty()){std::cerr<<"FAIL audio without artwork has no waveform preview\n";return 1;}++passed;
    }
    std::cout<<passed<<" preview decode checks passed\n";
}
