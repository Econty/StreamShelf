$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
function Assert-TestDirectory([string]$path) {
    $directory = Get-Item -LiteralPath $path -Force -ErrorAction Stop
    while ($directory) {
        if ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Linked test path is not allowed: $($directory.FullName)" }
        $directory = $directory.Parent
    }
}
Assert-TestDirectory $repoRoot
$auditRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'build/audit'))
if (-not $auditRoot.StartsWith($repoRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Test path outside repository' }
foreach ($directory in @((Join-Path $repoRoot 'build'), $auditRoot)) {
    if (Test-Path -LiteralPath $directory) { Assert-TestDirectory $directory }
    else { New-Item -ItemType Directory -Path $directory | Out-Null; Assert-TestDirectory $directory }
}
$scratchRoot = [IO.Path]::GetFullPath((Join-Path $auditRoot ('staging-tests-' + $PID)))
if (-not [IO.Path]::GetDirectoryName($scratchRoot).Equals($auditRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected test directory' }
if (Test-Path -LiteralPath $scratchRoot) { throw 'Test directory already exists' }
New-Item -ItemType Directory -Path (Join-Path $scratchRoot 'scripts') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot 'scripts/Build.ps1') -Destination (Join-Path $scratchRoot 'scripts/Build.ps1')
$global:streamShelfTestConfiguredArgs = @()
$global:streamShelfTestInvalidBetaStage = $false
function global:cmake {
    $global:LASTEXITCODE = 0
    if ($args[0] -eq '--preset') { $global:streamShelfTestConfiguredArgs = @($args) }
    if ($args[0] -eq '--install') {
        $index = [array]::IndexOf($args, '--prefix')
        if ($index -lt 0) { throw 'missing staging prefix' }
        $prefix = $args[$index + 1]
        New-Item -ItemType Directory -Path $prefix -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $prefix 'StreamShelf.exe') -Value 'new app'
        if ($global:streamShelfTestConfiguredArgs -contains '-DSTREAMSHELF_BETA_WITHOUT_FIREFOX=ON') {
            Set-Content -LiteralPath (Join-Path $prefix 'BETA-FIREFOX-UNAVAILABLE.md') -Value 'Firefox connector unavailable'
            if ($global:streamShelfTestInvalidBetaStage) {
                New-Item -ItemType Directory -Path (Join-Path $prefix 'extensions/firefox-signed') -Force | Out-Null
                Set-Content -LiteralPath (Join-Path $prefix 'extensions/firefox-signed/old.xpi') -Value 'stale package'
            }
        }
    }
}
function global:ninja { $global:LASTEXITCODE = 0 }
function global:ctest { $global:LASTEXITCODE = 0 }
function check([bool] $condition, [string] $message) { if (-not $condition) { throw $message } }
try {
    $scriptPath = Join-Path $scratchRoot 'scripts/Build.ps1'
    $stage = Join-Path $scratchRoot 'release/StreamShelf-Native'
    $failed = $false
    try { & $scriptPath -Preset release } catch { $failed = $true }
    check $failed 'Release staging accepted a missing signed Firefox connector'
    check ($global:streamShelfTestConfiguredArgs -contains '-DSTREAMSHELF_BETA_WITHOUT_FIREFOX=OFF') 'Full release did not clear the beta packaging mode'
    & $scriptPath -Preset release -BetaWithoutFirefox
    check ($global:streamShelfTestConfiguredArgs -contains '-DSTREAMSHELF_BETA_WITHOUT_FIREFOX=ON') 'Explicit incomplete beta did not enable its packaging mode'
    check (Test-Path -LiteralPath (Join-Path $stage 'BETA-FIREFOX-UNAVAILABLE.md') -PathType Leaf) 'Incomplete beta did not preserve its notice'
    check (-not (Test-Path -LiteralPath (Join-Path $stage 'extensions/firefox-signed'))) 'Incomplete beta included a Firefox signing package'
    $global:streamShelfTestInvalidBetaStage = $true
    $failed = $false
    try { & $scriptPath -Preset release -BetaWithoutFirefox } catch { $failed = $true }
    check $failed 'Incomplete beta accepted a staged Firefox signing package'
    check (Test-Path -LiteralPath (Join-Path $stage 'BETA-FIREFOX-UNAVAILABLE.md') -PathType Leaf) 'Rejected beta destroyed the previous stage'
    $global:streamShelfTestInvalidBetaStage = $false
    $failed = $false
    try { & $scriptPath -Preset release } catch { $failed = $true }
    check ($failed -and ($global:streamShelfTestConfiguredArgs -contains '-DSTREAMSHELF_BETA_WITHOUT_FIREFOX=OFF')) 'A previous beta silently enabled incomplete full release staging'
    $failed = $false
    try { & $scriptPath -Preset dev -BetaWithoutFirefox } catch { $failed = $true }
    check $failed 'The incomplete release beta option was accepted for the dev preset'
    New-Item -ItemType Directory -Path $stage -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $stage 'MediaInfo.dll') -Value 'old DLL'
    Set-Content -LiteralPath (Join-Path $stage 'obsolete.txt') -Value 'old package content'
    & $scriptPath -Preset dev
    check (Test-Path -LiteralPath (Join-Path $stage 'StreamShelf.exe')) 'New stage missing'
    check (-not (Test-Path -LiteralPath (Join-Path $stage 'MediaInfo.dll'))) 'Old DLL leaked into new stage'
    check (-not (Test-Path -LiteralPath (Join-Path $stage 'obsolete.txt'))) 'Old content leaked into new stage'
    check ($global:streamShelfTestConfiguredArgs -contains '-DSTREAMSHELF_MEDIAINFO_DLL=') 'Optional DLL cache was not cleared'
    check ($global:streamShelfTestConfiguredArgs -contains '-DSTREAMSHELF_BETA_WITHOUT_FIREFOX=OFF') 'Dev staging did not clear the beta packaging mode'

    $bundle = Join-Path $scratchRoot 'artifacts/bundled-tools'
    New-Item -ItemType Directory -Path $bundle -Force | Out-Null
    '{}' | Set-Content -LiteralPath (Join-Path $bundle 'checksums.json') -Encoding UTF8
    $failed = $false
    try { & $scriptPath -Preset dev -IncludeTools } catch { $failed = $true }
    check $failed 'Incomplete checksums were accepted'
    check (Test-Path -LiteralPath (Join-Path $stage 'StreamShelf.exe')) 'Failed bundle verification destroyed previous stage'

    $manifest = @{}
    foreach ($name in @('yt-dlp.exe','ffmpeg.exe','ffprobe.exe','deno.exe')) {
        $file = Join-Path $bundle $name
        Set-Content -LiteralPath $file -Value $name
        $manifest[$name] = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
    }
    $manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $bundle 'checksums.json') -Encoding UTF8
    & $scriptPath -Preset dev -IncludeTools
    check (Test-Path -LiteralPath (Join-Path $stage 'bundled-tools/ffmpeg.exe')) 'Verified tools were not staged'
    foreach ($extra in @('hidden.dll','unexpected.cmd','unexpected.ps1')) {
        $extraPath = Join-Path $bundle $extra
        Set-Content -LiteralPath $extraPath -Value 'unverified executable content'
        $failed = $false
        try { & $scriptPath -Preset dev -IncludeTools } catch { $failed = $true }
        check $failed "Unexpected $extra was accepted"
        Remove-Item -LiteralPath $extraPath -Force
    }
    $outside = Join-Path $scratchRoot 'outside-bundle'
    New-Item -ItemType Directory -Path $outside | Out-Null
    $link = Join-Path $bundle 'linked-content'
    New-Item -ItemType Junction -Path $link -Target $outside | Out-Null
    $failed = $false
    try { & $scriptPath -Preset dev -IncludeTools } catch { $failed = $true }
    check $failed 'Bundle junction was accepted'
    Remove-Item -LiteralPath $link -Force
    Set-Content -LiteralPath (Join-Path $bundle 'ffmpeg.exe') -Value 'tampered'
    $failed = $false
    try { & $scriptPath -Preset dev -IncludeTools } catch { $failed = $true }
    check $failed 'Checksum mismatch was accepted'
    check (Test-Path -LiteralPath (Join-Path $stage 'bundled-tools/ffmpeg.exe')) 'Failed update destroyed previous stage'
    Write-Output 'PASS explicit incomplete beta, full release guard, clean staging, cache reset, bundle verification, and rollback'
} finally {
    Remove-Item Function:\cmake,Function:\ninja,Function:\ctest -Force -ErrorAction SilentlyContinue
    Remove-Variable -Name streamShelfTestConfiguredArgs -Scope Global -ErrorAction SilentlyContinue
    Remove-Variable -Name streamShelfTestInvalidBetaStage -Scope Global -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $scratchRoot) {
        $resolved = (Resolve-Path -LiteralPath $scratchRoot -ErrorAction Stop).Path
        if (-not $resolved.Equals($scratchRoot, [StringComparison]::OrdinalIgnoreCase) -or
            -not [IO.Path]::GetDirectoryName($resolved).Equals($auditRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe test cleanup path' }
        Assert-TestDirectory $resolved
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
