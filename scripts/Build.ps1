param(
    [ValidateSet('dev','release')][string]$Preset = 'release',
    [switch]$IncludeTools,
    [string]$MediaInfoDll
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    foreach ($tool in @('cmake','ninja')) {
        if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "$tool not found on PATH" }
    }
    $configureArgs = @('--preset', $Preset)
    if ($MediaInfoDll) {
        if (-not (Test-Path -LiteralPath $MediaInfoDll -PathType Leaf)) { throw 'MediaInfo.dll not found' }
        $configureArgs += "-DSTREAMSHELF_MEDIAINFO_DLL=$MediaInfoDll"
    }
    cmake @configureArgs
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
    cmake --build --preset $Preset --parallel
    if ($LASTEXITCODE -ne 0) { throw 'C++ build failed' }
    ctest --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
    cmake --install "build/$Preset" --prefix release/StreamShelf-Native
    if ($LASTEXITCODE -ne 0) { throw 'Install failed' }
    if ($IncludeTools) {
        if (-not (Test-Path artifacts/bundled-tools/checksums.json)) { throw 'Verified tools bundle missing' }
        Copy-Item artifacts/bundled-tools release/StreamShelf-Native -Recurse -Force
    }
    Write-Host 'Ready: release/StreamShelf-Native/StreamShelf.exe'
} finally { Pop-Location }
