using System.Net.Http;
using System.IO.Compression;

namespace SevsModManager.Core;

internal static class MelonLoaderManager
{
    private const string Version = "0.7.3";
    private const string UrlX64 = $"https://github.com/LavaGang/MelonLoader/releases/download/v{Version}/MelonLoader.x64.zip";
    private const string UrlX86 = $"https://github.com/LavaGang/MelonLoader/releases/download/v{Version}/MelonLoader.x86.zip";

    private static readonly HttpClient _http = new() { Timeout = TimeSpan.FromMinutes(5) };
    static MelonLoaderManager() => _http.DefaultRequestHeaders.UserAgent.ParseAdd("SevsModManager/1.0");

    public static bool IsInstalled(string gameDir) =>
        Directory.Exists(Path.Combine(gameDir, "MelonLoader")) &&
        File.Exists(Path.Combine(gameDir, "version.dll"));

    public static bool ModsEnabled(string gameDir) =>
        File.Exists(Path.Combine(gameDir, "version.dll"));

    public static void ToggleMods(string gameDir)
    {
        string active   = Path.Combine(gameDir, "version.dll");
        string disabled = Path.Combine(gameDir, "version.dll.disabled");

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
        string url = AppState.IsExeX64(AppState.Settings.GamePath) ? UrlX64 : UrlX86;
        string tempZip = Path.Combine(Path.GetTempPath(), "MelonLoader_install.zip");

        progress.Report((5, "Downloading MelonLoader..."));

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

        progress.Report((88, "Extracting MelonLoader..."));
        ZipFile.ExtractToDirectory(tempZip, gameDir, overwriteFiles: true);

        Directory.CreateDirectory(Path.Combine(gameDir, "Mods"));
        Directory.CreateDirectory(Path.Combine(gameDir, "Plugins"));
        Directory.CreateDirectory(Path.Combine(gameDir, "UserData"));

        progress.Report((98, "Cleaning up..."));
        File.Delete(tempZip);

        progress.Report((100, "MelonLoader installed."));
    }

    public static void Uninstall(string gameDir)
    {
        foreach (var name in new[] { "MelonLoader", "Mods", "Plugins", "UserData" })
        {
            string path = Path.Combine(gameDir, name);
            if (Directory.Exists(path)) Directory.Delete(path, recursive: true);
        }

        foreach (var name in new[] { "version.dll", "version.dll.disabled" })
        {
            string path = Path.Combine(gameDir, name);
            if (File.Exists(path)) File.Delete(path);
        }
    }
}
