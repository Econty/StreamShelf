#include "app.hpp"
#include "language_options.hpp"
#include <iomanip>
#include <fstream>
#include <limits>
namespace ss {
// Preserve metadata and other non-chapter sections of an external ffmetadata
// file while replacing its chapters with the edited representation.
static std::string updateFfmetadataChapters(const std::string &original,const Json &chapters) {
    auto encoded=chaptersToFfmetadata(chapters);
    auto firstSection=encoded.find("[CHAPTER]");
    auto replacement=firstSection==std::string::npos?std::string():encoded.substr(firstSection);
    std::string result;
    bool chapterSection=false, inserted=false, continuation=false;
    for(size_t pos=0;pos<original.size();) {
        auto end=original.find('\n',pos);if(end==std::string::npos)end=original.size();else ++end;
        auto line=original.substr(pos,end-pos);
        auto trimmed=line;
        while(!trimmed.empty() && (trimmed.back()=='\r' || trimmed.back()=='\n'))trimmed.pop_back();
        if(!continuation && trimmed.size()>2 && trimmed.front()=='[' && trimmed.back()==']') {
            if(trimmed=="[CHAPTER]") {
                chapterSection=true;
                if(!inserted){result+=replacement;inserted=true;}
            } else chapterSection=false;
        }
        if(!chapterSection)result+=line;
        size_t backslashes=0;
        while(backslashes<trimmed.size() && trimmed[trimmed.size()-backslashes-1]=='\\')++backslashes;
        continuation=end<=original.size() && !line.empty() && line.back()=='\n' && backslashes%2==1;
        pos=end;
    }
    if(!inserted) {
        if(!result.empty() && result.back()!='\n')result+='\n';
        result+=replacement;
    }
    return result;
}
// FFmpeg can omit cover art from MOV and MP4 with use_metadata_tags.
// Append an iTunes cover atom to the final moov without moving media data.
static void writeMovCover(const fs::path &path, const fs::path &cover, bool png) {
    auto image=readFile(cover,16*1024*1024);
    if(image.empty())throw std::runtime_error("Файл обложки пуст.");
    auto atom=[](const char *type,const std::string &payload) {
        if(payload.size()>std::numeric_limits<uint32_t>::max()-8)throw std::runtime_error("Обложка слишком велика.");
        auto n=uint32_t(payload.size()+8);std::string result;
        for(int shift=24;shift>=0;shift-=8)result.push_back(char((n>>shift)&255));
        result.append(type,4);result+=payload;return result;
    };
    std::string data(8,'\0');data[3]=png?14:13;data+=image;
    auto covr=atom("covr",atom("data",data));
    std::string handler(24,'\0');handler.replace(8,4,"mdir");handler.replace(12,4,"appl");handler.push_back('\0');
    auto metadata=atom("meta",std::string(4,'\0')+atom("hdlr",handler)+atom("ilst",covr));
    auto extra=atom("udta",metadata);
    std::fstream file(path,std::ios::binary|std::ios::in|std::ios::out);
    if(!file)throw std::runtime_error("Не удалось открыть MOV для сохранения обложки.");
    file.seekg(0,std::ios::end);auto end=file.tellg();
    if(end<0)throw std::runtime_error("Не удалось прочитать MOV.");
    uint64_t length=uint64_t(end),pos=0,moov=0,moovSize=0;bool found=false;
    while(pos+8<=length) {
        char header[16]{};file.seekg(std::streamoff(pos));file.read(header,8);
        if(!file)throw std::runtime_error("Не удалось прочитать структуру MOV.");
        auto u32=[](const char *p){return (uint32_t((unsigned char)p[0])<<24)|(uint32_t((unsigned char)p[1])<<16)|(uint32_t((unsigned char)p[2])<<8)|uint32_t((unsigned char)p[3]);};
        uint64_t size=u32(header);unsigned headerSize=8;
        if(size==1) {file.read(header+8,8);if(!file)throw std::runtime_error("Не удалось прочитать структуру MOV.");size=(uint64_t(u32(header+8))<<32)|u32(header+12);headerSize=16;}
        else if(size==0)size=length-pos;
        if(size<headerSize || size>length-pos)throw std::runtime_error("Некорректная структура MOV.");
        if(std::string(header+4,4)=="moov") {
            if(found || headerSize!=8 || u32(header)==0 || pos+size!=length)throw std::runtime_error("Не удалось безопасно добавить обложку в MOV.");
            found=true;moov=pos;moovSize=size;
        }
        pos+=size;
    }
    if(!found || pos!=length || moovSize+extra.size()>std::numeric_limits<uint32_t>::max())
        throw std::runtime_error("Не удалось безопасно добавить обложку в MOV.");
    file.seekp(0,std::ios::end);file.write(extra.data(),std::streamsize(extra.size()));
    uint32_t size=uint32_t(moovSize+extra.size());char bytes[4];
    for(int i=0;i<4;++i)bytes[i]=char((size>>(24-8*i))&255);
    file.seekp(std::streamoff(moov));file.write(bytes,4);file.flush();
    if(!file)throw std::runtime_error("Не удалось сохранить обложку в MOV. Исходники сохранены.");
}
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
    auto hasCover=[](const Json &probe, int container) {
        for(const auto &stream:probe.value("streams",Json::array())) {
            auto codec=str(stream,"codec_name");
            if(codec!="mjpeg" && codec!="png")continue;
            if(container==0) {
                auto tags=stream.value("tags",Json::object());
                auto filename=str(tags,"filename");
                std::transform(filename.begin(),filename.end(),filename.begin(),[](unsigned char c){return char(std::tolower(c));});
                if(number(stream.value("disposition",Json::object()),"attached_pic")!=0 || filename=="cover.jpg" || filename=="cover.jpeg" || filename=="cover.png")return true;
            } else if(number(stream.value("disposition",Json::object()),"attached_pic")!=0) return true;
        }
        return false;
    };
    if(j->singleExport) {
        if(j->sources.size()!=1 || !j->sources[0].enabled)throw std::runtime_error("Для отдельного сохранения нужен один доступный источник.");
        const auto &source=j->sources[0];fs::path input=wide(source.path);
        if(!fs::is_regular_file(input))throw std::runtime_error("Источник недоступен. Сохранённые файлы не удалены.");
        auto exportCategory=source.role==0?"Видео":source.role==1?"Аудио":source.role==2?"Субтитры":source.role==3?"Изображения":"Данные";
        int format=j->exportFormat,quality=j->exportQuality;
        int maximum=source.role==3?3:source.role==2?4:source.role==4?2:source.role==5?1:source.role<2?3:0;
        if(format<0 || format>maximum || quality<0 || quality>2)throw std::runtime_error("Некорректные настройки сохранения.");
        int subtitleStream=-1;std::string subtitleCodec;
        std::wstring subtitleExtension;
        if(source.role==2 && format==0) {
            auto plan=originalSubtitlePlan(probeFile(input),source.stream);
            subtitleStream=plan.stream;subtitleCodec=plan.codec;subtitleExtension=plan.extension;
        }
        std::wstring extension=input.extension().wstring();
        if(format && source.role==0)extension=format==1?L".mkv":format==2?L".mp4":L".webm";
        if(format && source.role==1)extension=format==1?L".mp3":format==2?L".m4a":L".flac";
        if(source.role==5 && source.embeddedKind=="metadata")extension=format?L".txt":L".json";
        else if(format && source.role==5)extension=L".txt";
        else if(source.role==5 && source.metadataEdit)extension=L".json";
        if(format && source.role==3)extension=format==1?L".png":format==2?L".jpg":L".webp";
        else if(source.role==3 && source.embeddedKind=="cover") {
            if(source.probedCodec=="png")extension=L".png";
            else if(source.probedCodec=="mjpeg")extension=L".jpg";
            else throw std::runtime_error("Неизвестный формат встроенной обложки. Исходник сохранён.");
        }
        if(format && source.role==2)extension=(format==1||format==4)?L".srt":format==2?L".vtt":L".ass";
        else if(source.role==2)extension=subtitleExtension;
        if(format && source.role==4)extension=format==1?L".json":L".ffmetadata";
        else if(source.role==4 && source.embeddedKind=="chapters")extension=L".ffmetadata";
        if(extension.empty() || extension.size()>16 || !std::all_of(extension.begin()+1,extension.end(),[](wchar_t c){return (c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9');}))
            throw std::runtime_error("Не удалось определить формат исходного файла. Исходник сохранён.");
        auto output=folder/(L"result"+extension);
        if(!j->output.empty() && fs::path(wide(j->output))==output && fs::is_regular_file(output)) {
            j->workerOutput=j->output;j->workerCategory=exportCategory;return;
        }
        if(fs::exists(output))throw std::runtime_error("Итоговый файл уже существует. Исходник сохранён.");
        struct ExportCleanup {fs::path path;bool committed=false;~ExportCleanup(){if(!committed){std::error_code ec;fs::remove(path,ec);}}} cleanup{output};
        if(source.role==2 && format==4) {
            if(source.ocrExecutable.empty() || !fs::path(wide(source.ocrExecutable)).is_absolute() || !fs::is_regular_file(wide(source.ocrExecutable)))
                throw std::runtime_error("Не найден tesseract.exe. Укажите полный путь к программе; исходник сохранён.");
            if(source.ocrLanguage.empty() || source.ocrLanguage.size()>64 || !std::all_of(source.ocrLanguage.begin(),source.ocrLanguage.end(),[](unsigned char c){return std::isalnum(c)||c=='_'||c=='+'||c=='-';}))
                throw std::runtime_error("Проверьте языки Tesseract, например eng или rus+eng.");
            auto timed=[&](const fs::path &exe,const std::vector<std::wstring> &args,int timeoutSeconds) {
                std::atomic_bool cancel{false},finished{false},timedOut{false};
                std::thread watchdog([&]{for(int n=0;n<timeoutSeconds*10&&!finished&&!j->cancel&&!stopping;++n)Sleep(100);if(!finished){timedOut=!j->cancel&&!stopping;cancel=true;}});
                ProcessResult result;
                try {result=runProcess(exe,args,nullptr,cancel,true);}catch(...){finished=true;watchdog.join();throw;}
                finished=true;watchdog.join();
                if(j->cancel||stopping)throw std::runtime_error("Отменено");
                if(timedOut)throw std::runtime_error("Превышено время распознавания. Исходник сохранён.");
                return result;
            };
            auto info=probeFile(input);auto streams=info.value("streams",Json::array());
            int width=0,height=0;std::string subCodec;bool selected=false;
            for(const auto &stream:streams)if(str(stream,"codec_type")=="subtitle" && (source.stream<0 || int(number(stream,"index",-1))==source.stream)) {
                subCodec=str(stream,"codec_name");width=int(number(stream,"width"));height=int(number(stream,"height"));selected=true;break;
            }
            if(!selected || !isBitmapSubtitleCodec(subCodec))throw std::runtime_error("OCR доступен для графических субтитров. Исходник сохранён.");
            if(width<=0 || height<=0)for(const auto &stream:streams)if(str(stream,"codec_type")=="video" && number(stream.value("disposition",Json::object()),"attached_pic")==0) {
                width=int(number(stream,"width"));height=int(number(stream,"height"));if(width>0 && height>0)break;
            }
            if(width<=0 || height<=0 || width>8192 || height>8192 || int64_t(width)*height>40000000)
                throw std::runtime_error("Не удалось определить размер кадра графических субтитров. Исходник сохранён.");
            auto selector=source.stream>=0?std::to_wstring(source.stream):L"s:0";
            auto inspected=timed(tool(L"ffprobe.exe"),{L"-v",L"error",L"-select_streams",selector,L"-show_frames",L"-of",L"json",input.wstring()},120);
            auto data=Json::parse(inspected.output,nullptr,false);
            if(inspected.code || !data.is_object() || !data.contains("frames") || !data["frames"].is_array())
                throw std::runtime_error("Не удалось прочитать время графических субтитров. Исходник сохранён.");
            struct Event {double time,start,end;bool hasImage;};std::vector<Event> events;
            double base=0;try{base=std::stod(str(info.value("format",Json::object()),"start_time","0"));if(!std::isfinite(base))base=0;}catch(...){base=0;}
            for(const auto &frame:data["frames"]) {
                if(str(frame,"media_type")!="subtitle")continue;
                double pts=0;try{pts=std::stod(str(frame,"pts_time"))-base;}catch(...){throw std::runtime_error("Не удалось определить время субтитров. Исходник сохранён.");}
                double first=number(frame,"start_display_time",0)/1000.,last=number(frame,"end_display_time",0)/1000.;
                if(!std::isfinite(pts)||!std::isfinite(first)||!std::isfinite(last)||pts<-.1 || pts>100000000 || first<0 || last<0)
                    throw std::runtime_error("Некорректное время графических субтитров. Исходник сохранён.");
                events.push_back({std::max(0.,pts),first,last,number(frame,"num_rects",0)>0});
                if(events.size()>10000)throw std::runtime_error("Слишком много событий субтитров для распознавания. Исходник сохранён.");
            }
            std::stable_sort(events.begin(),events.end(),[](const auto &a,const auto &b){return a.time<b.time;});
            auto clock=[](double seconds) {
                auto ms=static_cast<long long>(std::llround(seconds*1000));std::ostringstream out;
                out<<std::setfill('0')<<std::setw(2)<<(ms/3600000)<<":"<<std::setw(2)<<((ms/60000)%60)<<":"<<std::setw(2)<<((ms/1000)%60)<<","<<std::setw(3)<<(ms%1000);return out.str();
            };
            auto framePath=folder/L"ocr-frame.png";
            struct FrameCleanup {fs::path path;~FrameCleanup(){std::error_code ec;fs::remove(path,ec);}}frameCleanup{framePath};
            std::string resultText;size_t recognized=0;
            for(size_t n=0;n<events.size();++n) {
                const auto &event=events[n];if(!event.hasImage)continue;
                if(++recognized>1000)throw std::runtime_error("Допускается не более 1000 фрагментов OCR за одну задачу. Исходник сохранён.");
                double start=event.time+event.start,end=event.end>event.start?event.time+event.end:0;
                if(n+1<events.size() && events[n+1].time>start && (end<=start || events[n+1].time<end))end=events[n+1].time;
                if(end<=start || end-start>120)throw std::runtime_error("Не удалось надёжно определить конец фрагмента субтитров. Исходник сохранён.");
                double sample=start+std::min(0.15,(end-start)/2);
                auto canvas=L"color=c=black:s="+std::to_wstring(width)+L"x"+std::to_wstring(height)+L":r=10:d="+std::to_wstring(sample+1);
                auto subtitle=source.stream>=0?L"1:"+std::to_wstring(source.stream):L"1:s:0";
                auto graph=L"[0:v]["+subtitle+L"]overlay=eof_action=pass,format=gray,negate[v]";
                std::error_code imageError;fs::remove(framePath,imageError);
                if(imageError)throw std::runtime_error("Не удалось подготовить временное изображение. Исходник сохранён.");
                auto image=timed(tool(L"ffmpeg.exe"),{L"-hide_banner",L"-loglevel",L"error",L"-nostdin",L"-f",L"lavfi",L"-i",canvas,L"-i",input.wstring(),L"-filter_complex",graph,L"-map",L"[v]",L"-ss",std::to_wstring(sample),L"-frames:v",L"1",L"-c:v",L"png",L"-update",L"1",L"-y",framePath.wstring()},120);
                if(image.code || !fs::is_regular_file(framePath))throw std::runtime_error("Не удалось извлечь изображение фрагмента субтитров. Исходник сохранён.");
                auto text=timed(fs::path(wide(source.ocrExecutable)),{framePath.wstring(),L"stdout",L"-l",wide(source.ocrLanguage),L"--psm",L"6"},60);
                if(text.code)throw std::runtime_error("Tesseract не выполнил распознавание. Проверьте установку и языковые данные. Исходник сохранён.");
                auto caption=text.output;caption.erase(std::remove(caption.begin(),caption.end(),'\f'),caption.end());
                while(!caption.empty() && (caption.back()=='\n'||caption.back()=='\r'||caption.back()==' '||caption.back()=='\t'))caption.pop_back();
                if(caption.empty())throw std::runtime_error("Tesseract не распознал один из фрагментов. Исходник сохранён; проверьте язык и изображение.");
                resultText+=std::to_string(recognized)+"\n"+clock(start)+" --> "+clock(end)+"\n"+caption+"\n\n";
                post([j,recognized]{j->state="Распознавание субтитров";j->detail="Распознано фрагментов: "+std::to_string(recognized);j->percent=-1;});
            }
            if(!recognized)throw std::runtime_error("Не найдено изображений субтитров для распознавания. Исходник сохранён.");
            writeFile(output,resultText);
            auto checked=probeFile(output);auto checkedStreams=checked.value("streams",Json::array());
            if(std::none_of(checkedStreams.begin(),checkedStreams.end(),[](const Json &stream){return str(stream,"codec_type")=="subtitle" && str(stream,"codec_name")=="subrip";}))
                throw std::runtime_error("Не удалось проверить полученный SRT. Исходник сохранён.");
        } else if(format==0 && source.role>=0 && source.role<=2) {
            // An unknown stream index is common for online inputs.  It still
            // represents one selected track, not the entire input container.
            const auto map=source.role==2 ? L"0:"+std::to_wstring(subtitleStream)
                : source.stream>=0 ? L"0:"+std::to_wstring(source.stream)
                : source.role==0 ? L"0:V:0" : L"0:a:0";
            auto result=runFfmpegWithProgress(j,{L"-hide_banner",L"-loglevel",L"error",L"-nostdin",L"-n",L"-i",input.wstring(),L"-map",map,L"-c",L"copy",output.wstring()},0);
            if(j->cancel||stopping)throw std::runtime_error("Отменено");
            if(result.code || !fs::is_regular_file(output))throw std::runtime_error("Не удалось сохранить дорожку в исходном формате. Выберите сборку с совместимым контейнером. Исходник сохранён. "+redact(result.error));
            auto streams=probeFile(output).value("streams",Json::array());auto type=source.role==0?"video":source.role==1?"audio":"subtitle";
            if(source.role==2) {
                if(streams.size()!=1 || str(streams[0],"codec_type")!="subtitle" || str(streams[0],"codec_name")!=subtitleCodec)
                    throw std::runtime_error("Не удалось проверить исходный кодек выбранной дорожки субтитров. Исходник сохранён.");
            } else if(streams.size()!=1 || str(streams[0],"codec_type")!=type ||
                      (source.role==0 && number(streams[0].value("disposition",Json::object()),"attached_pic")!=0))
                throw std::runtime_error("Не удалось проверить сохранённую дорожку. Исходник сохранён.");
        } else if(source.role==3 && source.embeddedKind=="cover" && format==0) {
            auto result=runFfmpegWithProgress(j,{L"-hide_banner",L"-loglevel",L"error",L"-nostdin",L"-n",L"-i",input.wstring(),L"-map",L"0:"+std::to_wstring(source.stream),L"-frames:v",L"1",L"-c:v",L"copy",L"-update",L"1",output.wstring()},0);
            if(result.code || !fs::is_regular_file(output))throw std::runtime_error("Не удалось извлечь обложку. Исходник сохранён.");
        } else if(source.role==5 && source.embeddedKind=="metadata") {
            writeFile(output,probeFile(input).value("format",Json::object()).value("tags",Json::object()).dump(2));
        } else if(source.role==4 && (source.chaptersReady || source.embeddedKind=="chapters" || format!=0)) {
            auto inputExt=input.extension().wstring();
            std::transform(inputExt.begin(),inputExt.end(),inputExt.begin(),::towlower);
            Json data;
            if(source.chaptersReady)data=editedChapters(source);
            else if(source.embeddedKind=="chapters" || inputExt==L".ffmetadata")data=chaptersFromProbe(probeFile(input));
            else {data=Json::parse(readFile(input,4*1024*1024));if(data.is_object())data=data.value("chapters",Json::array());}
            if(format==1)writeFile(output,data.dump(2));
            else if(format==0 && source.embeddedKind!="chapters" && inputExt==L".json") {
                // Preserve unrelated fields on both the document and each chapter.
                auto original=Json::parse(readFile(input,4*1024*1024));
                auto prior=original.is_object()?original.value("chapters",Json::array()):original;
                if(!prior.is_array())throw std::runtime_error("Некорректный файл глав. Исходник сохранён.");
                Json merged=Json::array();
                for(size_t i=0;i<data.size();++i) {
                    auto item=i<prior.size() && prior[i].is_object()?prior[i]:Json::object();
                    for(auto field=data[i].begin();field!=data[i].end();++field)item[field.key()]=field.value();
                    merged.push_back(std::move(item));
                }
                if(original.is_object()){original["chapters"]=std::move(merged);writeFile(output,original.dump(2));}
                else writeFile(output,merged.dump(2));
            } else if(format==0 && source.embeddedKind!="chapters" && inputExt==L".ffmetadata") {
                writeFile(output,updateFfmetadataChapters(readFile(input,4*1024*1024),data));
            } else writeFile(output,chaptersToFfmetadata(data));
        } else if(source.role==6) {
            throw std::runtime_error("Отдельное извлечение этого вложения пока недоступно; используйте сборку MKV. Исходник сохранён.");
        } else if(source.role==5 && (format!=0 || source.metadataEdit)) {
            auto content=readFile(input,4*1024*1024);
            if(source.metadataEdit)content=editedMetadataJson(content,source);
            else {auto data=Json::parse(content,nullptr,false);if(data.is_object() || data.is_array())content=data.dump(2);}
            writeFile(output,content);
        } else if(format==0) {
            fs::copy_file(input,output,fs::copy_options::none);
        } else {
            std::vector<std::wstring> command{L"-hide_banner",L"-loglevel",L"error",L"-nostdin",L"-n",L"-i",input.wstring()};
            if(source.role<2) {
                auto map=source.stream>=0?L"0:"+std::to_wstring(source.stream):source.role==0?L"0:v:0":L"0:a:0";
                command.insert(command.end(),{L"-map",map});
                if(source.role==0) {
                    command.insert(command.end(),{L"-c:v",format==3?L"libvpx-vp9":L"libx264",L"-crf",format==3?(quality==0?L"24":quality==1?L"32":L"40"):(quality==0?L"18":quality==1?L"23":L"28")});
                    if(format==3)command.insert(command.end(),{L"-b:v",L"0"});
                } else {
                    command.insert(command.end(),{L"-c:a",format==1?L"libmp3lame":format==2?L"aac":L"flac"});
                    if(format!=3)command.insert(command.end(),{L"-b:a",quality==0?L"256k":quality==1?L"192k":L"96k"});
                }
            } else if(source.role==3) {
                auto imageProbe=probeFile(input);auto info=imageProbe.value("format",Json::object());
                auto name=str(info,"format_name"),brand=str(info.value("tags",Json::object()),"major_brand");
                auto streams=imageProbe.value("streams",Json::array());
                if(source.embeddedKind!="cover" && ((name.find("image2")==std::string::npos && name.find("_pipe")==std::string::npos && name!="gif" && brand!="avif" && brand!="avis") || streams.size()!=1 || str(streams[0],"codec_type")!="video"))
                    throw std::runtime_error("Выберите изображение для преобразования. Исходник сохранён.");
                command.insert(command.end(),{L"-map",source.embeddedKind=="cover"?L"0:"+std::to_wstring(source.stream):L"0:v:0",L"-frames:v",L"1"});
                if(format==1)command.insert(command.end(),{L"-c:v",L"png",L"-compression_level",quality==0?L"9":quality==1?L"6":L"2"});
                if(format==2)command.insert(command.end(),{L"-c:v",L"mjpeg",L"-q:v",quality==0?L"2":quality==1?L"4":L"8"});
                if(format==3)command.insert(command.end(),{L"-c:v",L"libwebp",L"-quality",quality==0?L"95":quality==1?L"80":L"60"});
                command.insert(command.end(),{L"-update",L"1"});
            } else if(source.role==2) {
                auto info=probeFile(input);auto streams=info.value("streams",Json::array());
                for(const auto &stream:streams) {
                    if(str(stream,"codec_type")!="subtitle" || (source.stream>=0 && number(stream,"index",-1)!=source.stream))continue;
                    auto codec=str(stream,"codec_name");
                    if(isBitmapSubtitleCodec(codec))
                        throw std::runtime_error("Субтитры состоят из изображений и не преобразуются в текст без распознавания. Сохраните исходный файл.");
                    break;
                }
                command.insert(command.end(),{L"-map",source.stream>=0?L"0:"+std::to_wstring(source.stream):L"0:s:0",L"-c:s",format==1?L"srt":format==2?L"webvtt":L"ass"});
            } else throw std::runtime_error("Такое преобразование не поддерживается.");
            command.push_back(output.wstring());
            auto result=runFfmpegWithProgress(j,command,0);
            if(j->cancel||stopping)throw std::runtime_error("Отменено");
            if(result.code || !fs::is_regular_file(output))throw std::runtime_error("Не удалось преобразовать файл. Исходник сохранён. "+redact(result.error));
            auto probe=probeFile(output);auto streams=probe.value("streams",Json::array());
            auto expected=source.role==0||source.role==3?"video":source.role==1?"audio":"subtitle";
            if(std::none_of(streams.begin(),streams.end(),[&](const Json &v){return str(v,"codec_type")==expected;}))throw std::runtime_error("Не удалось проверить преобразованный файл. Исходник сохранён.");
        }
        cleanup.committed=true;j->workerOutput=utf8(output.wstring());j->workerCategory=exportCategory;return;
    }
    // Reuse the previously validated container if its move to the destination failed.
    auto saved = folder / (L"result." + wide(muxContainer(j->muxOptions)));
    if (!j->output.empty() && fs::path(wide(j->output)) == saved && fs::is_regular_file(saved)) {
        auto probe = probeFile(saved);
        bool video=false, audio=false;
        for (const auto &stream : probe.value("streams", Json::array())) {
            video |= str(stream,"codec_type")=="video" && number(stream.value("disposition",Json::object()),"attached_pic")==0;
            audio |= str(stream,"codec_type")=="audio";
        }
        if (!video && !audio) throw std::runtime_error("Не удалось проверить сохранённый контейнер.");
        if (std::any_of(j->sources.begin(),j->sources.end(),[](const auto &s){return s.enabled && s.role==3;}) && !hasCover(probe,j->muxOptions.container))
            throw std::runtime_error("Обложка не сохранилась в контейнере. Исходники сохранены.");
        j->workerOutput=utf8(saved.wstring()); j->workerCategory=video?"Видео":"Аудио";
        return;
    }
    struct Mapped {size_t source;int input,stream,type;};
    struct ExtraMapped {int input,stream;std::string type,name,codec,mime;};
    std::vector<ExtraMapped> extraMapped;
    std::vector<Mapped> mapped;std::vector<std::wstring> args{L"-hide_banner",L"-loglevel",L"error",L"-nostdin",L"-n"};
    double totalDuration=0;
    int input=0,chapterInput=-1,globalMetadataInput=-1,expectedChapters=0;Json expectedChapterList=Json::array(),originalGlobalMetadata=Json::object();std::string metadataTitle=j->assemblyMetadataTitle.empty()?j->title:j->assemblyMetadataTitle,artist=j->assemblyArtist,description=j->assemblyDescription;
    fs::path cover,chapters;const AssemblySource *coverSource=nullptr,*chapterSource=nullptr;
    auto effectiveGlobalTags=j->globalTags;
    std::set<std::string> removedMetadataTags;
    for(size_t n=0;n<j->sources.size();++n) {
        const auto &s=j->sources[n];if(!s.enabled)continue;
        if(j->cancel||stopping)throw std::runtime_error("Отменено");
        fs::path path=wide(s.path);if(!fs::is_regular_file(path))throw std::runtime_error("Исходный файл отсутствует. Сборка остановлена; остальные файлы сохранены.");
        if(s.role==3){cover=path;coverSource=&s;continue;}
        if(s.role==4){chapters=path;chapterSource=&s;continue;}
        if(s.role==5){
            if(s.embeddedKind=="metadata") {
                if(globalMetadataInput>=0)throw std::runtime_error("Выберите один источник общих метаданных.");
                auto probe=probeFile(path);originalGlobalMetadata=probe.value("format",Json::object()).value("tags",Json::object());
                globalMetadataInput=input++;args.insert(args.end(),{L"-i",path.wstring()});continue;
            }
            auto content=readFile(path,4*1024*1024);
            auto extension=path.extension().wstring();std::transform(extension.begin(),extension.end(),extension.begin(),::towlower);
            if(extension==L".txt"){if(description.empty())description=content;continue;}
            auto originalData=Json::parse(content,nullptr,false);
            if(!originalData.is_object())throw std::runtime_error("Источник метаданных должен содержать JSON-объект");
            auto data=s.metadataEdit?Json::parse(editedMetadataJson(content,s)):originalData;
            // Container tags are strings. A changed object/array/number cannot
            // round-trip through them; fail explicitly rather than discard it.
            if(s.metadataEdit) {
                for(const auto &[key,value]:s.metadataOverrides)
                    if(!metadataKeyAllowed(key) || !value.is_string())
                        throw std::runtime_error("Изменённое поле JSON нельзя сохранить как тег контейнера. Сохраните JSON отдельно. Исходники сохранены.");
                for(const auto &edit:s.metadataPathEdits) {
                    Json::json_pointer pointer(edit.at("path").get<std::string>());
                    auto path=pointer.to_string();
                    if(path.find('/',1)!=std::string::npos || (!edit.at("remove").get<bool>() && !edit.at("value").is_string()) || !metadataKeyAllowed(pointer.back()))
                        throw std::runtime_error("Вложенное или нетекстовое поле JSON нельзя сохранить как тег контейнера. Сохраните JSON отдельно. Исходники сохранены.");
                }
            }
            // Explicit edits to the JSON source must win over auto-filled
            // values inherited from the media input.
            bool titleEdited=s.metadataEdit && s.metadataEditTitle!=str(originalData,"title");
            bool artistEdited=s.metadataEdit && s.metadataEditArtist!=str(originalData,"artist");
            bool descriptionEdited=s.metadataEdit && s.metadataEditDescription!=str(originalData,"description");
            if(titleEdited || j->assemblyMetadataTitle.empty())metadataTitle=str(data,"title",metadataTitle);
            if(artistEdited || artist.empty())artist=str(data,"artist",str(data,"uploader"));
            if(descriptionEdited || description.empty())description=str(data,"description");
            for(auto tag=data.begin();tag!=data.end();++tag)if(metadataKeyAllowed(tag.key()) && tag->is_string()) {
                auto existing=std::find_if(effectiveGlobalTags.begin(),effectiveGlobalTags.end(),[&](const auto &item){return _stricmp(item.first.c_str(),tag.key().c_str())==0;});
                if(existing==effectiveGlobalTags.end())effectiveGlobalTags[tag.key()]=tag->get<std::string>();
                else if(!metadataTagChanged(j->originalGlobalTags,existing->first,existing->second))existing->second=tag->get<std::string>();
            }
            if(s.metadataEdit)for(auto tag=originalData.begin();tag!=originalData.end();++tag) {
                if(!metadataKeyAllowed(tag.key()) || !tag->is_string() || data.contains(tag.key()))continue;
                auto existing=std::find_if(effectiveGlobalTags.begin(),effectiveGlobalTags.end(),[&](const auto &item){return _stricmp(item.first.c_str(),tag.key().c_str())==0;});
                if(existing==effectiveGlobalTags.end()) {effectiveGlobalTags[tag.key()]="";removedMetadataTags.insert(tag.key());}
                else if(!metadataTagChanged(j->originalGlobalTags,existing->first,existing->second)) {existing->second="";removedMetadataTags.insert(existing->first);}
            }
            continue;
        }
        if(s.role==6) {
            if(j->muxOptions.container!=0)throw std::runtime_error("Для вложений и дополнительных потоков выберите MKV. Исходники сохранены.");
            auto probe=probeFile(path);bool found=false;
            for(const auto &stream:probe.value("streams",Json::array())) {
                auto type=str(stream,"codec_type");int index=int(number(stream,"index",-1));
                if(index==s.stream && (type=="attachment" || type=="data")) {
                    auto tags=stream.value("tags",Json::object());
                    extraMapped.push_back({input,index,type,str(tags,"filename"),str(stream,"codec_name"),str(tags,"mimetype")});found=true;break;
                }
            }
            if(!found)throw std::runtime_error("Вложение не найдено в исходном файле.");
            args.insert(args.end(),{L"-i",path.wstring()});++input;continue;
        }
        auto probe=probeFile(path);int count=0;
        for(const auto &stream:probe.value("streams",Json::array())) {
            auto type=str(stream,"codec_type");int role=type=="video"?0:type=="audio"?1:type=="subtitle"?2:-1;
            int index=int(number(stream,"index",-1));
            auto disposition=stream.value("disposition",Json::object());
            if(role==0&&number(disposition,"attached_pic")!=0)continue;
            if(role!=s.role && !(s.role==0&&s.includeAudio&&role==1))continue;
            if(s.stream>=0&&role==s.role&&index!=s.stream)continue;
            if(role==2 && role==s.role && s.codec>0 && isBitmapSubtitleCodec(str(stream,"codec_name")))
                throw std::runtime_error("Графические субтитры нельзя преобразовать в текст без распознавания. Оставьте исходный формат. Исходники сохранены.");
            if(s.codec==0 && role==s.role) {
                auto conflict=assemblyProbedCodecConflict(j->muxOptions.container,role,str(stream,"codec_name"));
                if(!conflict.empty())throw std::runtime_error(conflict+" Исходники сохранены.");
            }
            if(j->muxOptions.container>=4 && role==1 && s.codec==0) {
                auto codec=str(stream,"codec_name");auto required=j->muxOptions.container==4?"mp3":j->muxOptions.container==5?"aac":"flac";
                if(codec!=required)throw std::runtime_error("Исходный аудиокодек несовместим с выбранным форматом. Выберите соответствующий кодек в настройках дорожки. Исходники сохранены.");
            }
            double duration=muxDuration(stream);if(duration<=0)duration=muxDuration(probe.value("format",Json::object()));
            totalDuration=std::max(totalDuration,duration);
            mapped.push_back({n,input,index,role});++count;
        }
        if(!count && s.role==1 && s.optionalAudio)continue;
        if(!count)throw std::runtime_error("В источнике «"+s.name+"» нет дорожек назначенного типа. Измените роль в конструкторе.");
        args.insert(args.end(),{L"-i",path.wstring()});++input;
    }
    if(mapped.empty()||std::none_of(mapped.begin(),mapped.end(),[](const auto &m){return m.type<2;}))throw std::runtime_error("Нет доступных видео- или аудиодорожек");
    if(j->muxOptions.container>=4) {
        if(mapped.size()!=1 || mapped[0].type!=1)
            throw std::runtime_error("MP3, M4A и FLAC поддерживают одну аудиодорожку. Выберите другой формат или источник. Исходники сохранены.");
    }
    if(j->muxOptions.container==6 && j->sources[mapped[0].source].forced)
        throw std::runtime_error("FLAC не поддерживает признак принудительной дорожки. Снимите признак или выберите другой формат. Исходники сохранены.");
    static std::atomic_uint64_t temporarySerial{0};
    auto suffix=std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++temporarySerial);
    auto chapterFile=folder/(L"chapters-"+suffix+L".ffmetadata");
    struct ChapterCleanup {fs::path path;~ChapterCleanup(){std::error_code ec;fs::remove(path,ec);}} chapterCleanup{chapterFile};
    if(!chapters.empty()) {
        auto chapterExt=chapters.extension().wstring();std::transform(chapterExt.begin(),chapterExt.end(),chapterExt.begin(),::towlower);
        Json data;
        if(chapterSource && chapterSource->chaptersReady)data=editedChapters(*chapterSource);
        else if((chapterSource && chapterSource->embeddedKind=="chapters") || chapterExt==L".ffmetadata")data=chaptersFromProbe(probeFile(chapters));
        else {data=Json::parse(readFile(chapters,4*1024*1024));if(data.is_object())data=data.value("chapters",Json::array());}
        writeFile(chapterFile,chaptersToFfmetadata(data));expectedChapterList=data;expectedChapters=int(data.size());chapterInput=input;
        args.insert(args.end(),{L"-f",L"ffmetadata",L"-i",chapterFile.wstring()});++input;
    }
    if(j->muxOptions.container==4 && expectedChapters>255)
        throw std::runtime_error("MP3 поддерживает не более 255 глав. Выберите M4A или сохраните главы отдельно. Исходники сохранены.");
    std::vector<std::pair<std::string,std::string>> flacTags;
    if(j->muxOptions.container==6 && expectedChapters>0)flacTags=flacChapterTags(expectedChapterList,totalDuration);
    int coverInput=-1; std::string coverCodec;
    fs::path convertedCover;
    struct CoverCleanup {fs::path &path; ~CoverCleanup(){if(!path.empty()){std::error_code ec;fs::remove(path,ec);}}} coverCleanup{convertedCover};
    if(!cover.empty()) {
        if(j->muxOptions.container==3)
            throw std::runtime_error("Вложенная обложка не поддерживается в WebM. Выберите другой формат.");
        if(!coverSource || (coverSource->coverFormat!=0 && coverSource->coverFormat!=1))
            throw std::runtime_error("Выберите формат обложки PNG или JPG.");
        auto imageProbe=probeFile(cover);
        auto format=str(imageProbe.value("format",Json::object()),"format_name");
        auto streams=imageProbe.value("streams",Json::array());
        auto brand=str(imageProbe.value("format",Json::object()).value("tags",Json::object()),"major_brand");
        if(coverSource->embeddedKind!="cover" && format.find("image2")==std::string::npos && format.find("_pipe")==std::string::npos && format!="gif" && brand!="avif" && brand!="avis")
            throw std::runtime_error("Для обложки выберите файл изображения. Исходники сохранены.");
        if(coverSource->embeddedKind!="cover" && (streams.size()!=1 || str(streams[0],"codec_type")!="video"))
            throw std::runtime_error("Для обложки выберите файл изображения. Исходники сохранены.");
        bool png=coverSource->coverFormat==0;
        std::string originalCodec;
        for(const auto &stream:streams)if(coverSource->embeddedKind!="cover" || int(number(stream,"index",-1))==coverSource->stream){originalCodec=str(stream,"codec_name");break;}
        bool canCopy=(png && originalCodec=="png") || (!png && originalCodec=="mjpeg");
        convertedCover=folder/(L"cover-"+suffix+(png?L".png":L".jpg"));
        auto converted=runProcess(tool(L"ffmpeg.exe"),{L"-hide_banner",L"-loglevel",L"error",L"-nostdin",L"-n",L"-i",cover.wstring(),L"-map",coverSource->embeddedKind=="cover"?L"0:"+std::to_wstring(coverSource->stream):L"0:v:0",L"-frames:v",L"1",L"-c:v",canCopy?L"copy":png?L"png":L"mjpeg",L"-update",L"1",convertedCover.wstring()},nullptr,j->cancel,true);
        if(j->cancel||stopping)throw std::runtime_error("Отменено");
        if(converted.code || !fs::is_regular_file(convertedCover))
            throw std::runtime_error("Не удалось преобразовать обложку. Исходники сохранены. "+redact(converted.error));
        cover=convertedCover;coverCodec=png?"png":"mjpeg";
        if(j->muxOptions.container==1 || j->muxOptions.container>=4) {
            coverInput=input++;
            args.insert(args.end(),{L"-i",cover.wstring()});
        }
    }
    for(const auto &m:mapped)args.insert(args.end(),{L"-map",std::to_wstring(m.input)+L":"+std::to_wstring(m.stream)});
    if(coverInput>=0)args.insert(args.end(),{L"-map",std::to_wstring(coverInput)+L":v:0"});
    for(const auto &extra:extraMapped)args.insert(args.end(),{L"-map",std::to_wstring(extra.input)+L":"+std::to_wstring(extra.stream)});
    if(!extraMapped.empty())args.insert(args.end(),{L"-copy_unknown",L"-c:t",L"copy",L"-c:d",L"copy"});
    args.insert(args.end(),{L"-map_metadata",std::to_wstring(globalMetadataInput),L"-map_chapters",j->muxOptions.container==6?L"-1":std::to_wstring(chapterInput)});
    int attachmentIndex=0,dataIndex=0;
    for(const auto &extra:extraMapped) {
        auto spec=extra.type=="attachment"?L"t:"+std::to_wstring(attachmentIndex++):L"d:"+std::to_wstring(dataIndex++);
        args.insert(args.end(),{L"-map_metadata:s:"+spec,std::to_wstring(extra.input)+L":s:"+std::to_wstring(extra.stream)});
    }
    static const std::set<std::string> preservedFlags={"dub","original","comment","lyrics","karaoke","hearing_impaired","visual_impaired","clean_effects","timed_thumbnails","non_diegetic","captions","descriptions","metadata","dependent","still_image","multilayer"};
    int typeIndex[3]{},expected[3]{};bool hasPrimary[3]{},chosenPrimary[3]{},hasImported[3]{};
    for(const auto &m:mapped)if(j->sources[m.source].embeddedKind=="stream" && m.type==j->sources[m.source].role)hasImported[m.type]=true;
    for(const auto &m:mapped)if(j->sources[m.source].primary&&m.type==j->sources[m.source].role)hasPrimary[m.type]=true;
    for(const auto &m:mapped) {
        const auto &s=j->sources[m.source];int index=typeIndex[m.type]++;++expected[m.type];
        std::wstring spec=std::wstring(m.type==0?L"v":m.type==1?L"a":L"s")+L":"+std::to_wstring(index);
        bool primary=hasPrimary[m.type]?(s.primary&&s.role==m.type):(!hasImported[m.type]&&!chosenPrimary[m.type]);
        if(primary)chosenPrimary[m.type]=true;
        std::wstring disposition=primary?L"default":L"";
        if(s.forced && m.type==s.role)disposition+=(disposition.empty()?L"":L"+")+std::wstring(L"forced");
        if(m.type==s.role && s.originalDisposition.is_object())for(auto it=s.originalDisposition.begin();it!=s.originalDisposition.end();++it)
            if(preservedFlags.contains(it.key()) && number(s.originalDisposition,it.key().c_str())!=0)disposition+=(disposition.empty()?L"":L"+")+wide(it.key());
        if(disposition.empty())disposition=L"0";
        args.insert(args.end(),{L"-disposition:"+spec,disposition});
        args.insert(args.end(), {L"-map_metadata:s:"+spec, std::to_wstring(m.input)+L":s:"+std::to_wstring(m.stream)});
        if(s.metadataStream<0 || m.stream==s.metadataStream) {
            if(s.embeddedKind=="stream") {
                auto edit=[&](const char *field,const std::string &value,std::initializer_list<const char *> keys,const char *fallback) {
                    if(!metadataTagChanged(s.originalKnownTags,field,value))return;
                    auto key=metadataFirstKey(s.originalStreamTags,keys,fallback);
                    args.insert(args.end(),{L"-metadata:s:"+spec,wide(key+"="+value)});
                };
                edit("title",s.name,{"title","name"},"title");
                if(metadataTagChanged(s.originalKnownTags,"language",s.language))
                    args.insert(args.end(),{L"-metadata:s:"+spec,L"language="+(s.language.empty()?std::wstring():wide(normalizedLanguage(s.language)))});
                if(supportsExtendedTrackTags(j->muxOptions.container)) {
                    edit("description",s.description,{"description","comment"},"comment");
                    edit("artist",s.artist,{"artist"},"artist");edit("summary",s.summary,{"summary"},"SUMMARY");
                    edit("synopsis",s.synopsis,{"synopsis"},"SYNOPSIS");edit("date",s.date,{"date_released","date"},"DATE_RELEASED");
                }
                if(j->muxOptions.container==1 || j->muxOptions.container==2)edit("handler_name",s.handlerName,{"handler_name"},"handler_name");
            } else {
                args.insert(args.end(),{L"-metadata:s:"+spec,L"language="+(s.language.empty()?std::wstring():wide(normalizedLanguage(s.language)))});
                args.insert(args.end(),{L"-metadata:s:"+spec,L"name=",L"-metadata:s:"+spec,L"title="+wide(s.name)});
                if(supportsExtendedTrackTags(j->muxOptions.container)) {
                    args.insert(args.end(),{L"-metadata:s:"+spec,L"description="+wide(s.description),L"-metadata:s:"+spec,L"comment="+wide(s.description)});
                    args.insert(args.end(),{L"-metadata:s:"+spec,L"artist="+wide(s.artist),L"-metadata:s:"+spec,L"SUMMARY="+wide(s.summary)});
                    args.insert(args.end(),{L"-metadata:s:"+spec,L"SYNOPSIS="+wide(s.synopsis),L"-metadata:s:"+spec,L"DATE_RELEASED="+wide(s.date),L"-metadata:s:"+spec,L"date="+wide(s.date)});
                }
                if(j->muxOptions.container==1 || j->muxOptions.container==2)
                    args.insert(args.end(),{L"-metadata:s:"+spec,L"handler_name="+wide(s.handlerName)});
            }
            for(const auto &[key,value]:s.extraTags) {
                if(!metadataKeyAllowed(key))throw std::runtime_error("Некорректное имя тега дорожки.");
                if(metadataTagChanged(s.originalExtraTags,key,value))args.insert(args.end(),{L"-metadata:s:"+spec,wide(key+"="+value)});
            }
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
        if(j->muxOptions.container==0) {
            auto png=coverCodec=="png";
            auto spec=L"-metadata:s:t:"+std::to_wstring(attachmentIndex);
            auto original=coverSource?coverSource->originalCoverTags:Json::object();
            bool sameCodec=coverSource && coverSource->embeddedKind=="cover" && coverSource->probedCodec==coverCodec;
            auto filename=sameCodec?str(original,"filename"):std::string();
            auto mimetype=sameCodec?str(original,"mimetype"):std::string();
            if(filename.empty())filename=png?"cover.png":"cover.jpg";
            if(mimetype.empty())mimetype=png?"image/png":"image/jpeg";
            args.insert(args.end(),{L"-attach",cover.wstring(),spec,wide("mimetype="+mimetype),spec,wide("filename="+filename)});
            if(coverSource && !coverSource->name.empty())args.insert(args.end(),{spec,L"title="+wide(coverSource->name)});
            if(sameCodec && original.is_object())for(auto tag=original.begin();tag!=original.end();++tag) {
                std::string lower=tag.key();std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char ch){return char(std::tolower(ch));});
                if(lower=="title" || lower=="filename" || lower=="mimetype")continue;
                if(!tag->is_string() || tag.key().find('=')!=std::string::npos)throw std::runtime_error("Неподдерживаемый тег обложки. Исходники сохранены.");
                args.insert(args.end(),{spec,wide(tag.key()+"="+tag->get<std::string>())});
            }
        } else if(j->muxOptions.container==1 || j->muxOptions.container>=4) {
            auto spec=L"v:"+std::to_wstring(typeIndex[0]);
            args.insert(args.end(),{L"-c:"+spec,L"copy",L"-disposition:"+spec,L"attached_pic"});
            if(j->muxOptions.container==4 || j->muxOptions.container==6)
                args.insert(args.end(),{L"-metadata:s:"+spec,L"title=Album cover",L"-metadata:s:"+spec,L"comment=Cover (front)"});
        }
    }
    std::string descriptionKey=j->muxOptions.container==0?"SYNOPSIS":"description";
    if(originalGlobalMetadata.is_object())for(const char *key:{"description","synopsis","comment"}) {
        bool found=false;for(auto it=originalGlobalMetadata.begin();it!=originalGlobalMetadata.end();++it)if(_stricmp(it.key().c_str(),key)==0){descriptionKey=it.key();found=true;break;}
        if(found)break;
    }
    args.insert(args.end(),{L"-metadata",L"title="+wide(metadataTitle),L"-metadata",L"artist="+wide(artist),L"-metadata",wide(descriptionKey+"="+description)});
    for(const auto &[key,value]:effectiveGlobalTags) {
        if(!metadataKeyAllowed(key))throw std::runtime_error("Некорректное имя общего тега.");
        if(!value.empty() || metadataTagChanged(j->originalGlobalTags,key,value) || removedMetadataTags.count(key))args.insert(args.end(),{L"-metadata",wide(key+"="+value)});
    }
    if(j->muxOptions.container!=6 && expectedChapters>0) {
        for(size_t n=0;n<expectedChapterList.size();++n) {
            auto ordinal=std::to_wstring(n);
            auto title=str(expectedChapterList[n],"title","Глава");
            args.insert(args.end(),{L"-metadata:c:"+ordinal,L"title="+wide(title)});
            auto tags=expectedChapterList[n].value("tags",Json::object());
            if(tags.is_object())for(auto tag=tags.begin();tag!=tags.end();++tag) {
                if(_stricmp(tag.key().c_str(),"title")==0)continue;
                if(!tag->is_string() || tag.key().find('=')!=std::string::npos)throw std::runtime_error("Не удалось сохранить тег главы. Исходники сохранены.");
                args.insert(args.end(),{L"-metadata:c:"+ordinal,wide(tag.key()+"="+tag->get<std::string>())});
            }
        }
    }
    for(const auto &[key,value]:flacTags)args.insert(args.end(),{L"-metadata",wide(key+"="+value)});

    auto extension=wide(muxContainer(j->muxOptions));auto partial=folder/(L"result.part-"+suffix+L"."+extension),final=folder/(L"result."+extension);
    bool hasGlobalExtra=std::any_of(effectiveGlobalTags.begin(),effectiveGlobalTags.end(),[&](const auto &tag){return !tag.second.empty() || metadataTagChanged(j->originalGlobalTags,tag.first,tag.second) || removedMetadataTags.count(tag.first);});
    if ((j->muxOptions.container == 1 || j->muxOptions.container == 2) && (hasGlobalExtra || globalMetadataInput>=0 || (j->muxOptions.container == 2 && !description.empty())))
        args.insert(args.end(), {L"-movflags", L"use_metadata_tags"});
    args.push_back(partial.wstring());
    auto r=runFfmpegWithProgress(j,args,totalDuration);
    if(j->cancel||stopping)throw std::runtime_error("Отменено");
    if(r.code){auto detail=redact(r.error);if(detail.size()>600)detail=detail.substr(detail.size()-600);throw std::runtime_error("Не удалось собрать контейнер. Исходники сохранены отдельно. Проверьте совместимость кодеков и контейнера. "+detail);}
    j->workerOutput=utf8(partial.wstring());
    if(!cover.empty() && j->muxOptions.container==2)writeMovCover(partial,cover,coverCodec=="png");
    else if(!cover.empty() && j->muxOptions.container==1 && !hasCover(probeFile(partial),1))writeMovCover(partial,cover,coverCodec=="png");
    auto output=probeFile(partial);int actual[3]{};
    for(const auto &s:output.value("streams",Json::array())){auto type=str(s,"codec_type");if(type=="video" && number(s.value("disposition",Json::object()),"attached_pic")==0)++actual[0];if(type=="audio")++actual[1];if(type=="subtitle")++actual[2];}
    for(int i=0;i<3;i++)if(actual[i]<expected[i])throw std::runtime_error("В итоговом файле отсутствуют выбранные дорожки. Исходники сохранены.");
    auto globalOutputTags=output.value("format",Json::object()).value("tags",Json::object());
    if((!metadataTitle.empty() && !metadataTagMatchesAny(globalOutputTags,{"title","name"},metadataTitle)) ||
       (!artist.empty() && !metadataTagMatchesAny(globalOutputTags,{"artist","performer"},artist)) ||
       (!description.empty() && !metadataTagMatchesAny(globalOutputTags,{"description","synopsis","comment"},description)))
        throw std::runtime_error("Контейнер не сохранил название, автора или описание. Выберите другой формат. Исходники сохранены.");
    if(originalGlobalMetadata.is_object())for(auto tag=originalGlobalMetadata.begin();tag!=originalGlobalMetadata.end();++tag) {
        std::string lower=tag.key();std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char ch){return char(std::tolower(ch));});
        if(!tag->is_string() || lower=="encoder" || lower=="major_brand" || lower=="minor_version" || lower=="compatible_brands" || lower=="title" || lower=="artist" || _stricmp(tag.key().c_str(),descriptionKey.c_str())==0)continue;
        bool overridden=false;for(const auto &[key,value]:effectiveGlobalTags)if(_stricmp(key.c_str(),tag.key().c_str())==0){overridden=true;break;}
        if(!overridden && !metadataTagMatches(globalOutputTags,tag.key(),tag->get<std::string>()))
            throw std::runtime_error("Контейнер не сохранил исходный тег «"+tag.key()+"». Выберите другой формат. Исходники сохранены.");
    }
    for(const auto &[key,value]:effectiveGlobalTags) {
        if((!value.empty() || metadataTagChanged(j->originalGlobalTags,key,value) || removedMetadataTags.count(key)) && !metadataTagMatches(output.value("format",Json::object()).value("tags",Json::object()),key,value))
            throw std::runtime_error("Контейнер не сохранил общий тег «"+key+"». Выберите другой формат или удалите тег. Исходники сохранены.");
    }
    int checkedType[3]{};
    for(const auto &m:mapped) {
        int ordinal=checkedType[m.type]++;const auto &source=j->sources[m.source];
        if(source.metadataStream>=0 && m.stream!=source.metadataStream)continue;
        Json tags=Json::object(),outputStream=Json::object();int found=0;
        for(const auto &stream:output.value("streams",Json::array())) {
            auto type=str(stream,"codec_type");int role=type=="video"?0:type=="audio"?1:type=="subtitle"?2:-1;
            if(role==0 && number(stream.value("disposition",Json::object()),"attached_pic")!=0)continue;
            if(role==m.type && found++==ordinal){outputStream=stream;tags=stream.value("tags",Json::object());break;}
        }
        if(source.embeddedKind=="stream" && m.type==source.role) {
            if(source.codec==0) {
                auto mismatch=copiedStreamDifference(source.originalStreamInfo,outputStream);
                if(!mismatch.empty())throw std::runtime_error("Контейнер изменил характеристику дорожки «"+mismatch+"». Исходники сохранены.");
            }
            auto actualDisposition=outputStream.value("disposition",Json::object());
            // FLAC carries one audio stream, implicitly the default; the
            // format does not encode FFmpeg's default disposition flag.
            bool defaultMismatch=j->muxOptions.container!=6 && (number(actualDisposition,"default")!=0)!=source.primary;
            if(defaultMismatch || (number(actualDisposition,"forced")!=0)!=source.forced)
                throw std::runtime_error("Контейнер изменил признаки дорожки по умолчанию или принудительной дорожки. Исходники сохранены.");
            if(source.originalDisposition.is_object())for(auto flag=source.originalDisposition.begin();flag!=source.originalDisposition.end();++flag)
                if(preservedFlags.contains(flag.key()) && number(source.originalDisposition,flag.key().c_str())!=0 && number(actualDisposition,flag.key().c_str())==0)
                    throw std::runtime_error("Контейнер не сохранил признак дорожки «"+flag.key()+"». Исходники сохранены.");
        }
        auto checkKnown=[&](const char *key,const std::string &value,std::initializer_list<const char*> aliases) {
            if(metadataTagChanged(source.originalKnownTags,key,value) && !metadataTagMatchesAny(tags,aliases,value))
                throw std::runtime_error("Контейнер не сохранил изменённое поле дорожки «"+std::string(key)+"». Выберите другой формат. Исходники сохранены.");
        };
        checkKnown("title",source.name,{"title","name"});
        if(metadataTagChanged(source.originalKnownTags,"language",source.language) &&
           !metadataTagMatchesAny(tags,{"language"},source.language.empty()?std::string():normalizedLanguage(source.language)))
            throw std::runtime_error("Контейнер не сохранил язык дорожки. Выберите другой формат. Исходники сохранены.");
        if(supportsExtendedTrackTags(j->muxOptions.container)) {
            if(metadataTagChanged(source.originalKnownTags,"description",source.description)) {
                auto key=metadataFirstKey(source.originalStreamTags,{"description","comment"},"comment");
                if(!metadataTagMatches(tags,key,source.description))throw std::runtime_error("Контейнер не сохранил описание дорожки. Исходники сохранены.");
            }
            checkKnown("artist",source.artist,{"artist"});checkKnown("summary",source.summary,{"summary"});
            checkKnown("synopsis",source.synopsis,{"synopsis"});
            if(metadataTagChanged(source.originalKnownTags,"date",source.date)) {
                auto key=metadataFirstKey(source.originalStreamTags,{"date_released","date"},"DATE_RELEASED");
                if(!metadataTagMatches(tags,key,source.date))throw std::runtime_error("Контейнер не сохранил дату дорожки. Исходники сохранены.");
            }
        }
        if(j->muxOptions.container==1 || j->muxOptions.container==2)checkKnown("handler_name",source.handlerName,{"handler_name"});
        if(source.embeddedKind=="stream" && source.originalStreamTags.is_object())for(auto tag=source.originalStreamTags.begin();tag!=source.originalStreamTags.end();++tag) {
            if(!tag->is_string())continue;
            std::string lower=tag.key();std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char ch){return char(std::tolower(ch));});
            if(lower=="encoder" || lower=="duration" || lower=="number_of_frames" || lower=="number_of_bytes" || lower.rfind("_statistics_",0)==0)continue;
            auto changedKey=[&](const char *field,const std::string &value,std::initializer_list<const char*> aliases,const char *fallback) {
                return metadataTagChanged(source.originalKnownTags,field,value) && _stricmp(tag.key().c_str(),metadataFirstKey(source.originalStreamTags,aliases,fallback).c_str())==0;
            };
            if(changedKey("title",source.name,{"title","name"},"title") || changedKey("language",source.language,{"language"},"language") ||
               changedKey("description",source.description,{"description","comment"},"comment") || changedKey("artist",source.artist,{"artist"},"artist") ||
               changedKey("summary",source.summary,{"summary"},"SUMMARY") || changedKey("synopsis",source.synopsis,{"synopsis"},"SYNOPSIS") ||
               changedKey("date",source.date,{"date_released","date"},"DATE_RELEASED") || changedKey("handler_name",source.handlerName,{"handler_name"},"handler_name"))continue;
            bool extraChanged=false;for(const auto &[key,value]:source.extraTags)if(_stricmp(key.c_str(),tag.key().c_str())==0 && metadataTagChanged(source.originalExtraTags,key,value)){extraChanged=true;break;}
            if(!extraChanged && !metadataTagMatches(tags,tag.key(),tag->get<std::string>()))
                throw std::runtime_error("Контейнер не сохранил исходный тег дорожки «"+tag.key()+"». Исходники сохранены.");
        }
        for(const auto &[key,value]:source.extraTags)
            if(metadataTagChanged(source.originalExtraTags,key,value) && !metadataTagMatches(tags,key,value))
                throw std::runtime_error("Контейнер не сохранил тег дорожки «"+key+"». Выберите другой формат или отмените изменение. Исходники сохранены.");
    }
    if(int(output.value("chapters",Json::array()).size())<expectedChapters)throw std::runtime_error("Контейнер не сохранил все главы. Выберите другой формат.");
    if(expectedChapters>0) {
        auto actualChapters=chaptersFromProbe(output);
        for(size_t n=0;n<expectedChapterList.size();++n) {
            const auto &wanted=expectedChapterList[n];const auto &actualChapter=actualChapters[n];
            auto title=str(wanted,"title","Глава");
            auto wantedTags=wanted.value("tags",Json::object());auto actualTags=actualChapter.value("tags",Json::object());
            if(wantedTags.is_object())for(auto tag=wantedTags.begin();tag!=wantedTags.end();++tag) {
                std::string lower=tag.key();std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char ch){return char(std::tolower(ch));});
                if(lower=="title")continue;
                if(!tag->is_string())throw std::runtime_error("Неподдерживаемые метаданные главы. Исходники сохранены.");
                auto value=tag->get<std::string>();
                if(!metadataTagMatches(actualTags,tag.key(),value))throw std::runtime_error("Контейнер не сохранил тег главы «"+tag.key()+"». Исходники сохранены.");
            }
            if(std::abs(number(wanted,"start_time",-1)-number(actualChapter,"start_time",-1))>0.05 ||
               std::abs(number(wanted,"end_time",-1)-number(actualChapter,"end_time",-1))>0.05 ||
               title!=str(actualChapter,"title","Глава"))
                throw std::runtime_error("Контейнер изменил границы или названия глав. Выберите другой формат. Исходники сохранены.");
        }
    }
    std::vector<bool> matchedExtra(output.value("streams",Json::array()).size(),false);
    for(const auto &extra:extraMapped) {
        bool found=false;auto streams=output.value("streams",Json::array());
        for(size_t n=0;n<streams.size();++n) {
            const auto &stream=streams[n];if(matchedExtra[n] || str(stream,"codec_type")!=extra.type)continue;
            auto tags=stream.value("tags",Json::object());
            if((!extra.name.empty() && str(tags,"filename")!=extra.name) || (!extra.codec.empty() && str(stream,"codec_name")!=extra.codec) || (!extra.mime.empty() && str(tags,"mimetype")!=extra.mime))continue;
            matchedExtra[n]=true;found=true;break;
        }
        if(!found)throw std::runtime_error("Контейнер не сохранил одно из вложений. Исходники сохранены.");
    }
    if(!cover.empty()&&!hasCover(output,j->muxOptions.container))throw std::runtime_error("Обложка не сохранилась в контейнере. Исходники сохранены.");
    if(coverSource && coverSource->embeddedKind=="cover" && coverSource->probedCodec==coverCodec && coverSource->originalCoverTags.is_object()) {
        Json coverTags=Json::object();
        for(const auto &stream:output.value("streams",Json::array())) {
            auto codec=str(stream,"codec_name");if(codec!="png" && codec!="mjpeg")continue;
            auto tags=stream.value("tags",Json::object());auto filename=str(tags,"filename");std::transform(filename.begin(),filename.end(),filename.begin(),[](unsigned char ch){return char(std::tolower(ch));});
            if(number(stream.value("disposition",Json::object()),"attached_pic") || filename=="cover.png" || filename=="cover.jpg" || filename=="cover.jpeg"){coverTags=tags;break;}
        }
        auto titleKey=metadataFirstKey(coverSource->originalCoverTags,{"title"},"");
        auto originalTitle=titleKey.empty()?std::string():str(coverSource->originalCoverTags,titleKey.c_str());
        if(!originalTitle.empty() && !metadataTagMatches(coverTags,"title",coverSource->name))
            throw std::runtime_error("Контейнер не сохранил название обложки. Исходники сохранены.");
        for(auto tag=coverSource->originalCoverTags.begin();tag!=coverSource->originalCoverTags.end();++tag) {
            std::string lower=tag.key();std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char ch){return char(std::tolower(ch));});
            if(lower=="filename" || lower=="mimetype" || lower=="title")continue;
            if(!tag->is_string() || !metadataTagMatches(coverTags,tag.key(),tag->get<std::string>()))throw std::runtime_error("Контейнер не сохранил тег обложки «"+tag.key()+"». Исходники сохранены.");
        }
    }
    if(fs::exists(final))throw std::runtime_error("Итоговый файл уже существует; сохранённый файл не заменён");
    fs::rename(partial,final);j->workerOutput=utf8(final.wstring());j->workerCategory=actual[0]?"Видео":"Аудио";
}
}
