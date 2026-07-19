using System.Net.Http;
using System.IO.Compression;

namespace SevsModManager.Core;

internal static class ModInstaller
{
    private static readonly HttpClient _http = new() { Timeout = TimeSpan.FromMinutes(5) };
    static ModInstaller() => _http.DefaultRequestHeaders.UserAgent.ParseAdd("SevsModManager/1.0");

    public static List<InstalledMod> GetInstalled()
    {
        var result = new List<InstalledMod>();
        string? modsDir = AppState.ModsInstallDir;
        if (modsDir == null || !Directory.Exists(modsDir)) return result;

        foreach (var f in Directory.GetFiles(modsDir, "*.dll"))
            result.Add(new InstalledMod { Name = Path.GetFileNameWithoutExtension(f), FilePath = f, Enabled = true });

        foreach (var f in Directory.GetFiles(modsDir, "*.dll.disabled"))
        {
            string name = Path.GetFileNameWithoutExtension(f);
            if (name.EndsWith(".dll")) name = name[..^4];
            result.Add(new InstalledMod { Name = name, FilePath = f, Enabled = false });
        }

        foreach (var dir in Directory.GetDirectories(modsDir))
        {
            int active   = Directory.EnumerateFiles(dir, "*.dll", SearchOption.AllDirectories).Count();
            int disabled = Directory.EnumerateFiles(dir, "*.dll.disabled", SearchOption.AllDirectories).Count();
            if (active + disabled == 0) continue;

            result.Add(new InstalledMod { Name = Path.GetFileName(dir), FilePath = dir, Enabled = active > 0, IsFolder = true });
        }

        foreach (var m in result)
            m.KnownVersion = DataBridge.GetModVersion(m.Name) is { Length: > 0 } v ? v : null;

        return result.OrderBy(m => m.Name).ToList();
    }

    public static async Task InstallAsync(string downloadUrl, string dllName, IProgress<(int percent, string status)> progress)
    {
        string? modsDir = AppState.ModsInstallDir;
        if (modsDir == null) throw new Exception("No game path set.");
        Directory.CreateDirectory(modsDir);

        string dest = Path.Combine(modsDir, dllName + ".dll");
        string temp = Path.Combine(Path.GetTempPath(), dllName + ".dll");

        progress.Report((0, $"Downloading {dllName}..."));

        using (var response = await _http.GetAsync(downloadUrl, HttpCompletionOption.ResponseHeadersRead))
        {
            response.EnsureSuccessStatusCode();
            long? total = response.Content.Headers.ContentLength;
            await using var stream = await response.Content.ReadAsStreamAsync();
            await using var file   = File.Create(temp);

            var buf = new byte[65536];
            long downloaded = 0;
            int read;
            while ((read = await stream.ReadAsync(buf)) > 0)
            {
                await file.WriteAsync(buf.AsMemory(0, read));
                downloaded += read;
                if (total.HasValue)
                    progress.Report((int)(downloaded * 90 / total.Value), $"Downloading {dllName}... {downloaded / 1024}KB");
            }
        }

        progress.Report((95, $"Installing {dllName}..."));
        if (File.Exists(dest)) File.Delete(dest);
        File.Move(temp, dest);
        progress.Report((100, $"{dllName} installed."));
    }

    public static async Task InstallThunderstoreAsync(string downloadUrl, string folderName, IProgress<(int percent, string status)> progress)
    {
        var loader = AppState.DetectLoaderKind();
        string? modsDir = AppState.ModsInstallDir;
        if (modsDir == null) throw new Exception("No game path set.");
        Directory.CreateDirectory(modsDir);

        string tempZip = Path.Combine(Path.GetTempPath(), folderName + ".zip");
        string staging = Path.Combine(Path.GetTempPath(), "smm_pkg_" + Guid.NewGuid().ToString("N"));

        progress.Report((0, $"Downloading {folderName}..."));

        using (var response = await _http.GetAsync(downloadUrl, HttpCompletionOption.ResponseHeadersRead))
        {
            response.EnsureSuccessStatusCode();
            long? total = response.Content.Headers.ContentLength;
            await using var stream = await response.Content.ReadAsStreamAsync();
            await using var file   = File.Create(tempZip);

            var buf = new byte[65536];
            long downloaded = 0;
            int read;
            while ((read = await stream.ReadAsync(buf)) > 0)
            {
                await file.WriteAsync(buf.AsMemory(0, read));
                downloaded += read;
                if (total.HasValue)
                    progress.Report((int)(downloaded * 90 / total.Value), $"Downloading {folderName}... {downloaded / 1024}KB");
            }
        }

        try
        {
            progress.Report((95, $"Extracting {folderName}..."));
            Directory.CreateDirectory(staging);
            ZipFile.ExtractToDirectory(tempZip, staging, overwriteFiles: true);

            string content = UnwrapKnownRoot(staging);

            if (loader == ModLoaderKind.MelonLoader)
            {
                foreach (var dll in Directory.GetFiles(content, "*.dll", SearchOption.AllDirectories))
                    File.Copy(dll, Path.Combine(modsDir, Path.GetFileName(dll)), overwrite: true);
            }
            else
            {
                string destDir = Path.Combine(modsDir, folderName);
                if (Directory.Exists(destDir)) Directory.Delete(destDir, recursive: true);
                Directory.Move(content, destDir);
            }
        }
        finally
        {
            File.Delete(tempZip);
            if (Directory.Exists(staging)) Directory.Delete(staging, recursive: true);
        }

        progress.Report((100, $"{folderName} installed."));
    }

    private static string UnwrapKnownRoot(string extractRoot)
    {
        string[][] prefixChains =
        {
            new[] { "BepInEx", "plugins" },
            new[] { "BepInEx", "patchers" },
            new[] { "MelonLoader", "Mods" },
            new[] { "BepInEx" },
            new[] { "MelonLoader" },
            new[] { "Mods" },
            new[] { "Plugins" },
        };

        string current = extractRoot;
        foreach (var chain in prefixChains)
        {
            string candidate = extractRoot;
            bool matched = true;
            foreach (var part in chain)
            {
                string next = Path.Combine(candidate, part);
                if (!Directory.Exists(next)) { matched = false; break; }
                candidate = next;
            }
            if (matched) { current = candidate; break; }
        }

        if (current != extractRoot)
        {
            var entries = Directory.GetFileSystemEntries(current);
            if (entries.Length == 1 && Directory.Exists(entries[0]))
                current = entries[0];
        }

        return current;
    }

    public static void InstallLocal(string sourceDllPath)
    {
        string? modsDir = AppState.ModsInstallDir;
        if (modsDir == null) throw new Exception("No game path set.");
        Directory.CreateDirectory(modsDir);

        string name = Path.GetFileNameWithoutExtension(sourceDllPath);
        string dest = Path.Combine(modsDir, name + ".dll");
        File.Copy(sourceDllPath, dest, overwrite: true);
    }

    public static void Uninstall(string modName)
    {
        string? modsDir = AppState.ModsInstallDir;
        if (modsDir == null) return;

        string folderPath = Path.Combine(modsDir, modName);
        if (Directory.Exists(folderPath))
        {
            string folderTrash = folderPath + ".delete";
            if (Directory.Exists(folderTrash)) Directory.Delete(folderTrash, recursive: true);
            Directory.Move(folderPath, folderTrash);
            DataBridge.DeleteModVersion(modName);
            return;
        }

        string dll      = Path.Combine(modsDir, modName + ".dll");
        string disabled = dll + ".disabled";
        string target   = File.Exists(dll) ? dll : File.Exists(disabled) ? disabled : "";
        if (target.Length > 0)
        {
            string trash = target + ".delete";
            if (File.Exists(trash)) File.Delete(trash);
            File.Move(target, trash);
        }
        DataBridge.DeleteModVersion(modName);
    }

    public static void Enable(string modName)
    {
        string? modsDir = AppState.ModsInstallDir;
        if (modsDir == null) return;

        string folderPath = Path.Combine(modsDir, modName);
        if (Directory.Exists(folderPath)) { SetFolderEnabled(folderPath, enabled: true); return; }

        string dll      = Path.Combine(modsDir, modName + ".dll");
        string disabled = dll + ".disabled";
        if (File.Exists(disabled)) { if (File.Exists(dll)) File.Delete(dll); File.Move(disabled, dll); }
    }

    public static void Disable(string modName)
    {
        string? modsDir = AppState.ModsInstallDir;
        if (modsDir == null) return;

        string folderPath = Path.Combine(modsDir, modName);
        if (Directory.Exists(folderPath)) { SetFolderEnabled(folderPath, enabled: false); return; }

        string dll      = Path.Combine(modsDir, modName + ".dll");
        string disabled = dll + ".disabled";
        if (File.Exists(dll)) { if (File.Exists(disabled)) File.Delete(disabled); File.Move(dll, disabled); }
    }

    private static void SetFolderEnabled(string folderPath, bool enabled)
    {
        string pattern = enabled ? "*.dll.disabled" : "*.dll";
        foreach (var f in Directory.GetFiles(folderPath, pattern, SearchOption.AllDirectories))
        {
            string target = enabled ? f[..^".disabled".Length] : f + ".disabled";
            if (File.Exists(target)) File.Delete(target);
            File.Move(f, target);
        }
    }
}

file static class ProgressHelper
{
    public static void Report(this IProgress<(int, string)> p, int pct, string msg) => p.Report((pct, msg));
}
