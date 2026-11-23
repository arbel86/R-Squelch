; R-Squelch installer (Inno Setup 6)
; Built by make_installer.bat from the files in the Output folder (run build.bat first).
; Installs the VST3 into C:\Program Files\Common Files\VST3 and the standalone app into Program Files\RBell.

#define AppName "R-Squelch"
#define AppVer "0.1.0"
#define AppPublisher "RBell"

[Setup]
AppId={{E9D21CA8-FC0E-4594-88C1-CA8E8A30ED48}
AppName={#AppName}
AppVersion={#AppVer}
AppVerName={#AppName} {#AppVer}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\RBell\R-Squelch
DefaultGroupName=RBell
DisableProgramGroupPage=yes
OutputDir=..\Installer
OutputBaseFilename=R-Squelch-Setup-{#AppVer}
SetupIconFile=..\assets\rsquelch.ico
UninstallDisplayIcon={app}\R-Squelch.exe
UninstallDisplayName={#AppName} by {#AppPublisher}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
CloseApplications=yes

[Types]
Name: "full"; Description: "VST3 plugin + standalone app"
Name: "vst3only"; Description: "VST3 plugin only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "R-Squelch VST3 plugin (for your DAW)"; Types: full vst3only custom; Flags: fixed
Name: "standalone"; Description: "R-Squelch standalone app (the pyramid window)"; Types: full

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut for the standalone app"; Components: standalone

[Files]
Source: "..\Output\R-Squelch.vst3\*"; DestDir: "{commoncf64}\VST3\R-Squelch.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\Output\R-Squelch.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{group}\R-Squelch"; Filename: "{app}\R-Squelch.exe"; Components: standalone
Name: "{group}\Uninstall R-Squelch"; Filename: "{uninstallexe}"
Name: "{autodesktop}\R-Squelch"; Filename: "{app}\R-Squelch.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\R-Squelch.exe"; Description: "Open R-Squelch now"; Components: standalone; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\R-Squelch.vst3"

[Messages]
FinishedLabel=R-Squelch is installed.%n%nIn your DAW, rescan plugins: R-Squelch is under RBell (VST3).%nYour own presets stay in Documents\RBell\R-Squelch\Presets.
