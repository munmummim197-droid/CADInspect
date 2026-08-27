#define AppName "CADInspect"
#define AppVersion "0.1.0"
#define AppPublisher "AIHung"
#define AppExecutable "CADInspect.exe"
#define SourcePackage "..\dist"

[Setup]
AppId={{0609F401-2514-470D-880C-3F8A32777590}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
AllowNoIcons=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
OutputDir=..\release-installer
OutputBaseFilename=CADInspect-Setup-x64-{#AppVersion}
SetupIconFile=..\resources\icons\StepCompare.ico
UninstallDisplayIcon={app}\{#AppExecutable}
UninstallDisplayName={#AppName}
Compression=lzma2/ultra64
SolidCompression=yes
LZMANumBlockThreads=4
WizardStyle=modern
WizardSizePercent=110
CloseApplications=yes
RestartApplications=no
SetupLogging=yes
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} Setup
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}.0
VersionInfoVersion={#AppVersion}.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Tạo biểu tượng CADInspect ngoài màn hình"; GroupDescription: "Biểu tượng bổ sung:"; Flags: unchecked

[Files]
; Release binaries are intentionally not committed. Build dist locally first.
; Authenticode signing is a publication gate documented in docs/releasing.md.
Source: "{#SourcePackage}\StepCompare.exe"; DestDir: "{app}"; DestName: "{#AppExecutable}"; Flags: ignoreversion
; Runtime dependencies and license notices only. CLI, symbols and tests are excluded.
Source: "{#SourcePackage}\*"; DestDir: "{app}"; Excludes: "StepCompare.exe,stepcompare-cli.exe"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExecutable}"; WorkingDir: "{app}"; IconFilename: "{app}\{#AppExecutable}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExecutable}"; WorkingDir: "{app}"; IconFilename: "{app}\{#AppExecutable}"; Tasks: desktopicon

[Registry]
Root: HKLM64; Subkey: "Software\Microsoft\Windows\CurrentVersion\App Paths\{#AppExecutable}"; ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExecutable}"; Flags: uninsdeletekey
Root: HKLM64; Subkey: "Software\Microsoft\Windows\CurrentVersion\App Paths\{#AppExecutable}"; ValueType: string; ValueName: "Path"; ValueData: "{app}"; Flags: uninsdeletekey
