using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Management;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using XFYUsbKey.Manager.Avalonia.Models;

namespace XFYUsbKey.Manager.Avalonia.Services;

public class UsbKeyService
{
    [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern bool LogonUser(string lpszUsername, string lpszDomain,
        string lpszPassword, int dwLogonType, int dwLogonProvider, out IntPtr phToken);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool CloseHandle(IntPtr hObject);

    private const int LOGON32_LOGON_NETWORK = 3;
    private const int LOGON32_PROVIDER_DEFAULT = 0;

    public bool VerifyPassword(string password)
    {
        var username = Environment.UserName;
        var domain = Environment.UserDomainName;
        var success = LogonUser(username, domain, password, LOGON32_LOGON_NETWORK,
            LOGON32_PROVIDER_DEFAULT, out var token);
        if (token != IntPtr.Zero)
            CloseHandle(token);
        return success;
    }
    public List<DriveItem> GetRemovableDrives()
    {
        var drives = new List<DriveItem>();
        foreach (var di in DriveInfo.GetDrives())
        {
            if (!di.IsReady || (di.DriveType != DriveType.Removable && di.DriveType != DriveType.CDRom))
                continue;
            drives.Add(new DriveItem
            {
                Path = di.Name.TrimEnd('\\'),
                Name = di.Name.TrimEnd('\\') + (di.DriveType == DriveType.Removable ? " [USB]" : " [CD]"),
                VolumeLabel = string.IsNullOrWhiteSpace(di.VolumeLabel) ? "未命名" : di.VolumeLabel,
                TotalSize = di.TotalSize
            });
        }
        return drives;
    }

    public string? GetUsbSerial()
    {
        try
        {
            using var searcher = new ManagementObjectSearcher(
                "SELECT * FROM Win32_DiskDrive WHERE InterfaceType='USB'");
            foreach (var obj in searcher.Get())
            {
                if (obj["SerialNumber"] is string serial && !string.IsNullOrWhiteSpace(serial))
                    return serial.Trim();
            }
        }
        catch
        {
            // WMI query may fail in some environments
        }
        return null;
    }

    public void WriteKey(string drivePath, string password)
    {
        var keyDir = Path.Combine(drivePath, ".xfykey");
        var keyFile = Path.Combine(keyDir, "cred.dat");
        Directory.CreateDirectory(keyDir);

        var usbSerial = GetUsbSerial();
        var entropy = Encoding.UTF8.GetBytes(usbSerial ?? "XFYUSBKEY");

        var username = $"{Environment.UserDomainName}\\{Environment.UserName}";
        var keyData = $"{username}\0{password}";
        var plainData = Encoding.UTF8.GetBytes(keyData);
        var encrypted = ProtectedData.Protect(plainData, entropy, DataProtectionScope.LocalMachine);

        using var fs = new FileStream(keyFile, FileMode.Create, FileAccess.Write);
        using var bw = new BinaryWriter(fs);
        bw.Write(encrypted.Length);
        bw.Write(entropy.Length);
        bw.Write(encrypted);
        bw.Write(entropy);
    }
}
