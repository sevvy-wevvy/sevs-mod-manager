using Microsoft.Win32;
using System.Text.RegularExpressions;

namespace SevsModManager.Core;

internal static class SteamLocator
{
    public static string? FindGamePath(int appId, string exeFileName)
    {
        foreach (var gameDir in FindInstallDirs(appId))
        {
            string exePath = Path.Combine(gameDir, exeFileName);
            if (File.Exists(exePath)) return exePath;

            var found = Directory.GetDirectories(gameDir)
                .Select(d => Path.Combine(d, exeFileName))
                .FirstOrDefault(File.Exists);
            if (found != null) return found;
        }
        return null;
    }

    public static List<(int AppId, string Name)> ListInstalledGames()
    {
        var manifestPaths = GetLibraryFolders()
            .Select(lib => Path.Combine(lib, "steamapps"))
            .Where(Directory.Exists)
            .SelectMany(dir => Directory.EnumerateFiles(dir, "appmanifest_*.acf"))
            .ToList();

        var result = new System.Collections.Concurrent.ConcurrentBag<(int, string)>();
        Parallel.ForEach(manifestPaths, manifestPath =>
        {
            try
            {
                string content = File.ReadAllText(manifestPath);
                string? appIdStr = ParseVdfValue(content, "appid");
                string? name = ParseVdfValue(content, "name");
                if (appIdStr != null && name != null && int.TryParse(appIdStr, out int appId))
                    result.Add((appId, name));
            }
            catch { }
        });

        return result.DistinctBy(g => g.Item1).OrderBy(g => g.Item2, StringComparer.OrdinalIgnoreCase).ToList();
    }

    public static List<string> FindExeCandidates(int appId)
    {
        var result = new List<string>();
        foreach (var gameDir in FindInstallDirs(appId))
            result.AddRange(Directory.GetFiles(gameDir, "*.exe", SearchOption.TopDirectoryOnly)
                .Where(f => !Path.GetFileName(f).Contains("CrashHandler", StringComparison.OrdinalIgnoreCase)
                         && !Path.GetFileName(f).Contains("redist", StringComparison.OrdinalIgnoreCase)));
        return result;
    }

    private static IEnumerable<string> FindInstallDirs(int appId)
    {
        foreach (var libraryDir in GetLibraryFolders())
        {
            string manifestPath = Path.Combine(libraryDir, "steamapps", $"appmanifest_{appId}.acf");
            if (!File.Exists(manifestPath)) continue;

            string? installDir = ParseVdfValue(File.ReadAllText(manifestPath), "installdir");
            if (installDir == null) continue;

            string gameDir = Path.Combine(libraryDir, "steamapps", "common", installDir);
            if (Directory.Exists(gameDir)) yield return gameDir;
        }
    }

    private static List<string> GetLibraryFolders()
    {
        var result = new List<string>();
        string? steamPath = GetSteamPath();
        if (steamPath == null) return result;

        result.Add(steamPath);

        string vdfPath = Path.Combine(steamPath, "steamapps", "libraryfolders.vdf");
        if (File.Exists(vdfPath))
        {
            try
            {
                string content = File.ReadAllText(vdfPath);
                foreach (Match m in Regex.Matches(content, "\"path\"\\s*\"([^\"]+)\""))
                {
                    string path = m.Groups[1].Value.Replace("\\\\", "\\");
                    if (!result.Contains(path, StringComparer.OrdinalIgnoreCase)) result.Add(path);
                }
            }
            catch { }
        }
        return result;
    }

    private static string? GetSteamPath()
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam");
            if (key?.GetValue("SteamPath") is string p) return p.Replace('/', '\\');
        }
        catch { }
        try
        {
            using var key = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Valve\Steam")
                          ?? Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Valve\Steam");
            if (key?.GetValue("InstallPath") is string p) return p;
        }
        catch { }
        return null;
    }

    private static string? ParseVdfValue(string content, string key)
    {
        var m = Regex.Match(content, $"\"{Regex.Escape(key)}\"\\s*\"([^\"]+)\"", RegexOptions.IgnoreCase);
        return m.Success ? m.Groups[1].Value : null;
    }
}
