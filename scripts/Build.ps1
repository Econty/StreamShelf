param(
    [ValidateSet('dev','release')][string]$Preset = 'release',
    [switch]$IncludeTools,
    [string]$MediaInfoDll,
    [switch]$BetaWithoutFirefox
)
$ErrorActionPreference = 'Stop'
if ($BetaWithoutFirefox -and $Preset -ne 'release') {
    throw '-BetaWithoutFirefox is only available for an explicitly incomplete release beta'
}
$root = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
function Assert-NoReparseAncestors([string]$path) {
    $item = Get-Item -LiteralPath $path -Force -ErrorAction Stop
    while ($item) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Linked path is not allowed: $($item.FullName)" }
        $item = $item.Parent
    }
}
Push-Location $root
try {
    foreach ($tool in @('cmake','ninja')) {
        if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "$tool not found on PATH" }
    }
    $configureArgs = @('--preset', $Preset)
    # Always reset the cached packaging mode: a previous beta build must not
    # silently allow the next full release to omit its signed Firefox XPI.
    if ($BetaWithoutFirefox) {
        $configureArgs += '-DSTREAMSHELF_BETA_WITHOUT_FIREFOX=ON'
    } else {
        $configureArgs += '-DSTREAMSHELF_BETA_WITHOUT_FIREFOX=OFF'
    }
    # Always override the cached value. Omitting the switch means this build
    # must not silently reuse a DLL chosen by an earlier CMake configuration.
    if ($MediaInfoDll) {
        if (-not (Test-Path -LiteralPath $MediaInfoDll -PathType Leaf)) { throw 'MediaInfo.dll not found' }
        $configureArgs += "-DSTREAMSHELF_MEDIAINFO_DLL=$MediaInfoDll"
    } else {
        $configureArgs += '-DSTREAMSHELF_MEDIAINFO_DLL='
    }
    cmake @configureArgs
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
    cmake --build --preset $Preset --parallel
    if ($LASTEXITCODE -ne 0) { throw 'C++ build failed' }
    ctest --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
    if ($Preset -eq 'release' -and -not $BetaWithoutFirefox -and -not (Test-Path -LiteralPath (Join-Path $root 'extension/firefox-signed/StreamShelf-Connector-1.0.5.xpi') -PathType Leaf)) {
        throw 'The Firefox 1.0.5 extension needs Mozilla signing before a release package can be staged; see FIREFOX-SIGNING.md'
    }

    Assert-NoReparseAncestors $root
    $releaseRoot = [IO.Path]::GetFullPath((Join-Path $root 'release'))
    if (-not $releaseRoot.Equals((Join-Path $root 'release'), [StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected release path' }
    if (Test-Path -LiteralPath $releaseRoot) { Assert-NoReparseAncestors $releaseRoot }
    $finalStage = Join-Path $releaseRoot 'StreamShelf-Native'
    $serial = [Guid]::NewGuid().ToString('N')
    $newStage = Join-Path $releaseRoot "StreamShelf-Native.staging-$serial"
    $oldStage = Join-Path $releaseRoot "StreamShelf-Native.backup-$serial"
    New-Item -ItemType Directory -Path $releaseRoot -Force | Out-Null
    Assert-NoReparseAncestors $releaseRoot
    function Assert-StagePath([string]$path) {
        $absolute = [IO.Path]::GetFullPath($path)
        if (-not [IO.Path]::GetDirectoryName($absolute).Equals($releaseRoot, [StringComparison]::OrdinalIgnoreCase)) { throw "Stage outside release directory: $absolute" }
        Assert-NoReparseAncestors $releaseRoot
        if (Test-Path -LiteralPath $absolute) {
            $resolved = (Resolve-Path -LiteralPath $absolute -ErrorAction Stop).Path
            if (-not $resolved.Equals($absolute, [StringComparison]::OrdinalIgnoreCase)) { throw "Unexpected resolved stage path: $resolved" }
            Assert-NoReparseAncestors $absolute
        }
    }
    Assert-StagePath $newStage
    Assert-StagePath $oldStage
    Assert-StagePath $finalStage
    $oldMoved = $false
    $published = $false
    try {
        # Install in a new directory so removed files can never survive into a
        # later release. Keep the old release until the new one is complete.
        cmake --install "build/$Preset" --prefix $newStage
        if ($LASTEXITCODE -ne 0) { throw 'Install failed' }
        Assert-StagePath $newStage
        if (-not (Test-Path -LiteralPath (Join-Path $newStage 'StreamShelf.exe') -PathType Leaf)) {
            throw 'Staged application is missing'
        }
        if ($BetaWithoutFirefox) {
            if (-not (Test-Path -LiteralPath (Join-Path $newStage 'BETA-FIREFOX-UNAVAILABLE.md') -PathType Leaf)) {
                throw 'The incomplete beta package must include its Firefox availability notice'
            }
            if (Test-Path -LiteralPath (Join-Path $newStage 'extensions/firefox-signed')) {
                throw 'An incomplete beta package must not include any Firefox signing package'
            }
        }
        if ($IncludeTools) {
            $bundle = Join-Path $root 'artifacts/bundled-tools'
            $checksumFile = Join-Path $bundle 'checksums.json'
            if (-not (Test-Path -LiteralPath $checksumFile -PathType Leaf)) { throw 'Verified tools bundle missing checksums.json' }
            try { $manifest = Get-Content -LiteralPath $checksumFile -Raw -Encoding UTF8 | ConvertFrom-Json }
            catch { throw 'Invalid tools checksums.json' }
            # Accepted contract: an object keyed by executable filename, with
            # either a SHA-256 string or an object containing sha256. A `files`
            # object may wrap the mapping. Unknown formats fail closed.
            if ($manifest.PSObject.Properties['files']) { $manifest = $manifest.files }
            $expectedTools = @('yt-dlp.exe','ffmpeg.exe','ffprobe.exe','deno.exe')
            foreach ($name in $expectedTools) {
                $property = $manifest.PSObject.Properties[$name]
                if (-not $property) { throw "Missing checksum entry for $name" }
                $hash = $property.Value
                if ($hash -and $hash -isnot [string] -and $hash.PSObject.Properties['sha256']) { $hash = $hash.sha256 }
                if ($hash -isnot [string] -or $hash -notmatch '^[0-9a-fA-F]{64}$') { throw "Invalid SHA-256 for $name" }
                $file = Join-Path $bundle $name
                if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing bundled tool: $name" }
                $item = Get-Item -LiteralPath $file -Force
                if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Bundled tool is a link: $name" }
                if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ine $hash) { throw "SHA-256 mismatch: $name" }
            }
            # Walk without following links. Only verified root executables and
            # inert documentation/manifest files are allowed in the package.
            Assert-NoReparseAncestors $bundle
            $pending = New-Object 'System.Collections.Generic.Stack[string]'
            $pending.Push($bundle)
            while ($pending.Count) {
                $directory = $pending.Pop()
                foreach ($entry in (Get-ChildItem -LiteralPath $directory -Force)) {
                    if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Bundle contains a link: $($entry.FullName)" }
                    if ($entry.PSIsContainer) { $pending.Push($entry.FullName); continue }
                    $isExpectedTool = $entry.DirectoryName -ieq $bundle -and $expectedTools -icontains $entry.Name
                    $isDocumentation = @('.txt','.md','.json','.html','.htm','.license') -icontains $entry.Extension -or @('LICENSE','COPYING','README') -icontains $entry.Name
                    if (-not $isExpectedTool -and -not $isDocumentation) { throw "Unexpected file in tools bundle: $($entry.FullName)" }
                }
            }
            Copy-Item -LiteralPath $bundle -Destination (Join-Path $newStage 'bundled-tools') -Recurse -Force
        }
        if (Test-Path -LiteralPath $finalStage) {
            Assert-StagePath $finalStage
            Assert-StagePath $oldStage
            Move-Item -LiteralPath $finalStage -Destination $oldStage
            $oldMoved = $true
        }
        Assert-StagePath $newStage
        Assert-StagePath $finalStage
        Move-Item -LiteralPath $newStage -Destination $finalStage
        $published = $true
    } catch {
        if ($oldMoved -and -not (Test-Path -LiteralPath $finalStage)) {
            Assert-StagePath $oldStage
            Assert-StagePath $finalStage
            Move-Item -LiteralPath $oldStage -Destination $finalStage
            $oldMoved = $false
        }
        throw
    } finally {
        if (Test-Path -LiteralPath $newStage) { Assert-StagePath $newStage; Remove-Item -LiteralPath $newStage -Recurse -Force }
        if ($published -and $oldMoved -and (Test-Path -LiteralPath $oldStage)) { Assert-StagePath $oldStage; Remove-Item -LiteralPath $oldStage -Recurse -Force }
    }
    if ($BetaWithoutFirefox) { Write-Host 'Ready: release/StreamShelf-Native/StreamShelf.exe (beta without signed Firefox connector)' }
    else { Write-Host 'Ready: release/StreamShelf-Native/StreamShelf.exe' }
} finally { Pop-Location }
