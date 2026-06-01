; XFY USB Key — Installer
; 需要先编译所有项目再运行此脚本

#define MyAppName "XFY USB Key"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "XFY"
#define MyAppURL ""

#define CPDir "..\x64\Release"
#define ManagerWpfDir "..\XFYUsbKey.Manager\bin\Release"
#define ManagerAvaloniaDir "..\XFYUsbKey.Manager.Avalonia\bin\Release\net9.0"
#define ManagerAvaloniaExeDir "..\XFYUsbKey.Manager.Avalonia\bin\Release"

[Setup]
AppId={{B5F3E2A1-7C8D-4E9F-9A0B-1C2D3E4F5A6B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=XFYUsbKey_Setup_v{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
UninstallDisplayIcon={app}\Manager\XFYUsbKey.Manager.exe
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
ShowComponentSizes=no

[Languages]
Name: "chinese"; MessagesFile: "ChineseSimplified.isl"

[Types]
Name: "typical"; Description: "标准安装"
Name: "custom"; Description: "自定义"; Flags: iscustom

[Components]
Name: "cp"; Description: "凭据提供程序 (Credential Provider)"; Types: typical custom; Flags: fixed
Name: "manager"; Description: "管理器"; Types: typical custom; Flags: dontinheritcheck
Name: "manager\wpf"; Description: "WPF 版本 (Windows 自带运行环境)"; Types: typical; Check: ManagerVersionCheck
Name: "manager\avalonia"; Description: "现代版本 (需 .NET 9 运行环境)"; Types: custom

[Files]
; ——— Credential Provider ———
Source: "{#CPDir}\XFYUsbKeyCredentialProvider.dll"; DestDir: "{sys}"; Flags: ignoreversion 32bit; Components: cp; Check: IsWin64
Source: "{#CPDir}\..\XFYUsbKey.CredentialProvider\register.reg"; DestDir: "{tmp}"; Flags: ignoreversion; Components: cp
Source: "{#CPDir}\..\XFYUsbKey.CredentialProvider\Unregister.reg"; DestDir: "{tmp}"; Flags: ignoreversion; Components: cp

; ——— Manager (WPF) ———
Source: "{#ManagerWpfDir}\XFYUsbKey.Manager.exe"; DestDir: "{app}\Manager"; Flags: ignoreversion; Components: manager\wpf
Source: "{#ManagerWpfDir}\XFYUsbKey.Manager.exe.config"; DestDir: "{app}\Manager"; Flags: ignoreversion; Components: manager\wpf

; ——— Manager (Avalonia) ———
Source: "{#ManagerAvaloniaDir}\XFYUsbKey.Manager.Avalonia.dll"; DestDir: "{app}\Manager"; Flags: ignoreversion; Components: manager\avalonia
Source: "{#ManagerAvaloniaDir}\XFYUsbKey.Manager.Avalonia.exe"; DestDir: "{app}\Manager"; Flags: ignoreversion; Components: manager\avalonia
Source: "{#ManagerAvaloniaDir}\*.dll"; DestDir: "{app}\Manager"; Flags: ignoreversion skipifsourcedoesntexist; Components: manager\avalonia
Source: "{#ManagerAvaloniaDir}\*.json"; DestDir: "{app}\Manager"; Flags: ignoreversion skipifsourcedoesntexist; Components: manager\avalonia
Source: "{#ManagerAvaloniaDir}\*.pdb"; DestDir: "{app}\Manager"; Flags: ignoreversion skipifsourcedoesntexist; Components: manager\avalonia

[Icons]
Name: "{group}\XFY USB Key 管理器"; Filename: "{app}\Manager\XFYUsbKey.Manager.exe"; Components: manager\wpf
Name: "{group}\XFY USB Key 管理器 (现代版)"; Filename: "{app}\Manager\XFYUsbKey.Manager.Avalonia.exe"; Components: manager\avalonia
Name: "{group}\卸载 XFY USB Key"; Filename: "{uninstallexe}"
Name: "{commondesktop}\XFY USB Key"; Filename: "{app}\Manager\XFYUsbKey.Manager.exe"; Components: manager\wpf
Name: "{commondesktop}\XFY USB Key (现代版)"; Filename: "{app}\Manager\XFYUsbKey.Manager.Avalonia.exe"; Components: manager\avalonia

[Registry]
; 注册 Credential Provider
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\{c101a055-a911-4a8a-a179-beeb4cf24b33}"; ValueType: string; ValueName: ""; ValueData: "XFYUsbKeyCredentialProvider"; Components: cp; Flags: uninsdeletekey
Root: HKCR; Subkey: "CLSID\{c101a055-a911-4a8a-a179-beeb4cf24b33}"; ValueType: string; ValueName: ""; ValueData: "XFYUsbKeyCredentialProvider"; Components: cp; Flags: uninsdeletekey
Root: HKCR; Subkey: "CLSID\{c101a055-a911-4a8a-a179-beeb4cf24b33}\InprocServer32"; ValueType: string; ValueName: ""; ValueData: "{sys}\XFYUsbKeyCredentialProvider.dll"; Components: cp; Flags: uninsdeletekey
Root: HKCR; Subkey: "CLSID\{c101a055-a911-4a8a-a179-beeb4cf24b33}\InprocServer32"; ValueType: string; ValueName: "ThreadingModel"; ValueData: "Apartment"; Components: cp

[Run]
; 注册 CP（确保 DLL 已就位后执行 regedit）
Filename: "regedit.exe"; Parameters: "/s ""{tmp}\register.reg"""; Flags: runhidden; Components: cp

[UninstallRun]
Filename: "regedit.exe"; Parameters: "/s ""{tmp}\Unregister.reg"""; Flags: runhidden; Components: cp

[Code]
function IsDotNet9Installed: Boolean;
var
  Version: string;
begin
  Result := RegQueryStringValue(HKLM, 'SOFTWARE\dotnet\Setup\InstalledVersions\x64\sharedfx\Microsoft.NETCore.App\9.0.0', 'Version', Version) or
            RegQueryStringValue(HKCU, 'SOFTWARE\dotnet\Setup\InstalledVersions\x64\sharedfx\Microsoft.NETCore.App\9.0.0', 'Version', Version);
  if not Result then
    Result := RegKeyExists(HKLM, 'SOFTWARE\Microsoft\NET Core Setup\NDP\v9.0\9.0.0');
end;

function ManagerVersionCheck: Boolean;
begin
  Result := not IsDotNet9Installed;
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoGroupInfo, MemoTasksInfo: String): String;
var
  S: String;
begin
  S := '';
  S := S + MemoDirInfo + NewLine + NewLine;
  S := S + MemoGroupInfo + NewLine + NewLine;

  if IsComponentSelected('cp') then
    S := S + '  凭据提供程序 (Credential Provider)' + NewLine;
  if IsComponentSelected('manager\wpf') then
    S := S + '  管理器 (WPF 版)' + NewLine;
  if IsComponentSelected('manager\avalonia') then
    S := S + '  管理器 (现代版)' + NewLine;

  if IsComponentSelected('manager\avalonia') and not IsDotNet9Installed then
    S := S + NewLine + '⚠ 注意: 现代版需要 .NET 9 运行环境' + NewLine +
                     '  请访问 https://dotnet.microsoft.com/download/dotnet/9.0 安装' + NewLine;

  Result := S;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    if IsComponentSelected('manager\avalonia') and not IsDotNet9Installed then
      MsgBox('.NET 9 运行环境未检测到，现代版可能无法运行。' + NewLine + NewLine +
             '请访问以下链接安装:' + NewLine +
             'https://dotnet.microsoft.com/download/dotnet/9.0' + NewLine + NewLine +
             '或选择 WPF 版本重新安装。', mbInformation, MB_OK);
  end;
end;
