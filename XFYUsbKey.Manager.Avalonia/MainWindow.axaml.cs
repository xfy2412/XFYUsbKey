using System.Collections.Generic;
using Avalonia.Controls;
using FluentAvalonia.UI.Controls;
using XFYUsbKey.Manager.Avalonia.Views.Pages;

namespace XFYUsbKey.Manager.Avalonia;

public partial class MainWindow : Window
{
    private readonly Dictionary<string, Control> _pages = new();

    private static readonly Dictionary<string, string> Titles = new()
    {
        ["KeyManagement"] = "密钥管理",
        ["Settings"] = "设置",
        ["About"] = "关于"
    };

    public MainWindow()
    {
        InitializeComponent();

        _pages["KeyManagement"] = new KeyManagementPage();
        _pages["Settings"] = new SettingsPage();
        _pages["About"] = new AboutPage();

        PageTitle.Text = "密钥管理";
        PageContent.Content = _pages["KeyManagement"];
    }

    private void NavView_OnItemInvoked(object? sender, NavigationViewItemInvokedEventArgs e)
    {
        if (e.InvokedItemContainer is NavigationViewItem item && item.Tag is string tag)
        {
            NavigateTo(tag);
        }
    }

    private void NavigateTo(string tag)
    {
        if (_pages.TryGetValue(tag, out var page))
        {
            PageTitle.Text = Titles.GetValueOrDefault(tag, "");
            PageContent.Content = page;
        }
    }
}
