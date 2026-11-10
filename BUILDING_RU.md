# Сборка StreamShelf

### [English](BUILDING.md) | Русский

Нужны Windows 10/11 x64, CMake 3.22 или новее, Ninja, компилятор C++20 (MSVC с Windows SDK либо LLVM-MinGW) и Node.js с поддержкой `node --test` (рекомендуется Node.js 22). Исходники не содержат .NET-проекта. Используйте инструменты x64 и запускайте команды из корня репозитория. Для MSVC откройте x64 Native Tools Command Prompt либо Developer PowerShell с активированными инструментами MSVC; для LLVM-MinGW добавьте его `bin` в PATH.

```powershell
cmake --preset dev
cmake --build --preset dev --parallel
ctest --preset dev
```

Повторите эти команды после изменений: Ninja пересоберёт затронутые файлы. Готовое приложение и папки Assets и extensions находятся в `build/dev`. Конфигурация `release` работает аналогично: замените `dev` на `release`. Если Node.js отсутствует при настройке CMake, JavaScript-тесты не регистрируются; установите Node.js и запустите настройку заново. CTest выводит только фактически зарегистрированные тесты.

Для включения MediaInfo в выпускной комплект задайте CMake параметр `STREAMSHELF_MEDIAINFO_DLL` с полным путём к совместимой подписанной `MediaInfo.dll`. При установке библиотека будет скопирована рядом с `StreamShelf.exe`, а уведомление поставщика — из `licenses/MediaInfo-License.html`. Например, для установленной MediaInfo:

```powershell
cmake --preset release "-DSTREAMSHELF_MEDIAINFO_DLL=C:/Program Files/MediaInfo/MediaInfo.dll"
cmake --build --preset release --parallel
ctest --preset release
```

Без указанного параметра DLL в комплект не включается; приложение будет искать её в своей папке или стандартной папке установки MediaInfo. При подготовке архива проверяйте архитектуру, подпись и лицензии фактической библиотеки.

Для установки в `release/StreamShelf-Native`:

```powershell
cmake --install build/release --prefix release/StreamShelf-Native
# Или выполнить настройку, сборку, тесты и установку одной командой:
./scripts/Build.ps1 -Preset release
```

Для отключения тестов используйте отдельную папку сборки, например `cmake -S . -B build/app -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF`. По умолчанию среда C++ связывается статически; `-DSTREAMSHELF_STATIC_RUNTIME=OFF` выбирает динамическую среду. Для распространения динамической сборки потребуются совместимые runtime DLL или соответствующий Microsoft Visual C++ Redistributable. Убедитесь в работоспособности комплекта на чистой машине.

Интерактивный тест мастера установки требует доступного рабочего стола и включается параметром `-DSTREAMSHELF_NATIVE_GUIDE_TESTS=ON` при настройке. Он не входит в обычный CI. Поддерживаемый способ сборки использует CMake presets и скрипт `scripts/Build.ps1`.

## Инструменты и расширения для выпуска

Программа устанавливает yt-dlp, FFmpeg/FFprobe и Deno через раздел «Настройки → Компоненты» в `%LOCALAPPDATA%\StreamShelfNative\tools`. Компоненты из `bundled-tools` программа не использует. См. README и THIRD_PARTY.md.

В комплект входят исходники Chromium-расширения и имеющийся подписанный Firefox XPI. После изменения исходников Firefox прежний XPI не обновляется автоматически: для выпуска требуется новая подписанная версия, см. FIREFOX-SIGNING.md. Для подтверждения выпуска отдельно проверьте установку, тесты и загрузку медиа на Windows; исторический VERIFICATION.md не является результатом для текущего коммита.
