; Inno Setup 6 - PLANET VEL by Trill Tron: Windows 64-bit installer.
; Built by CI:  ISCC /DRepo=<repo root> /DDll=<path to Planet Vel.dll> /DVst3=<path to Planet Vel.vst3 folder> /DApp=<path to Planet Vel.exe> Installer\windows\PlanetVel.iss
#ifndef Repo
  #define Repo "..\.."
#endif
#ifndef Dll
  #define Dll Repo + "\pkg\Planet Vel.dll"
#endif
#ifndef Vst3
  #define Vst3 Repo + "\build\PlanetVel_artefacts\Release\VST3\Planet Vel.vst3"
#endif
#ifndef App
  #define App Repo + "\build\PlanetVel_artefacts\Release\Standalone\Planet Vel.exe"
#endif

[Setup]
AppId={{B7C3A1E4-5E2D-4C57-9D1B-7A5E9F5A0D11}
AppName=Planet Vel
AppVersion=1.0.0
AppPublisher=Trill Tron
DefaultDirName={autopf}\Trill Tron\Planet Vel
DisableProgramGroupPage=yes
DisableDirPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#Repo}\pkg
OutputBaseFilename=PlanetVel-Setup
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=Planet Vel (VST2 / VST3 / App)

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst2"; Description: "VST2 plugin (.dll) - for FL Studio 20.0 and newer"; Types: full custom
Name: "vst3"; Description: "VST3 plugin - for FL Studio 20.7 and newer, other DAWs"; Types: full custom
Name: "app";  Description: "Standalone app (play without a DAW)"; Types: full custom

[Files]
Source: "{#Dll}"; DestDir: "{code:GetVst2Dir}"; Components: vst2; Flags: ignoreversion
Source: "{#Vst3}\*"; DestDir: "{commoncf64}\VST3\Planet Vel.vst3"; Components: vst3; Flags: recursesubdirs ignoreversion
Source: "{#App}"; DestDir: "{app}"; Components: app; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Planet Vel"; Filename: "{app}\Planet Vel.exe"; Components: app

[Code]
var Vst2Page: TInputDirWizardPage;
procedure InitializeWizard;
begin
  Vst2Page := CreateInputDirPage(wpSelectComponents, 'VST2 plugin folder',
    'Where does FL Studio look for VST2 plugins?',
    'The default works if FL scans "Program Files\VSTPlugins". If you use another folder (Options > File settings > Plugin search paths in FL), pick it here.',
    False, '');
  Vst2Page.Add('');
  Vst2Page.Values[0] := ExpandConstant('{commonpf64}\VSTPlugins');
end;
function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PageID = Vst2Page.ID) and (not WizardIsComponentSelected('vst2'));
end;
function GetVst2Dir(Param: String): String;
begin
  Result := Vst2Page.Values[0];
end;
