#include "app.hpp"
#include "language_options.hpp"
#include <fstream>
namespace ss {
void App::openConstructor(bool automatic, bool completed) {
    sourcePool.clear(); draft.clear(); draftSelection=-1;
    assemblyName = title.empty() ? "Мой контейнер" : title; assemblyArtist.clear(); assemblyDescription.clear();
    if(completed) for(const auto &job:jobs) if(job->id==selectedJob&&job->assembly){
        draft=job->sources;assemblyName=job->title;assemblyArtist=job->assemblyArtist;assemblyDescription=job->assemblyDescription;muxOptions=job->muxOptions;
        for(auto &source:draft){
            if(!source.dependency.empty())for(const auto &child:jobs)if(child->id==source.dependency&&child->done&&child->state=="Готово")source.path=child->output;
            source.dependency.clear();source.enabled=true;
        }
        sourcePool=draft;
        const size_t initialCount=sourcePool.size();
        for(size_t i=0;i<initialCount;++i)if(sourcePool[i].role==0&&sourcePool[i].includeAudio){
            auto audio=sourcePool[i];audio.role=1;audio.stream=-1;audio.includeAudio=false;audio.derivedAudio=true;audio.name+=" · звук";sourcePool.push_back(audio);
        }
        constructorOpen=true;downloadChoice=false;requestedPage=0;draftSelection=draft.empty()?-1:0;
        if(job->waitingSources&&!job->claimed){job->cancel=true;job->done=true;job->state="Отменено";}
        return;
    }
    if (completed) {
        for (const auto &j : jobs) if (selectedJobs.count(j->id) && j->done && j->state=="Готово" && fs::is_regular_file(wide(j->output))) {
            auto source=assemblySource(j->item,j->output);sourcePool.push_back(source);
            if(source.role==0 && !j->item.videoOnly && j->item.acodec!="none") {
                auto audio=source;audio.role=1;audio.derivedAudio=true;audio.optionalAudio=j->item.acodec.empty();
                audio.name=source.name+(audio.optionalAudio?" · звук (если есть)":" · звук");sourcePool.push_back(audio);
            }
        }
    } else {
        for (const auto &m : media) if (m.selected && !m.drm) {
            auto source=assemblySource(m); sourcePool.push_back(source);
            if(source.role==0 && !m.videoOnly && m.acodec!="none") {
                auto audio=source; audio.role=1; audio.derivedAudio=true; audio.optionalAudio=m.acodec.empty();
                audio.name=m.title+(audio.optionalAudio?" · звук (если есть)":" · звук"); sourcePool.push_back(audio);
            }
        }
    }
    constructorOpen=true; downloadChoice=false; requestedPage=0;
    muxOptions={};
    if (automatic) {
        for (const auto &s : sourcePool) if ((completed || s.media.selected) && !s.derivedAudio) draft.push_back(s);
        autoConstruct();
    }
}
void App::autoConstruct() {
    if (draft.empty()) for (const auto &s : sourcePool) if ((s.media.selected || !s.path.empty()) && !s.derivedAudio) draft.push_back(s);
    std::stable_sort(draft.begin(),draft.end(),[](const auto &a,const auto &b){return a.role<b.role;});
    // Auxiliary sections accept one source each; the remaining choices stay in the pool.
    bool auxiliary[6]{};
    draft.erase(std::remove_if(draft.begin(),draft.end(),[&](const auto &s){if(s.role<3)return false;bool duplicate=auxiliary[s.role];auxiliary[s.role]=true;return duplicate;}),draft.end());
    bool first[6]{};
    for (auto &s : draft) { s.primary=s.role<3 && !first[s.role]; first[s.role]=true; }
    draftSelection=draft.empty()?-1:0;
}
void App::importAssemblyFile() {
    work([this] {
        auto path=pickFile(hwnd); if(path.empty())return;
        Media m; m.title=utf8(fs::path(wide(path)).filename().wstring());
        auto ext=fs::path(wide(path)).extension().wstring(); std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);
        m.ext=utf8(ext.empty()?L"":ext.substr(1)); m.key="local:"+path;
        if(m.ext=="json") { m.category="Данные"; m.id="chapters";
            auto data=Json::parse(readFile(wide(path),4*1024*1024),nullptr,false);
            if(!data.is_array())m.id="metadata";
            post([this,m,path]{sourcePool.push_back(assemblySource(m,path));}); return;
        }
        if(m.ext=="png"||m.ext=="jpg"||m.ext=="jpeg") {
            m.category="Изображения"; post([this,m,path]{sourcePool.push_back(assemblySource(m,path));});return;
        }
        std::atomic_bool cancel{false},finished{false};
        std::thread watchdog([&]{for(int i=0;i<150&&!finished&&!stopping;i++)Sleep(100);if(!finished)cancel=true;});
        ProcessResult r;
        try { std::shared_lock lock(toolsMutex); r=runProcess(tool(L"ffprobe.exe"),{L"-v",L"error",L"-show_streams",L"-of",L"json",wide(path)},nullptr,cancel,true); }
        catch(...){finished=true;watchdog.join();throw;}
        finished=true;watchdog.join();
        auto probe=Json::parse(r.output,nullptr,false);
        if(r.code||!probe.is_object())throw std::runtime_error("Не удалось прочитать дорожки локального файла");
        std::vector<AssemblySource> imported;
        bool hasVideo=false;
        for(const auto &stream:probe.value("streams",Json::array()))if(str(stream,"codec_type")=="video" && number(stream.value("disposition",Json::object()),"attached_pic")!=1)hasVideo=true;
        for(const auto &stream:probe.value("streams",Json::array())) {
            auto type=str(stream,"codec_type"); if(type!="video"&&type!="audio"&&type!="subtitle")continue;
            auto item=m; item.category=type=="video"?"Видео":type=="audio"?"Аудио":"Субтитры";
            auto s=assemblySource(item,path);s.role=type=="video"?0:type=="audio"?1:2;s.stream=int(number(stream,"index"));s.includeAudio=false;s.derivedAudio=hasVideo&&s.role==1;
            auto tags=stream.value("tags",Json::object());s.language=str(tags,"language");s.name=str(tags,"title",m.title)+" · #"+std::to_string(s.stream);
            s.description=str(tags,"description",str(tags,"comment"));s.artist=str(tags,"artist");s.date=str(tags,"date");
            imported.push_back(s);
        }
        if(imported.empty())throw std::runtime_error("В файле не найдены поддерживаемые дорожки");
        post([this,imported]{sourcePool.insert(sourcePool.end(),imported.begin(),imported.end());});
    });
}
void App::enqueueAssembly() {
    if(draft.empty())throw std::runtime_error("Добавьте медиа в конструктор");
    if(draft.size()>64)throw std::runtime_error("В одной сборке допускается до 64 источников");
    if(!fs::path(wide(destination)).is_absolute())throw std::runtime_error("Выберите полную папку сохранения");
    if(assemblyName.empty())throw std::runtime_error("Укажите имя контейнера");
    (void)muxContainer(muxOptions);
    int primary[3]{}; bool playable=false; int cover=0,chapters=0,metadata=0;
    for(const auto &s:draft) {
        if(s.role<0||s.role>5||s.codec<0||s.codec>(s.role==0?10:4)||s.quality<0||s.quality>2)throw std::runtime_error("Некорректные параметры дорожки");
        auto numeric=[](const std::string &v,int limit){if(v.empty())return true;if(v.size()>7||!std::all_of(v.begin(),v.end(),[](unsigned char c){return std::isdigit(c);}))return false;try{int n=std::stoi(v);return n>0&&n<=limit;}catch(...){return false;}};
        if(!numeric(s.bitrate,1000000)||!numeric(s.maxrate,1000000)||!numeric(s.bufsize,2000000)||!numeric(s.sampleRate,384000)||!numeric(s.frameRate,240)||!numeric(s.width,16384))throw std::runtime_error("Проверьте числовые параметры перекодирования: укажите положительные значения без единиц измерения");
        if(s.role==0&&(s.maxrate.empty()!=s.bufsize.empty()))throw std::runtime_error("Укажите максимальный битрейт и размер буфера вместе");
        if(s.role==1&&s.codec==4&&!s.bitrate.empty())throw std::runtime_error("FLAC использует качество без потерь; уберите заданный битрейт");
        if(s.codec==0&&(!s.bitrate.empty()||!s.maxrate.empty()||!s.bufsize.empty()||!s.sampleRate.empty()||!s.frameRate.empty()||!s.width.empty()))throw std::runtime_error("Для расширенных параметров выберите кодек перекодирования");
        if(muxOptions.container!=0 && s.role<3 && (!s.description.empty()||!s.artist.empty()||!s.date.empty()))throw std::runtime_error("Этот контейнер не гарантирует сохранение описания, автора и даты отдельной дорожки. Выберите MKV или очистите эти поля.");
        if(s.role>=3 && (!s.description.empty()||!s.artist.empty()||!s.date.empty()||!s.language.empty()))throw std::runtime_error("Для обложки, глав и источника метаданных дополнительные поля дорожки не поддерживаются. Очистите их или измените исходные данные.");
        if(!s.language.empty() && normalizedLanguage(s.language)=="und") {
            auto value=wide(s.language);
            if(CompareStringOrdinal(value.c_str(),-1,L"und",-1,TRUE)!=CSTR_EQUAL && CompareStringOrdinal(value.c_str(),-1,L"Не определён",-1,TRUE)!=CSTR_EQUAL && CompareStringOrdinal(value.c_str(),-1,L"Undetermined",-1,TRUE)!=CSTR_EQUAL)
                throw std::runtime_error("Введите существующий код ISO 639 или название языка; для неизвестного языка укажите und");
        }
        if(!s.date.empty()) {
            static const std::regex datePattern(R"(^([0-9]{4})(-([0-9]{2})-([0-9]{2}))?$)");std::smatch match;
            if(!std::regex_match(s.date,match,datePattern))throw std::runtime_error("Дата должна быть в формате ГГГГ или ГГГГ-ММ-ДД");
            if(std::stoi(match[1])<1)throw std::runtime_error("Год должен быть больше нуля");
            if(match[2].matched){int year=std::stoi(match[1]),month=std::stoi(match[3]),day=std::stoi(match[4]);int days[]={31,28,31,30,31,30,31,31,30,31,30,31};if(year%4==0&&(year%100!=0||year%400==0))days[1]=29;if(month<1||month>12||day<1||day>days[month-1])throw std::runtime_error("Укажите существующую календарную дату");}
        }
        bool unavailable=s.codec>0&&((s.role==0&&!videoEncoders[s.codec])||(s.role==1&&!audioEncoders[s.codec])||(s.role==2&&!subtitleEncoders[s.codec]));
        if(unavailable)throw std::runtime_error(videoEncodersReady?"Выбранный кодировщик недоступен. Выберите другой кодек.":"Проверка кодировщиков ещё выполняется. Дождитесь её завершения.");
        if(s.media.drm)throw std::runtime_error("Защищённые потоки нельзя скачать");
        if(s.primary&&s.role<3&&++primary[s.role]>1)throw std::runtime_error("В каждой категории выберите одну дорожку по умолчанию");
        playable|=s.role<2; cover+=s.role==3;chapters+=s.role==4;metadata+=s.role==5;
        if(!s.path.empty()&&!fs::is_regular_file(wide(s.path)))throw std::runtime_error("Локальный файл недоступен");
    }
    if(!playable)throw std::runtime_error("Добавьте видео или аудио");
    if(cover>1||chapters>1||metadata>1)throw std::runtime_error("Выберите одну обложку, один источник глав и один источник метаданных");
    if(cover&&muxOptions.container!=0)throw std::runtime_error("Для вложенной обложки выберите MKV");
    auto parent=std::make_shared<Job>();parent->assembly=true;parent->mux=true;parent->muxOptions=muxOptions;
    parent->id=std::to_string(GetTickCount64())+"-"+std::to_string(++jobSerial);
    parent->title=assemblyName;parent->item.title=assemblyName;parent->item.category="Требуют проверки";
    parent->assemblyArtist=assemblyArtist;parent->assemblyDescription=assemblyDescription;
    parent->folder=utf8((fs::path(wide(destination))/wide(parent->id+"-container")).wstring());
    parent->outputDirectory=destination;parent->waitingSources=true;parent->state="Ожидание источников";
    std::vector<std::shared_ptr<Job>> children;
    std::map<std::string,std::string> downloads;
    for(auto source:draft) {
        source.enabled=true;
        if(source.path.empty()) {
            auto identity=source.media.key.empty()?source.media.page+"|"+source.media.id+"|"+source.media.url:source.media.key;
            auto previous=downloads.find(identity);
            if(previous!=downloads.end()){source.dependency=previous->second;if(source.codec>0)for(auto &child:children)if(child->id==source.dependency)child->conversionSource=true;parent->sources.push_back(source);continue;}
            auto child=std::make_shared<Job>();child->id=std::to_string(GetTickCount64())+"-"+std::to_string(++jobSerial);
            child->item=source.media;child->title=source.name;child->page=source.media.page;child->assemblyInput=true;child->conversionSource=source.codec>0;
            child->folder=utf8((fs::path(wide(destination))/wide(parent->id+"-sources")/wide(child->id)).wstring());
            child->outputDirectory=utf8((fs::path(wide(destination))/wide(parent->id+"-sources")).wstring());
            auto it=mediaSessions.find(source.media.key);child->session=it==mediaSessions.end()?session():it->second;
            if(child->item.category=="Данные")child->local=child->item.id=="chapters"?analysis.raw.value("chapters",Json::array()).dump(2):sanitizeMetadata(analysis.raw);
            source.dependency=child->id;downloads[identity]=child->id;children.push_back(child);
        }
        parent->sources.push_back(source);
    }
    {std::lock_guard lock(jobsMutex);jobs.insert(jobs.end(),children.begin(),children.end());jobs.push_back(parent);}
    selectedJob=parent->id;constructorOpen=false;requestedPage=2;
    saveHistory();jobsCv.notify_all();notify("Загрузка источников и сборка добавлены в очередь");
}
void App::resolveAssembly(const std::shared_ptr<Job> &parent,bool partial) {
    auto resolved=parent->sources;bool missing=false,playable=false;
    for(auto &s:resolved) {
        if(!s.dependency.empty()) {
            auto it=std::find_if(jobs.begin(),jobs.end(),[&](const auto &j){return j->id==s.dependency;});
            if(it!=jobs.end()&&(*it)->done&&(*it)->state=="Готово")s.path=(*it)->output;
            else if(it!=jobs.end())s.path.clear();
        }
        bool exists=!s.path.empty()&&fs::is_regular_file(wide(s.path));
        if(!exists){missing=true;if(partial)s.enabled=false;}
        if(exists&&s.enabled&&s.role<2)playable=true;
    }
    if(missing&&!partial)throw std::runtime_error("Часть источников не скачана");
    if(!playable)throw std::runtime_error("Нет сохранённых видео- или аудиодорожек для сборки. Файлы остальных типов остаются на диске.");
    {std::lock_guard lock(jobsMutex);parent->sources=std::move(resolved);parent->decisionNeeded=false;parent->waitingSources=false;parent->state="Ожидание";}
    saveHistory();jobsCv.notify_all();
}
void App::updateAssemblies() {
    for(const auto &parent:jobs) {
        if(!parent->assembly||!parent->waitingSources||parent->done||parent->cancel||parent->decisionNeeded)continue;
        bool waiting=false,missing=false;int saved=0;std::string failed,paths;
        for(const auto &s:parent->sources) {
            std::string path=s.path;
            if(!s.dependency.empty()) {
                auto it=std::find_if(jobs.begin(),jobs.end(),[&](const auto &j){return j->id==s.dependency;});
                if(it!=jobs.end()&&(!(*it)->done||((*it)->state!="Готово"&&(*it)->state!="Ошибка"&&(*it)->state!="Отменено"&&(*it)->state!="Ожидание повтора"))){waiting=true;continue;}
                if(it!=jobs.end())path=(*it)->state=="Готово"?(*it)->output:"";
            }
            if(path.empty()||!fs::is_regular_file(wide(path))){missing=true;failed+="- "+s.name+"\n";}
            else {++saved;paths+=path+"\n";}
        }
        if(waiting)continue;
        if(!missing) {try{resolveAssembly(parent,false);}catch(const std::exception &e){parent->decisionNeeded=true;parent->state="Нужен выбор";parent->detail=e.what();}continue;}
        if(!modalTitle.empty())continue;
        parent->decisionNeeded=true;parent->state="Нужен выбор";parent->detail="Не все источники доступны. Сохранённые файлы оставлены отдельно.";
        modalTitle="Не все медиа удалось скачать";
        modalBody="Не скачаны или недоступны:\n"+failed+"\nПодтверждено файлов: "+std::to_string(saved)+"\n"+paths+"\nФайлы, скачанные до ошибки, могут оставаться в папках задач. Повторная попытка проверит их снова. Можно также собрать контейнер без отсутствующих медиа. Сохранённые файлы не удаляются.";
        modalConfirmLabel="Собрать доступное";
        bool playable=false;for(const auto &s:parent->sources)if(s.role<2){for(const auto &child:jobs)if(child->id==s.dependency&&child->done&&child->state=="Готово"&&!child->output.empty())playable=true;if(s.dependency.empty()&&!s.path.empty()&&fs::is_regular_file(wide(s.path)))playable=true;}
        modalConfirm=playable?std::function<void()>([this,parent]{try{resolveAssembly(parent,true);}catch(const std::exception &e){error(e.what());}}):nullptr;
        selectedJob=parent->id;requestedPage=2;saveHistory();
    }
}
}
