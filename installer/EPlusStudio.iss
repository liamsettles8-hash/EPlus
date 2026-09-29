[Setup]
AppId={{F5D36D18-7B9E-4E76-9F35-EPLUSSTUDIO01}}
AppName=E#+ Studio
AppVersion=2.0.0
AppPublisher=E#+ Project
DefaultDirName={autopf}\EPlus Studio
DefaultGroupName=E#+ Studio
OutputDir=..\installer-output
OutputBaseFilename=EPlusStudio-Setup
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
UninstallDisplayIcon={app}\EPlusStudio.exe
[Files]
Source: "..\build\EPlusStudio.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\eplus-engine.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\EPlusGameRuntime.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\examples\*.eplus"; DestDir: "{app}\examples"; Flags: ignoreversion
[Icons]
Name: "{group}\E#+ Studio"; Filename: "{app}\EPlusStudio.exe"
Name: "{autodesktop}\E#+ Studio"; Filename: "{app}\EPlusStudio.exe"
[Run]
Filename: "{app}\EPlusStudio.exe"; Description: "Launch E#+ Studio"; Flags: nowait postinstall skipifsilent
