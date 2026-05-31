param(
    [switch]$Uninstall,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

# ─────────────────────────────────────────────
# 配置
# ─────────────────────────────────────────────
$Guid       = "{c101a055-a911-4a8a-a179-beeb4cf24b33}"
$DllName    = "XFYUsbKeyCredentialProvider.dll"

# VMware 共享目录（源文件位置）
$ShareRoot = "\\vmware-host\Shared Folders\VMShare\XFY\new\XFYUsbKey"

# 本地工作目录（所有操作在本地进行）
$WorkDir    = "$env:LOCALAPPDATA\CredProvDeploy"

# ─────────────────────────────────────────────
# 检查管理员权限
# ─────────────────────────────────────────────
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "[-] 请以管理员身份运行！" -ForegroundColor Red
    exit 1
}

# ─────────────────────────────────────────────
# 卸载
# ─────────────────────────────────────────────
if ($Uninstall) {
    Write-Host "[*] 卸载 Credential Provider..." -ForegroundColor Yellow

    $regPaths = @(
        "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$Guid",
        "HKCR:\CLSID\$Guid"
    )
    foreach ($path in $regPaths) {
        if (Test-Path $path) {
            Remove-Item $path -Recurse -Force
            Write-Host "[+] 注册表已删除: $path" -ForegroundColor Green
        }
    }

    $dllDest = "$env:SystemRoot\System32\$DllName"
    if (Test-Path $dllDest) {
        Remove-Item $dllDest -Force
        Write-Host "[+] DLL 已删除: $dllDest" -ForegroundColor Green
    }

    Write-Host "[✓] 卸载完成" -ForegroundColor Green
    exit 0
}

# ─────────────────────────────────────────────
# 部署
# ─────────────────────────────────────────────
Write-Host "[*] 部署 Credential Provider..." -ForegroundColor Yellow

# 1) 检查共享目录是否可访问
if (-not (Test-Path $ShareRoot)) {
    Write-Host "[-] 无法访问共享目录，请确认 VMware 共享文件夹已启用" -ForegroundColor Red
    Write-Host "    预期路径: $ShareRoot" -ForegroundColor Red
    exit 1
}
Write-Host "[+] 共享目录已连接: $ShareRoot" -ForegroundColor Green

# 2) 在本地创建工作目录，复制必要文件
if (-not (Test-Path $WorkDir)) {
    New-Item -ItemType Directory -Path $WorkDir -Force | Out-Null
}
Write-Host "[*] 复制文件到本地: $WorkDir" -ForegroundColor Yellow

# 搜索 DLL（递归查找共享目录下的 x64 输出）

$searchPaths = @(
    "x64\Release\$DllName",
    "x64\Debug\$DllName"
)
$dllSource = $null
foreach ($relPath in $searchPaths) {
    $fullPath = Join-Path $ShareRoot $relPath
    if (Test-Path $fullPath) {
        $dllSource = $fullPath
        break
    }
}
if (-not $dllSource) {
    Write-Host "[-] 未找到 $DllName，请先编译！" -ForegroundColor Red
    exit 1
}
Write-Host "[+] 找到 DLL: $dllSource" -ForegroundColor Green

# 查找 .reg 文件
$regSource = Join-Path $ShareRoot "XFYUsbKey.CredentialProvider\register.reg"
if (-not (Test-Path $regSource)) {
    Write-Host "[-] 未找到 register.reg" -ForegroundColor Red
    exit 1
}

# 复制到本地
Copy-Item $dllSource  (Join-Path $WorkDir $DllName) -Force
Copy-Item $regSource  (Join-Path $WorkDir "register.reg") -Force
$unregSource = Join-Path $ShareRoot "XFYUsbKey.CredentialProvider\Unregister.reg"
if (Test-Path $unregSource) {
    Copy-Item $unregSource (Join-Path $WorkDir "Unregister.reg") -Force
}

# 3) 复制 DLL 到 System32
$dllDest = "$env:SystemRoot\System32\$DllName"
if (Test-Path $dllDest) {
    takeown /f $dllDest /a 2>$null
    icacls $dllDest /grant "Administrators:(F)" /q 2>$null
}
Copy-Item (Join-Path $WorkDir $DllName) $dllDest -Force
Write-Host "[+] DLL 已部署 → $dllDest" -ForegroundColor Green

# 4) 导入注册表（从本地路径）
$localRegFile = Join-Path $WorkDir "register.reg"
Write-Host "[*] 导入注册表..." -ForegroundColor Yellow
& regedit /s $localRegFile
if ($?) {
    Write-Host "[+] 注册表导入成功" -ForegroundColor Green
}
else {
    Write-Host "[-] 注册表导入失败" -ForegroundColor Red
    exit 1
}

# 5) 验证
$cpPath = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Authentication\Credential Providers\$Guid"
if (Test-Path $cpPath) {
    Write-Host "[✓] 部署成功！" -ForegroundColor Green
    Write-Host "`n    操作说明:" -ForegroundColor Cyan
    Write-Host "      部署:  .\deploy.ps1" -ForegroundColor White
    Write-Host "      卸载:  .\deploy.ps1 -Uninstall" -ForegroundColor White
    Write-Host "      强制:  .\deploy.ps1 -Force" -ForegroundColor White

    # ─────────────────────────────────────────────
    # 询问是否锁屏测试
    # ─────────────────────────────────────────────
    Write-Host ""
    $choice = Read-Host "是否立即锁屏查看效果？(y/n)"
    if ($choice -eq 'y' -or $choice -eq 'Y') {
        Write-Host "[*] 正在锁屏..." -ForegroundColor Yellow
        Start-Sleep -Seconds 1
        # P/Invoke LockWorkStation，仅锁 VM 内部的系统
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class VMHelper {
    [DllImport("user32.dll")]
    public static extern bool LockWorkStation();
}
"@
        [VMHelper]::LockWorkStation()
    }
}
else {
    Write-Host "[-] 注册验证失败" -ForegroundColor Red
    exit 1
}
