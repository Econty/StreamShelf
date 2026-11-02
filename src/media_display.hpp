#pragma once
#include "core.hpp"
namespace ss {
inline bool matchesMediaCategory(const Media &m, const std::string &category) {
    if (category == "Все категории") return true;
    if (category == "Видео") return m.category == "Видео" || m.category == "Видео без звука" || m.category == "Видео · звук не проверен";
    return m.category == category;
}
inline std::string videoSoundLabel(const Media &m) {
    if (m.videoOnly || m.category == "Видео без звука") return "Без звука";
    if (!m.acodec.empty() && m.acodec != "none") return "Со звуком";
    return "Звук не проверен";
}
}
