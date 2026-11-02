#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
compiler=${CXX:-/workspace/tools/mingw/usr/bin/x86_64-w64-mingw32-g++-posix}
mkdir -p build/windows release/StreamShelf-Native
flags=(-std=c++20 -O2 -DNDEBUG -DUNICODE -D_UNICODE -DNOMINMAX -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0A00 -Isrc -Ivendor -pthread)
for name in core platform connector preview extension_installer render app assembly assembly_download local_media constructor_ui ui main; do
 if [[ ! -f "build/windows/$name.o" ]] || [[ "src/$name.cpp" -nt "build/windows/$name.o" ]] || find src -name '*.hpp' -newer "build/windows/$name.o" | read -r; then
  "$compiler" "${flags[@]}" -c "src/$name.cpp" -o "build/windows/$name.o"
 fi
done
"$compiler" build/windows/*.o -o release/StreamShelf-Native/StreamShelf.exe -municode -mwindows -static -static-libgcc -static-libstdc++ -lopengl32 -lgdi32 -luser32 -lshell32 -lole32 -loleaut32 -luuid -luiautomationcore -lversion -lwinhttp -lbcrypt -lcrypt32 -ladvapi32 -lcomdlg32 -lshlwapi -lwindowscodecs
