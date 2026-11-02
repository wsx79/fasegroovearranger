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
ChangesAssociations=yes

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"; InfoAfterFile: "after-install-en.txt"
Name: "it"; MessagesFile: "compiler:Languages\Italian.isl"; InfoAfterFile: "after-install-it.txt"

[CustomMessages]
en.UserGuide=User guide
it.UserGuide=Guida di utilizzo
en.AssociateFga=Associate .fga projects with Fase Groove Arranger
it.AssociateFga=Associa i progetti .fga a Fase Groove Arranger

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

Name: "associatefga"; Description: "{cm:AssociateFga}"; GroupDescription: "{cm:AdditionalIcons}"

[Registry]
; Keep an existing extension default; Windows also preserves an explicit UserChoice.
Root: HKLM64; Subkey: "Software\Classes\.fga"; ValueType: string; ValueName: ""; ValueData: "FaseGrooveArranger.Project"; Flags: createvalueifdoesntexist uninsdeletekeyifempty; Tasks: associatefga
Root: HKLM64; Subkey: "Software\Classes\.fga\OpenWithProgids"; ValueType: string; ValueName: "FaseGrooveArranger.Project"; ValueData: ""; Flags: uninsdeletevalue uninsdeletekeyifempty; Tasks: associatefga
Root: HKLM64; Subkey: "Software\Classes\FaseGrooveArranger.Project"; ValueType: string; ValueName: ""; ValueData: "Fase Groove Arranger project"; Flags: uninsdeletekey; Tasks: associatefga
Root: HKLM64; Subkey: "Software\Classes\FaseGrooveArranger.Project\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\Resources\arranger-project.ico"",0"; Tasks: associatefga
Root: HKLM64; Subkey: "Software\Classes\FaseGrooveArranger.Project\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Fase Groove Arranger.exe"" ""%1"""; Tasks: associatefga

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

// Do not remove a different application's association chosen after installation.
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  CurrentDefault: String;
begin
  if CurUninstallStep = usUninstall then
    if RegQueryStringValue(HKLM64, 'Software\Classes\.fga', '', CurrentDefault) then
      if CurrentDefault = 'FaseGrooveArranger.Project' then
        RegDeleteValue(HKLM64, 'Software\Classes\.fga', '');
end;
