; Inno Setup script - builds NuSynth-<ver>-Windows-Setup.exe
; Usage: ISCC.exe /DAppVersion=0.1.0 /DVst3Dir="C:\path\to\NuSynth.vst3" /O"dist" NuSynth.iss

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef Vst3Dir
  #error Vst3Dir must be defined (path to the built NuSynth.vst3 bundle folder)
#endif

[Setup]
AppId={{6F1D2C54-8B7E-4D61-9A3B-5E0C4B9F7A21}
AppName=NuSynth
AppVersion={#AppVersion}
AppPublisher=Aron H
AppPublisherURL=https://www.instagram.com/aronh.official/
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
OutputBaseFilename=NuSynth-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=NuSynth VST3

[Messages]
FinishedHeadingLabel=NuSynth is installed
FinishedLabel=Setup has finished installing NuSynth (VST3). Restart your DAW and rescan plugins.%n%nFollow Aron H on Instagram:%nhttps://www.instagram.com/aronh.official/

[Files]
Source: "{#Vst3Dir}\*"; DestDir: "{commoncf64}\VST3\NuSynth.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\NuSynth.vst3"

[Run]
; last page of the installer: tick-box that opens Aron H's Instagram
Filename: "https://www.instagram.com/aronh.official/"; Description: "Follow Aron H on Instagram"; Flags: postinstall shellexec skipifsilent nowait
