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

        ScanFlatDlls(modsDir, result);
        ScanFolderMods(modsDir, result);

        if (AppState.DetectLoaderKind() == ModLoaderKind.BepInEx && AppState.BepInExDir is { } bepDir)
        {
            string patchersDir = Path.Combine(bepDir, "patchers");
            if (Directory.Exists(patchersDir))
            {
                ScanFlatDlls(patchersDir, result);
                ScanFolderMods(patchersDir, result);
            }
        }

        foreach (var m in result)
            m.KnownVersion = DataBridge.GetModVersion(m.Name) is { Length: > 0 } v ? v : null;

        return result.OrderBy(m => m.Name).ToList();
    }

    private static void ScanFlatDlls(string dir, List<InstalledMod> result)
    {
        foreach (var f in Directory.GetFiles(dir, "*.dll"))
            result.Add(new InstalledMod { Name = Path.GetFileNameWithoutExtension(f), FilePath = f, Enabled = true, SizeBytes = new FileInfo(f).Length });

        foreach (var f in Directory.GetFiles(dir, "*.dll.disabled"))
        {
            string name = Path.GetFileNameWithoutExtension(f);
            if (name.EndsWith(".dll")) name = name[..^4];
            result.Add(new InstalledMod { Name = name, FilePath = f, Enabled = false, SizeBytes = new FileInfo(f).Length });
        }
    }

    private static void ScanFolderMods(string dir, List<InstalledMod> result)
    {
        foreach (var sub in Directory.GetDirectories(dir))
        {
            if (sub.EndsWith(".delete", StringComparison.OrdinalIgnoreCase)) continue;

            int active   = Directory.EnumerateFiles(sub, "*.dll", SearchOption.AllDirectories).Count();
            int disabled = Directory.EnumerateFiles(sub, "*.dll.disabled", SearchOption.AllDirectories).Count();
            if (active + disabled == 0) continue;

            long size = Directory.EnumerateFiles(sub, "*", SearchOption.AllDirectories).Sum(f => new FileInfo(f).Length);
            result.Add(new InstalledMod { Name = Path.GetFileName(sub), FilePath = sub, Enabled = active > 0, IsFolder = true, SizeBytes = size });
        }
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

        var extractTask = Task.Run(() =>
        {
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
                MoveDirectory(content, destDir);
            }
        });

        progress.Report((95, $"Extracting {folderName}..."));
        var timeout = Task.Delay(TimeSpan.FromMinutes(3));
        if (await Task.WhenAny(extractTask, timeout) == timeout)
            throw new Exception($"Extracting {folderName} timed out. It may still finish in the background; check the plugins folder before retrying.");

        await extractTask;

        File.Delete(tempZip);
        if (Directory.Exists(staging)) Directory.Delete(staging, recursive: true);

        progress.Report((100, $"{folderName} installed."));
    }

    private static void MoveDirectory(string source, string dest)
    {
        try
        {
            Directory.Move(source, dest);
        }
        catch (IOException)
        {
            CopyDirectory(source, dest);
            Directory.Delete(source, recursive: true);
        }
    }

    private static void CopyDirectory(string source, string dest)
    {
        Directory.CreateDirectory(dest);
        foreach (var dir in Directory.GetDirectories(source))
            CopyDirectory(dir, Path.Combine(dest, Path.GetFileName(dir)));
        foreach (var file in Directory.GetFiles(source))
            File.Copy(file, Path.Combine(dest, Path.GetFileName(file)), overwrite: true);
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
