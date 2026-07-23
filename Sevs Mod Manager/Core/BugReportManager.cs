using System.Text;
using System.IO.Compression;

namespace SevsModManager.Core;

internal static class BugReportManager
{
    public static void Export(string destZipPath, IReadOnlyList<string> logLines)
    {
        string staging = Path.Combine(Path.GetTempPath(), "smm_bugreport_" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(staging);
        try
        {
            File.WriteAllLines(Path.Combine(staging, "log.txt"), logLines);
            File.WriteAllText(Path.Combine(staging, "system-info.txt"), BuildSystemInfo());
            File.WriteAllText(Path.Combine(staging, "installed-mods.txt"), BuildModList());

            if (File.Exists(destZipPath)) File.Delete(destZipPath);
            ZipFile.CreateFromDirectory(staging, destZipPath);
        }
        finally
        {
            try { Directory.Delete(staging, recursive: true); } catch { }
        }
    }

    private static string BuildSystemInfo()
    {
        var sb = new StringBuilder();
        sb.AppendLine("Sev's Mod Manager Bug Report");
        sb.AppendLine($"Generated: {DateTime.Now}");
        sb.AppendLine($"App build: {Program.CurrentVersion}");
        sb.AppendLine($"OS: {Environment.OSVersion}");
        sb.AppendLine($".NET runtime: {Environment.Version}");
        sb.AppendLine($"64-bit OS: {Environment.Is64BitOperatingSystem}");
        sb.AppendLine($"Game: {AppState.Settings.GameName}");
        sb.AppendLine($"Game path: {AppState.Settings.GamePath}");

        var loader = AppState.DetectLoaderKind();
        bool loaderInstalled = AppState.GameDir != null && AppState.IsLoaderInstalled(loader, AppState.GameDir);
        sb.AppendLine($"Mod loader: {AppState.LoaderName(loader)} ({(loaderInstalled ? "installed" : "not found")})");
        return sb.ToString();
    }

    private static string BuildModList()
    {
        var mods = ModInstaller.GetInstalled();
        if (mods.Count == 0) return "(no mods installed)";

        var sb = new StringBuilder();
        foreach (var m in mods)
            sb.AppendLine($"{m.Name} - {(m.Enabled ? "enabled" : "disabled")}{(m.KnownVersion is { Length: > 0 } v ? $" - v{v}" : "")}");
        return sb.ToString();
    }
}
