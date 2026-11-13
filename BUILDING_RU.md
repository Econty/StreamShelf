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

После успешной подготовки выпускного комплекта соберите установщик с помощью Inno Setup:

```powershell
iscc .\scripts\StreamShelf-Setup.iss
```

Установщик появится в `release/StreamShelf-Setup-v2026.11.1-Beta.exe`.

Чтобы явно подготовить ограниченную бета-версию без подписанного расширения Firefox, выполните:

```powershell
./scripts/Build.ps1 -Preset release -BetaWithoutFirefox
```

При прямой сборке CMake используйте параметр `-DSTREAMSHELF_BETA_WITHOUT_FIREFOX=ON` и устанавливайте комплект в новую пустую папку. В этой бета-версии нет подписанного Firefox XPI; постоянная установка расширения Firefox будет недоступна до подписи Mozilla для Connector 1.0.5. Для обычного выпуска задайте `-DSTREAMSHELF_BETA_WITHOUT_FIREFOX=OFF` и предоставьте подписанный XPI.

Для отключения тестов используйте отдельную папку сборки, например `cmake -S . -B build/app -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF`. По умолчанию среда C++ связывается статически; `-DSTREAMSHELF_STATIC_RUNTIME=OFF` выбирает динамическую среду. Для распространения динамической сборки потребуются совместимые runtime DLL или соответствующий Microsoft Visual C++ Redistributable. Убедитесь в работоспособности комплекта на чистой машине.

Интерактивный тест мастера установки требует доступного рабочего стола и включается параметром `-DSTREAMSHELF_NATIVE_GUIDE_TESTS=ON` при настройке. Он не входит в обычный CI. Поддерживаемый способ сборки использует CMake presets и скрипт `scripts/Build.ps1`.

## Инструменты и расширения для выпуска

Программа хранит yt-dlp, FFmpeg/FFprobe и Deno в `%LOCALAPPDATA%\StreamShelfNative\tools`. Если пакет собран с `scripts/Build.ps1 -IncludeTools`, в разделе «Настройки → Компоненты» доступна явная установка недостающих файлов из комплекта без сети. Программа повторно проверяет SHA-256 по манифесту и запуск версий и не заменяет существующие компоненты. Напрямую из `bundled-tools` файлы не запускаются и автоматически не восстанавливаются. См. README и THIRD_PARTY.md.

В обычный выпуск входят исходники Chromium-расширения и подписанный Firefox XPI текущей версии. Имеющийся XPI 1.0.4 устарел: необходимо получить подпись Mozilla для 1.0.5, как описано в FIREFOX-SIGNING.md. Обычная Release-установка через `cmake --install` проверяет исходники, контрольные суммы файлов, криптографическую подпись и цепочку сертификатов; для проверки нужны Python 3 и Windows PowerShell, отзыв сертификатов автономно не проверяется. Явный бета-режим выше подготавливает комплект без подписанного Firefox XPI. После получения подписи дополнительно проверьте установку в Firefox. Debug-установка без XPI доступна для разработки. Для подтверждения выпуска отдельно проверьте установку, тесты и загрузку медиа на Windows; исторический VERIFICATION.md не является результатом для текущего коммита.
