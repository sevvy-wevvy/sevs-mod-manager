namespace SevsModManager.Core;

internal static class AppIcons
{
    private static Icon? _icon;
    private static Image? _png;

    public static Icon Icon => _icon ??= LoadIcon();
    public static Image Png => _png ??= LoadPng();

    private static Icon LoadIcon()
    {
        using var s = typeof(AppIcons).Assembly.GetManifestResourceStream("SevsModManager.icon.ico")!;
        var bytes = new byte[s.Length];
        s.ReadExactly(bytes);
        return new Icon(new MemoryStream(bytes));
    }

    private static Image LoadPng()
    {
        using var s = typeof(AppIcons).Assembly.GetManifestResourceStream("SevsModManager.icon.png")!;
        var bytes = new byte[s.Length];
        s.ReadExactly(bytes);
        return Image.FromStream(new MemoryStream(bytes));
    }

    public static Image? TryExtractGameIcon(string exePath)
    {
        if (string.IsNullOrEmpty(exePath) || !File.Exists(exePath)) return null;
        try
        {
            using var icon = Icon.ExtractAssociatedIcon(exePath);
            return icon?.ToBitmap();
        }
        catch { return null; }
    }
}
