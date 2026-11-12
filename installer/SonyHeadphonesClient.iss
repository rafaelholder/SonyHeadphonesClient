; Inno Setup script for the rafaelholder fork of SonyHeadphonesClient (Windows x64, per-user install).
; Build with tooling\build-installer.ps1, which passes AppVersion, GitHash and SourceExe.
;
; - No admin rights: installs to %LOCALAPPDATA%\Programs\SonyHeadphonesClient ({autopf} with lowest privileges).
; - Optional tasks: desktop shortcut, "Iniciar com o Windows" (HKCU Run value, same one the app's tray toggles).
; - Uninstaller (Settings > Apps): closes the app, removes files, shortcuts and the Run value,
;   and asks whether to delete %APPDATA%\rafaelholder\SonyHeadphonesClient (saved headset + EQ presets).

#ifndef AppVersion
  #define AppVersion "2.0.0"
#endif
#ifndef GitHash
  #define GitHash "dev"
#endif
#ifndef SourceExe
  #define SourceExe "..\build\client\Release\SonyHeadphonesClient.exe"
#endif
#define AppExe "SonyHeadphonesClient.exe"

[Setup]
; Never change AppId: it is how upgrades and the uninstaller find this app.
AppId={{833ABAFA-C890-4FFF-873C-354CF6117990}
AppName=SonyHeadphonesClient
AppVersion={#AppVersion}
AppVerName=SonyHeadphonesClient {#AppVersion} ({#GitHash})
AppPublisher=rafaelholder
AppPublisherURL=https://github.com/rafaelholder/SonyHeadphonesClient
AppSupportURL=https://github.com/rafaelholder/SonyHeadphonesClient/issues
AppComments=Controle do Sony WH-1000XM5 no Windows. Fork de mos9527/SonyHeadphonesClient.
VersionInfoVersion={#AppVersion}
VersionInfoDescription=Instalador do SonyHeadphonesClient (fork rafaelholder)
DefaultDirName={autopf}\SonyHeadphonesClient
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\build\installer
OutputBaseFilename=SonyHeadphonesClient-Setup-{#AppVersion}-x64
SetupIconFile=..\client\Platform\Windows\app.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName=SonyHeadphonesClient (WH-1000XM5)
WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes
; The app hides to the tray on WM_CLOSE, so Restart Manager can't close it: [Code] stops it instead.
CloseApplications=no

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[CustomMessages]
brazilianportuguese.AutostartTask=Iniciar com o Windows (abre direto na bandeja)
brazilianportuguese.LaunchApp=Abrir o SonyHeadphonesClient agora
brazilianportuguese.DeleteSettings=Apagar também as configurações do SonyHeadphonesClient?%n%nIsso remove o fone salvo e os seus presets de EQ:%n%1%n%nEscolha Não para mantê-los numa futura reinstalação.

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
; Initial state comes from the registry (see CurPageChanged), so it matches what the tray menu shows.
Name: "autostart"; Description: "{cm:AutostartTask}"; Flags: unchecked

[Files]
Source: "{#SourceExe}"; DestDir: "{app}"; Flags: ignoreversion
; MIT requires shipping the copyright and permission notice with the software.
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\SonyHeadphonesClient"; Filename: "{app}\{#AppExe}"; Comment: "Controle do fone Sony"
Name: "{autodesktop}\SonyHeadphonesClient"; Filename: "{app}\{#AppExe}"; Comment: "Controle do fone Sony"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchApp}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: dirifempty; Name: "{app}"

[Code]
const
  RunKey = 'Software\Microsoft\Windows\CurrentVersion\Run';
  RunValue = 'SonyHeadphonesClient';

var
  AutostartInitialized: Boolean;

// The client keeps running in the tray and hides on WM_CLOSE, so end it before touching its files.
// Settings are written as they change, so nothing is lost.
procedure StopClient();
var
  ResultCode: Integer;
begin
  Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM {#AppExe}', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Sleep(300);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  StopClient();
  Result := '';
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  // Mirror the current state once, so the checkbox shows whether autostart is on right now.
  // Silent installs also go through this event: there /TASKS must win, so skip it.
  if (CurPageID = wpSelectTasks) and not AutostartInitialized and not WizardSilent then
  begin
    if RegValueExists(HKEY_CURRENT_USER, RunKey, RunValue) then
      WizardSelectTasks('autostart')
    else
      WizardSelectTasks('!autostart');
    AutostartInitialized := True;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    Log('Autostart task selected: ' + IntToStr(Ord(WizardIsTaskSelected('autostart'))));
    // Kept on purpose: makes /LOG show what the installer decided about autostart.
    if WizardIsTaskSelected('autostart') then
      // Same command the app writes from its tray menu, so both stay in agreement.
      RegWriteStringValue(HKEY_CURRENT_USER, RunKey, RunValue,
        '"' + ExpandConstant('{app}\{#AppExe}') + '" --tray')
    else if not WizardSilent then
      // Unticked in the wizard: turn it off. Silent installs only ever add, never remove.
      RegDeleteValue(HKEY_CURRENT_USER, RunKey, RunValue);
  end;
end;

function InitializeUninstall(): Boolean;
begin
  StopClient();
  Result := True;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  SettingsDir: String;
begin
  if CurUninstallStep = usUninstall then
    // Also covers autostart turned on later from the tray menu (not tracked by the installer).
    RegDeleteValue(HKEY_CURRENT_USER, RunKey, RunValue);

  if CurUninstallStep = usPostUninstall then
  begin
    SettingsDir := ExpandConstant('{userappdata}\rafaelholder\SonyHeadphonesClient');
    // Silent uninstalls keep settings; interactive ones ask (default: keep).
    if DirExists(SettingsDir) and not UninstallSilent then
      if MsgBox(FmtMessage(CustomMessage('DeleteSettings'), [SettingsDir]), mbConfirmation,
                MB_YESNO or MB_DEFBUTTON2) = IDYES then
      begin
        DelTree(SettingsDir, True, True, True);
        RemoveDir(ExpandConstant('{userappdata}\rafaelholder')); // Only succeeds when empty.
      end;
  end;
end;
