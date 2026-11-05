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

[Files]
Source: "{#ExePath}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\DaniRig"; Filename: "{app}\DaniRig.exe"
Name: "{autodesktop}\DaniRig"; Filename: "{app}\DaniRig.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Crea un'icona sul desktop"; Flags: unchecked

[Run]
Filename: "{app}\DaniRig.exe"; Description: "Avvia DaniRig"; Flags: nowait postinstall skipifsilent
