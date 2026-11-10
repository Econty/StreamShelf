# Building StreamShelf

### English | [Русский](BUILDING_RU.md)

Build on Windows 10/11 x64 with CMake 3.22 or newer, Ninja, a C++20 compiler (MSVC with the Windows SDK or LLVM-MinGW), and Node.js with `node --test` support (Node.js 22 is recommended). This repository does not contain a .NET project. Use x64 tools and run commands from the repository root. For MSVC, open an x64 Native Tools Command Prompt or Developer PowerShell with the MSVC tools active; for LLVM-MinGW, add its `bin` directory to `PATH`.

```powershell
cmake --preset dev
cmake --build --preset dev --parallel
ctest --preset dev
```

Repeat these commands after making changes; Ninja rebuilds the affected files. The application and its `Assets` and `extensions` directories are placed in `build/dev`. The release configuration works the same way: use `release` instead of `dev`. If Node.js is unavailable during configuration, the JavaScript tests are not registered; install Node.js and configure again. CTest reports only the tests that were registered.

## Including MediaInfo

To include MediaInfo in a release package, set `STREAMSHELF_MEDIAINFO_DLL` to the full path of a compatible, signed `MediaInfo.dll`. Installation copies it next to `StreamShelf.exe` and includes the provider's license and notices from `licenses/MediaInfo-License.html`. For example:

```powershell
cmake --preset release "-DSTREAMSHELF_MEDIAINFO_DLL=C:/Program Files/MediaInfo/MediaInfo.dll"
cmake --build --preset release --parallel
ctest --preset release
```

Without this parameter the DLL is not packaged; the application looks for MediaInfo beside the executable or in its standard installation directory. Before distribution, verify the architecture, signature, and licenses of the actual library.

## Staging a release

```powershell
cmake --install build/release --prefix release/StreamShelf-Native
# Or configure, build, test, and stage in one command:
./scripts/Build.ps1 -Preset release
```

To disable tests, use a separate build directory, for example `cmake -S . -B build/app -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF`. The C++ runtime is linked statically by default; `-DSTREAMSHELF_STATIC_RUNTIME=OFF` selects the dynamic runtime. A distributed dynamic build requires compatible runtime DLLs or the appropriate Microsoft Visual C++ Redistributable. Check the package on a clean machine.

The interactive extension setup guide test requires an available desktop and is enabled with the CMake option `-DSTREAMSHELF_NATIVE_GUIDE_TESTS=ON`. It is not part of the regular CI. CMake presets and `scripts/Build.ps1` are the supported build path.

## Tools and browser extensions

StreamShelf installs yt-dlp, FFmpeg/FFprobe, and Deno through **Settings → Components** into `%LOCALAPPDATA%\StreamShelfNative\tools`. It does not use copies from `bundled-tools` as installed components. See the [README](README.md) and [third-party notices](THIRD_PARTY.md) (Russian).

The package includes the Chromium extension source and a signed Firefox XPI. Changing the Firefox source does not automatically update the signed XPI: a new signed version is required for release. See [Firefox signing](FIREFOX-SIGNING.md). Verify installation, tests, and media downloads on Windows for the actual build; [VERIFICATION.md](VERIFICATION.md) is not a substitute for checking the current commit.
