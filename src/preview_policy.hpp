#pragma once
#include "core.hpp"

namespace ss {
inline bool previewAudioCandidate(const Media &item) {
    if (item.category == "Аудио" || str(item.data, "kind") == "Аудио" ||
        (item.vcodec == "none" && !item.acodec.empty() && item.acodec != "none")) return true;
    auto extension=item.ext;
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return extension == "mp3" || extension == "m4a" || extension == "aac" || extension == "flac" ||
        extension == "ogg" || extension == "opus" || extension == "wav" || extension == "aiff" ||
        extension == "amr" || extension == "ac3" || extension == "ape";
}
inline bool previewFrameCandidate(const Media &item) {
    if (item.drm) return false;
    // Extractors sometimes return the webpage as their fallback URL. It is
    // never a media input, even if the format metadata says video/mp4.
    if (item.format && !item.page.empty() &&
        item.url.substr(0, item.url.find('#')) == item.page.substr(0, item.page.find('#'))) return false;
    if (item.category.find("Видео") != std::string::npos || item.category == "Аудио" ||
        item.category == "HLS" || item.category == "DASH") return true;
    const auto kind = str(item.data, "kind");
    if (kind.find("Видео") != std::string::npos || kind == "Аудио") return true;
    std::string extension = item.ext;
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return extension == "mp4" || extension == "webm" || extension == "mkv" || extension == "mov" ||
        extension == "m4v" || extension == "avi" || extension == "ts" || extension == "mpeg" ||
        extension == "mpg" || extension == "flv" || extension == "wmv" || extension == "asf" ||
        previewAudioCandidate(item);
}
inline bool previewManifest(const Media &item) {
    if (item.category == "HLS" || item.category == "DASH" || item.protocol == "m3u8" ||
        item.protocol == "m3u8_native" || item.protocol == "dash" || item.protocol == "http_dash_segments") return true;
    auto path = item.url.substr(0, item.url.find_first_of("?#"));
    std::transform(path.begin(), path.end(), path.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return path.ends_with(".m3u8") || path.ends_with(".mpd");
}
inline bool previewHasCookies(std::string_view jar) {
    while (!jar.empty()) {
        const auto end=jar.find('\n');
        auto line=jar.substr(0,end);
        if ((!line.empty() && line.front()!='#' && line.find('\t')!=line.npos) ||
            line.starts_with("#HttpOnly_")) return true;
        if (end==jar.npos) break;
        jar.remove_prefix(end+1);
    }
    return false;
}
} // namespace ss
