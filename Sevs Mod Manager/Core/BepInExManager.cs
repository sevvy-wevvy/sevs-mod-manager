using System.Net.Http;
using System.IO.Compression;

namespace SevsModManager.Core;

internal static class BepInExManager
{
    private const string Version = "5.4.23.5";
    private const string UrlX64 = $"https://github.com/BepInEx/BepInEx/releases/download/v{Version}/BepInEx_win_x64_{Version}.zip";
    private const string UrlX86 = $"https://github.com/BepInEx/BepInEx/releases/download/v{Version}/BepInEx_win_x86_{Version}.zip";

    private static readonly HttpClient _http = new() { Timeout = TimeSpan.FromMinutes(5) };
    static BepInExManager() => _http.DefaultRequestHeaders.UserAgent.ParseAdd("SevsModManager/1.0");

    public static bool IsInstalled(string gameDir) =>
        Directory.Exists(Path.Combine(gameDir, "BepInEx")) &&
        File.Exists(Path.Combine(gameDir, "winhttp.dll"));

    public static bool ModsEnabled(string gameDir) =>
        File.Exists(Path.Combine(gameDir, "winhttp.dll"));

    public static void ToggleMods(string gameDir)
    {
        string active   = Path.Combine(gameDir, "winhttp.dll");
        string disabled = Path.Combine(gameDir, "winhttp.dll.disabled");

        if (File.Exists(active))
        {
            if (File.Exists(disabled)) File.Delete(disabled);
            File.Move(active, disabled);
        }
        else if (File.Exists(disabled))
        {
            File.Move(disabled, active);
        }
    }

    public static async Task InstallAsync(string gameDir, IProgress<(int percent, string status)> progress)
    {
        string url = PickDownloadUrl(AppState.Settings.GamePath);
        string tempZip = Path.Combine(Path.GetTempPath(), "BepInEx_install.zip");

        progress.Report((5, "Downloading BepInEx..."));

        using (var response = await _http.GetAsync(url, HttpCompletionOption.ResponseHeadersRead))
        {
            response.EnsureSuccessStatusCode();
            long? total = response.Content.Headers.ContentLength;
            await using var stream = await response.Content.ReadAsStreamAsync();
            await using var file   = File.Create(tempZip);

            var buf = new byte[81920];
            long downloaded = 0;
            int read;
            while ((read = await stream.ReadAsync(buf)) > 0)
            {
                await file.WriteAsync(buf.AsMemory(0, read));
                downloaded += read;
                if (total.HasValue)
                    progress.Report((5 + (int)(downloaded * 80 / total.Value), $"Downloading... {downloaded / 1024}KB / {total.Value / 1024}KB"));
            }
        }

        progress.Report((88, "Extracting BepInEx..."));
        ZipFile.ExtractToDirectory(tempZip, gameDir, overwriteFiles: true);

        string bepDir = Path.Combine(gameDir, "BepInEx");
        Directory.CreateDirectory(Path.Combine(bepDir, "plugins"));
        Directory.CreateDirectory(Path.Combine(bepDir, "config"));
        Directory.CreateDirectory(Path.Combine(bepDir, "patchers"));

        progress.Report((98, "Cleaning up..."));
        File.Delete(tempZip);

        progress.Report((100, "BepInEx installed."));
    }

    private static string PickDownloadUrl(string gameExePath) =>
        AppState.IsExeX64(gameExePath) ? UrlX64 : UrlX86;
}
