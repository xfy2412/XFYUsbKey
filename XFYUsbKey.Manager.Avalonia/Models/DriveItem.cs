using System.Globalization;

namespace XFYUsbKey.Manager.Avalonia.Models;

public class DriveItem
{
    public string Path { get; init; } = "";
    public string Name { get; init; } = "";
    public string VolumeLabel { get; init; } = "";
    public long TotalSize { get; init; }

    public string Display => $"{Name}  {VolumeLabel} ({TotalSize / 1073741824.0:F1}GB)";
}
