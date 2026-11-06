#pragma once
#include "core.hpp"
#include "mux_options.hpp"
#include <set>
namespace ss {
struct MediaInfoField { std::string section, name, value; int ordinal = 0; };
inline bool metadataKeyAllowed(const std::string &name) {
    if(name.empty() || name.size()>256)return false;
    bool nonspace=false;
    for(unsigned char ch:name) { if(ch<32 || ch==127 || ch=='=')return false; if(ch!=' ')nonspace=true; }
    if(!nonspace)return false;
    std::string lower=name;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char ch){return char(std::tolower(ch));});
    static const std::set<std::string> reserved={"title","name","language","description","comment","artist","summary","synopsis","handler_name","date_released","encoder","major_brand","minor_version","compatible_brands"};
    return !reserved.contains(lower);
}
inline bool metadataTagMatches(const Json &tags,const std::string &key,const std::string &value) {
    if(!tags.is_object())return value.empty();
    for(auto it=tags.begin();it!=tags.end();++it)
        if(_stricmp(it.key().c_str(),key.c_str())==0 && it->is_string())return it->get<std::string>()==value;
    return value.empty();
}
inline bool metadataTagMatchesAny(const Json &tags,std::initializer_list<const char*> keys,const std::string &value) {
    if(value.empty()) {
        for(auto key:keys)if(!metadataTagMatches(tags,key,value))return false;
        return true;
    }
    for(auto key:keys)if(metadataTagMatches(tags,key,value))return true;
    return false;
}
inline std::string metadataFirstKey(const Json &tags,std::initializer_list<const char*> keys,const char *fallback) {
    if(tags.is_object())for(auto key:keys)for(auto it=tags.begin();it!=tags.end();++it)
        if(_stricmp(it.key().c_str(),key)==0)return it.key();
    return fallback;
}
inline bool metadataTagChanged(const std::map<std::string,std::string> &original,const std::string &key,const std::string &value) {
    auto found=original.find(key);return found==original.end()? !value.empty():found->second!=value;
}
// Values which describe the decoded signal should survive stream copy. Container
// identifiers, bitrate estimates and time bases are deliberately excluded.
inline std::string copiedStreamDifference(const Json &source,const Json &output) {
    if(!source.is_object() || !output.is_object())return "stream";
    for(const char *key:{"codec_name","width","height","pix_fmt","sample_aspect_ratio","color_range","color_space","color_transfer","color_primaries","sample_rate","channels","channel_layout"}) {
        if(!source.contains(key) || source[key].is_null())continue;
        auto value=str(source,key);if(value.empty() || value=="unknown" || value=="N/A" || value=="0")continue;
        if(!output.contains(key) || str(output,key)!=value)return key;
    }
    auto original=source.value("side_data_list",Json::array()),actual=output.value("side_data_list",Json::array());
    if(!original.is_array() || !actual.is_array())return "side_data";
    for(const auto &entry:original) {
        auto type=str(entry,"side_data_type");if(type.empty())continue;
        auto match=std::find_if(actual.begin(),actual.end(),[&](const Json &candidate){return str(candidate,"side_data_type")==type;});
        if(match==actual.end())return type;
        for(auto it=entry.begin();it!=entry.end();++it) {
            if(it.key()=="side_data_type" || it.key()=="displaymatrix" || it.key()=="size" || it->is_null())continue;
            if(!match->contains(it.key()) || (*match)[it.key()]!=*it)return type;
        }
    }
    return {};
}
struct ChapterEdit { std::string start, end, title; Json tags = Json::object(); };
struct MetadataTreeItem {std::string path, value; bool editable=false;};
struct AssemblySource {
    Media media;
    std::string path, dependency, name, language, description, artist, date, bitrate, maxrate, bufsize, sampleRate, frameRate, width;
    std::string embeddedKind;
    std::string summary, synopsis, handlerName, probedCodec;
    std::string metadataEditTitle, metadataEditArtist, metadataEditDescription, metadataFieldKey, metadataFieldValue;
    std::string ocrExecutable, ocrLanguage = "eng";
    std::map<std::string,Json> metadataOverrides;
    std::vector<std::string> metadataRemoved;
    Json metadataPathEdits = Json::array();
    Json originalDisposition = Json::object(), originalCoverTags = Json::object(), originalStreamTags = Json::object(), originalStreamInfo = Json::object();
    std::vector<MetadataTreeItem> metadataTree;
    bool metadataTreeReady = false;
    std::vector<MediaInfoField> mediaInfoFields;
    std::map<std::string,std::string> extraTags, originalExtraTags, originalKnownTags;
    int mediaInfoOrdinal = 0;
    std::vector<ChapterEdit> chapterEdits;
    bool chaptersReady = false;
    std::string extraTagName;
    int role = 0, stream = -1, codec = 0, quality = 1, coverFormat = 0, exportFormat = 0, exportQuality = 1;
    uint64_t uiId = 0;
    bool metadataPending = false, metadataEdit = false;
    int metadataStream = -1;
    bool primary = false, forced = false, includeAudio = false, enabled = true, optionalAudio = false, derivedAudio = false;
};
inline Json editedChapters(const AssemblySource &source) {
    Json result=Json::array();
    for(const auto &c:source.chapterEdits) {
        try {
            size_t a=0,b=0;double start=std::stod(c.start,&a),end=std::stod(c.end,&b);
            if(a!=c.start.size() || b!=c.end.size())throw std::runtime_error("bad time");
            result.push_back({{"start_time",start},{"end_time",end},{"title",c.title},{"tags",c.tags}});
        } catch(...) {throw std::runtime_error("Проверьте время глав: укажите число секунд.");}
    }
    (void)chaptersToFfmetadata(result);
    return result;
}
inline bool isBitmapSubtitleCodec(const std::string &codec) {
    return codec=="hdmv_pgs_subtitle" || codec=="dvd_subtitle" || codec=="dvb_subtitle" || codec=="xsub";
}
// A copied subtitle needs a muxer that supports its codec; never copy the
// entire input container when the user selected one subtitle stream.
inline std::wstring originalSubtitleExtension(const std::string &codec) {
    if(codec=="subrip" || codec=="srt") return L".srt";
    if(codec=="ass") return L".ass";
    if(codec=="ssa") return L".ssa";
    if(codec=="webvtt") return L".vtt";
    if(codec=="hdmv_pgs_subtitle") return L".sup";
    if(codec=="dvd_subtitle" || codec=="dvb_subtitle") return L".mkv";
    if(codec=="mov_text") return L".mp4";
    throw std::runtime_error("Для этого кодека не найден поддерживаемый формат отдельной дорожки. Выберите преобразование или сохраните исходный файл целиком.");
}
struct OriginalSubtitlePlan { int stream; std::string codec; std::wstring extension; };
inline OriginalSubtitlePlan originalSubtitlePlan(const Json &probe, int selectedStream) {
    for(const auto &stream:probe.value("streams",Json::array())) {
        int index=int(number(stream,"index",-1));
        if(str(stream,"codec_type")=="subtitle" && index>=0 && (selectedStream<0 || selectedStream==index)) {
            auto codec=str(stream,"codec_name");
            return {index,codec,originalSubtitleExtension(codec)};
        }
    }
    throw std::runtime_error("Выбранная дорожка субтитров не найдена. Исходник сохранён.");
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
    if(s.role==3 && (m.ext=="jpg" || m.ext=="jpeg"))s.coverFormat=1;
    return s;
}
// Split a local container into individually selectable sources while retaining
// the original path and absolute stream index for lossless remapping.
inline std::vector<AssemblySource> assemblySourcesFromProbe(const Media &media,const std::string &path,const Json &probe) {
    std::vector<AssemblySource> result;
    auto streams=probe.value("streams",Json::array());
    if(!streams.is_array())return result;
    auto formatName=str(probe.value("format",Json::object()),"format_name");
    if(streams.size()==1 && str(streams[0],"codec_type")=="video" && number(streams[0].value("disposition",Json::object()),"attached_pic")==0 &&
       (formatName.find("image2")!=std::string::npos || formatName.find("_pipe")!=std::string::npos || formatName=="gif" || formatName=="apng")) {
        auto source=assemblySource(media,path);source.role=3;source.stream=-1;source.media.category="Изображения";source.probedCodec=str(streams[0],"codec_name");source.coverFormat=source.probedCodec=="mjpeg"?1:0;result.push_back(std::move(source));return result;
    }
    for(const auto &stream:streams) {
        const auto type=str(stream,"codec_type"),codec=str(stream,"codec_name");
        const auto tags=stream.value("tags",Json::object());
        const auto disposition=stream.value("disposition",Json::object());
        auto filename=str(tags,"filename");std::string lower=filename;
        std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});
        bool cover=number(disposition,"attached_pic")!=0 || ((codec=="png" || codec=="mjpeg") && (lower=="cover.png" || lower=="cover.jpg" || lower=="cover.jpeg"));
        int role=cover?3:type=="video"?0:type=="audio"?1:type=="subtitle"?2:6;
        int index=int(number(stream,"index",-1));if(role<0 || index<0)continue;
        auto source=assemblySource(media,path);source.role=role;source.stream=index;source.includeAudio=false;
        auto languageKey=metadataFirstKey(tags,{"language"},"");
        source.language=role<3 && !languageKey.empty()?str(tags,languageKey.c_str()):"";
        auto labelKey=metadataFirstKey(tags,{"title","name"},"");
        auto label=labelKey.empty()?filename:str(tags,labelKey.c_str());
        source.name=label.empty()?media.title+" · #"+std::to_string(index):label;
        source.probedCodec=codec;source.primary=number(disposition,"default")!=0;source.forced=role<3 && number(disposition,"forced")!=0;source.originalDisposition=disposition;
        source.originalStreamTags=tags;source.originalStreamInfo=stream;
        if(role<3)source.embeddedKind="stream";
        if(role==0)source.media.vcodec=codec;else if(role==1)source.media.acodec=codec;
        if(role==3){source.embeddedKind="cover";source.coverFormat=codec=="mjpeg"?1:0;source.originalCoverTags=tags;}
        if(role==6)source.embeddedKind=type;
        result.push_back(std::move(source));
    }
    for(const char *key:{"programs","stream_groups"}) {
        auto entries=probe.value(key,Json::array());
        if(entries.is_array() && !entries.empty()) {
            auto source=assemblySource(media,path);source.role=6;source.stream=std::string(key)=="programs"?-2:-3;
            source.embeddedKind=key;source.name=std::string(key)=="programs"?"Программы контейнера":"Группы потоков";source.language.clear();result.push_back(std::move(source));
        }
    }
    auto chapters=probe.value("chapters",Json::array());
    if(chapters.is_array() && !chapters.empty()) {
        auto source=assemblySource(media,path);source.role=4;source.stream=-1;source.embeddedKind="chapters";source.name="Главы · "+media.title;source.language.clear();result.push_back(std::move(source));
    }
    auto tags=probe.value("format",Json::object()).value("tags",Json::object());
    if(tags.is_object() && !tags.empty()) {
        auto source=assemblySource(media,path);source.role=5;source.stream=-1;source.embeddedKind="metadata";source.name="Метаданные · "+media.title;source.language.clear();result.push_back(std::move(source));
    }
    return result;
}
inline Json assemblyJson(const AssemblySource &s) {
    Json chapters=Json::array();for(const auto &c:s.chapterEdits)chapters.push_back({{"start",c.start},{"end",c.end},{"title",c.title},{"tags",c.tags}});
    return {{"path",s.path},{"embeddedKind",s.embeddedKind},{"dependency",s.dependency},{"name",s.name},{"language",s.language},{"description",s.description},{"summary",s.summary},{"synopsis",s.synopsis},{"handlerName",s.handlerName},{"artist",s.artist},{"date",s.date},{"bitrate",s.bitrate},{"maxrate",s.maxrate},{"bufsize",s.bufsize},{"sampleRate",s.sampleRate},{"frameRate",s.frameRate},{"width",s.width},{"role",s.role},{"stream",s.stream},{"metadataStream",s.metadataStream},
            {"chapterEdits",chapters},{"chaptersReady",s.chaptersReady},{"originalDisposition",s.originalDisposition},{"originalCoverTags",s.originalCoverTags},{"originalStreamTags",s.originalStreamTags},{"originalStreamInfo",s.originalStreamInfo},{"extraTags",s.extraTags},{"originalExtraTags",s.originalExtraTags},{"originalKnownTags",s.originalKnownTags},{"ocrExecutable",s.ocrExecutable},{"ocrLanguage",s.ocrLanguage},{"metadataOverrides",s.metadataOverrides},{"metadataRemoved",s.metadataRemoved},{"metadataPathEdits",s.metadataPathEdits},{"metadataEdit",s.metadataEdit},{"metadataEditTitle",s.metadataEditTitle},{"metadataEditArtist",s.metadataEditArtist},{"metadataEditDescription",s.metadataEditDescription},{"probedCodec",s.probedCodec},{"codec",s.codec},{"quality",s.quality},{"coverFormat",s.coverFormat},{"exportFormat",s.exportFormat},{"exportQuality",s.exportQuality},{"primary",s.primary},{"forced",s.forced},{"includeAudio",s.includeAudio},{"enabled",s.enabled},{"optionalAudio",s.optionalAudio},{"derivedAudio",s.derivedAudio}};
}
inline AssemblySource assemblyFrom(const Json &v) {
    AssemblySource s; s.path = str(v,"path"); s.embeddedKind=str(v,"embeddedKind"); s.dependency = str(v,"dependency"); s.name = str(v,"name"); s.language = str(v,"language");
    if(v.contains("metadataPathEdits") && v["metadataPathEdits"].is_array())s.metadataPathEdits=v["metadataPathEdits"];
    if(v.contains("metadataOverrides") && v["metadataOverrides"].is_object())s.metadataOverrides=v["metadataOverrides"].get<std::map<std::string,Json>>();
    if(v.contains("metadataRemoved") && v["metadataRemoved"].is_array())s.metadataRemoved=v["metadataRemoved"].get<std::vector<std::string>>();
    if(v.contains("extraTags") && v["extraTags"].is_object())s.extraTags=v["extraTags"].get<std::map<std::string,std::string>>();
    if(v.contains("originalExtraTags") && v["originalExtraTags"].is_object())s.originalExtraTags=v["originalExtraTags"].get<std::map<std::string,std::string>>();
    if(v.contains("originalKnownTags") && v["originalKnownTags"].is_object())s.originalKnownTags=v["originalKnownTags"].get<std::map<std::string,std::string>>();
    if(v.contains("originalStreamInfo") && v["originalStreamInfo"].is_object())s.originalStreamInfo=v["originalStreamInfo"];
    if(v.contains("originalStreamTags") && v["originalStreamTags"].is_object())s.originalStreamTags=v["originalStreamTags"];
    if(v.contains("originalCoverTags") && v["originalCoverTags"].is_object())s.originalCoverTags=v["originalCoverTags"];
    if(v.contains("originalDisposition") && v["originalDisposition"].is_object())s.originalDisposition=v["originalDisposition"];
    if(v.contains("chapterEdits") && v["chapterEdits"].is_array())for(const auto &c:v["chapterEdits"])if(c.is_object())s.chapterEdits.push_back({str(c,"start"),str(c,"end"),str(c,"title"),c.value("tags",Json::object())});
    s.chaptersReady=v.value("chaptersReady",false);
    s.ocrExecutable=str(v,"ocrExecutable");s.ocrLanguage=str(v,"ocrLanguage","eng");
    s.probedCodec=str(v,"probedCodec");s.metadataEdit=v.value("metadataEdit",false);s.metadataEditTitle=str(v,"metadataEditTitle");s.metadataEditArtist=str(v,"metadataEditArtist");s.metadataEditDescription=str(v,"metadataEditDescription");
    s.description=str(v,"description");s.summary=str(v,"summary");s.synopsis=str(v,"synopsis");s.handlerName=str(v,"handlerName");s.artist=str(v,"artist");s.date=str(v,"date");s.bitrate=str(v,"bitrate");s.maxrate=str(v,"maxrate");s.bufsize=str(v,"bufsize");s.sampleRate=str(v,"sampleRate");s.frameRate=str(v,"frameRate");s.width=str(v,"width");
    s.role=v.value("role",0); s.stream=v.value("stream",-1); s.metadataStream=v.value("metadataStream",-1); s.codec=v.value("codec",0); s.quality=v.value("quality",1); s.coverFormat=v.value("coverFormat",0); s.exportFormat=v.value("exportFormat",0);s.exportQuality=v.value("exportQuality",1);
    s.primary=v.value("primary",false); s.forced=v.value("forced",false); s.includeAudio=v.value("includeAudio",false); s.enabled=v.value("enabled",true);s.optionalAudio=v.value("optionalAudio",false);s.derivedAudio=v.value("derivedAudio",false);
    return s;
}
}
