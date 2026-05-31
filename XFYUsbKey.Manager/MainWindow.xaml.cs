using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Windows;
using System.Management;

namespace XFYUsbKey.Manager
{
    // Drive item for the ComboBox
    public class DriveItem
    {
        public string Path { get; set; }         // Actual drive path (e.g. "C:")
        public string Name { get; set; }         // Display name (e.g. "C: [本地]")
        public string Display => $"{Name}  {VolumeLabel} ({Size})";
        public string VolumeLabel { get; set; }
        public string Size { get; set; }
    }

    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            PageHome.Visibility = Visibility.Visible;
            PageSettings.Visibility = Visibility.Collapsed;
            PageAbout.Visibility = Visibility.Collapsed;
            RefreshDriveList();
        }

        // 导航切换
        private void Nav_Checked(object sender, RoutedEventArgs e)
        {
            if (PageHome == null)
            {
                return;
            }
            PageHome.Visibility = NavHome.IsChecked == true ? Visibility.Visible : Visibility.Collapsed;
            PageSettings.Visibility = NavSettings.IsChecked == true ? Visibility.Visible : Visibility.Collapsed;
            PageAbout.Visibility = NavAbout.IsChecked == true ? Visibility.Visible : Visibility.Collapsed;
            if (NavHome.IsChecked == true)
            {
                RefreshDriveList();
            }
        }

        // 刷新盘符列表
        private void RefreshDriveList()
        {
            var drives = new List<DriveItem>();
            foreach (var di in DriveInfo.GetDrives())
            {
                if (!di.IsReady)
                {
                    continue;
                }
                string size = $"{di.TotalSize / 1073741824.0:F1}GB";
                string typeTag = di.DriveType == DriveType.Removable ? " [USB]" :
                                 di.DriveType == DriveType.CDRom ? " [CD]" :
                                 di.DriveType == DriveType.Fixed ? " [本地]" : " [?]";
                drives.Add(new DriveItem
                {
                    Path = di.Name.TrimEnd('\\'),
                    Name = di.Name.TrimEnd('\\') + typeTag,
                    VolumeLabel = string.IsNullOrWhiteSpace(di.VolumeLabel) ? "未命名" : di.VolumeLabel,
                    Size = size
                });
            }
            DriveSelector.ItemsSource = drives;
            if (drives.Any())
            {
                DriveSelector.SelectedIndex = 0;
                StatusText.Text = $"检测到 {drives.Count} 个可移动磁盘";
                StatusText.Foreground = FindResource("TextSecondaryBrush") as System.Windows.Media.Brush;
            }
            else
            {
                StatusText.Text = "未检测到 U 盘，请插入后重试";
                StatusText.Foreground = System.Windows.Media.Brushes.IndianRed;
            }
        }

        // 写入密钥文件
        private void BtnWriteKey_Click(object sender, RoutedEventArgs e)
        {
            var drive = DriveSelector.SelectedItem as DriveItem;
            if (drive == null)
            {
                MessageBox.Show(this, "请先选择目标 U 盘", "提示", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            string password = PasswordBox.Password;
            if (string.IsNullOrEmpty(password))
            {
                MessageBox.Show(this, "请输入 Windows 登录密码", "提示", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            try
            {
                string keyDir = $@"{drive.Path}\.xfykey";
                string keyFile = $@"{keyDir}\cred.dat";
                Directory.CreateDirectory(keyDir);

                // 获取 U 盘序列号用于绑定
                string usbSerial = GetUsbSerial(drive.Name);
                byte[] entropy = Encoding.UTF8.GetBytes(usbSerial ?? "XFYUSBKEY");

                // 数据格式：username\0password (UTF-8, null-separated)
                string username = $"{Environment.UserDomainName}\\{Environment.UserName}";
                string keyData = $"{username}\0{password}\0";
                byte[] plainData = Encoding.UTF8.GetBytes(keyData);
                byte[] encrypted = ProtectedData.Protect(plainData, entropy, DataProtectionScope.LocalMachine);

                // 写入文件格式：[加密数据长度(4字节)] + [USB序列号长度(4字节)] + [加密数据] + [USB序列号]
                using (var fs = new FileStream(keyFile, FileMode.Create, FileAccess.Write))
                using (var bw = new BinaryWriter(fs))
                {
                    bw.Write(encrypted.Length);
                    bw.Write(entropy.Length);
                    bw.Write(encrypted);
                    bw.Write(entropy);
                }

                StatusText.Text = $"✓ 密钥已写入 {drive.Name}\\.xfykey\\cred.dat";
                StatusText.Foreground = System.Windows.Media.Brushes.SeaGreen;
                PasswordBox.Clear();
                PinBox.Clear();
            }
            catch (Exception ex)
            {
                MessageBox.Show(this, $"写入失败：{ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        // 获取 USB 物理序列号（通过 WMI）
        private string GetUsbSerial(string driveRoot)
        {
            try
            {
                // 先通过卷找到对应的物理驱动器
                string query = $"ASSOCIATORS OF {{Win32_LogicalDisk.DeviceID='{driveRoot}'}} WHERE ResultClass=Win32_DiskPartition";
                // 简化方法：直接查所有 USB 驱动器
                using (var searcher = new System.Management.ManagementObjectSearcher(
                    "SELECT * FROM Win32_DiskDrive WHERE InterfaceType='USB'"))
                {
                    foreach (var obj in searcher.Get())
                    {
                        if (obj["SerialNumber"] != null)
                        {
                            return obj["SerialNumber"].ToString().Trim();
                        }
                    }
                }
            }
            catch
            {
                // WMI 不可用时降级
            }
            return null;
        }
    }
}
