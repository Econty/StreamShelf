#define AppVersion "v2026.11.1 Beta"
#ifndef StagePath
#define StagePath "..\release\StreamShelf-Native"
#endif
[Setup]
AppId={{DE9C8B64-0F1B-4DCE-BD11-D6A48340BD38}
AppName=StreamShelf
AppVersion={#AppVersion}
VersionInfoVersion=2026.11.1.0
AppPublisher=Krechetov Labs
DefaultDirName={localappdata}\Programs\StreamShelf
DefaultGroupName=StreamShelf
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\release
OutputBaseFilename=StreamShelf-Setup-v2026.11.1-Beta
SetupIconFile=..\Assets\StreamShelf.ico
UninstallDisplayIcon={app}\StreamShelf.exe
Compression=lzma2/fast
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
ShowLanguageDialog=yes
LanguageDetectionMethod=uilanguage

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[CustomMessages]
english.DesktopIcon=Create a desktop shortcut
russian.DesktopIcon=Создать ярлык на рабочем столе
english.AdditionalTasks=Additional tasks:
russian.AdditionalTasks=Дополнительные действия:
english.LaunchApp=Launch StreamShelf
russian.LaunchApp=Запустить StreamShelf

[Tasks]
Name: "desktopicon"; Description: "{cm:DesktopIcon}"; GroupDescription: "{cm:AdditionalTasks}"; Flags: unchecked

[Files]
; Build and stage the application with scripts/Build.ps1 before compiling the installer.
Source: "{#StagePath}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[InstallDelete]
; These are program files shipped by known older builds. MediaInfo may be
; optional in the new package, so clear the old DLL before installing files.
Type: files; Name: "{app}\MediaInfo.dll"
Type: files; Name: "{app}\extensions\firefox-signed\StreamShelf-Connector-1.0.2.xpi"
Type: files; Name: "{app}\extensions\firefox-signed\StreamShelf-Connector-1.0.3.xpi"
Type: files; Name: "{app}\extensions\firefox-signed\StreamShelf-Connector-1.0.4.xpi"
Type: files; Name: "{app}\extensions\firefox-signed\StreamShelf-Connector-1.0.5.xpi"

[Icons]
Name: "{group}\StreamShelf"; Filename: "{app}\StreamShelf.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\StreamShelf"; Filename: "{app}\StreamShelf.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\StreamShelf.exe"; Description: "{cm:LaunchApp}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; UninstallRun runs before installed files are removed. The executable removes
; registration only if it still belongs to this installation.
Filename: "{app}\StreamShelf.exe"; Parameters: "--unregister-connector-if-owned"; Flags: runhidden waituntilterminated skipifdoesntexist; RunOnceId: "StreamShelfNativeMessaging"
