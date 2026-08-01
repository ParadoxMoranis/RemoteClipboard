#ifndef MyAppVersion
  #error MyAppVersion must be defined by the build script
#endif

#ifndef SourceRoot
  #error SourceRoot must be defined by the build script
#endif

#ifndef OutputRoot
  #error OutputRoot must be defined by the build script
#endif

#define MyAppName "Remote Clipboard"
#define MyAppExeName "remote-clipboard-windows.exe"

[Setup]
AppId={{5A460A7C-5FB5-46BB-BD55-796E070832DA}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=Remote Clipboard contributors
AppPublisherURL=https://github.com/ParadoxMoranis/RemoteClipboard
AppSupportURL=https://github.com/ParadoxMoranis/RemoteClipboard/issues
AppUpdatesURL=https://github.com/ParadoxMoranis/RemoteClipboard/releases
DefaultDirName={localappdata}\Programs\Remote Clipboard
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir={#OutputRoot}
OutputBaseFilename=RemoteClipboard-client-windows-x64-setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
UninstallDisplayIcon={app}\bin\{#MyAppExeName}
VersionInfoVersion={#MyAppVersion}.0
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\bin\{#MyAppExeName}"; WorkingDir: "{app}\bin"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\bin\{#MyAppExeName}"; WorkingDir: "{app}\bin"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
