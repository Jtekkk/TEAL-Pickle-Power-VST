; ============================================================================
;  Pickle Power 🥒⚡  —  Inno Setup installer script (Windows)
;
;  Build the plugin first:
;     cmake -B build -DCMAKE_BUILD_TYPE=Release
;     cmake --build build --config Release
;  Then compile this script with Inno Setup 6 (ISCC.exe PicklePower.iss).
;  Output lands in packaging\windows\Output\.
; ============================================================================

#define MyAppName       "TEAL-Pickle-Power-VST"
#define MyAppVersion    "1.0.0"
#define MyPublisher     "Tekk Engineering Audio Labs"
#define ArtefactsDir    "..\..\build\PicklePower_artefacts\Release"

[Setup]
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyPublisher}
DefaultDirName={autopf}\{#MyPublisher}\{#MyAppName}
DisableProgramGroupPage=yes
OutputBaseFilename=TEAL-Pickle-Power-VST-{#MyAppVersion}-Windows
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64
WizardStyle=modern

[Components]
Name: "vst3";       Description: "VST3 plugin";        Types: full custom; Flags: checkablealone
Name: "standalone"; Description: "Standalone app";     Types: full custom

[Files]
; VST3 bundle -> the shared system VST3 folder.
Source: "{#ArtefactsDir}\VST3\TEAL-Pickle-Power-VST.vst3\*"; \
    DestDir: "{commoncf64}\VST3\TEAL-Pickle-Power-VST.vst3"; \
    Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion

; Standalone app.
Source: "{#ArtefactsDir}\Standalone\TEAL-Pickle-Power-VST.exe"; \
    DestDir: "{app}"; Components: standalone; Flags: ignoreversion

; Jingle: extracted to a temp folder and played while the installer runs.
Source: "..\..\assets\audio\TEAL_pickle_power.mp3"; Flags: dontcopy

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\TEAL-Pickle-Power-VST.exe"; Components: standalone

[Code]
function mciSendString(lpstrCommand, lpstrReturnString: string;
  uReturnLength: Cardinal; hwndCallback: Integer): Integer;
  external 'mciSendStringW@winmm.dll stdcall';

procedure PlayJingle;
var
  TmpFile: string;
begin
  ExtractTemporaryFile('TEAL_pickle_power.mp3');
  TmpFile := ExpandConstant('{tmp}\TEAL_pickle_power.mp3');
  mciSendString('open "' + TmpFile + '" type mpegvideo alias picklejingle', '', 0, 0);
  mciSendString('play picklejingle', '', 0, 0);
end;

procedure InitializeWizard();
begin
  PlayJingle;   { 🥒⚡ play the Pickle Power jingle when the installer opens }
end;

procedure DeinitializeSetup();
begin
  mciSendString('close picklejingle', '', 0, 0);
end;
