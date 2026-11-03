#define AppVersion "2.2 Beta"
[Setup]
AppId={{DE9C8B64-0F1B-4DCE-BD11-D6A48340BD38}
AppName=StreamShelf
AppVersion={#AppVersion}
VersionInfoVersion=2.2.0.0
AppPublisher=Krechetov Labs
DefaultDirName={localappdata}\Programs\StreamShelf
DefaultGroupName=StreamShelf
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\release
OutputBaseFilename=StreamShelf-Setup-2.2-Beta
SetupIconFile=..\Assets\StreamShelf.ico
UninstallDisplayIcon={app}\StreamShelf.exe
Compression=lzma2/fast
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes

[Languages]
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "Создать ярлык на рабочем столе"; GroupDescription: "Дополнительные действия:"; Flags: unchecked

[Files]
Source: "..\release\StreamShelf-2.2-Beta\StreamShelf.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Assets\*"; DestDir: "{app}\Assets"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\extension\chromium-development\*"; DestDir: "{app}\extensions\chromium-development"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\extension\firefox-signed\StreamShelf-Connector-1.0.2.xpi"; DestDir: "{app}\extensions\firefox-signed"; Flags: ignoreversion
Source: "..\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\release\StreamShelf-2.2-Beta\bundled-tools\*"; DestDir: "{app}\bundled-tools"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD_PARTY.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\VERIFICATION.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\BROWSER-INSTALLER.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\StreamShelf"; Filename: "{app}\StreamShelf.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\StreamShelf"; Filename: "{app}\StreamShelf.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\StreamShelf.exe"; Description: "Запустить StreamShelf"; Flags: nowait postinstall skipifsilent
