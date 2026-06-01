using Avalonia.Media;
using FluentAvalonia.UI.Controls;

namespace XFYUsbKey.Manager.Avalonia;

public class FluentIconSource : FontIconSource
{
    public FluentIconSource()
    {
        FontFamily = new FontFamily(
            "avares://XFYUsbKey.Manager.Avalonia/Assets/Fonts/#FluentSystemIcons-Resizable");
    }
}
