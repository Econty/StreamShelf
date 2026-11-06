# Сторонние компоненты C++-версии

- ChromeExtensionInstaller — MIT, https://github.com/Ultimate-Destroyer/ChromeExtensionInstaller (ветка `py`). Сценарий пошаговой установки переработан в нативный C++/WinAPI-модуль `src/extension_installer.cpp`; исходные Python, Tkinter и PyAutoGUI не входят в программу. Текст лицензии сохранён без изменений в `licenses/ChromeExtensionInstaller-LICENSE.txt`.

- MediaInfoLib 26.10 — MediaArea.net SARL; подключается динамически для анализа локальных файлов. Выпускной архив включает подписанную `MediaInfo.dll` и предоставленный разработчиком текст лицензии и уведомления о сторонних компонентах в `licenses/MediaInfo-License.html`. This product uses MediaInfo library, Copyright (c) 2002-2025 MediaArea.net SARL, https://mediaarea.net/MediaInfo. Исходный код: https://github.com/MediaArea/MediaInfoLib.
- nlohmann/json 3.12.0 — MIT, https://github.com/nlohmann/json, `licenses/json-LICENSE.txt` и уведомления в vendor/json.hpp.
- stb_image / stb_image_write — MIT или public domain, https://github.com/nothings/stb. `stb_image_write` используется только GPU-проверкой. Текст в `licenses/stb-LICENSE.txt` и заголовках.
- Khronos OpenGL/EGL headers — MIT, уведомления в заголовках `vendor` и `licenses/Khronos-LICENSE.txt`.
- SDF-атлас DejaVu из предоставленного пользователем архива KRECHETOV LABS: Bitstream Vera / DejaVu; полный текст `Assets/fonts/LICENSE.txt`. Подход к отрисовке адаптирован с Android GLES на C++ desktop OpenGL. Apple-код, San Francisco и другие проприетарные компоненты Apple не используются.
- LLVM-MinGW / MinGW-w64 — использованы для локальной сборки Windows; компилятор в выпуск не включён. Уведомления включённых компонентов см. в `licenses`.
- yt-dlp — https://github.com/yt-dlp/yt-dlp, основной код Unlicense; Windows executable включает Python и зависимости с собственными лицензиями.
- FFmpeg/FFprobe Windows Essentials — https://www.gyan.dev/ffmpeg/builds/, GPL-сборка. `bundled-tools/FFmpeg-LICENSE.txt`; исходники https://ffmpeg.org/download.html и https://git.ffmpeg.org/ffmpeg.git. При дальнейшей дистрибуции необходимо выполнить лицензионные требования точной сборки и её зависимостей.
- Deno — MIT, https://github.com/denoland/deno, со сторонними компонентами поставщика.

Инструменты и метаданные восстановлены из ранее предоставленного архива StreamShelf. Версии/поставщики зафиксированы в `bundled-tools/sources.json`, SHA-256 EXE — в `bundled-tools/checksums.json`. Хэши проверяют совпадение с прежней поставкой, не подтверждают независимо её происхождение; это не Authenticode-подпись StreamShelf.

.NET, WPF, OpenTK, GLFW и StbImageSharp в C++-версии не используются.
