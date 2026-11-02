param([string]$AppPath=(Join-Path ([Environment]::GetFolderPath('UserProfile')) 'Downloads\StreamShelf-Native\StreamShelf.exe'))
$ErrorActionPreference='Stop'
if(!(Test-Path -LiteralPath $AppPath)){throw 'StreamShelf.exe not found'}
$wsh=New-Object -ComObject WScript.Shell
$backupRoot=Join-Path (Split-Path $PSScriptRoot -Parent) 'backups\shortcuts'
$index=0
foreach($directory in @([Environment]::GetFolderPath('Desktop'),(Join-Path ([Environment]::GetFolderPath('StartMenu')) 'Programs'))){
 $path=Join-Path $directory 'StreamShelf.lnk'
 if(Test-Path -LiteralPath $path){
  New-Item -ItemType Directory -Force -Path $backupRoot | Out-Null
  Copy-Item -LiteralPath $path -Destination (Join-Path $backupRoot ('StreamShelf-'+$index+'-'+(Get-Date -Format yyyyMMddHHmmss)+'.lnk'))
 }
 $shortcut=$wsh.CreateShortcut($path)
 $shortcut.TargetPath=$AppPath
 $shortcut.WorkingDirectory=Split-Path $AppPath -Parent
 $shortcut.IconLocation=$AppPath+',0'
 $shortcut.Description='StreamShelf — видео, аудио и изображения'
 $shortcut.Save()
 Write-Output $path
 $index++
}
