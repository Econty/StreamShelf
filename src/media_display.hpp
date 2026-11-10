#pragma once
#include "core.hpp"
#include "interface_text.hpp"
namespace ss {
inline std::string displayMediaTitle(std::string_view language, const Media &m) {
    if (language != "en") return m.title;
    if (m.id == "metadata" && m.title == "Метаданные JSON") return "JSON metadata";
    if (m.id == "chapters" && m.title == "Главы") return "Chapters";
    if (m.id == "description" && m.title == "Описание") return "Description";
    if (m.id.rfind("thumb:", 0) == 0 && m.title == "Обложка") return "Cover";
    return m.title;
}
inline std::string mediaSearchText(std::string_view language, const Media &m) {
    return m.title + " " + displayMediaTitle(language, m) + " " + m.language + " " + m.id + " " + m.vcodec + " " + m.acodec;
}
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
