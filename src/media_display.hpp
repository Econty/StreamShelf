#pragma once
#include "core.hpp"
#include "interface_text.hpp"
namespace ss {
// Each item carries the title of its own page, even when results from multiple
// analyses are kept in the same list.
inline std::string mediaJobTitle(const Media &m) {
    if (m.sourceTitle.empty()) return m.title;
    if (m.title.empty() || m.title == m.sourceTitle) return m.sourceTitle;
    return m.sourceTitle + " · " + m.title;
}
inline std::string displayMediaTitle(std::string_view language, const Media &m) {
    if (language != "en") return m.title;
    if (m.id == "metadata" && m.title == "Метаданные JSON") return "JSON metadata";
    if (m.id == "chapters" && m.title == "Главы") return "Chapters";
    if (m.id == "description" && m.title == "Описание") return "Description";
    if (m.id.rfind("thumb:", 0) == 0 && m.title == "Обложка") return "Cover";
    return m.title;
}
inline std::string mediaSearchText(std::string_view language, const Media &m) {
    return m.sourceTitle + " " + m.title + " " + displayMediaTitle(language, m) + " " + m.language + " " + m.id + " " + m.vcodec + " " + m.acodec;
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
