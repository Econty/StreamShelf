#define AppVersion "2.5 Beta"
#ifndef StagePath
#define StagePath "..\release\StreamShelf-Native"
#endif
[Setup]
AppId={{DE9C8B64-0F1B-4DCE-BD11-D6A48340BD38}
AppName=StreamShelf
AppVersion={#AppVersion}
VersionInfoVersion=2.5.0.0
AppPublisher=Krechetov Labs
DefaultDirName={localappdata}\Programs\StreamShelf
DefaultGroupName=StreamShelf
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\release
OutputBaseFilename=StreamShelf-Setup-2.5-Beta
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

[Icons]
Name: "{group}\StreamShelf"; Filename: "{app}\StreamShelf.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\StreamShelf"; Filename: "{app}\StreamShelf.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\StreamShelf.exe"; Description: "{cm:LaunchApp}"; Flags: nowait postinstall skipifsilent
