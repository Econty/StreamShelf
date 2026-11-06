#pragma once
#include "core.hpp"
#include <iomanip>
namespace ss {
struct MuxOptions { int container = 0, video = 0, audio = 0, subtitles = 0; };
inline bool supportsExtendedTrackTags(int container) {
    return container == 0 || container == 3;
}
inline bool supportsForcedSubtitles(int container) {
    return container == 0 || container == 1 || container == 3;
}
inline std::string muxContainer(const MuxOptions &o) {
    static const char *v[] = {"mkv", "mp4", "mov", "webm", "mp3", "m4a", "flac"};
    if (o.container < 0 || o.container >= 7) throw std::runtime_error("Неизвестный контейнер");
    return v[o.container];
}
inline std::vector<std::wstring> muxCodecArgs(const MuxOptions &o) {
    static const wchar_t *v[] = {L"copy", L"libx264", L"libx265", L"libvpx-vp9", L"libaom-av1"};
    static const wchar_t *a[] = {L"copy", L"aac", L"libopus", L"libmp3lame", L"flac"};
    static const wchar_t *s[] = {L"copy", L"srt", L"mov_text", L"webvtt"};
    if (o.video < 0 || o.video >= 5 || o.audio < 0 || o.audio >= 5 || o.subtitles < 0 || o.subtitles >= 4)
        throw std::runtime_error("Неизвестный кодек");
    return {L"-c:v", v[o.video], L"-c:a", a[o.audio], L"-c:s", s[o.subtitles]};
}
inline double muxDuration(const Json &object) {
    if(!object.is_object())return 0;
    try { double value=std::stod(str(object,"duration","0")); return std::isfinite(value)&&value>0?value:0; }
    catch (...) {return 0;}
}
inline Json chaptersFromProbe(const Json &probe) {
    auto entries=probe.value("chapters",Json::array());
    if(!entries.is_array() || entries.empty())throw std::runtime_error("Список глав пуст или имеет неверный формат");
    Json result=Json::array();
    for(const auto &entry:entries) {
        double start=-1,end=-1;
        try {
            auto parse=[](const Json &j,const char *key) {auto value=j.at(key);return value.is_number()?value.get<double>():std::stod(value.get<std::string>());};
            start=parse(entry,"start_time");end=parse(entry,"end_time");
        } catch (...) {throw std::runtime_error("Не удалось определить границы глав");}
        if(!std::isfinite(start)||!std::isfinite(end)||start<0||end<=start||end>100000000)throw std::runtime_error("Не удалось определить границы глав");
        auto tags=entry.value("tags",Json::object());std::string title="Глава";
        if(tags.is_object())for(auto it=tags.begin();it!=tags.end();++it) {
            auto key=it.key();std::transform(key.begin(),key.end(),key.begin(),[](unsigned char ch){return char(std::tolower(ch));});
            if(key=="title" && it->is_string()){title=it->get<std::string>();break;}
        }
        result.push_back({{"start_time",start},{"end_time",end},{"title",title},{"tags",tags}});
    }
    return result;
}
inline std::string chaptersToFfmetadata(const Json &chapters) {
    if (!chapters.is_array() || chapters.empty()) throw std::runtime_error("Список глав пуст или имеет неверный формат");
    std::string out = ";FFMETADATA1\n";
    for (const auto &c : chapters) {
        double start = number(c, "start_time", -1), end = number(c, "end_time", -1);
        if (!std::isfinite(start) || !std::isfinite(end) || start < 0 || end <= start || end > 100000000)
            throw std::runtime_error("Не удалось определить границы глав");
        auto escape=[](const std::string &value) {std::string result;for(char ch:value){if(ch=='='||ch==';'||ch=='#'||ch=='\\'||ch=='\r'||ch=='\n')result+='\\';result+=ch;}return result;};
        std::string title = str(c, "title", "Глава");
        out += "[CHAPTER]\nTIMEBASE=1/1000\nSTART=" + std::to_string((long long)std::llround(start * 1000)) + "\nEND=" + std::to_string((long long)std::llround(end * 1000)) + "\ntitle=" + escape(title) + "\n";
        auto tags=c.value("tags",Json::object());
        if(!tags.is_object())throw std::runtime_error("Некорректные метаданные главы");
        for(auto it=tags.begin();it!=tags.end();++it) {
            auto key=it.key();std::transform(key.begin(),key.end(),key.begin(),[](unsigned char ch){return char(std::tolower(ch));});
            if(key=="title")continue;
            if(!it->is_string())throw std::runtime_error("Неподдерживаемые метаданные главы");
            out+=escape(it.key())+"="+escape(it->get<std::string>())+"\n";
        }
    }
    return out;
}
inline std::vector<std::pair<std::string,std::string>> flacChapterTags(const Json &chapters,double duration) {
    if(!chapters.is_array() || chapters.empty() || chapters.size()>1000)
        throw std::runtime_error("FLAC поддерживает от 1 до 1000 глав. Выберите M4A или сохраните главы отдельно.");
    if(!std::isfinite(duration) || duration<=0)throw std::runtime_error("Не удалось определить длительность аудио для глав FLAC.");
    std::vector<std::pair<std::string,std::string>> result;
    double previousEnd=-1;
    for(size_t n=0;n<chapters.size();++n) {
        const auto &chapter=chapters[n];
        auto chapterTags=chapter.value("tags",Json::object());
        if(chapterTags.is_object())for(auto it=chapterTags.begin();it!=chapterTags.end();++it){auto key=it.key();std::transform(key.begin(),key.end(),key.begin(),[](unsigned char ch){return char(std::tolower(ch));});if(key!="title")throw std::runtime_error("FLAC не сохраняет дополнительные теги глав. Выберите другой контейнер.");}
        auto start=number(chapter,"start_time",-1),end=number(chapter,"end_time",-1);
        if(!std::isfinite(start) || !std::isfinite(end) || start<0 || end<=start || end>=360000 || std::llround(start*1000)>=360000000 || (n>0 && std::abs(start-previousEnd)>0.0015))
            throw std::runtime_error("FLAC сохраняет главы без промежутков и длительностью до 100 часов. Выберите M4A или сохраните главы отдельно.");
        previousEnd=end;auto milliseconds=static_cast<long long>(std::llround(start*1000));
        std::ostringstream key,time;key<<"CHAPTER"<<std::setfill('0')<<std::setw(3)<<n;
        time<<std::setfill('0')<<std::setw(2)<<(milliseconds/3600000)<<":"<<std::setw(2)<<((milliseconds/60000)%60)<<":"<<std::setw(2)<<((milliseconds/1000)%60)<<"."<<std::setw(3)<<(milliseconds%1000);
        auto title=str(chapter,"title","Глава");
        result.emplace_back(key.str(),time.str());result.emplace_back(key.str()+"NAME",std::move(title));
    }
    if(std::abs(previousEnd-duration)>0.005)throw std::runtime_error("Для FLAC конец последней главы должен совпадать с концом аудио. Выберите M4A или сохраните главы отдельно.");
    return result;
}
}
