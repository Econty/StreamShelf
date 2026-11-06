#include "app.hpp"
#include "language_options.hpp"
#include <fstream>
namespace ss {
void App::openConstructor(bool completed) {
    ++constructorGeneration;constructorPending=0;
    sourcePool.clear(); draft.clear(); draftSelection=-1; singleExportMode=false;constructorModeManual=false;constructorFormatManual=false;
    assemblyName = title.empty() ? "Мой контейнер" : title; assemblyMetadataTitle.clear(); assemblyArtist.clear(); assemblyDescription.clear(); globalTags.clear(); originalGlobalTags.clear(); globalTagName.clear(); globalTagsLoaded=false;
    for(const char *key:{"album","album_artist","composer","genre","date","copyright","publisher"})globalTags[key]="";
    if(completed) for(const auto &job:jobs) if(job->id==selectedJob&&job->assembly){
        singleExportMode=job->singleExport;
        sourcePool=job->sources;assemblyName=job->title;assemblyMetadataTitle=job->assemblyMetadataTitle;assemblyArtist=job->assemblyArtist;assemblyDescription=job->assemblyDescription;globalTags=job->globalTags;originalGlobalTags=job->originalGlobalTags;globalTagsLoaded=true;muxOptions=job->muxOptions;
        for(auto &source:sourcePool){
            if(job->singleExport){source.exportFormat=job->exportFormat;source.exportQuality=job->exportQuality;}
            if(!source.dependency.empty())for(const auto &child:jobs)if(child->id==source.dependency&&child->done&&child->state=="Готово")source.path=child->output;
            source.dependency.clear();source.enabled=true;
        }
        const size_t initialCount=sourcePool.size();
        for(size_t i=0;i<initialCount;++i)if(sourcePool[i].role==0&&sourcePool[i].includeAudio){
            auto audio=sourcePool[i];audio.role=1;audio.stream=-1;audio.includeAudio=false;audio.derivedAudio=true;audio.name+=" · звук";sourcePool.push_back(audio);
        }
        constructorOpen=true;downloadChoice=false;requestedPage=0;
        return;
    }
    if (completed) {
        for (const auto &j : jobs) if (selectedJobs.count(j->id) && j->done && j->state=="Готово" && fs::is_regular_file(wide(j->output))) {
            auto extension=fs::path(wide(j->output)).extension().wstring();std::transform(extension.begin(),extension.end(),extension.begin(),::towlower);
            if(j->item.category=="Изображения" || j->item.category=="Превью" || extension==L".json" || extension==L".ffmetadata" || extension==L".txt") {
                sourcePool.push_back(assemblySource(j->item,j->output));continue;
            }
            queueConstructorProbe(j->item,j->output,constructorGeneration);
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
    singleExportMode=!sourcePool.empty() && std::all_of(sourcePool.begin(),sourcePool.end(),[](const auto &s){return s.role>=2;});
    constructorOpen=true; downloadChoice=false; requestedPage=0;
    muxOptions={};

}
void App::updateConstructorRecommendation() {
    if(draft.empty())return;
    bool video=false,audio=false;
    for(const auto &source:draft){video|=source.role==0;audio|=source.role==1;}
    if(!constructorModeManual)singleExportMode=!video && !audio;
    if(!constructorFormatManual && !singleExportMode) {
        if(video && muxOptions.container>=4)muxOptions.container=0;
    }
}
void App::autoConstruct() {
    if(constructorPending>0){notify("Дождитесь чтения дорожек файлов");return;}
    for (const auto &source : sourcePool) {
        bool alreadyAdded = std::any_of(draft.begin(), draft.end(), [&](const auto &item) {
            return item.role == source.role && item.path == source.path &&
                   item.stream == source.stream && item.media.key == source.media.key &&
                   item.media.id == source.media.id;
        });
        if (!alreadyAdded) draft.push_back(source);
    }
    std::stable_sort(draft.begin(),draft.end(),[](const auto &a,const auto &b){return a.role<b.role;});
    for (int role=0; role<3; ++role) {
        if (std::any_of(draft.begin(), draft.end(), [role](const auto &s){return s.role==role && s.primary;}))
            continue;
        if(std::any_of(draft.begin(),draft.end(),[role](const auto &s){return s.role==role && s.embeddedKind=="stream";}))continue;
        auto first=std::find_if(draft.begin(), draft.end(), [role](const auto &s){return s.role==role;});
        if (first!=draft.end()) first->primary=true;
    }
    draftSelection=draft.empty()?-1:0;
    updateConstructorRecommendation();
}
void App::queueConstructorProbe(const Media &media,const std::string &path,uint64_t generation) {
    ++constructorPending;
    work([this,media,path,generation] {
        Json probe;std::atomic_bool cancel{false},finished{false};
        std::thread watchdog([&]{for(int n=0;n<150&&!finished&&!stopping;++n)Sleep(100);if(!finished)cancel=true;});
        try {std::shared_lock lock(toolsMutex);auto result=runProcess(tool(L"ffprobe.exe"),{L"-v",L"error",L"-show_streams",L"-show_programs",L"-show_stream_groups",L"-show_chapters",L"-show_format",L"-of",L"json",wide(path)},nullptr,cancel,true);
            if(result.code && !cancel)result=runProcess(tool(L"ffprobe.exe"),{L"-v",L"error",L"-show_streams",L"-show_programs",L"-show_chapters",L"-show_format",L"-of",L"json",wide(path)},nullptr,cancel,true);
            if(!result.code)probe=Json::parse(result.output,nullptr,false);}catch(...){}
        finished=true;watchdog.join();
        std::vector<AssemblySource> sources;
        try {if(probe.is_object())sources=assemblySourcesFromProbe(media,path,probe);}catch(...) {}
        post([this,generation,sources=std::move(sources)] {
            if(!constructorOpen || generation!=constructorGeneration)return;
            constructorPending=std::max(0,constructorPending-1);
            if(sources.empty()){notify("Не удалось разобрать потоки файла; исходник сохранён");return;}
            sourcePool.insert(sourcePool.end(),sources.begin(),sources.end());
        });
    });
}
void App::importAssemblyFile() {
    auto generation=constructorGeneration;++constructorPending;
    work([this,generation] {
        struct Finish { App *app;uint64_t generation;~Finish(){app->post([app=app,generation=generation]{if(app->constructorOpen && app->constructorGeneration==generation)app->constructorPending=std::max(0,app->constructorPending-1);});} } finish{this,generation};
        auto path=pickFile(hwnd); if(path.empty())return;
        Media m; m.title=utf8(fs::path(wide(path)).filename().wstring());
        auto ext=fs::path(wide(path)).extension().wstring(); std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);
        m.ext=utf8(ext.empty()?L"":ext.substr(1)); m.key="local:"+path;
        if(m.ext=="txt") {m.category="Данные";m.id="metadata";post([this,m,path,generation]{if(constructorOpen && generation==constructorGeneration)sourcePool.push_back(assemblySource(m,path));});return;}
        if(m.ext=="ffmetadata") {m.category="Данные";m.id="chapters";post([this,m,path,generation]{if(constructorOpen && generation==constructorGeneration)sourcePool.push_back(assemblySource(m,path));});return;}
        if(m.ext=="json") { m.category="Данные"; m.id="chapters";
            auto data=Json::parse(readFile(wide(path),4*1024*1024),nullptr,false);
            if(!data.is_array() && !(data.is_object() && data.contains("chapters") && data["chapters"].is_array()))m.id="metadata";
            post([this,m,path,generation]{if(constructorOpen && generation==constructorGeneration)sourcePool.push_back(assemblySource(m,path));}); return;
        }
        if(m.ext=="png"||m.ext=="jpg"||m.ext=="jpeg"||m.ext=="webp"||m.ext=="avif"||m.ext=="bmp"||m.ext=="gif") {
            m.category="Изображения"; post([this,m,path,generation]{if(constructorOpen && generation==constructorGeneration)sourcePool.push_back(assemblySource(m,path));});return;
        }
        std::atomic_bool cancel{false},finished{false};
        std::thread watchdog([&]{for(int i=0;i<150&&!finished&&!stopping;i++)Sleep(100);if(!finished)cancel=true;});
        ProcessResult r;
        try { std::shared_lock lock(toolsMutex); r=runProcess(tool(L"ffprobe.exe"),{L"-v",L"error",L"-show_streams",L"-show_programs",L"-show_stream_groups",L"-show_chapters",L"-show_format",L"-of",L"json",wide(path)},nullptr,cancel,true);
            if(r.code && !cancel)r=runProcess(tool(L"ffprobe.exe"),{L"-v",L"error",L"-show_streams",L"-show_programs",L"-show_chapters",L"-show_format",L"-of",L"json",wide(path)},nullptr,cancel,true); }
        catch(...){finished=true;watchdog.join();throw;}
        finished=true;watchdog.join();
        auto probe=Json::parse(r.output,nullptr,false);
        if(r.code||!probe.is_object())throw std::runtime_error("Не удалось прочитать дорожки локального файла");
        auto imported=assemblySourcesFromProbe(m,path,probe);
        if(imported.empty())throw std::runtime_error("В файле не найдены поддерживаемые дорожки");
        post([this,imported,generation]{if(constructorOpen && generation==constructorGeneration)sourcePool.insert(sourcePool.end(),imported.begin(),imported.end());});
    });
}
static std::string metadataTag(const Json &tags, std::initializer_list<const char *> keys) {
    if (!tags.is_object()) return {};
    for (auto key : keys)
        for (auto it=tags.begin(); it!=tags.end(); ++it)
            if (it->is_string() && _stricmp(it.key().c_str(), key)==0 && !it->get_ref<const std::string &>().empty())
                return it->get<std::string>();
    return {};
}
// Load MediaInfo from the application directory or its standard installation,
// never from the current working directory. The report describes the source.
static std::vector<MediaInfoField> readMediaInfoReport(const std::string &path) {
    auto dll=executable().parent_path()/L"MediaInfo.dll";
    HMODULE lib=LoadLibraryExW(dll.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    if(!lib) {
        wchar_t programFiles[MAX_PATH]{};
        if (!GetEnvironmentVariableW(L"ProgramFiles",programFiles,MAX_PATH)) return {};
        dll=fs::path(programFiles)/L"MediaInfo"/L"MediaInfo.dll";
        lib=LoadLibraryExW(dll.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    }
    if(!lib)return {};
    using New = void* (__stdcall*)();
    using Open = size_t (__stdcall*)(void*, const wchar_t*);
    using Inform = const wchar_t* (__stdcall*)(void*, size_t);
    using Delete = void (__stdcall*)(void*);
    auto create = reinterpret_cast<New>(GetProcAddress(lib, "MediaInfo_New"));
    auto open = reinterpret_cast<Open>(GetProcAddress(lib, "MediaInfo_Open"));
    auto inform = reinterpret_cast<Inform>(GetProcAddress(lib, "MediaInfo_Inform"));
    auto destroy = reinterpret_cast<Delete>(GetProcAddress(lib, "MediaInfo_Delete"));
    std::wstring report;
    if (create && open && inform && destroy) {
        void *handle = create();
        if (handle) {
            if (open(handle, wide(path).c_str())) {
                if (const auto *value = inform(handle, 0)) report = value;
            }
            destroy(handle);
        }
    }
    FreeLibrary(lib);
    std::vector<MediaInfoField> fields;
    std::map<std::string,int> counts;
    std::string section; int ordinal=0;
    std::wstring line;
    auto flush = [&] {
        auto colon=line.find(L" : ");
        if(colon==std::wstring::npos) {
            if(!line.empty()) { section=utf8(line); auto suffix=section.find(" #"); if(suffix!=std::string::npos)section.erase(suffix); ordinal=counts[section]++; }
        } else if(!section.empty()) {
            auto label=line.substr(0,colon);
            while(!label.empty() && label.back()==L' ')label.pop_back();
            auto value=line.substr(colon+3);
            if(!label.empty() && !value.empty())fields.push_back({section,utf8(label),utf8(value),ordinal});
        }
        line.clear();
    };
    for(wchar_t ch:report) { if(ch==L'\r')continue; if(ch==L'\n')flush(); else line+=ch; }
    if(!line.empty())flush();
    return fields;
}
void App::readDroppedMetadata(const AssemblySource &original) {
    work([this,original] {
        Json probe;
        std::atomic_bool cancel{false},finished{false};
        std::thread watchdog([&]{
            for(int i=0;i<150 && !finished && !stopping;++i) Sleep(100);
            if(!finished) cancel=true;
        });
        try {
            std::shared_lock lock(toolsMutex);
            auto result=runProcess(tool(L"ffprobe.exe"),{L"-v",L"error",L"-show_streams",L"-show_chapters",L"-show_format",L"-of",L"json",wide(original.path)},nullptr,cancel,true);
            if(!result.code) probe=Json::parse(result.output,nullptr,false);
        } catch (...) {}
        finished=true;
        watchdog.join();
        auto report = readMediaInfoReport(original.path);
        Json metadataSource;
        if(original.role==5 && original.embeddedKind!="metadata")try {metadataSource=Json::parse(readFile(wide(original.path),4*1024*1024));}catch(...) {}
        Json chapters;
        if(original.role==4)try {
            auto ext=fs::path(wide(original.path)).extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);
            if(original.embeddedKind=="chapters" || ext==L".ffmetadata")chapters=chaptersFromProbe(probe);
            else {chapters=Json::parse(readFile(wide(original.path),4*1024*1024));if(chapters.is_object())chapters=chapters.value("chapters",Json::array());}
            (void)chaptersToFfmetadata(chapters);
        } catch(...) {chapters=Json();}
        post([this,original,probe=std::move(probe),report=std::move(report),chapters=std::move(chapters),metadataSource=std::move(metadataSource)] {
            auto it=std::find_if(draft.begin(),draft.end(),[&](const auto &s){return s.uiId==original.uiId && s.metadataPending;});
            if(!constructorOpen || it==draft.end()) return;
            it->metadataPending=false;
            it->mediaInfoFields=std::move(report);
            if(original.role==6)return;
            if(original.role==5 && original.embeddedKind!="metadata") {
                if(metadataSource.is_object()) {
                    it->metadataEdit=true;it->metadataEditTitle=str(metadataSource,"title");it->metadataEditArtist=str(metadataSource,"artist");it->metadataEditDescription=str(metadataSource,"description");
                    for(auto tag=metadataSource.begin();tag!=metadataSource.end();++tag)
                        if(metadataKeyAllowed(tag.key()) && tag->is_string())it->extraTags[tag.key()]=tag->get<std::string>();
                    it->originalExtraTags=it->extraTags;
                }
                return;
            }
            if(original.role==4) {
                if(!chapters.is_array()) {notify("Не удалось прочитать главы для редактирования");return;}
                it->chapterEdits.clear();
                for(const auto &chapter:chapters)it->chapterEdits.push_back({std::to_string(number(chapter,"start_time",0)),std::to_string(number(chapter,"end_time",0)),str(chapter,"title","Глава"),chapter.value("tags",Json::object())});
                it->chaptersReady=true;return;
            }
            if(!probe.is_object()) { notify("Не удалось прочитать метаданные источника"); return; }
            auto type=original.role==0 || original.role==3?"video":original.role==1?"audio":original.role==2?"subtitle":"";
            int ordinal=0;
            auto streams=probe.value("streams",Json::array());
            for(const auto &stream:streams) {
                if(str(stream,"codec_type")!=type) continue;
                auto codec=str(stream,"codec_name");auto streamTags=stream.value("tags",Json::object());auto filename=str(streamTags,"filename");
                std::transform(filename.begin(),filename.end(),filename.begin(),[](unsigned char ch){return char(std::tolower(ch));});
                bool isCover=number(stream.value("disposition",Json::object()),"attached_pic")!=0 || ((codec=="png" || codec=="mjpeg") && (filename=="cover.png" || filename=="cover.jpg" || filename=="cover.jpeg"));
                if(original.role==0 && isCover)continue;
                if(original.role==3 && original.embeddedKind=="cover" && !isCover)continue;
                if(original.stream>=0 && number(stream,"index")!=original.stream) { ++ordinal; continue; }
                it->mediaInfoOrdinal=ordinal;
                it->metadataStream=int(number(stream,"index",-1));
                it->probedCodec=str(stream,"codec_name");
                if(original.role==0)it->media.vcodec=it->probedCodec;
                else if(original.role==1)it->media.acodec=it->probedCodec;
                if(original.role==3)break;
                auto tags=stream.value("tags",Json::object());
                it->originalStreamTags=tags;it->originalStreamInfo=stream;
                if(original.role<3 && it->extraTags.empty() && tags.is_object()) {
                    for(auto tag=tags.begin();tag!=tags.end();++tag) {
                        auto key=tag.key(); std::string lower=key;
                        std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});
                        if(!metadataKeyAllowed(key) || !tag->is_string() || lower=="title" || lower=="language" || lower=="description" || lower=="comment" || lower=="artist" || lower=="summary" || lower=="synopsis" || lower=="handler_name" || lower=="date" || lower=="date_released" || lower=="encoder" || lower=="duration" || lower=="number_of_frames" || lower=="number_of_bytes" || lower.rfind("_statistics_",0)==0)continue;
                        it->extraTags[key]=tag->get<std::string>();
                    }
                    if(original.role==1)for(const char *key:{"composer","performer","genre"}) {
                        auto found=std::find_if(it->extraTags.begin(),it->extraTags.end(),[&](const auto &item){return _stricmp(item.first.c_str(),key)==0;});
                        if(found==it->extraTags.end())it->extraTags[key]="";
                    }
                    it->originalExtraTags=it->extraTags;
                }
                auto fill=[&](std::string AssemblySource::*member,std::initializer_list<const char *> keys) {
                    auto value=metadataTag(tags,keys);
                    if(!value.empty() && ((*it).*member)==(original.*member)) (*it).*member=std::move(value);
                };
                fill(&AssemblySource::name,{"title","name"});
                fill(&AssemblySource::language,{"language"});
                fill(&AssemblySource::description,{"description","comment"});
                fill(&AssemblySource::summary,{"summary"});
                fill(&AssemblySource::synopsis,{"synopsis"});
                if(muxOptions.container==1 || muxOptions.container==2)
                    fill(&AssemblySource::handlerName,{"handler_name"});
                fill(&AssemblySource::artist,{"artist"});
                fill(&AssemblySource::date,{"date_released","date"});
                auto baseline=[&](std::string AssemblySource::*member,std::initializer_list<const char *> keys) {
                    auto value=metadataTag(tags,keys);return value.empty()?original.*member:value;
                };
                it->originalKnownTags={{"title",baseline(&AssemblySource::name,{"title","name"})},
                    {"language",baseline(&AssemblySource::language,{"language"})},
                    {"description",baseline(&AssemblySource::description,{"description","comment"})},
                    {"artist",baseline(&AssemblySource::artist,{"artist"})},
                    {"summary",baseline(&AssemblySource::summary,{"summary"})},
                    {"synopsis",baseline(&AssemblySource::synopsis,{"synopsis"})},
                    {"handler_name",original.handlerName.empty() && muxOptions.container!=1 && muxOptions.container!=2?std::string():baseline(&AssemblySource::handlerName,{"handler_name"})},
                    {"date",baseline(&AssemblySource::date,{"date_released","date"})}};
                break;
            }
            auto tags=probe.value("format",Json::object()).value("tags",Json::object());
            if(!globalTagsLoaded && tags.is_object()) {
                for(auto tag=tags.begin();tag!=tags.end();++tag) {
                    std::string lower=tag.key();std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});
                    if(!metadataKeyAllowed(tag.key()) || !tag->is_string() || lower=="title" || lower=="name" || lower=="artist" || lower=="description" || lower=="synopsis" || lower=="comment" || lower=="encoder" || lower=="major_brand" || lower=="minor_version" || lower=="compatible_brands")continue;
                    auto found=std::find_if(globalTags.begin(),globalTags.end(),[&](const auto &item){return _stricmp(item.first.c_str(),tag.key().c_str())==0;});
                    if(found==globalTags.end())globalTags[tag.key()]=tag->get<std::string>();
                    else if(found->second.empty())found->second=tag->get<std::string>();
                }
                originalGlobalTags=globalTags;globalTagsLoaded=true;
            }
            if(assemblyMetadataTitle.empty()) assemblyMetadataTitle=metadataTag(tags,{"title","name"});
            if(assemblyArtist.empty()) assemblyArtist=metadataTag(tags,{"artist"});
            if(assemblyDescription.empty()) assemblyDescription=metadataTag(tags,{"description","synopsis","comment"});
        });
    });
}
void App::enqueueAssembly() {
    if(constructorPending>0)throw std::runtime_error("Дождитесь чтения всех импортированных файлов");
    if(std::any_of(draft.begin(),draft.end(),[](const auto &s){return s.metadataPending;}))
        throw std::runtime_error("Дождитесь чтения метаданных дорожек");
    if(singleExportMode) {
        if(draft.empty() || draft.size()>64)throw std::runtime_error("Выберите от 1 до 64 источников.");
        if(!fs::path(wide(destination)).is_absolute())throw std::runtime_error("Выберите полную папку сохранения");
        if(assemblyName.empty())throw std::runtime_error("Укажите имя итогового файла");
        for(const auto &source:draft) {
            auto exportFormat=source.exportFormat, exportQuality=source.exportQuality;
            if(exportQuality<0 || exportQuality>2)throw std::runtime_error("Проверьте качество сохранения.");
            if(source.media.drm)throw std::runtime_error("Защищённые потоки нельзя скачать");
            if(!source.path.empty()&&!fs::is_regular_file(wide(source.path)))throw std::runtime_error("Локальный файл недоступен");
            int maxFormat=source.role==3?3:source.role==2?4:source.role==4?2:source.role==5?1:source.role<2?3:0;
            if(exportFormat<0||exportFormat>maxFormat)throw std::runtime_error("Проверьте формат сохранения.");
            if(source.role==2 && exportFormat==0 && !source.probedCodec.empty())
                (void)originalSubtitleExtension(source.probedCodec);
            if(exportFormat>0 && exportFormat!=4 && source.role==2 && isBitmapSubtitleCodec(source.probedCodec))
                throw std::runtime_error("Субтитры состоят из изображений: выберите исходный формат или используйте распознавание текста.");
            if(source.role==2 && exportFormat==4) {
                if(!source.probedCodec.empty() && !isBitmapSubtitleCodec(source.probedCodec))throw std::runtime_error("Распознавание применяется к графическим субтитрам. Для текста выберите SRT, WebVTT или ASS.");
                if(source.ocrExecutable.empty() || !fs::path(wide(source.ocrExecutable)).is_absolute() || !fs::is_regular_file(wide(source.ocrExecutable)))throw std::runtime_error("Укажите полный путь к установленному tesseract.exe.");
                if(source.ocrLanguage.empty() || source.ocrLanguage.size()>64 || !std::all_of(source.ocrLanguage.begin(),source.ocrLanguage.end(),[](unsigned char c){return std::isalnum(c)||c=='_'||c=='+'||c=='-';}))throw std::runtime_error("Укажите языки Tesseract, например eng или rus+eng.");
            }
            if(exportFormat>0 && exportFormat!=4 && (source.role==2 || source.role==3)) {
                bool available=source.role==3?imageEncoders[exportFormat]:subtitleEncoders[exportFormat==1?1:exportFormat==2?3:4];
                if(!available)throw std::runtime_error(videoEncodersReady?"Преобразователь выбранного формата недоступен. Выберите исходный или другой формат.":"Дождитесь проверки доступных преобразователей.");
            }
            if(exportFormat>0 && source.role<2) {
                int encoder=source.role==0?(exportFormat==3?3:1):(exportFormat==1?3:exportFormat==2?1:4);
                bool available=source.role==0?videoEncoders[encoder]:audioEncoders[encoder];
                if(!available)throw std::runtime_error(videoEncodersReady?"Кодировщик выбранного формата недоступен. Выберите исходный или другой формат.":"Дождитесь проверки доступных кодировщиков.");
            }
        }
        std::vector<std::shared_ptr<Job>> queued;
        for(auto source:draft) {
            auto parent=std::make_shared<Job>();parent->assembly=true;parent->singleExport=true;parent->exportFormat=source.exportFormat;parent->exportQuality=source.exportQuality;
            parent->id=std::to_string(GetTickCount64())+"-"+std::to_string(++jobSerial);parent->title=draft.size()==1?assemblyName:(source.name.empty()?assemblyName:source.name);parent->item=source.media;parent->item.title=parent->title;
            parent->item.category=source.role==0?"Видео":source.role==1?"Аудио":source.role==2?"Субтитры":source.role==3?"Изображения":"Данные";
            if(draft.size()>1 && !source.media.ext.empty()) {
                auto suffix="."+source.media.ext;
                if(parent->title.size()>suffix.size() && _stricmp(parent->title.c_str()+parent->title.size()-suffix.size(),suffix.c_str())==0)
                    parent->title.resize(parent->title.size()-suffix.size());
                parent->item.title=parent->title;
            }
            parent->folder=utf8((fs::path(wide(destination))/wide(parent->id+"-export")).wstring());parent->outputDirectory=destination;parent->waitingSources=true;parent->state="Ожидание источника";
            if(source.path.empty()) {
                auto child=std::make_shared<Job>();child->id=std::to_string(GetTickCount64())+"-"+std::to_string(++jobSerial);
                child->item=source.media;child->title=source.name;child->page=source.media.page;child->assemblyInput=true;
                child->folder=utf8((fs::path(wide(destination))/wide(parent->id+"-sources")/wide(child->id)).wstring());
                child->outputDirectory=utf8((fs::path(wide(destination))/wide(parent->id+"-sources")).wstring());
                auto it=mediaSessions.find(source.media.key);child->session=it==mediaSessions.end()?session():it->second;
                if(child->item.category=="Данные")child->local=dataSourceContent(child->item);
                source.dependency=child->id;queued.push_back(child);
            }
            source.enabled=true;parent->sources.push_back(source);queued.push_back(parent);selectedJob=parent->id;
        }
        {std::lock_guard lock(jobsMutex);jobs.insert(jobs.end(),queued.begin(),queued.end());}
        constructorOpen=false;requestedPage=2;saveHistory();jobsCv.notify_all();notify("Отдельное сохранение добавлено в очередь: "+std::to_string(draft.size()));return;
    }
    if(std::any_of(draft.begin(),draft.end(),[](const auto &s){return s.metadataPending;}))
        throw std::runtime_error("Дождитесь чтения метаданных дорожек");
    if(draft.empty())throw std::runtime_error("Добавьте медиа в конструктор");
    if(draft.size()>64)throw std::runtime_error("В одной сборке допускается до 64 источников");
    if(!fs::path(wide(destination)).is_absolute())throw std::runtime_error("Выберите полную папку сохранения");
    if(assemblyName.empty())throw std::runtime_error("Укажите имя контейнера");
    (void)muxContainer(muxOptions);
    int primary[3]{},manualPrimary[3]{}; bool playable=false; int cover=0,chapters=0,metadata=0;
    for(const auto &s:draft) {
        if(muxOptions.container>=4) {
            if(s.role==0 || s.role==2)throw std::runtime_error("MP3, M4A и FLAC поддерживают одну аудиодорожку без видео и субтитров. Выберите другой формат или удалите лишние источники.");
            int required=muxOptions.container==4?3:muxOptions.container==5?1:4;
            if(s.role==1 && s.codec!=0 && s.codec!=required)throw std::runtime_error("Для выбранного формата выберите соответствующий аудиокодек или исходный кодек.");
        }
        if(s.role==2 && s.codec>0 && isBitmapSubtitleCodec(s.probedCodec))throw std::runtime_error("Графические субтитры нельзя преобразовать в текст без распознавания. Оставьте исходный формат или выберите другой источник.");
        if(auto conflict=assemblyCodecConflict(muxOptions.container,s);!conflict.empty())throw std::runtime_error(conflict);
        if(s.codec==0 && !s.probedCodec.empty()) {
            auto conflict=assemblyProbedCodecConflict(muxOptions.container,s.role,s.probedCodec);
            if(!conflict.empty())throw std::runtime_error(conflict);
        }
        if(s.codec==0 && s.role<2) {
            auto conflict=assemblyCopiedCodecConflict(muxOptions.container,s.role,s.role==0?s.media.vcodec:s.media.acodec);
            if(!conflict.empty())throw std::runtime_error(conflict);
        }
        if(s.role==6 && (s.embeddedKind=="programs" || s.embeddedKind=="stream_groups"))throw std::runtime_error("Программы или группы потоков этого файла пока нельзя безопасно перенести. Исходник сохранён.");
        if(s.role==6 && s.embeddedKind!="attachment" && s.embeddedKind!="data")throw std::runtime_error("Неизвестный тип потока нельзя безопасно перенести; исходник сохранён.");
        if(s.role==6 && muxOptions.container!=0)throw std::runtime_error("Для сохранения вложений и дополнительных потоков выберите MKV; исходник останется без изменений.");
        if(s.role==3 && s.embeddedKind=="cover" && s.probedCodec!="png" && s.probedCodec!="mjpeg")throw std::runtime_error("Формат встроенной обложки нельзя сохранить без преобразования. Исходник сохранён.");
        if(s.role==3 && (s.coverFormat<0||s.coverFormat>1))throw std::runtime_error("Выберите формат обложки PNG или JPG");
        if(s.role<0||s.role>6||s.codec<0||s.codec>(s.role==0?10:4)||s.quality<0||s.quality>2)throw std::runtime_error("Некорректные параметры дорожки");
        auto numeric=[](const std::string &v,int limit){if(v.empty())return true;if(v.size()>7||!std::all_of(v.begin(),v.end(),[](unsigned char c){return std::isdigit(c);}))return false;try{int n=std::stoi(v);return n>0&&n<=limit;}catch(...){return false;}};
        if(!numeric(s.bitrate,1000000)||!numeric(s.maxrate,1000000)||!numeric(s.bufsize,2000000)||!numeric(s.sampleRate,384000)||!numeric(s.frameRate,240)||!numeric(s.width,16384))throw std::runtime_error("Проверьте числовые параметры перекодирования: укажите положительные значения без единиц измерения");
        if(s.role==0&&(s.maxrate.empty()!=s.bufsize.empty()))throw std::runtime_error("Укажите максимальный битрейт и размер буфера вместе");
        if(s.role==1&&s.codec==4&&!s.bitrate.empty())throw std::runtime_error("FLAC использует качество без потерь; уберите заданный битрейт");
        if(s.codec==0&&(!s.bitrate.empty()||!s.maxrate.empty()||!s.bufsize.empty()||!s.sampleRate.empty()||!s.frameRate.empty()||!s.width.empty()))throw std::runtime_error("Для расширенных параметров выберите кодек перекодирования");
        if(!supportsExtendedTrackTags(muxOptions.container) && s.role<3 && (!s.description.empty()||!s.artist.empty()||!s.date.empty()||!s.summary.empty()||!s.synopsis.empty()))throw std::runtime_error("Для расширенных тегов дорожки выберите MKV или WebM либо очистите эти поля.");
        if(!s.handlerName.empty() && (s.role>=3 || (muxOptions.container!=1 && muxOptions.container!=2)))throw std::runtime_error("Имя обработчика дорожки доступно для MP4 и MOV.");
        if(s.role==2 && s.forced && !supportsForcedSubtitles(muxOptions.container))throw std::runtime_error("Выбранный контейнер не сохраняет флаг принудительных субтитров. Снимите флаг или смените контейнер.");
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
        if(s.role==4 && s.chaptersReady)(void)editedChapters(s);
        if(s.media.drm)throw std::runtime_error("Защищённые потоки нельзя скачать");
        if(s.primary&&s.role<3){++primary[s.role];if(s.embeddedKind!="stream")++manualPrimary[s.role];}
        playable|=s.role<2; cover+=s.role==3;chapters+=s.role==4;metadata+=s.role==5;
        if(!s.path.empty()&&!fs::is_regular_file(wide(s.path)))throw std::runtime_error("Локальный файл недоступен");
    }
    for(int role=0;role<3;++role)if(manualPrimary[role]>0 && primary[role]>1)throw std::runtime_error("В каждой категории выберите одну дорожку по умолчанию");
    if(!playable)throw std::runtime_error("Добавьте видео или аудио");
    if(muxOptions.container>=4 && std::count_if(draft.begin(),draft.end(),[](const auto &s){return s.role==1;})!=1)throw std::runtime_error("Для MP3, M4A и FLAC выберите ровно одну аудиодорожку.");
    if(cover>1||chapters>1||metadata>1)throw std::runtime_error("Выберите одну обложку, один источник глав и один источник метаданных");
    if(cover && muxOptions.container==3)throw std::runtime_error("Для вложенной обложки выберите MKV, MP4 или MOV; WebM не поддерживается.");
    auto parent=std::make_shared<Job>();parent->assembly=true;parent->mux=true;parent->muxOptions=muxOptions;
    parent->id=std::to_string(GetTickCount64())+"-"+std::to_string(++jobSerial);
    parent->title=assemblyName;parent->item.title=assemblyName;parent->item.category="Требуют проверки";
    parent->assemblyMetadataTitle=assemblyMetadataTitle;parent->assemblyArtist=assemblyArtist;parent->assemblyDescription=assemblyDescription;parent->globalTags=globalTags;parent->originalGlobalTags=originalGlobalTags;
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
            if(child->item.category=="Данные")child->local=dataSourceContent(child->item);
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
        if(exists&&s.enabled&&(s.role<2 || parent->singleExport))playable=true;
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
