#pragma once
#include "core.hpp"

namespace ss {
inline bool looksLikeHtml(const std::string &data) {
    size_t start = data.rfind("\xEF\xBB\xBF", 0) == 0 ? 3 : 0;
    while (start < data.size() && (data[start] == ' ' || data[start] == '\t' || data[start] == '\r' || data[start] == '\n')) ++start;
    auto prefix = data.substr(start, 32);
    std::transform(prefix.begin(), prefix.end(), prefix.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return prefix.rfind("<!doctype html", 0) == 0 || prefix.rfind("<html", 0) == 0 || prefix.rfind("<head", 0) == 0 || prefix.rfind("<body", 0) == 0;
}
enum class ExpectedStreams { AnyMedia, Audio, Video, VideoAndAudio };

// A nonzero exit or malformed/empty output is not evidence of a media file.
inline bool probeMatches(int exitCode, const Json &probe, ExpectedStreams expected) {
    if (exitCode != 0 || !probe.is_object() || !probe.contains("streams") || !probe["streams"].is_array()) return false;
    bool video = false, audio = false;
    for (const auto &stream : probe["streams"]) {
        if (!stream.is_object()) continue;
        auto it = stream.find("codec_type");
        if (it == stream.end() || !it->is_string()) continue;
        video |= it->get_ref<const std::string &>() == "video";
        audio |= it->get_ref<const std::string &>() == "audio";
    }
    switch (expected) {
    case ExpectedStreams::AnyMedia: return video || audio;
    case ExpectedStreams::Audio: return audio;
    case ExpectedStreams::Video: return video;
    case ExpectedStreams::VideoAndAudio: return video && audio;
    }
    return false;
}

// The marker is emitted by yt-dlp's after_move hook; only complete lines count.
inline std::optional<std::string> finalDownloadedPath(const std::string &output) {
    constexpr const char *prefix = "STREAMSHELF_FINAL:";
    std::optional<std::string> result;
    std::istringstream input(output);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind(prefix, 0) != 0) continue;
        auto path = line.substr(std::char_traits<char>::length(prefix));
        if (path.empty() || (result && *result != path)) return {};
        result = std::move(path);
    }
    return result;
}
} // namespace ss
