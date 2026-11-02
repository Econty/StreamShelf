#pragma once
#include "core.hpp"
namespace ss {
// Failed probes leave type and sound uncertain; a URL or a claimed MIME type is not proof.
inline bool applyMediaProbe(Media &item, const Json &probe) {
    if (!probe.is_object() || !probe.contains("streams") || !probe["streams"].is_array()) return false;
    if (probe.contains("format") && probe["format"].is_object()) {
        auto name = str(probe["format"], "format_name");
        if (name == "image2" || name.find("_pipe") != std::string::npos) return false;
    }
    const Json *video = nullptr, *audio = nullptr;
    for (const auto &stream : probe["streams"]) {
        if (!stream.is_object()) continue;
        if (stream.contains("disposition") && stream["disposition"].is_object() && number(stream["disposition"], "attached_pic") == 1) continue;
        const auto type = str(stream, "codec_type");
        if (type == "video" && !video) video = &stream;
        if (type == "audio" && !audio) audio = &stream;
    }
    if (!video && !audio) return false;
    if (video) {
        item.vcodec = str(*video, "codec_name");
        bool segmented = item.protocol == "m3u8" || item.protocol == "dash";
        item.acodec = audio ? str(*audio, "codec_name") : segmented ? "" : "none";
        item.videoOnly = !audio && !segmented;
        item.category = audio ? "Видео" : segmented ? "Видео · звук не проверен" : "Видео без звука";
        auto height = number(*video, "height");
        item.quality = height > 0 ? std::to_string(int(height)) + "p" : "Параметры определены";
    } else {
        item.acodec = str(*audio, "codec_name");
        item.videoOnly = false;
        item.category = "Аудио";
        item.quality = "Параметры определены";
    }
    if (probe.contains("format") && probe["format"].is_object()) {
        auto duration = str(probe["format"], "duration");
        if (!duration.empty()) { try { auto value = std::stod(duration); if (std::isfinite(value) && value > 0) item.data["duration"] = value; } catch (...) {} }
    }
    return true;
}
}
