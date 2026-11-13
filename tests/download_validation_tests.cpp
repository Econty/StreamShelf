#include "download_validation.hpp"
#include <iostream>
#include <stdexcept>
using namespace ss;
int main() {
    try {
        auto check = [](bool ok, const char *message) { if (!ok) throw std::runtime_error(message); std::cout << "PASS " << message << '\n'; };
        Json video = {{"streams", Json::array({{{"codec_type", "video"}}})}};
        Json audio = {{"streams", Json::array({{{"codec_type", "audio"}}})}};
        Json both = {{"streams", Json::array({{{"codec_type", "video"}}, {{"codec_type", "audio"}}})}};
        check(probeMatches(0, video, ExpectedStreams::Video), "video-only download validates as video");
        check(!probeMatches(0, video, ExpectedStreams::VideoAndAudio), "video-only output cannot claim a successful merge");
        check(probeMatches(0, both, ExpectedStreams::VideoAndAudio), "merged audio and video both required");
        check(probeMatches(0, audio, ExpectedStreams::Audio) && !probeMatches(0, audio, ExpectedStreams::Video), "audio does not pass a video check");
        Json coverAudio = {{"streams", Json::array({{{"codec_type", "audio"}}, {{"codec_type", "video"}, {"disposition", {{"attached_pic", 1}}}}})}};
        check(probeMatches(0, coverAudio, ExpectedStreams::Audio), "audio with embedded cover validates as audio");
        check(!probeMatches(0, coverAudio, ExpectedStreams::Video) && !probeMatches(0, coverAudio, ExpectedStreams::VideoAndAudio), "attached cover is not playable video");
        Json coverAndVideo = {{"streams", Json::array({{{"codec_type", "audio"}}, {{"codec_type", "video"}, {"disposition", {{"attached_pic", 1}}}}, {{"codec_type", "video"}, {"disposition", {{"attached_pic", 0}}}}})}};
        check(probeMatches(0, coverAndVideo, ExpectedStreams::VideoAndAudio), "a real video stream still validates alongside an embedded cover");
        check(!probeMatches(1, both, ExpectedStreams::AnyMedia) && !probeMatches(0, Json::object(), ExpectedStreams::AnyMedia), "failed or empty ffprobe does not validate");
        check(finalDownloadedPath("progress\r\nSTREAMSHELF_FINAL:C:\\output\\media.mkv\r\n") == std::optional<std::string>("C:\\output\\media.mkv"), "extract only final moved path");
        check(!finalDownloadedPath("STREAMSHELF_FINAL:C:\\one\nSTREAMSHELF_FINAL:C:\\two\n"), "ambiguous final paths are rejected");
        check(!finalDownloadedPath("warning STREAMSHELF_FINAL:wrong\n"), "marker embedded in output is not trusted");
        check(looksLikeHtml("\xEF\xBB\xBF  <!DOCTYPE HTML><html>403") && looksLikeHtml("\n<html>error"), "HTML error content rejected regardless of filename");
        check(!looksLikeHtml("WEBVTT\n\n00:00") && !looksLikeHtml("\x89PNG\r\n"), "valid non-HTML content not rejected by HTML check");
        return 0;
    } catch (const std::exception &e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
