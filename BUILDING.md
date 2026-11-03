# Сборка StreamShelf

Нужны Windows 10/11 x64, CMake 3.22 или новее, Ninja, компилятор C++20 (MSVC с Windows SDK либо LLVM-MinGW) и Node.js с поддержкой `node --test` (рекомендуется Node.js 22). Исходники не содержат .NET-проекта. Используйте инструменты x64 и запускайте команды из корня репозитория. Для MSVC откройте x64 Native Tools Command Prompt либо Developer PowerShell с активированными инструментами MSVC; для LLVM-MinGW добавьте его `bin` в PATH.

```powershell
cmake --preset dev
cmake --build --preset dev --parallel
ctest --preset dev
```

Повторите эти команды после изменений: Ninja пересоберёт затронутые файлы. Готовое приложение и папки Assets и extensions находятся в `build/dev`. Конфигурация `release` работает аналогично: замените `dev` на `release`. Если Node.js отсутствует при настройке CMake, JavaScript-тесты не регистрируются; установите Node.js и запустите настройку заново. CTest выводит только фактически зарегистрированные тесты.

Для установки в `release/StreamShelf-Native`:

```powershell
cmake --install build/release --prefix release/StreamShelf-Native
# Или выполнить настройку, сборку, тесты и установку одной командой:
./scripts/Build.ps1 -Preset release
```

Для отключения тестов используйте отдельную папку сборки, например `cmake -S . -B build/app -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF`. По умолчанию среда C++ связывается статически; `-DSTREAMSHELF_STATIC_RUNTIME=OFF` выбирает динамическую среду. Для распространения динамической сборки потребуются совместимые runtime DLL или соответствующий Microsoft Visual C++ Redistributable. Убедитесь в работоспособности комплекта на чистой машине.

Интерактивный тест мастера установки требует доступного рабочего стола и включается параметром `-DSTREAMSHELF_NATIVE_GUIDE_TESTS=ON` при настройке. Он не входит в обычный CI. Старые `Build-Local.ps1`, `Build-App.ps1` и `build-cross.sh` сохранены для прежних сценариев и используют отдельные правила и кэши; основной поддерживаемый путь — CMake presets.

## Инструменты и расширения для выпуска

Сборка не создаёт `bundled-tools`: подготовьте отдельно совместимые yt-dlp, FFmpeg/FFprobe и Deno, `checksums.json`, сведения об источниках и лицензии. `scripts/Build.ps1 -IncludeTools` копирует заранее подготовленную папку `artifacts/bundled-tools`; сама эта команда не подтверждает происхождение файлов. См. README и THIRD_PARTY.md.

В комплект входят исходники Chromium-расширения и имеющийся подписанный Firefox XPI. После изменения исходников Firefox прежний XPI не обновляется автоматически: для выпуска требуется новая подписанная версия, см. FIREFOX-SIGNING.md. Для подтверждения выпуска отдельно проверьте установку, тесты и загрузку медиа на Windows; исторический VERIFICATION.md не является результатом для текущего коммита.
