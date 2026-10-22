; Fase Groove Arranger - Windows installer (Inno Setup 6)
; Copyright (C) 2026 Fabio 'Pitch' Serra - SPDX-License-Identifier: AGPL-3.0-or-later
; Adapted from the Fase Groove Piano/Drums/Guitar family installer, standalone only.
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef PackageDir
  #error PackageDir must name the tested native distribution folder
#endif
#ifndef RepoDir
  #define RepoDir "..\.."
#endif
#define AppName "Fase Groove Arranger"
#define Publisher "FS Pitch - Fabio 'Pitch' Serra"
#define AppUrl "https://github.com/wsx79/fasegroovearranger"

[Setup]
AppId={{A72E9073-9AD8-45DF-A2C1-413BD1F306A8}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#Publisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}/issues
AppUpdatesURL={#AppUrl}/releases/latest
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
LicenseFile={#RepoDir}\LICENSE
OutputDir={#RepoDir}\installer-output
OutputBaseFilename=FaseGrooveArranger-Windows-Setup
SetupIconFile=FaseGrooveArranger.ico
UninstallDisplayIcon={app}\FaseGrooveArranger.ico
WizardStyle=modern
WizardImageFile=wizard-large.bmp,wizard-large-2x.bmp
WizardSmallImageFile=wizard-small.bmp,wizard-small-2x.bmp
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
ShowLanguageDialog=auto
VersionInfoVersion={#AppVersion}
VersionInfoCompany={#Publisher}
VersionInfoDescription={#AppName} installer
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"; InfoAfterFile: "after-install-en.txt"
Name: "it"; MessagesFile: "compiler:Languages\Italian.isl"; InfoAfterFile: "after-install-it.txt"

[CustomMessages]
en.UserGuide=User guide
it.UserGuide=Guida di utilizzo

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#PackageDir}\Standalone\Fase Groove Arranger.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageDir}\*"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PackageDir}\docs\*"; DestDir: "{app}\docs"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PackageDir}\Resources\*"; DestDir: "{app}\Resources"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#PackageDir}\ThirdParty\*"; DestDir: "{app}\ThirdParty"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "FaseGrooveArranger.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "FaseGrooveArranger.install.ini"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\Fase Groove Arranger.exe"
Name: "{group}\{cm:UserGuide}"; Filename: "{#AppUrl}/blob/main/docs/GUIDE.md"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\Fase Groove Arranger.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Fase Groove Arranger.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
Filename: "{app}\Fase Groove Arranger.exe"; Flags: nowait runasoriginaluser; Check: RelaunchRequested

[Code]
function RelaunchRequested: Boolean;
begin
  Result := ExpandConstant('{param:RELAUNCH|0}') = '1';
end;
