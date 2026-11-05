#pragma once
#include "core.hpp"
namespace ss {
struct MetadataTreeItem {std::string path, value; bool editable=false;};
struct AssemblySource {
    Media media;
    std::string path, dependency, name, language, description, artist, date, bitrate, maxrate, bufsize, sampleRate, frameRate, width;
    std::string summary, synopsis, handlerName, probedCodec;
    std::string metadataEditTitle, metadataEditArtist, metadataEditDescription, metadataFieldKey, metadataFieldValue;
    std::string ocrExecutable, ocrLanguage = "eng";
    std::map<std::string,Json> metadataOverrides;
    std::vector<std::string> metadataRemoved;
    Json metadataPathEdits = Json::array();
    std::vector<MetadataTreeItem> metadataTree;
    bool metadataTreeReady = false;
    int role = 0, stream = -1, codec = 0, quality = 1, coverFormat = 0, exportFormat = 0, exportQuality = 1;
    uint64_t uiId = 0;
    bool metadataPending = false, metadataEdit = false;
    int metadataStream = -1;
    bool primary = false, forced = false, includeAudio = false, enabled = true, optionalAudio = false, derivedAudio = false;
};
inline bool isBitmapSubtitleCodec(const std::string &codec) {
    return codec=="hdmv_pgs_subtitle" || codec=="dvd_subtitle" || codec=="dvb_subtitle" || codec=="xsub";
}
inline std::string editedMetadataJson(const std::string &content, const AssemblySource &source) {
    auto data=Json::parse(content,nullptr,false);
    if(!data.is_object())throw std::runtime_error("Редактирование доступно для JSON-объекта. Исходник сохранён.");
    if(str(data,"title")!=source.metadataEditTitle)data["title"]=source.metadataEditTitle;
    if(str(data,"artist")!=source.metadataEditArtist)data["artist"]=source.metadataEditArtist;
    if(str(data,"description")!=source.metadataEditDescription)data["description"]=source.metadataEditDescription;
    for(const auto &key:source.metadataRemoved)data.erase(key);
    for(const auto &[key,value]:source.metadataOverrides)data[key]=value;
    if(!source.metadataPathEdits.is_array())throw std::runtime_error("Некорректный список изменений JSON.");
    for(const auto &edit:source.metadataPathEdits) {
        try {
            auto path=edit.at("path").get<std::string>();
            if(path.empty() || path[0]!='/')throw std::runtime_error("Некорректный путь");
            Json::json_pointer pointer(path);auto key=pointer.back();pointer.pop_back();
            auto &parent=data.at(pointer);bool remove=edit.at("remove").get<bool>();
            if(parent.is_object()) {
                if(remove) {if(!parent.contains(key))throw std::runtime_error("Поле не найдено");parent.erase(key);}
                else parent[key]=edit.at("value");
            } else if(parent.is_array()) {
                if(key.empty() || (key.size()>1 && key[0]=='0') || !std::all_of(key.begin(),key.end(),[](unsigned char c){return c>='0'&&c<='9';}))throw std::runtime_error("Некорректный индекс массива");
                auto index=std::stoull(key);
                if(index>=parent.size())throw std::runtime_error("Индекс вне массива");
                if(remove)parent.erase(size_t(index));else parent[size_t(index)]=edit.at("value");
            } else throw std::runtime_error("Родительское значение не является объектом или массивом");
        } catch(const std::exception &) {throw std::runtime_error("Не удалось применить путь JSON. Проверьте существование родительских полей и индексов; исходник сохранён.");}
    }
    return data.dump(2);
}
inline void stageMetadataField(AssemblySource &source, const std::string &key, const std::string &value, bool remove) {
    if(key.empty() || key.size()>256)throw std::runtime_error("Укажите имя поля длиной до 256 символов.");
    if(key=="title" || key=="artist" || key=="description")throw std::runtime_error("Для этого поля используйте соответствующую строку выше.");
    if(key[0]=='/') {
        try {Json::json_pointer pointer(key);(void)pointer;}catch(const std::exception &){throw std::runtime_error("Некорректный путь JSON. Используйте /объект/поле; ~ и / в именах кодируются как ~0 и ~1.");}
        if(!source.metadataPathEdits.is_array() || source.metadataPathEdits.size()>=256)throw std::runtime_error("Допускается не более 256 изменений вложенных полей.");
        Json item={{"path",key},{"remove",remove}};
        if(!remove) {
            if(value.size()>65536)throw std::runtime_error("Значение слишком велико.");
            auto parsed=Json::parse(value,nullptr,false);
            if(parsed.is_discarded())throw std::runtime_error("Введите корректное значение JSON: например, \"текст\", 42, true или null.");
            item["value"]=std::move(parsed);
        }
        source.metadataPathEdits.push_back(std::move(item));return;
    }
    if(remove) {
        source.metadataOverrides.erase(key);
        if(std::find(source.metadataRemoved.begin(),source.metadataRemoved.end(),key)==source.metadataRemoved.end())source.metadataRemoved.push_back(key);
    } else {
        if(value.size()>65536)throw std::runtime_error("Значение слишком велико.");
        auto parsed=Json::parse(value,nullptr,false);
        if(parsed.is_discarded())throw std::runtime_error("Введите корректное значение JSON: например, \"текст\", 42, true или null.");
        source.metadataOverrides[key]=std::move(parsed);
        std::erase(source.metadataRemoved,key);
    }
}
inline std::vector<MetadataTreeItem> makeMetadataTree(const Json &root) {
    std::vector<MetadataTreeItem> rows;
    auto escape=[](const std::string &key) {std::string result;for(char c:key) {if(c=='~')result+="~0";else if(c=='/')result+="~1";else result+=c;}return result;};
    std::function<void(const Json &,const std::string &,int)> visit;
    visit=[&](const Json &value,const std::string &path,int depth) {
        if(depth>=8 || rows.size()>=128)return;
        auto add=[&](const std::string &key,const Json &child) {
            if(rows.size()>=128)return;
            if(path.empty() && (key=="title" || key=="artist" || key=="description"))return;
            auto childPath=path+"/"+escape(key);std::string text;bool editable=child.is_primitive();
            if(editable && child.is_string() && child.get_ref<const std::string &>().size()>65536)editable=false;
            if(editable)text=child.dump();
            rows.push_back({childPath,std::move(text),editable});
            if(child.is_object() || child.is_array())visit(child,childPath,depth+1);
        };
        if(value.is_object())for(auto it=value.begin();it!=value.end() && rows.size()<128;++it)add(it.key(),it.value());
        else if(value.is_array())for(size_t n=0;n<value.size() && rows.size()<128;++n)add(std::to_string(n),value[n]);
    };
    visit(root,"",0);return rows;
}
// Only reject combinations known to be unsupported; copied streams are checked after probing.
inline std::string assemblyCodecConflict(int container, const AssemblySource &s) {
    if(s.codec==0)return {};
    if(container==3) {
        if(s.role==0 && (s.codec==1 || s.codec==2 || s.codec>=5))return "WebM: выберите VP9 или AV1 либо контейнер MKV/MP4.";
        if(s.role==1 && s.codec!=2)return "WebM: выберите аудиокодек Opus или контейнер MKV.";
        if(s.role==2 && s.codec!=3)return "WebM: выберите субтитры WebVTT или контейнер MKV.";
    }
    if((container==1 || container==2) && s.role==2 && s.codec!=2)
        return "MP4/MOV: выберите субтитры mov_text или контейнер MKV.";
    if(container==0 && s.role==2 && s.codec==2)
        return "MKV: выберите SRT, WebVTT или ASS вместо mov_text.";
    return {};
}
inline std::string assemblyCopiedCodecConflict(int container,int role,const std::string &codec) {
    if(codec.empty() || codec=="none")return {};
    if(container>=4 && container<=6 && role==1) {
        auto required=container==4?"mp3":container==5?"aac":"flac";
        bool known=codec=="mp3"||codec=="aac"||codec=="flac"||codec=="opus"||codec=="vorbis"||codec=="alac"||codec=="ac3"||codec=="pcm_s16le"||codec=="pcm_s24le";
        if(known && codec!=required)return "Выбранный аудиоформат требует другого кодека: выберите перекодирование или совместимый формат.";
    }
    if(container==1 && role==0 && (codec=="theora"||codec=="wmv3"||codec=="vc1"||codec=="ffv1"))
        return "MP4: перекодируйте видео в H.264/H.265 или выберите MKV.";
    if(container==3) {
        if(role==0 && (codec=="h264"||codec=="hevc"||codec=="mpeg4"||codec=="mpeg2video"))return "WebM: перекодируйте видео в VP9/AV1 или выберите MKV.";
        if(role==1 && (codec=="aac"||codec=="mp3"||codec=="flac"||codec=="ac3"))return "WebM: перекодируйте аудио в Opus или выберите MKV.";
        if(role==2 && (codec=="subrip"||codec=="srt"||codec=="ass"||codec=="mov_text"))return "WebM: преобразуйте субтитры в WebVTT или выберите MKV.";
    }
    if((container==1||container==2) && role==2 && (codec=="subrip"||codec=="srt"||codec=="ass"||codec=="webvtt"))return "MP4/MOV: преобразуйте субтитры в mov_text или выберите MKV.";
    if(container==0 && role==2 && codec=="mov_text")return "MKV: преобразуйте субтитры в SRT или выберите MP4.";
    return {};
}
// ffprobe returns the actual codec name, unlike extractor labels such as avc1.640028.
inline std::string assemblyProbedCodecConflict(int container,int role,const std::string &codec) {
    auto known=assemblyCopiedCodecConflict(container,role,codec);
    if(!known.empty() || codec.empty())return known;
    if(container==3) {
        if(role==0 && codec!="vp8" && codec!="vp9" && codec!="av1")return "WebM: выберите видео VP8, VP9 или AV1 либо другой контейнер.";
        if(role==1 && codec!="vorbis" && codec!="opus")return "WebM: выберите аудио Vorbis или Opus либо другой контейнер.";
        if(role==2 && codec!="webvtt")return "WebM: преобразуйте текстовые субтитры в WebVTT или выберите MKV.";
    }
    if(container>=4 && container<=6 && role==1) {
        auto required=container==4?"mp3":container==5?"aac":"flac";
        if(codec!=required)return "Для выбранного аудиоформата требуется другой кодек. Выберите перекодирование или совместимый формат.";
    }
    return {};
}
inline int sourceRole(const Media &m) {
    if (m.id == "chapters") return 4;
    if (m.category == "Данные") return 5;
    if (m.category == "Изображения" || m.category == "Превью") return 3;
    if ((m.category.find("Субтитры") != std::string::npos || m.category == "Автосубтитры") || m.ext == "srt" || m.ext == "vtt" || m.ext == "ass") return 2;
    if (m.category == "Аудио") return 1;
    return 0;
}
inline AssemblySource assemblySource(const Media &m, const std::string &path = "") {
    AssemblySource s; s.media = m; s.path = path; s.name = m.title; s.language = m.language;
    s.role = sourceRole(m); s.includeAudio = false;
    return s;
}
inline Json assemblyJson(const AssemblySource &s) {
    return {{"path",s.path},{"dependency",s.dependency},{"name",s.name},{"language",s.language},{"description",s.description},{"summary",s.summary},{"synopsis",s.synopsis},{"handlerName",s.handlerName},{"artist",s.artist},{"date",s.date},{"bitrate",s.bitrate},{"maxrate",s.maxrate},{"bufsize",s.bufsize},{"sampleRate",s.sampleRate},{"frameRate",s.frameRate},{"width",s.width},{"role",s.role},{"stream",s.stream},{"metadataStream",s.metadataStream},
            {"ocrExecutable",s.ocrExecutable},{"ocrLanguage",s.ocrLanguage},{"metadataOverrides",s.metadataOverrides},{"metadataRemoved",s.metadataRemoved},{"metadataPathEdits",s.metadataPathEdits},{"metadataEdit",s.metadataEdit},{"metadataEditTitle",s.metadataEditTitle},{"metadataEditArtist",s.metadataEditArtist},{"metadataEditDescription",s.metadataEditDescription},{"probedCodec",s.probedCodec},{"codec",s.codec},{"quality",s.quality},{"coverFormat",s.coverFormat},{"exportFormat",s.exportFormat},{"exportQuality",s.exportQuality},{"primary",s.primary},{"forced",s.forced},{"includeAudio",s.includeAudio},{"enabled",s.enabled},{"optionalAudio",s.optionalAudio},{"derivedAudio",s.derivedAudio}};
}
inline AssemblySource assemblyFrom(const Json &v) {
    AssemblySource s; s.path = str(v,"path"); s.dependency = str(v,"dependency"); s.name = str(v,"name"); s.language = str(v,"language");
    if(v.contains("metadataPathEdits") && v["metadataPathEdits"].is_array())s.metadataPathEdits=v["metadataPathEdits"];
    if(v.contains("metadataOverrides") && v["metadataOverrides"].is_object())s.metadataOverrides=v["metadataOverrides"].get<std::map<std::string,Json>>();
    if(v.contains("metadataRemoved") && v["metadataRemoved"].is_array())s.metadataRemoved=v["metadataRemoved"].get<std::vector<std::string>>();
    s.ocrExecutable=str(v,"ocrExecutable");s.ocrLanguage=str(v,"ocrLanguage","eng");
    s.probedCodec=str(v,"probedCodec");s.metadataEdit=v.value("metadataEdit",false);s.metadataEditTitle=str(v,"metadataEditTitle");s.metadataEditArtist=str(v,"metadataEditArtist");s.metadataEditDescription=str(v,"metadataEditDescription");
    s.description=str(v,"description");s.summary=str(v,"summary");s.synopsis=str(v,"synopsis");s.handlerName=str(v,"handlerName");s.artist=str(v,"artist");s.date=str(v,"date");s.bitrate=str(v,"bitrate");s.maxrate=str(v,"maxrate");s.bufsize=str(v,"bufsize");s.sampleRate=str(v,"sampleRate");s.frameRate=str(v,"frameRate");s.width=str(v,"width");
    s.role=v.value("role",0); s.stream=v.value("stream",-1); s.metadataStream=v.value("metadataStream",-1); s.codec=v.value("codec",0); s.quality=v.value("quality",1); s.coverFormat=v.value("coverFormat",0); s.exportFormat=v.value("exportFormat",0);s.exportQuality=v.value("exportQuality",1);
    s.primary=v.value("primary",false); s.forced=v.value("forced",false); s.includeAudio=v.value("includeAudio",false); s.enabled=v.value("enabled",true);s.optionalAudio=v.value("optionalAudio",false);s.derivedAudio=v.value("derivedAudio",false);
    return s;
}
}
