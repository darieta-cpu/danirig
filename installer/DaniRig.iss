; Installer Windows per DaniRig (Inno Setup 6). Uso: ISCC /DExePath="percorso\DaniRig.exe" /DAppVersion="0.1.0" DaniRig.iss
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef ExePath
  #define ExePath "..\build\DaniRig_artefacts\Release\DaniRig.exe"
#endif

[Setup]
AppId={{8C5D2A64-3B1F-4E7A-9D52-6F0A1C4B7E21}
AppName=DaniRig
AppVersion={#AppVersion}
AppPublisher=DaniArieta
DefaultDirName={autopf}\DaniRig
DefaultGroupName=DaniRig
OutputBaseFilename=DaniRig-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayIcon={app}\DaniRig.exe
WizardStyle=modern

[Types]
Name: "full"; Description: "Completa (app standalone + plugin VST3)"
Name: "custom"; Description: "Personalizzata"; Flags: iscustom

[Components]
Name: "app"; Description: "DaniRig (app standalone)"; Types: full custom; Flags: fixed
#ifdef Vst3Dir
Name: "vst3"; Description: "Plugin VST3 (in C:\Program Files\Common Files\VST3)"; Types: full
#endif

[Files]
Source: "{#ExePath}"; DestDir: "{app}"; Components: app; Flags: ignoreversion
#ifdef Vst3Dir
Source: "{#Vst3Dir}\*"; DestDir: "{commoncf64}\VST3\DaniRig.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
#endif

[Icons]
Name: "{group}\DaniRig"; Filename: "{app}\DaniRig.exe"
Name: "{autodesktop}\DaniRig"; Filename: "{app}\DaniRig.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Crea un'icona sul desktop"; Flags: unchecked

[Run]
Filename: "{app}\DaniRig.exe"; Description: "Avvia DaniRig"; Flags: nowait postinstall skipifsilent
