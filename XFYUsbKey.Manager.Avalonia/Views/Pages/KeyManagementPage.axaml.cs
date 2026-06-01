using System;
using System.Linq;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Threading;
using XFYUsbKey.Manager.Avalonia.Models;
using XFYUsbKey.Manager.Avalonia.Services;

namespace XFYUsbKey.Manager.Avalonia.Views.Pages;

public partial class KeyManagementPage : UserControl
{
    private readonly UsbKeyService _service = new();
    private readonly DispatcherTimer _pollTimer;

    public KeyManagementPage()
    {
        InitializeComponent();
        BtnWriteKey.Click += BtnWriteKey_Click;
        BtnRefresh.Click += (_, _) => RefreshDriveList();

        _pollTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(2) };
        _pollTimer.Tick += (_, _) => RefreshDriveList();
        _pollTimer.Start();

        RefreshDriveList();
    }

    public void RefreshDriveList()
    {
        var drives = _service.GetRemovableDrives();
        DriveSelector.ItemsSource = drives;

        if (drives.Any())
        {
            DriveSelector.SelectedIndex = 0;
            UpdateStatus("success", $"检测到 {drives.Count} 个可移动磁盘",
                "已就绪，选择目标 U 盘后即可写入密钥");
            _pollTimer.Stop();
        }
        else
        {
            UpdateStatus("warning", "未检测到 USB 密钥",
                "未检测到 U 盘，请插入后重试");
            _pollTimer.Start();
        }
    }

    private void UpdateStatus(string badgeClass, string status, string detail)
    {
        StatusBadge.Classes.Clear();
        StatusBadge.Classes.Add("status-badge");
        StatusBadge.Classes.Add(badgeClass);
        StatusText.Text = status;
        StatusDetail.Text = detail;
    }

    private async void BtnWriteKey_Click(object? sender, EventArgs e)
    {
        var drive = DriveSelector.SelectedItem as DriveItem;
        if (drive == null)
        {
            await ShowDialog("提示", "请先选择目标 U 盘");
            return;
        }

        var password = PasswordBox.Text;
        if (string.IsNullOrEmpty(password))
        {
            await ShowDialog("提示", "请输入 Windows 登录密码");
            return;
        }

        try
        {
            if (!_service.VerifyPassword(password))
            {
                await ShowDialog("密码错误", "当前 Windows 登录密码不正确，请重试");
                return;
            }

            _service.WriteKey(drive.Path, password);
            UpdateStatus("success", "✓ 密钥已写入",
                $"凭据已加密保存至 {drive.Path}\\.xfykey\\cred.dat");
            PasswordBox.Clear();
        }
        catch (Exception ex)
        {
            await ShowDialog("错误", $"写入失败：{ex.Message}");
        }
    }

    private async Task ShowDialog(string title, string message)
    {
        var window = TopLevel.GetTopLevel(this) as Window;
        if (window != null)
        {
            var dialog = new FluentAvalonia.UI.Controls.TaskDialog
            {
                Title = title,
                Content = message,
                XamlRoot = window,
                Buttons = [FluentAvalonia.UI.Controls.TaskDialogButton.OKButton]
            };
            await dialog.ShowAsync();
        }
    }
}
