param([string]$Compiler,[switch]$SkipBaseline,[switch]$IncludeUiTests)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$Compiler){$Compiler=Join-Path (Split-Path $root -Parent) 'tools\llvm-mingw-20260908-ucrt-x86_64\bin\clang++.exe'}
if(!(Test-Path -LiteralPath $Compiler)){throw 'Verified LLVM-MinGW compiler not found'}
Push-Location $root
try {
 New-Item -ItemType Directory -Force -Path 'build-local','release\StreamShelf-Native' | Out-Null
 $common=@('-std=c++20','-O2','-static','-DUNICODE','-D_UNICODE','-DNOMINMAX','-DWIN32_LEAN_AND_MEAN','-D_WIN32_WINNT=0x0A00','-Isrc','-Ivendor')
 $libraries=@('-lopengl32','-lgdi32','-luser32','-lshell32','-lole32','-loleaut32','-luuid','-luiautomationcore','-lversion','-lwinhttp','-lbcrypt','-lcrypt32','-ladvapi32','-lcomdlg32','-lshlwapi','-lwindowscodecs')
 & $Compiler @common src/core.cpp tests/core_tests.cpp -o build-local/core_tests.exe
 if($LASTEXITCODE){throw 'Core test compilation failed'}
 & .\build-local\core_tests.exe
 if($LASTEXITCODE){throw 'Core tests failed'}
 if(!$SkipBaseline){
  & $Compiler @common '-DSTREAMSHELF_BASELINE' src/core.cpp src/platform.cpp baseline/connector.cpp tests/connector_windows_tests.cpp @libraries -o build-local/connector_baseline.exe
  if($LASTEXITCODE){throw 'Baseline compilation failed'}
  & .\build-local\connector_baseline.exe
  if($LASTEXITCODE -eq 0){throw 'Original transport did not reproduce the regression'}
  if($LASTEXITCODE -ne 1){throw 'Baseline test did not exit cleanly'}
 }
 & $Compiler @common tests/assembly_batch_tests.cpp -o build-local/assembly_batch_tests.exe
 if($LASTEXITCODE){throw 'Assembly batch test compilation failed'}
 & .\build-local\assembly_batch_tests.exe
 if($LASTEXITCODE){throw 'Assembly batch tests failed'}
 & $Compiler @common src/core.cpp src/platform.cpp tests/container_plan_tests.cpp @libraries -o build-local/container_plan_tests.exe
 if($LASTEXITCODE){throw 'Container plan test compilation failed'}
 & .\build-local\container_plan_tests.exe
 if($LASTEXITCODE){throw 'Container plan tests failed'}
 & $Compiler @common src/core.cpp src/platform.cpp src/connector.cpp tests/connector_windows_tests.cpp @libraries -o build-local/connector_tests.exe
 if($LASTEXITCODE){throw 'Connector test compilation failed'}
 & .\build-local\connector_tests.exe
 if($LASTEXITCODE){throw 'Connector tests failed'}
 & $Compiler @common src/core.cpp src/platform.cpp src/preview.cpp tests/preview_tests.cpp @libraries -o build-local/preview_tests.exe
 if($LASTEXITCODE){throw 'Preview test compilation failed'}
 & .\build-local\preview_tests.exe
 if($LASTEXITCODE){throw 'Preview tests failed'}
 & $Compiler @common src/core.cpp src/platform.cpp src/render.cpp src/extension_installer.cpp tests/extension_installer_tests.cpp @libraries -o build-local/extension_installer_tests.exe
 if($LASTEXITCODE){throw 'Extension installer test compilation failed'}
 & .\build-local\extension_installer_tests.exe
 if($LASTEXITCODE){throw 'Extension installer tests failed'}
 if($IncludeUiTests){
  & $Compiler @common '-DSTREAMSHELF_INSTALLER_TESTING' src/core.cpp src/platform.cpp src/render.cpp src/extension_installer.cpp tests/extension_installer_native_tests.cpp @libraries -o build-local/extension_installer_native_tests.exe
  if($LASTEXITCODE){throw 'Native installer test compilation failed'}
  Copy-Item -LiteralPath Assets -Destination build-local -Recurse -Force
  & .\build-local\extension_installer_native_tests.exe
  if($LASTEXITCODE){throw 'Native installer tests failed'}
 }
 $node=Get-Command node -ErrorAction SilentlyContinue
 if($node){
  & $node.Source tests/extension-tests.mjs
  if($LASTEXITCODE){throw 'Extension tests failed'}
  & $node.Source tests/media-discovery-tests.mjs
  if($LASTEXITCODE){throw 'Media discovery tests failed'}
 }else{Write-Warning 'Node.js not found; JavaScript extension tests were not run'}
 $windres=Join-Path (Split-Path $Compiler -Parent) 'x86_64-w64-mingw32-windres.exe'
 & $windres src/app.rc -O coff -o build-local/app.res
 if($LASTEXITCODE){throw 'Icon resource compilation failed'}
 # Compile application files separately, retaining objects between builds.
 # Header or compiler changes invalidate the cache conservatively.
 $objectDir = 'build-local\app-objects'
 New-Item -ItemType Directory -Force -Path $objectDir | Out-Null
 $signature = "$((Resolve-Path -LiteralPath $Compiler).Path)|$((Get-Item -LiteralPath $Compiler).LastWriteTimeUtc.Ticks)|$($common -join '|')|-gline-tables-only"
 $signaturePath = Join-Path $objectDir 'build-signature.txt'
 $rebuildAll = !(Test-Path -LiteralPath $signaturePath) -or ((Get-Content -LiteralPath $signaturePath -Raw) -ne $signature)
 $headerTime = (Get-ChildItem -LiteralPath src,vendor -File -Recurse | Where-Object { $_.Extension -in '.h','.hpp' } | Measure-Object -Property LastWriteTimeUtc -Maximum).Maximum
 $objects = @()
 foreach ($name in @('main','core','platform','preview','connector','extension_installer','render','app','assembly','assembly_download','local_media','constructor_ui','ui')) {
  $source = "src/$name.cpp"
  $object = Join-Path $objectDir "$name.o"
  if ($rebuildAll -or !(Test-Path -LiteralPath $object) -or (Get-Item -LiteralPath $source).LastWriteTimeUtc -gt (Get-Item -LiteralPath $object).LastWriteTimeUtc -or $headerTime -gt (Get-Item -LiteralPath $object).LastWriteTimeUtc) {
   & $Compiler @common '-gline-tables-only' -c $source -o $object
   if($LASTEXITCODE){throw "Application compilation failed: $source"}
  }
  $objects += $object
 }
 [IO.File]::WriteAllText((Join-Path (Get-Location).Path $signaturePath), $signature)
 & $Compiler @common '-municode' '-mwindows' @objects build-local/app.res @libraries -o release/StreamShelf-Native/StreamShelf.exe
 if($LASTEXITCODE){throw 'Application compilation failed'}
 foreach($directory in @('Assets','licenses','extension')){
  $target=if($directory -eq 'extension'){'extensions'}else{$directory}
  $destination=Join-Path 'release/StreamShelf-Native' $target
  New-Item -ItemType Directory -Force -Path $destination | Out-Null
  Get-ChildItem -LiteralPath $directory | Where-Object { $directory -ne 'extension' -or $_.Name -ne 'firefox' } | Copy-Item -Destination $destination -Recurse -Force
 }
 Copy-Item -LiteralPath README.md,THIRD_PARTY.md,VERIFICATION.md,BROWSER-INSTALLER.md,BROWSER-INSTALLER-VERIFICATION.md -Destination release/StreamShelf-Native -Force
 Write-Output 'Local build and regression tests complete'
}finally{Pop-Location}
