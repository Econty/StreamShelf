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

After a successful release staging, compile the Windows installer with Inno Setup:

```powershell
iscc .\scripts\StreamShelf-Setup.iss
```

The installer is written to `release/StreamShelf-Setup-v2026.11.1-Beta.exe`.

To stage the explicitly limited beta without a signed Firefox connector, use:

```powershell
./scripts/Build.ps1 -Preset release -BetaWithoutFirefox
```

For a direct CMake build, configure with `-DSTREAMSHELF_BETA_WITHOUT_FIREFOX=ON` and install to a new, empty staging directory. This beta does not include a signed Firefox XPI; permanent Firefox installation is unavailable until Mozilla signs Connector 1.0.5. For a normal release, configure with `-DSTREAMSHELF_BETA_WITHOUT_FIREFOX=OFF` and provide the signed XPI.

To disable tests, use a separate build directory, for example `cmake -S . -B build/app -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF`. The C++ runtime is linked statically by default; `-DSTREAMSHELF_STATIC_RUNTIME=OFF` selects the dynamic runtime. A distributed dynamic build requires compatible runtime DLLs or the appropriate Microsoft Visual C++ Redistributable. Check the package on a clean machine.

The interactive extension setup guide test requires an available desktop and is enabled with the CMake option `-DSTREAMSHELF_NATIVE_GUIDE_TESTS=ON`. It is not part of the regular CI. CMake presets and `scripts/Build.ps1` are the supported build path.

## Tools and browser extensions

StreamShelf manages yt-dlp, FFmpeg/FFprobe, and Deno in `%LOCALAPPDATA%\StreamShelfNative\tools`. A package built with `scripts/Build.ps1 -IncludeTools` offers **Settings → Components → Install bundled tools**: the user can explicitly import missing files without network access. StreamShelf checks the bundled SHA-256 manifest and executable versions again and does not replace existing tools. Bundled copies are not used directly or restored automatically. See the [README](README.md) and [third-party notices](THIRD_PARTY.md) (Russian).

A normal release includes the Chromium extension source and a signed Firefox XPI matching the current connector source. The available 1.0.4 XPI is outdated: Mozilla must sign version 1.0.5, as described in [Firefox signing](FIREFOX-SIGNING.md). Normal Release `cmake --install` checks the source, file digests, cryptographic signature and certificate chain; this check requires Python 3 and Windows PowerShell and does not check revocation offline. The explicit beta option above stages without a signed Firefox XPI. Confirm installation in Firefox separately when a signed XPI becomes available. Debug installation remains available without the XPI. Verify installation, tests, and media downloads on Windows for the actual build; [VERIFICATION.md](VERIFICATION.md) is not a substitute for checking the current commit.
