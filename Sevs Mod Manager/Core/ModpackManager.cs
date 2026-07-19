using System.Text.Json;
using System.IO.Compression;

namespace SevsModManager.Core;

internal readonly record struct PackRoot(string Name, string Prefix, string AbsolutePath);

internal static class ModpackManager
{
    private const string ManifestEntry = "smmpack.json";

    private static readonly JsonSerializerOptions _jsonOptions = new() { WriteIndented = true };

    public static string PacksDir =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "SevsModManager", "Modpacks", AppState.CurrentPreset.Slug);

    public static List<PackRoot> GetPackRoots()
    {
        var result = new List<PackRoot>();
        if (AppState.DetectLoaderKind() == ModLoaderKind.MelonLoader)
        {
            if (AppState.GameDir is not { } gameDir) return result;
            foreach (var name in new[] { "Mods", "UserData", "Plugins", "MelonLoader" })
                result.Add(new PackRoot(name, name + "/", Path.Combine(gameDir, name)));
        }
        else
        {
            if (AppState.BepInExDir is not { } bepDir) return result;
            result.Add(new PackRoot("BepInEx", "BepInEx/", bepDir));
        }
        return result;
    }

    public static HashSet<string> DefaultRootNames =>
        AppState.DetectLoaderKind() == ModLoaderKind.MelonLoader
            ? new(StringComparer.OrdinalIgnoreCase) { "Mods", "UserData" }
            : new(StringComparer.OrdinalIgnoreCase) { "plugins", "config" };

    public static List<ModpackInfo> GetSavedPacks()
    {
        var result = new List<ModpackInfo>();
        if (!Directory.Exists(PacksDir)) return result;

        foreach (var f in Directory.GetFiles(PacksDir, "*.smmpack"))
        {
            var manifest = TryReadManifest(f);
            if (manifest != null) result.Add(new ModpackInfo { FilePath = f, Manifest = manifest });
        }
        return result.OrderBy(p => p.DisplayName).ToList();
    }

    public static ModpackManifest? TryReadManifest(string packPath)
    {
        try
        {
            using var zip = ZipFile.OpenRead(packPath);
            var entry = zip.GetEntry(ManifestEntry);
            if (entry == null) return null;
            using var stream = entry.Open();
            return JsonSerializer.Deserialize<ModpackManifest>(stream, _jsonOptions);
        }
        catch { return null; }
    }

    public static List<string> GetCurrentModNames()
    {
        string? modsDir = AppState.ModsInstallDir;
        return modsDir != null && Directory.Exists(modsDir)
            ? Directory.GetFiles(modsDir, "*.dll", SearchOption.TopDirectoryOnly)
                .Select(Path.GetFileNameWithoutExtension).Select(n => n!).ToList()
            : new List<string>();
    }

    public static bool CurrentSetupMatchesSavedPack()
    {
        var current = new HashSet<string>(GetCurrentModNames(), StringComparer.OrdinalIgnoreCase);
        return GetSavedPacks().Any(p => current.SetEquals(p.Manifest.Mods));
    }

    public static async Task CreateBlankSetup(IProgress<(int percent, string status)>? progress = null)
    {
        string? gameDir = AppState.GameDir;
        if (gameDir == null) throw new Exception("No game path set.");
        var loader = AppState.DetectLoaderKind();

        progress?.Report((0, "Removing current setup..."));
        foreach (var root in GetPackRoots())
            if (Directory.Exists(root.AbsolutePath)) Directory.Delete(root.AbsolutePath, recursive: true);

        await AppState.InstallLoaderAsync(loader, gameDir, new Progress<(int percent, string status)>(p =>
            progress?.Report((p.percent, p.status))));
    }

    public static string SaveCurrentAsPack(string name, string author, IProgress<(int percent, string status)>? progress = null)
    {
        var roots = GetPackRoots();
        if (roots.Count == 0) throw new Exception("No mod loader folder found.");

        var files = roots.SelectMany(r => Directory.Exists(r.AbsolutePath)
            ? Directory.GetFiles(r.AbsolutePath, "*", SearchOption.AllDirectories)
            : Array.Empty<string>()).ToList();

        return WritePack(name, author, roots, files, progress);
    }

    public static string SaveCurrentAsPackSelective(string name, string author, IEnumerable<string> includedAbsolutePaths, IProgress<(int percent, string status)>? progress = null)
    {
        var roots = GetPackRoots();
        if (roots.Count == 0) throw new Exception("No mod loader folder found.");

        var files = includedAbsolutePaths.Where(File.Exists).ToList();
        return WritePack(name, author, roots, files, progress);
    }

    private static string WritePack(string name, string author, List<PackRoot> roots, List<string> files, IProgress<(int percent, string status)>? progress)
    {
        Directory.CreateDirectory(PacksDir);
        string safeName = string.Join("_", name.Split(Path.GetInvalidFileNameChars()));
        if (safeName.Length == 0) safeName = "Modpack";
        string dest = Path.Combine(PacksDir, safeName + ".smmpack");
        if (File.Exists(dest)) File.Delete(dest);

        var manifest = new ModpackManifest
        {
            Name = name,
            Author = author,
            GameSlug = AppState.CurrentPreset.Slug,
            CreatedUtc = DateTime.UtcNow,
            Mods = GetCurrentModNames(),
        };

        progress?.Report((0, "Scanning files..."));

        using (var zip = ZipFile.Open(dest, ZipArchiveMode.Create))
        {
            var manifestEntry = zip.CreateEntry(ManifestEntry);
            using (var w = new StreamWriter(manifestEntry.Open()))
                w.Write(JsonSerializer.Serialize(manifest, _jsonOptions));

            for (int i = 0; i < files.Count; i++)
            {
                string file = files[i];
                var root = roots.FirstOrDefault(r => file.StartsWith(r.AbsolutePath + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase));
                if (root.AbsolutePath == null) continue;

                string rel = root.Prefix + Path.GetRelativePath(root.AbsolutePath, file).Replace('\\', '/');
                zip.CreateEntryFromFile(file, rel, CompressionLevel.Optimal);
                progress?.Report((files.Count == 0 ? 100 : (i + 1) * 100 / files.Count, $"Zipping {Path.GetFileName(file)}..."));
            }
        }

        return dest;
    }

    public static async Task ApplyPack(string packPath, IProgress<(int percent, string status)>? progress = null)
    {
        string? gameDir = AppState.GameDir;
        if (gameDir == null) throw new Exception("No game path set.");
        var loader = AppState.DetectLoaderKind();
        var roots = GetPackRoots();
        if (roots.Count == 0) throw new Exception("No mod loader folder found.");

        progress?.Report((0, "Removing current setup..."));
        foreach (var root in roots)
            if (Directory.Exists(root.AbsolutePath)) Directory.Delete(root.AbsolutePath, recursive: true);

        await AppState.InstallLoaderAsync(loader, gameDir, new Progress<(int percent, string status)>(p =>
            progress?.Report((p.percent * 40 / 100, p.status))));

        using var zip = ZipFile.OpenRead(packPath);
        var entries = zip.Entries
            .Where(e => e.Name.Length > 0 && roots.Any(r => e.FullName.StartsWith(r.Prefix, StringComparison.OrdinalIgnoreCase)))
            .ToList();

        for (int i = 0; i < entries.Count; i++)
        {
            var entry = entries[i];
            var root = roots.First(r => entry.FullName.StartsWith(r.Prefix, StringComparison.OrdinalIgnoreCase));
            string rel = entry.FullName[root.Prefix.Length..];
            if (rel.Length == 0) continue;

            string dest = Path.Combine(root.AbsolutePath, rel.Replace('/', Path.DirectorySeparatorChar));
            Directory.CreateDirectory(Path.GetDirectoryName(dest)!);
            entry.ExtractToFile(dest, overwrite: true);
            progress?.Report((40 + (entries.Count == 0 ? 60 : (i + 1) * 60 / entries.Count), $"Extracting {entry.Name}..."));
        }
    }

    public static void DeletePack(string packPath)
    {
        if (File.Exists(packPath)) File.Delete(packPath);
    }

    public static void ExportPack(string packPath, string destPath) =>
        File.Copy(packPath, destPath, overwrite: true);

    public static string ImportPack(string sourcePath)
    {
        Directory.CreateDirectory(PacksDir);
        string dest = Path.Combine(PacksDir, Path.GetFileName(sourcePath));
        if (File.Exists(dest))
            dest = Path.Combine(PacksDir, $"{Path.GetFileNameWithoutExtension(sourcePath)}_{DateTime.Now:HHmmss}.smmpack");
        File.Copy(sourcePath, dest, overwrite: false);
        return dest;
    }
}
