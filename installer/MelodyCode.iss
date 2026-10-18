; Windows installer: Standalone in Program Files, VST3 in Common Files\VST3.
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef DistDir
  #define DistDir "..\dist"
#endif

[Setup]
AppId={{8C2F6E1B-4D7A-4E2B-9F1C-6D3A5B7E9A41}
AppName=Melody Code
AppVersion={#AppVersion}
AppPublisher=Audio Makers
DefaultDirName={autopf}\Audio Makers\Melody Code
DefaultGroupName=Melody Code
DisableProgramGroupPage=yes
DisableDirPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputBaseFilename=Melody Code {#AppVersion} Instalador
OutputDir=..\dist
SetupIconFile=..\build\MelodyCode_artefacts\JuceLibraryCode\icon.ico
UninstallDisplayIcon={app}\Melody Code.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "pt"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Tasks]
Name: "desktopicon"; Description: "Criar atalho na área de trabalho"

[Files]
Source: "{#DistDir}\Melody Code.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\Melody Code.vst3\*"; DestDir: "{commoncf64}\VST3\Melody Code.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\Assets\Samples\LICENSES.txt"; DestDir: "{app}"; DestName: "Sample credits.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\Melody Code"; Filename: "{app}\Melody Code.exe"
Name: "{autodesktop}\Melody Code"; Filename: "{app}\Melody Code.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Melody Code.exe"; Description: "Abrir o Melody Code"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Melody Code.vst3"
