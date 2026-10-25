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
Source: "{#DistDir}\Melody Code.vst3\*"; DestDir: "{code:Vst3Dir}\Melody Code.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\Assets\Samples\LICENSES.txt"; DestDir: "{app}"; DestName: "Sample credits.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\Melody Code"; Filename: "{app}\Melody Code.exe"
Name: "{autodesktop}\Melody Code"; Filename: "{app}\Melody Code.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Melody Code.exe"; Description: "Abrir o Melody Code"; Flags: nowait postinstall skipifsilent

[Registry]
Root: HKLM; Subkey: "Software\Audio Makers\Melody Code"; ValueType: string; ValueName: "VST3Path"; ValueData: "{code:Vst3Dir}"; Flags: uninsdeletekey

[UninstallDelete]
Type: filesandordirs; Name: "{reg:HKLM\Software\Audio Makers\Melody Code,VST3Path|{commoncf64}\VST3}\Melody Code.vst3"

[Code]
var
  Vst3Page: TInputDirWizardPage;

procedure InitializeWizard;
begin
  Vst3Page := CreateInputDirPage(wpSelectTasks, 'Pasta do VST3',
    'Onde o plugin VST3 vai ser instalado?',
    'O Melody Code.vst3 vai para a pasta abaixo. A padr' + #227 + 'o ' + #233 + ' a pasta VST3 que todas as DAWs procuram. Clique em Procurar para escolher outra.',
    False, '');
  Vst3Page.Add('');
  Vst3Page.Values[0] := GetPreviousData('VST3Path', ExpandConstant('{commoncf64}\VST3'));
end;

procedure RegisterPreviousData(PreviousDataKey: Integer);
begin
  SetPreviousData(PreviousDataKey, 'VST3Path', Vst3Page.Values[0]);
end;

function Vst3Dir(Param: String): String;
begin
  Result := Vst3Page.Values[0];
end;
