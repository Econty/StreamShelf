#include "probe_result.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
using namespace ss;
int main() {
    Media item; item.category = "Требуют проверки"; item.protocol = "https"; item.data = Json::object();
    assert(!applyMediaProbe(item, Json::object()));
    assert(item.category == "Требуют проверки" && !item.videoOnly);
    Json video = {{"streams", Json::array({{{"codec_type", "video"}, {"codec_name", "h264"}, {"height", 720}}})}, {"format", {{"format_name", "mov,mp4"}, {"duration", "3.4"}}}};
    assert(applyMediaProbe(item, video)); assert(item.category == "Видео без звука" && item.videoOnly && item.quality == "720p");
    video["streams"].push_back({{"codec_type", "audio"}, {"codec_name", "aac"}});
    assert(applyMediaProbe(item, video)); assert(item.category == "Видео" && !item.videoOnly && item.acodec == "aac");
    Media hls; hls.protocol = "m3u8"; hls.data = Json::object(); video["streams"].erase(1);
    assert(applyMediaProbe(hls, video)); assert(hls.category == "Видео · звук не проверен" && !hls.videoOnly);
    Media image; image.category = "Требуют проверки"; image.data = Json::object(); video["format"]["format_name"] = "png_pipe";
    assert(!applyMediaProbe(image, video)); assert(image.category == "Требуют проверки");
    Media cover; cover.data = Json::object(); Json withCover = {{"streams", Json::array({{{"codec_type", "video"}, {"disposition", {{"attached_pic", 1}}}}})}};
    assert(!applyMediaProbe(cover, withCover));
    std::cout << "PASS video, audio, silent video, uncertain HLS, invalid response and images\n";
}
