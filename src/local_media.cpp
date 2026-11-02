#include "app.hpp"
namespace ss {
void App::detectVideoEncoders() {
    work([this] {
        {std::unique_lock lock(readyMutex);readyCv.wait(lock,[this]{return ready||stopping;});}
        if(stopping)return;
        static const wchar_t *names[]={L"h264_nvenc",L"hevc_nvenc",L"h264_qsv",L"hevc_qsv",L"h264_amf",L"hevc_amf"};
        static const std::string gpuNames[]={"h264_nvenc","hevc_nvenc","h264_qsv","hevc_qsv","h264_amf","hevc_amf"};
        const std::string videoNames[]={"","libx264","libx265","libvpx-vp9","libaom-av1"};
        const std::string audioNames[]={"","aac","libopus","libmp3lame","flac"};
        const std::string subNames[]={"","srt","mov_text","webvtt","ass"};
        std::set<std::string> available;
        std::atomic_bool listCancel{false},listFinished{false};
        std::thread listWatchdog([&]{for(int n=0;n<80&&!listFinished&&!stopping;++n)Sleep(100);if(!listFinished)listCancel=true;});
        try {
            std::shared_lock lock(toolsMutex);
            auto result=runProcess(tool(L"ffmpeg.exe"),{L"-hide_banner",L"-encoders"},nullptr,listCancel,true);
            if(!result.code&&!listCancel){std::istringstream lines(result.output);std::string line;while(std::getline(lines,line)){
                std::istringstream row(line);std::string flags,name;row>>flags>>name;
                if(flags.size()==6&&!name.empty()&&(flags[0]=='V'||flags[0]=='A'||flags[0]=='S'))available.insert(name);
            }}
        } catch(...) {}
        listFinished=true;listWatchdog.join();
        std::array<bool,5> video{true,false,false,false,false},audio{true,false,false,false,false},subs{true,false,false,false,false};
        for(int i=1;i<5;++i){video[i]=available.count(videoNames[i])!=0;audio[i]=available.count(audioNames[i])!=0;subs[i]=available.count(subNames[i])!=0;}
        if(!stopping)post([this,video,audio,subs]{for(int i=1;i<5;++i)videoEncoders[i]=video[i];audioEncoders=audio;subtitleEncoders=subs;});
        for(int i=0;i<6&&!stopping;++i) {
            if(!available.count(gpuNames[i]))continue;
            std::atomic_bool cancelled{false},finished{false};
            std::thread watchdog([&]{for(int n=0;n<80&&!finished&&!stopping;++n)Sleep(100);if(!finished)cancelled=true;});
            bool available=false;
            try {
                std::shared_lock lock(toolsMutex);
                auto r=runProcess(tool(L"ffmpeg.exe"),{L"-hide_banner",L"-loglevel",L"error",L"-nostdin",L"-f",L"lavfi",L"-i",L"color=size=128x128:rate=1",L"-frames:v",L"1",L"-c:v",names[i],L"-f",L"null",L"-"},nullptr,cancelled,true);
                available=!r.code&&!cancelled&&!stopping;
            } catch(...) {}
            finished=true;watchdog.join();
            if(available)post([this,i]{videoEncoders[i+5]=true;});
        }
        if(!stopping)post([this]{videoEncodersReady=true;});
    });
}
void App::addLocalFiles(const std::vector<std::string> &paths) {
    if(paths.empty())return;
    if(paths.size()>256){error("Можно добавить не более 256 файлов за раз");return;}
    work([this,paths]{
        {std::unique_lock lock(readyMutex);readyCv.wait(lock,[this]{return ready||stopping;});}
        if(stopping)return;
        int rejected=0;
        for(const auto &value:paths){
            if(stopping)break;
            try {
                fs::path path=wide(value);
                if(!path.is_absolute()||!fs::is_regular_file(path)){++rejected;continue;}
                auto ext=path.extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);
                Media item;item.key="local:"+value;item.id=item.key;item.title=utf8(path.filename().wstring());item.ext=utf8(ext.empty()?L"":ext.substr(1));
                if(ext==L".jpg"||ext==L".jpeg"||ext==L".png"||ext==L".webp"||ext==L".avif")item.category="Изображения";
                else if(ext==L".json"){item.category="Данные";auto json=Json::parse(readFile(path,4*1024*1024),nullptr,false);item.id=json.is_array()?"chapters":"metadata";}
                else {
                    std::atomic_bool cancelled{false},finished{false};
                    std::thread watchdog([&]{for(int n=0;n<150&&!finished&&!stopping;++n)Sleep(100);if(!finished)cancelled=true;});
                    ProcessResult result{};
                    try {std::shared_lock lock(toolsMutex);result=runProcess(tool(L"ffprobe.exe"),{L"-v",L"error",L"-show_entries",L"stream=codec_name,codec_type:stream_disposition=attached_pic",L"-of",L"json",path.wstring()},nullptr,cancelled,true);}
                    catch(...){finished=true;watchdog.join();throw;}
                    finished=true;watchdog.join();if(stopping)break;
                    auto data=Json::parse(result.output,nullptr,false);
                    if(result.code||!data.is_object()||!data.contains("streams")||!data["streams"].is_array()){++rejected;continue;}
                    bool video=false,audio=false,sub=false;
                    for(const auto &stream:data["streams"]){auto type=str(stream,"codec_type");
                        if(type=="video"&&number(stream.value("disposition",Json::object()),"attached_pic")!=1){video=true;item.vcodec=str(stream,"codec_name");}
                        if(type=="audio"){audio=true;item.acodec=str(stream,"codec_name");}
                        if(type=="subtitle")sub=true;
                    }
                    if(!video&&!audio&&!sub){++rejected;continue;}
                    item.category=video?"Видео":audio?"Аудио":"Субтитры";item.videoOnly=video&&!audio;if(video&&!audio)item.acodec="none";
                }
                std::error_code ec;auto size=fs::file_size(path,ec);if(!ec)item.size=double(size);
                post([this,item,value]{
                    auto existing=std::find_if(jobs.begin(),jobs.end(),[&](const auto &j){return j->localFile&&j->output==value;});
                    if(existing!=jobs.end()){selectedJob=(*existing)->id;return;}
                    auto job=std::make_shared<Job>();job->id=std::to_string(GetTickCount64())+"-"+std::to_string(++jobSerial);
                    job->localFile=true;job->title=item.title;job->item=item;job->folder=utf8(fs::path(wide(value)).parent_path().wstring());
                    job->output=value;job->state="Готово";job->detail="Файл на компьютере";job->percent=100;job->claimed=true;job->done=true;
                    {std::lock_guard lock(jobsMutex);jobs.push_back(job);}selectedJob=job->id;saveHistory();
                });
            } catch(...) {++rejected;}
        }
        post([this,rejected]{notify(rejected?"Не удалось добавить файлов: "+std::to_string(rejected):"Файлы добавлены");});
    });
}
}
