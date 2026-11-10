#pragma once
#include "interface_text.hpp"
#include <string>
#include <string_view>

namespace ss {
struct LogEntry {
    std::string text;
    bool localizable = true;
};

// External program output and commands are never translated or modified.
inline std::string displayLog(const LogEntry &entry, std::string_view language) {
    if (language != "en" || !entry.localizable) return entry.text;
    const auto &line = entry.text;
    const bool timestamp = line.size() >= 10 && line[2] == ':' && line[5] == ':' && line[8] == ' ' && line[9] == ' ';
    const auto header = timestamp ? line.substr(0, 10) : std::string();
    const auto message = timestamp ? line.substr(10) : line;
    const auto translated = interfaceText(language, message);
    if (translated != message) return header + translated;
    static const std::pair<std::string_view, std::string_view> prefixes[] = {
        {"Обновлён yt-dlp ", "Updated yt-dlp "},
        {"Обновление недоступно. Используется локальная версия: ", "Update unavailable. Using local version: "},
        {"Компоненты: ", "Components: "},
        {"Не удалось уточнить параметры: ", "Could not determine properties: "},
        {"Проверка компонентов только в папке: ", "Checking components only in folder: "},
        {"Файл отсутствует: ", "File missing: "},
        {"Получаю контрольную сумму: ", "Retrieving checksum: "},
        {"Загрузка ", "Downloading "},
        {"SHA-256 проверен: ", "SHA-256 verified: "},
        {"Установлен ", "Installed "},
        {"Команда завершена: код ", "Command finished: exit code "},
        {"Ошибка команды: ", "Command error: "}
    };
    for (const auto &[ru, en] : prefixes) {
        if (message.rfind(ru, 0) != 0) continue;
        auto remainder = message.substr(ru.size());
        if (ru == "Установлен ") {
            const auto in = remainder.find(" в ");
            if (in != std::string::npos) remainder.replace(in, std::string_view(" в ").size(), " in ");
        }
        return header + std::string(en) + remainder;
    }
    // The component names are fixed by the application; version text remains untouched.
    for (const auto name : {"yt-dlp", "FFmpeg", "FFprobe", "Deno"}) {
        const std::string label = std::string(name) + ": ";
        if (message.rfind(label, 0) != 0) continue;
        auto rest = message.substr(label.size());
        if (rest == "не установлен или не запускается") return header + label + "not installed or failed to start";
        const std::string installed = "установлена ", available = ", доступна ", sep = "; ";
        if (rest.rfind(installed, 0) != 0) return line;
        auto split = rest.find(available, installed.size());
        auto status = split == std::string::npos ? split : rest.find(sep, split + available.size());
        if (split == std::string::npos || status == std::string::npos) return line;
        auto current = rest.substr(installed.size(), split - installed.size());
        auto latest = rest.substr(split + available.size(), status - split - available.size());
        if (current == "нет") current = "none";
        if (latest == "неизвестно") latest = "unknown";
        return header + label + "installed " + current + ", available " + latest + sep + interfaceText(language, rest.substr(status + sep.size()));
    }
    return line;
}
}
