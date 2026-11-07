; Inno Setup 6 script - PLANET VEL by Trill Tron (Windows 64-bit installer, UNTESTED)
; Build the plugin first (Docs/BUILD.md), then:  iscc Installer\windows\PlanetVel.iss
#define MyAppName "Planet Vel"
#define MyAppVersion "1.0.0"
#define Artefacts "..\..\build\PlanetVel_artefacts\Release"

[Setup]
AppId={{B7C3A1E4-5E2D-4C57-9D1B-7A5E9F5A0D11}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=Trill Tron
DefaultDirName={autopf}\Trill Tron\Planet Vel
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
OutputBaseFilename=PlanetVel-{#MyAppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
WizardStyle=modern

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin  (FL Studio 20+, all modern DAWs)"; Types: full custom; Flags: fixed
Name: "vst2"; Description: "VST2 plugin (only if you built it - requires VST2 SDK license)"; Types: full
Name: "presets"; Description: "620 factory presets (.pvpreset)"; Types: full custom

[Files]
Source: "{#Artefacts}\VST3\Planet Vel.vst3\*"; DestDir: "{commoncf64}\VST3\Planet Vel.vst3"; Components: vst3; Flags: recursesubdirs ignoreversion
Source: "{#Artefacts}\VST\Planet Vel.dll"; DestDir: "{code:GetVst2Dir}"; Components: vst2; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\..\Resources\Presets\Factory\*"; DestDir: "{code:GetLibraryDir}\Presets\Factory"; Components: presets; Flags: recursesubdirs ignoreversion

[Code]
var LibPage: TInputDirWizardPage; Vst2Page: TInputDirWizardPage;
procedure InitializeWizard;
begin
  LibPage := CreateInputDirPage(wpSelectComponents, 'Sample library location',
    'Where should PLANET VEL keep presets and (future) sample content?',
    'Choose a folder with enough free space, then click Next.', False, '');
  LibPage.Add(''); LibPage.Values[0] := ExpandConstant('{commondocs}\Trill Tron\Planet Vel');
  Vst2Page := CreateInputDirPage(LibPage.ID, 'VST2 folder (optional)',
    'Folder that FL Studio scans for VST2 plugins', 'Only used if you selected the VST2 component.', False, '');
  Vst2Page.Add(''); Vst2Page.Values[0] := ExpandConstant('{pf64}\VSTPlugins');
end;
function GetLibraryDir(Param: String): String; begin Result := LibPage.Values[0]; end;
function GetVst2Dir(Param: String): String; begin Result := Vst2Page.Values[0]; end;
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    SaveStringToFile(ExpandConstant('{userappdata}\Trill Tron\Planet Vel\library_path.txt'), LibPage.Values[0], False);
end;
