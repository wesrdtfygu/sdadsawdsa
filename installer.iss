#define MyAppName "StrafeHelper Remake"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "StrafeHelper Remake"
#define MyAppExeName "StrafeHelper.exe"

[Setup]
AppId={{A7E6C4E4-6B7A-4D8E-A8A9-7F9D2D4F21B0}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\StrafeHelper Remake
DefaultGroupName={#MyAppName}
OutputDir=dist\installer
OutputBaseFilename=StrafeHelper-Setup-v{#MyAppVersion}
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}

[Files]
Source: "dist\StrafeHelper\StrafeHelper.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "dist\StrafeHelper\README.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autodesktop}\StrafeHelper Remake"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\StrafeHelper Remake"; Filename: "{app}\{#MyAppExeName}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch StrafeHelper Remake"; Flags: nowait postinstall skipifsilent
