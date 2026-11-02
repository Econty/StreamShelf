param(
 [ValidateSet('chrome','yandex')][string]$Browser='chrome',
 [string]$Executable,
 [string]$Report,
 [switch]$InstallationFlow
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$Executable){$Executable=Join-Path $root 'release\StreamShelf-Native\StreamShelf.exe'}
if(!$Report){$Report=Join-Path $root ('build-local\e2e-'+$Browser+'-v3.json')}
if(Get-Process StreamShelf -ErrorAction SilentlyContinue){throw 'Close the running StreamShelf instance before this desktop test'}
New-Item -ItemType Directory -Path (Split-Path $Report -Parent) -Force | Out-Null
Write-Output 'Interactive test: click the StreamShelf main window once if Windows denies foreground focus.'
$testArgs=@('--test-browser-guide',$Browser,'--test-report',('"'+$Report+'"'))
if($InstallationFlow){$testArgs+='--test-installation-flow'}
$process=Start-Process -FilePath $Executable -ArgumentList $testArgs -WindowStyle Normal -PassThru
$deadline=if($InstallationFlow){380000}else{45000}
if(!$process.WaitForExit($deadline)){throw 'StreamShelf desktop test did not finish before its deadline'}
$process.Refresh()
if(!(Test-Path -LiteralPath $Report)){throw 'Application did not produce a desktop test report'}
$state=Get-Content -LiteralPath $Report -Raw | ConvertFrom-Json
if($process.ExitCode -ne 0 -or !$state.passed){
 Write-Output ($state | ConvertTo-Json -Depth 8)
 throw ('Browser guide test failed for '+$Browser)
}
$expected=if($Browser -eq 'chrome'){'chrome://extensions'}else{'browser://extensions'}
if(!$state.internal_url.StartsWith($expected)){throw 'The actual observed URL does not match the selected browser'}
Write-Output ($state | ConvertTo-Json -Depth 8)
Write-Output ('PASS desktop application -> '+$Browser+' window -> verified extension page -> visible guide and outline')
