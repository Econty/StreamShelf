#pragma once
#include "core.hpp"
namespace ss {
struct AssemblySource {
    Media media;
    std::string path, dependency, name, language, description, artist, date, bitrate, maxrate, bufsize, sampleRate, frameRate, width;
    int role = 0, stream = -1, codec = 0, quality = 1;
    uint64_t uiId = 0;
    bool primary = false, forced = false, includeAudio = false, enabled = true, optionalAudio = false, derivedAudio = false;
};
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
    return {{"path",s.path},{"dependency",s.dependency},{"name",s.name},{"language",s.language},{"description",s.description},{"artist",s.artist},{"date",s.date},{"bitrate",s.bitrate},{"maxrate",s.maxrate},{"bufsize",s.bufsize},{"sampleRate",s.sampleRate},{"frameRate",s.frameRate},{"width",s.width},{"role",s.role},{"stream",s.stream},
            {"codec",s.codec},{"quality",s.quality},{"primary",s.primary},{"forced",s.forced},{"includeAudio",s.includeAudio},{"enabled",s.enabled},{"optionalAudio",s.optionalAudio},{"derivedAudio",s.derivedAudio}};
}
inline AssemblySource assemblyFrom(const Json &v) {
    AssemblySource s; s.path = str(v,"path"); s.dependency = str(v,"dependency"); s.name = str(v,"name"); s.language = str(v,"language");
    s.description=str(v,"description");s.artist=str(v,"artist");s.date=str(v,"date");s.bitrate=str(v,"bitrate");s.maxrate=str(v,"maxrate");s.bufsize=str(v,"bufsize");s.sampleRate=str(v,"sampleRate");s.frameRate=str(v,"frameRate");s.width=str(v,"width");
    s.role=v.value("role",0); s.stream=v.value("stream",-1); s.codec=v.value("codec",0); s.quality=v.value("quality",1);
    s.primary=v.value("primary",false); s.forced=v.value("forced",false); s.includeAudio=v.value("includeAudio",false); s.enabled=v.value("enabled",true);s.optionalAudio=v.value("optionalAudio",false);s.derivedAudio=v.value("derivedAudio",false);
    return s;
}
}
