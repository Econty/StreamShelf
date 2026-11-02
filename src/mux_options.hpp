#pragma once
#include "core.hpp"
namespace ss {
struct MuxOptions { int container = 0, video = 0, audio = 0, subtitles = 0; };
inline std::string muxContainer(const MuxOptions &o) {
    static const char *v[] = {"mkv", "mp4", "mov", "webm"};
    if (o.container < 0 || o.container >= 4) throw std::runtime_error("Неизвестный контейнер");
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
inline std::string chaptersToFfmetadata(const Json &chapters) {
    if (!chapters.is_array() || chapters.empty()) throw std::runtime_error("Список глав пуст или имеет неверный формат");
    std::string out = ";FFMETADATA1\n";
    for (const auto &c : chapters) {
        double start = number(c, "start_time", -1), end = number(c, "end_time", -1);
        if (!std::isfinite(start) || !std::isfinite(end) || start < 0 || end <= start || end > 100000000)
            throw std::runtime_error("Не удалось определить границы глав");
        std::string title = str(c, "title", "Глава");
        std::string escaped;
        for (char ch : title) { if (ch == '\r' || ch == '\n') { escaped += ' '; continue; } if (ch == '=' || ch == ';' || ch == '#' || ch == '\\') escaped += '\\'; escaped += ch; }
        out += "[CHAPTER]\nTIMEBASE=1/1000\nSTART=" + std::to_string((long long)std::llround(start * 1000)) + "\nEND=" + std::to_string((long long)std::llround(end * 1000)) + "\ntitle=" + escaped + "\n";
    }
    return out;
}
}
