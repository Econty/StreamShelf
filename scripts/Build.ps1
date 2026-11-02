param([switch]$IncludeTools)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    cmake -S . -B build
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
    cmake --build build --config Release
    if ($LASTEXITCODE -ne 0) { throw 'C++ build failed' }
    ctest --test-dir build -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Core tests failed' }
    cmake --install build --config Release --prefix release/StreamShelf-Native
    if ($LASTEXITCODE -ne 0) { throw 'Install failed' }
    if ($IncludeTools) {
        if (-not (Test-Path artifacts/bundled-tools/checksums.json)) { throw 'Verified tools bundle missing' }
        Copy-Item artifacts/bundled-tools release/StreamShelf-Native -Recurse -Force
    }
    Write-Host 'Ready: release/StreamShelf-Native/StreamShelf.exe'
} finally { Pop-Location }
