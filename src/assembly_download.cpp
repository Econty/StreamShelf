#include "app.hpp"
#include "language_options.hpp"
#include <iomanip>
namespace ss {
ProcessResult App::runFfmpegWithProgress(const std::shared_ptr<Job> &j, const std::vector<std::wstring> &args, double duration) {
    std::vector<std::wstring> command{L"-progress",L"pipe:1",L"-nostats"};
    command.insert(command.end(),args.begin(),args.end());
    post([j]{j->state="Сборка контейнера";j->detail="Подготовка обработки FFmpeg…";j->percent=-1;});
    double seconds=0;std::string speed,frame,fps,bitrate;
    auto formatTime=[](double value){
        auto t=static_cast<long long>(std::max(0.0,value));std::ostringstream out;
        out<<std::setfill('0')<<std::setw(2)<<(t/3600)<<":"<<std::setw(2)<<((t/60)%60)<<":"<<std::setw(2)<<(t%60);
        return out.str();
    };
    auto result=runProcess(tool(L"ffmpeg.exe"),command,[this,j,duration,&seconds,&speed,&frame,&fps,&bitrate,formatTime](const std::string &line){
        if(line.rfind("out_time_us=",0)==0 || line.rfind("out_time_ms=",0)==0){
            auto pos=line.find('=');try{double value=std::stod(line.substr(pos+1))/1000000.;if(std::isfinite(value)&&value>=0)seconds=value;}catch(...){}
        } else if(line.rfind("speed=",0)==0) {
            speed=line.substr(6);while(!speed.empty()&&speed.front()==' ')speed.erase(speed.begin());
            if(speed=="N/A")speed.clear();
        } else if(line.rfind("frame=",0)==0)frame=line.substr(6);
        else if(line.rfind("fps=",0)==0)fps=line.substr(4);
        else if(line.rfind("bitrate=",0)==0)bitrate=line.substr(8);
        else if(line.rfind("progress=",0)==0) {
            double progress=duration>0?std::clamp(seconds*100./duration,0.,99.): -1.;
            std::string detail="Обработано "+formatTime(seconds);
            if(duration>0)detail+=" из "+formatTime(duration)+" · примерно "+std::to_string(int(progress))+"%";
            std::string metrics;
            if(!frame.empty()&&frame!="N/A")metrics+="Кадр "+frame;
            if(!fps.empty()&&fps!="N/A")metrics+=(metrics.empty()?"":" · ")+std::string("FPS ")+fps;
            if(!bitrate.empty()&&bitrate!="N/A")metrics+=(metrics.empty()?"":" · ")+std::string("Битрейт ")+bitrate;
            if(!speed.empty())metrics+=(metrics.empty()?"":" · ")+std::string("Скорость ")+speed;
            post([j,progress,detail,metrics]{j->state="Сборка контейнера";j->percent=progress;j->detail=detail;j->metrics=metrics;});
        }
    },j->cancel,true);
    if(!result.code&&!j->cancel)post([j]{j->state="Проверка результата";j->detail="Обработка завершена, проверяем итоговый файл…";if(j->percent>=0)j->percent=99;});
    return result;
}
void App::downloadAssembly(const std::shared_ptr<Job> &j) {
    auto folder=fs::path(wide(j->folder));fs::create_directories(folder);
    auto probeFile=[&](const fs::path &path) {
        std::atomic_bool cancel{false},finished{false};
        std::thread watchdog([&]{for(int i=0;i<300&&!finished&&!j->cancel&&!stopping;i++)Sleep(100);if(!finished)cancel=true;});
        ProcessResult r;
        try{r=runProcess(tool(L"ffprobe.exe"),{L"-v",L"error",L"-show_streams",L"-show_chapters",L"-show_format",L"-of",L"json",path.wstring()},nullptr,cancel,true);}
        catch(...){finished=true;watchdog.join();throw;}
        finished=true;watchdog.join();if(j->cancel||stopping)throw std::runtime_error("Отменено");
        auto data=Json::parse(r.output,nullptr,false);
        if(r.code||!data.is_object())throw std::runtime_error("Не удалось прочитать дорожки одного из сохранённых файлов. Исходники оставлены на диске.");
        return data;
    };
    struct Mapped {size_t source;int input,stream,type;};
    std::vector<Mapped> mapped;std::vector<std::wstring> args{L"-hide_banner",L"-loglevel",L"error",L"-nostdin",L"-n"};
    double totalDuration=0;
    int input=0,chapterInput=-1,expectedChapters=0;std::string metadataTitle=j->assemblyMetadataTitle.empty()?j->title:j->assemblyMetadataTitle,artist=j->assemblyArtist,description=j->assemblyDescription;
    fs::path cover,chapters;const AssemblySource *coverSource=nullptr;
    for(size_t n=0;n<j->sources.size();++n) {
        const auto &s=j->sources[n];if(!s.enabled)continue;
        if(j->cancel||stopping)throw std::runtime_error("Отменено");
        fs::path path=wide(s.path);if(!fs::is_regular_file(path))throw std::runtime_error("Исходный файл отсутствует. Сборка остановлена; остальные файлы сохранены.");
        if(s.role==3){cover=path;coverSource=&s;continue;}
        if(s.role==4){chapters=path;continue;}
        if(s.role==5){
            auto content=readFile(path,4*1024*1024);
            auto extension=path.extension().wstring();std::transform(extension.begin(),extension.end(),extension.begin(),::towlower);
            if(extension==L".txt"){if(description.empty())description=content;continue;}
            auto data=Json::parse(content,nullptr,false);
            if(!data.is_object())throw std::runtime_error("Источник метаданных должен содержать JSON-объект");
            if(artist.empty())artist=str(data,"artist",str(data,"uploader"));
            if(description.empty())description=str(data,"description");
            continue;
        }
        auto probe=probeFile(path);int count=0;
        for(const auto &stream:probe.value("streams",Json::array())) {
            auto type=str(stream,"codec_type");int role=type=="video"?0:type=="audio"?1:type=="subtitle"?2:-1;
            int index=int(number(stream,"index",-1));
            auto disposition=stream.value("disposition",Json::object());
            if(role==0&&number(disposition,"attached_pic")!=0)continue;
            if(role!=s.role && !(s.role==0&&s.includeAudio&&role==1))continue;
            if(s.stream>=0&&role==s.role&&index!=s.stream)continue;
            double duration=muxDuration(stream);if(duration<=0)duration=muxDuration(probe.value("format",Json::object()));
            totalDuration=std::max(totalDuration,duration);
            mapped.push_back({n,input,index,role});++count;
        }
        if(!count && s.role==1 && s.optionalAudio)continue;
        if(!count)throw std::runtime_error("В источнике «"+s.name+"» нет дорожек назначенного типа. Измените роль в конструкторе.");
        if(chapterInput<0&&!probe.value("chapters",Json::array()).empty()) {chapterInput=input;expectedChapters=int(probe["chapters"].size());}
        args.insert(args.end(),{L"-i",path.wstring()});++input;
    }
    if(mapped.empty()||std::none_of(mapped.begin(),mapped.end(),[](const auto &m){return m.type<2;}))throw std::runtime_error("Нет доступных видео- или аудиодорожек");
    auto chapterFile=folder/L"chapters.ffmetadata";
    if(!chapters.empty()) {
        auto data=Json::parse(readFile(chapters,4*1024*1024));
        if(data.is_object())data=data.value("chapters",Json::array());
        writeFile(chapterFile,chaptersToFfmetadata(data));expectedChapters=int(data.size());chapterInput=input;
        args.insert(args.end(),{L"-f",L"ffmetadata",L"-i",chapterFile.wstring()});++input;
    }
    for(const auto &m:mapped)args.insert(args.end(),{L"-map",std::to_wstring(m.input)+L":"+std::to_wstring(m.stream)});
    args.insert(args.end(),{L"-map_metadata",L"-1",L"-map_chapters",std::to_wstring(chapterInput)});
    int typeIndex[3]{},expected[3]{};bool hasPrimary[3]{},chosenPrimary[3]{};
    for(const auto &m:mapped)if(j->sources[m.source].primary&&m.type==j->sources[m.source].role)hasPrimary[m.type]=true;
    for(const auto &m:mapped) {
        const auto &s=j->sources[m.source];int index=typeIndex[m.type]++;++expected[m.type];
        std::wstring spec=std::wstring(m.type==0?L"v":m.type==1?L"a":L"s")+L":"+std::to_wstring(index);
        bool primary=!chosenPrimary[m.type]&&(hasPrimary[m.type]?(s.primary&&s.role==m.type):true);
        if(primary)chosenPrimary[m.type]=true;
        std::wstring disposition=primary?L"default":L"0";
        if(m.type==2&&s.forced)disposition=primary?L"default+forced":L"forced";
        args.insert(args.end(),{L"-disposition:"+spec,disposition});
        args.insert(args.end(), {L"-map_metadata:s:"+spec, std::to_wstring(m.input)+L":s:"+std::to_wstring(m.stream)});
        if(s.metadataStream<0 || m.stream==s.metadataStream) {
            if(!s.language.empty())args.insert(args.end(),{L"-metadata:s:"+spec,L"language="+wide(normalizedLanguage(s.language))});
            if(!s.name.empty())args.insert(args.end(),{L"-metadata:s:"+spec,L"title="+wide(s.name)});
            if(!s.description.empty())args.insert(args.end(),{L"-metadata:s:"+spec,L"comment="+wide(s.description)});
            if(!s.artist.empty())args.insert(args.end(),{L"-metadata:s:"+spec,L"artist="+wide(s.artist)});
            if(!s.summary.empty())args.insert(args.end(),{L"-metadata:s:"+spec,L"SUMMARY="+wide(s.summary)});
            if(!s.synopsis.empty())args.insert(args.end(),{L"-metadata:s:"+spec,L"SYNOPSIS="+wide(s.synopsis)});
            if(!s.handlerName.empty())args.insert(args.end(),{L"-metadata:s:"+spec,L"handler_name="+wide(s.handlerName)});
            if(!s.date.empty())args.insert(args.end(),{L"-metadata:s:"+spec,L"DATE_RELEASED="+wide(s.date)});
        }
        int codec=m.type==s.role?s.codec:0;
        static const wchar_t *video[]={L"copy",L"libx264",L"libx265",L"libvpx-vp9",L"libaom-av1",L"h264_nvenc",L"hevc_nvenc",L"h264_qsv",L"hevc_qsv",L"h264_amf",L"hevc_amf"};
        static const wchar_t *audio[]={L"copy",L"aac",L"libopus",L"libmp3lame",L"flac"};
        static const wchar_t *subs[]={L"copy",L"srt",L"mov_text",L"webvtt",L"ass"};
        if(codec<0||codec>(m.type==0?10:4)||s.quality<0||s.quality>2)throw std::runtime_error("Некорректный кодек или качество");
        args.insert(args.end(),{L"-c:"+spec,m.type==0?video[codec]:m.type==1?audio[codec]:subs[codec]});
        if(m.type==0&&codec>0&&codec<=4 && s.bitrate.empty()) {
            int crf=codec<3?(s.quality==0?18:s.quality==1?23:28):(s.quality==0?24:s.quality==1?32:40);
            args.insert(args.end(),{L"-crf:"+spec,std::to_wstring(crf)});
            if(codec>=3)args.insert(args.end(),{L"-b:"+spec,L"0"});
        }
        if(m.type==0&&codec>=5&&s.bitrate.empty())args.insert(args.end(),{L"-b:"+spec,s.quality==0?L"8000k":s.quality==1?L"4500k":L"2000k"});
        if(m.type==1&&codec>0&&codec<4&&s.bitrate.empty())args.insert(args.end(),{L"-b:"+spec,s.quality==0?L"256k":s.quality==1?L"192k":L"96k"});
        if(codec && !s.bitrate.empty())args.insert(args.end(),{L"-b:"+spec,wide(s.bitrate)+L"k"});
        if(codec && !s.maxrate.empty())args.insert(args.end(),{L"-maxrate:"+spec,wide(s.maxrate)+L"k"});
        if(codec && !s.bufsize.empty())args.insert(args.end(),{L"-bufsize:"+spec,wide(s.bufsize)+L"k"});
        if(m.type==0&&codec&&!s.frameRate.empty())args.insert(args.end(),{L"-r:"+spec,wide(s.frameRate)});
        if(m.type==0&&codec&&!s.width.empty())args.insert(args.end(),{L"-filter:"+spec,L"scale="+wide(s.width)+L":-2"});
        if(m.type==1&&codec&&!s.sampleRate.empty())args.insert(args.end(),{L"-ar:"+spec,wide(s.sampleRate)});
    }
    if(!cover.empty()) {
        if(j->muxOptions.container!=0)throw std::runtime_error("Для вложенной обложки нужен контейнер MKV");
        auto ext=cover.extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);
        if(ext!=L".png"&&ext!=L".jpg"&&ext!=L".jpeg")throw std::runtime_error("Обложка должна быть PNG или JPEG");
        args.insert(args.end(),{L"-attach",cover.wstring(),L"-metadata:s:t:0",ext==L".png"?L"mimetype=image/png":L"mimetype=image/jpeg",L"-metadata:s:t:0",ext==L".png"?L"filename=cover.png":L"filename=cover.jpg"});
        if(coverSource){
            if(!coverSource->name.empty())args.insert(args.end(),{L"-metadata:s:t:0",L"title="+wide(coverSource->name)});
            if(!coverSource->description.empty())args.insert(args.end(),{L"-metadata:s:t:0",L"description="+wide(coverSource->description)});
            if(!coverSource->artist.empty())args.insert(args.end(),{L"-metadata:s:t:0",L"artist="+wide(coverSource->artist)});
            if(!coverSource->date.empty())args.insert(args.end(),{L"-metadata:s:t:0",L"date="+wide(coverSource->date)});
        }
    }
    args.insert(args.end(),{L"-metadata",L"title="+wide(metadataTitle),L"-metadata",L"artist="+wide(artist),L"-metadata",(j->muxOptions.container==0?L"SYNOPSIS=":L"description=")+wide(description)});
    auto extension=wide(muxContainer(j->muxOptions));auto partial=folder/(L"result.part."+extension),final=folder/(L"result."+extension);
    struct Cleanup {fs::path partial,chapters;~Cleanup(){std::error_code ec;fs::remove(partial,ec);fs::remove(chapters,ec);}}cleanup{partial,chapterFile};
    if (j->muxOptions.container == 2 && !description.empty())
        args.insert(args.end(), {L"-movflags", L"use_metadata_tags"});
    args.push_back(partial.wstring());
    auto r=runFfmpegWithProgress(j,args,totalDuration);
    if(j->cancel||stopping)throw std::runtime_error("Отменено");
    if(r.code){auto detail=redact(r.error);if(detail.size()>600)detail=detail.substr(detail.size()-600);throw std::runtime_error("Не удалось собрать контейнер. Исходники сохранены отдельно. Проверьте совместимость кодеков и контейнера. "+detail);}
    auto output=probeFile(partial);int actual[3]{};
    for(const auto &s:output.value("streams",Json::array())){auto type=str(s,"codec_type");if(type=="video")++actual[0];if(type=="audio")++actual[1];if(type=="subtitle")++actual[2];}
    for(int i=0;i<3;i++)if(actual[i]<expected[i])throw std::runtime_error("В итоговом файле отсутствуют выбранные дорожки. Исходники сохранены.");
    if(int(output.value("chapters",Json::array()).size())<expectedChapters)throw std::runtime_error("Контейнер не сохранил все главы. Выберите другой формат.");
    if(!cover.empty()&&std::none_of(output["streams"].begin(),output["streams"].end(),[](const Json &s){return str(s,"codec_type")=="attachment"||number(s.value("disposition",Json::object()),"attached_pic")!=0;}))throw std::runtime_error("Обложка не сохранилась в контейнере. Исходники сохранены.");
    if(fs::exists(final))throw std::runtime_error("Итоговый файл уже существует; сохранённый файл не заменён");
    fs::rename(partial,final);j->workerOutput=utf8(final.wstring());j->workerCategory=actual[0]?"Видео":"Аудио";
}
}
