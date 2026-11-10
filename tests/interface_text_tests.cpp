#include "interface_text.hpp"
#include "log_text.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
int main() {
    using ss::interfaceText; using ss::interfaceSize;
    assert(std::string(ss::defaultInterfaceLanguage(0x0419)) == "ru");
    assert(std::string(ss::defaultInterfaceLanguage(0x0819)) == "ru");
    assert(std::string(ss::defaultInterfaceLanguage(0x0409)) == "en");
    assert(std::string(ss::defaultInterfaceLanguage(0x0407)) == "en");
    assert(std::string(ss::defaultInterfaceLanguage(0)) == "en");
    assert(ss::displayLog({"12:34:56  Проверка компонентов запущена", true}, "en") == "12:34:56  Component check started");
    assert(ss::displayLog({"12:34:56  Проверка компонентов запущена", true}, "ru") == "12:34:56  Проверка компонентов запущена");
    assert(ss::displayLog({"12:34:56  Проверка компонентов запущена", false}, "en") == "12:34:56  Проверка компонентов запущена");
    assert(ss::displayLog({"12:34:56  Файл отсутствует: C:/Видео/file.txt", true}, "en") == "12:34:56  File missing: C:/Видео/file.txt");
    assert(ss::displayLog({"12:34:56  yt-dlp: установлена нет, доступна неизвестно; Не установлен", true}, "en") == "12:34:56  yt-dlp: installed none, available unknown; Not installed");
    assert(interfaceText("en", "Скачивание") == "Downloading");
    assert(interfaceText("ru", "Скачивание") == "Скачивание");
    assert(interfaceText("en", "Видео без звука") == "Video without audio");
    assert(interfaceText("en", "Расширение подключено · Chrome · Test") == "Extension connected · Chrome · Test");
    assert(interfaceText("en", "User-provided text") == "User-provided text");
    assert(interfaceText("en", "Контейнер не сохранил дату дорожки. Исходники сохранены.") == "The container did not preserve the track date. Sources were kept.");
    assert(interfaceText("en", "Не удалось преобразовать файл. Исходник сохранён. diagnostic") == "Could not convert the file. The original was kept. diagnostic");
    assert(interfaceText("en", "Контейнер не сохранил общий тег «Название». Выберите другой формат или удалите тег. Исходники сохранены.") == "The container did not preserve the global tag \"Название\". Choose another format or remove the tag. Sources were kept.");
    assert(interfaceText("en", "HTTP 429: субтитры отложены на 60 с") == "HTTP 429: subtitles delayed for 60 s");
    assert(interfaceText("en", "Загрузка: FFmpeg и FFprobe") == "Downloading: FFmpeg and FFprobe");
    assert(ss::displayLog({"Установлен yt-dlp 1.0 в C:/Видео/file", true}, "en") == "Installed yt-dlp 1.0 in C:/Видео/file");
    assert(interfaceText("en", "Ожидалось: видео и аудио; обнаружено: только видео. Скачанный файл сохранён в папке задачи.") == "Expected: video and audio; found: video only. The download was kept in the task folder.");
    assert(interfaceText("en", "yt-dlp не получил доступные форматы для этой страницы. Проверьте, что ролик воспроизводится в подключённом браузере, затем повторите анализ. Доступность форматов также зависит от YouTube и учётной записи.\n\nraw diagnostic") == "yt-dlp did not receive available formats for this page. Check playback in the connected browser and retry analysis. Format availability also depends on YouTube and the account.\n\nraw diagnostic");
    assert(interfaceText("en", "Скачано 12 МиБ из 24 МиБ · 2 МиБ/с") == "Downloaded 12 MiB of 24 MiB · 2 MiB/s");
    assert(interfaceText("en", "yt-dlp 2026.01 · движок готов") == "yt-dlp 2026.01 · ready");
    assert(interfaceSize("en", "12.5 МБ ~") == "12.5 MB ~");
    assert(interfaceSize("ru", "12.5 МБ ~") == "12.5 МБ ~");
    assert(interfaceSize("en", "Неизвестен") == "Unknown");
    assert(interfaceSize("en", "12 unknown") == "12 unknown");
}
