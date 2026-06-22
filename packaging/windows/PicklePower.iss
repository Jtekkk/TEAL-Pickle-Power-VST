; ============================================================================
;  Pickle Power 🥒⚡  —  Inno Setup installer script (Windows)
;
;  Build the plugin first:
;     cmake -B build -DCMAKE_BUILD_TYPE=Release
;     cmake --build build --config Release
;  Then compile this script with Inno Setup 6 (ISCC.exe PicklePower.iss).
;  Output lands in packaging\windows\Output\.
; ============================================================================

#define MyAppName       "Pickle Power"
#define MyAppVersion    "0.1.0"
#define MyPublisher     "Pickle Audio"
#define ArtefactsDir    "..\..\build\PicklePower_artefacts\Release"

[Setup]
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyPublisher}
DefaultDirName={autopf}\{#MyPublisher}\{#MyAppName}
DisableProgramGroupPage=yes
OutputBaseFilename=PicklePower-{#MyAppVersion}-Windows
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
Source: "{#ArtefactsDir}\VST3\Pickle Power.vst3\*"; \
    DestDir: "{commoncf64}\VST3\Pickle Power.vst3"; \
    Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion

; Standalone app.
Source: "{#ArtefactsDir}\Standalone\Pickle Power.exe"; \
    DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\Pickle Power.exe"; Components: standalone
