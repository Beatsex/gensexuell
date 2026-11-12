; Windows installer for gensexuell (Inno Setup 6). After build.cmd, build with:
;   "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\gensexuell.iss
; The setup lands in build\installer. Run installer\make-source.py first: the source zip ships inside.

#define AppVersion "0.4.1"
#define Artefacts "..\build\Seedling_artefacts\Release"

[Setup]
; the AppId identifies the app for upgrades and uninstall: never change it
AppId={{7c1e4a52-9d3b-4f60-8a27-3b6e5d91c0f4}
AppName=gensexuell
AppVersion={#AppVersion}
AppVerName=gensexuell {#AppVersion} beta
AppPublisher=beatsexuell
VersionInfoVersion={#AppVersion}.0
VersionInfoDescription=gensexuell: grow sounds from a seed, by text or from a sample. Fully local synth.
AppCopyright=Copyright (C) 2026 beatsexuell (GNU AGPL v3)
CloseApplications=yes
DefaultDirName={autopf}\beatsexuell\gensexuell
DefaultGroupName=gensexuell
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
; the plugin folders under Common Files need admin rights
PrivilegesRequired=admin
OutputDir=..\build\installer
OutputBaseFilename=gensexuell-beta-{#AppVersion}-win64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
SetupIconFile=icon.ico
UninstallDisplayIcon={app}\gensexuell.exe
UninstallDisplayName=gensexuell {#AppVersion} beta
WizardImageFile=wizard.bmp,wizard@2x.bmp
WizardSmallImageFile=small.bmp,small@2x.bmp
InfoBeforeFile=README.txt

[Languages]
Name: "de"; MessagesFile: "compiler:Languages\German.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
de.ShotTitle=So sieht gensexuell aus
de.ShotDesc=Links beschreiben oder Klänge reinziehen, in der Mitte Äste ziehen, rechts feinjustieren.
en.ShotTitle=This is gensexuell
en.ShotDesc=Describe or drop sounds on the left, drag branches in the middle, fine-tune on the right.
de.Full=Alle Formate und die App
en.Full=All formats and the app
de.Custom=Auswählen
en.Custom=Choose
de.Standalone=Eigenständige App
en.Standalone=Standalone app
de.Manual=Handbuch
en.Manual=User Manual
de.Start=gensexuell jetzt starten
en.Start=Start gensexuell now
de.NoWebView=gensexuell braucht die Microsoft WebView2 Runtime für seine Oberfläche, und sie scheint auf diesem PC zu fehlen.%n%nDownload-Seite jetzt öffnen? Dort „Evergreen Bootstrapper“ wählen und installieren.
en.NoWebView=gensexuell needs the Microsoft WebView2 Runtime for its interface, and it seems to be missing on this PC.%n%nOpen the download page now? Choose "Evergreen Bootstrapper" there and install it.

[Types]
Name: "full"; Description: "{cm:Full}"
Name: "custom"; Description: "{cm:Custom}"; Flags: iscustom

[Components]
Name: "standalone"; Description: "{cm:Standalone}"; Types: full custom
Name: "vst3"; Description: "VST3 (Common Files\VST3)"; Types: full custom
Name: "clap"; Description: "CLAP (Common Files\CLAP)"; Types: full custom
Name: "lv2"; Description: "LV2 (Common Files\LV2)"; Types: full custom

[Dirs]
; read-only folder + hidden desktop.ini: how Explorer shows the VST3 folder's icon
Name: "{commoncf64}\VST3\gensexuell.vst3"; Attribs: readonly; Components: vst3

[Files]
Source: "app.bmp"; Flags: dontcopy
Source: "{#Artefacts}\Standalone\gensexuell.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#Artefacts}\VST3\gensexuell.vst3\*"; DestDir: "{commoncf64}\VST3\gensexuell.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "desktop.ini"
Source: "{#Artefacts}\VST3\gensexuell.vst3\desktop.ini"; DestDir: "{commoncf64}\VST3\gensexuell.vst3"; Components: vst3; Flags: ignoreversion; Attribs: hidden system
Source: "{#Artefacts}\CLAP\gensexuell.clap"; DestDir: "{commoncf64}\CLAP"; Components: clap; Flags: ignoreversion
Source: "{#Artefacts}\LV2\gensexuell.lv2\*"; DestDir: "{commoncf64}\LV2\gensexuell.lv2"; Components: lv2; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*.exp,*.lib"
Source: "README.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\docs\manual.html"; DestDir: "{app}"; DestName: "USER_MANUAL.html"; Flags: ignoreversion
Source: "..\docs\handbuch.html"; DestDir: "{app}"; DestName: "BENUTZERHANDBUCH.html"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\THIRD-PARTY-LICENSES.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\installer\gensexuell-{#AppVersion}-source.zip"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\gensexuell"; Filename: "{app}\gensexuell.exe"; Components: standalone
Name: "{group}\gensexuell {cm:Manual}"; Filename: "{app}\USER_MANUAL.html"; Languages: en
Name: "{group}\gensexuell {cm:Manual}"; Filename: "{app}\BENUTZERHANDBUCH.html"; Languages: de
Name: "{group}\gensexuell Source + License"; Filename: "{app}"
Name: "{group}\{cm:UninstallProgram,gensexuell}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\gensexuell.exe"; Description: "{cm:Start}"; Components: standalone; Flags: nowait postinstall skipifsilent

; patches and renders in Documents\Seedling stay on uninstall on purpose

[Code]
const
  WebViewKey = 'Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}';

function HasVersion(Root: Integer; Key: String): Boolean;
var
  V: String;
begin
  Result := RegQueryStringValue(Root, Key, 'pv', V) and (V <> '') and (V <> '0.0.0.0');
end;

function WebViewInstalled: Boolean;
begin
  Result := HasVersion(HKLM, 'SOFTWARE\WOW6432Node\' + WebViewKey) or HasVersion(HKCU, 'Software\' + WebViewKey);
end;

procedure InitializeWizard;
var
  Page: TWizardPage;
  Shot: TBitmapImage;
  H, W: Integer;
begin
  Page := CreateCustomPage(wpWelcome, CustomMessage('ShotTitle'), CustomMessage('ShotDesc'));
  ExtractTemporaryFile('app.bmp');
  Shot := TBitmapImage.Create(Page);
  Shot.Parent := Page.Surface;
  Shot.Bitmap.LoadFromFile(ExpandConstant('{tmp}\app.bmp'));
  Shot.Stretch := True;
  H := Page.SurfaceHeight;
  W := H * 8 div 5;
  if W > Page.SurfaceWidth then
  begin
    W := Page.SurfaceWidth;
    H := W * 5 div 8;
  end;
  Shot.SetBounds((Page.SurfaceWidth - W) div 2, 0, W, H);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  Code: Integer;
begin
  if (CurStep = ssPostInstall) and (not WizardSilent) and (not WebViewInstalled) then
    if MsgBox(CustomMessage('NoWebView'), mbConfirmation, MB_YESNO) = IDYES then
      ShellExec('open', 'https://developer.microsoft.com/microsoft-edge/webview2/', '', '', SW_SHOWNORMAL, ewNoWait, Code);
end;
