using System.Text.Json.Serialization;
using System.Text.Json;

namespace SevsModManager.Core;

internal static class AppState
{
    private static readonly string _stateFile =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "SevsModManager", "state.json");

    public static AppSettings Settings { get; private set; } = new();

    public static string? GameDir     => Settings.GamePath.Length > 0 ? Path.GetDirectoryName(Settings.GamePath) : null;
    public static string? SbDir       => GameDir is { } d ? Path.Combine(d, "Several Bees") : null;
    public static string? BepInExDir  => GameDir is { } d ? Path.Combine(d, "BepInEx") : null;
    public static string? PluginsDir  => BepInExDir is { } b ? Path.Combine(b, "plugins") : null;
    public static string? MelonModsDir => GameDir is { } d ? Path.Combine(d, "Mods") : null;

    public static string? ModsInstallDir =>
        DetectLoaderKind() == ModLoaderKind.MelonLoader ? MelonModsDir : PluginsDir;

    public static readonly GamePreset[] Presets =
    [
        new("Gorilla Tag", "gorillatag",
        [
            @"C:\Program Files (x86)\Steam\steamapps\common\Gorilla Tag\Gorilla Tag.exe",
            @"C:\Program Files\Steam\steamapps\common\Gorilla Tag\Gorilla Tag.exe",
            @"C:\Program Files\Oculus\Software\Software\another-axiom-gorilla-tag\Gorilla Tag.exe",
        ], SteamAppId: 1533390),
        new("ULTRAKILL", "ultrakill",
        [
            @"C:\Program Files (x86)\Steam\steamapps\common\ULTRAKILL\ULTRAKILL.exe",
            @"C:\Program Files\Steam\steamapps\common\ULTRAKILL\ULTRAKILL.exe",
        ], DefaultThunderstoreCommunity: "ultrakill", SteamAppId: 1229490),
        new("REPO", "repo",
        [
            @"C:\Program Files (x86)\Steam\steamapps\common\REPO\REPO.exe",
            @"C:\Program Files\Steam\steamapps\common\REPO\REPO.exe",
        ], DefaultThunderstoreCommunity: "repo", SteamAppId: 3241660),
        new("White Knuckle", "whiteknuckle",
        [
            @"C:\Program Files (x86)\Steam\steamapps\common\White Knuckle\White Knuckle.exe",
            @"C:\Program Files\Steam\steamapps\common\White Knuckle\White Knuckle.exe",
        ], DefaultThunderstoreCommunity: "white-knuckle", SteamAppId: 3195790),
        new("BONELAB", "bonelab",
        [
            @"C:\Program Files (x86)\Steam\steamapps\common\BONELAB\BONELAB_Steam_Windows64.exe",
            @"C:\Program Files\Steam\steamapps\common\BONELAB\BONELAB_Steam_Windows64.exe",
        ], DefaultThunderstoreCommunity: "bonelab", SteamAppId: 1592190),
        new("Custom", "custom", []),
    ];

    public static GamePreset CurrentPreset =>
        Presets.FirstOrDefault(p => p.Name == Settings.GameName) ?? Presets[^1];

    public static string CurrentGameSlug =>
        CurrentPreset.Name != "Custom" ? CurrentPreset.Slug : DeriveSlug(Settings.GameName);

    public static string DeriveSlug(string name) => new(name.ToLowerInvariant().Where(char.IsLetterOrDigit).ToArray());

    private static readonly JsonSerializerOptions _jsonOptions = new() { WriteIndented = true, IncludeFields = true };

    public static void Load()
    {
        if (!File.Exists(_stateFile)) return;
        try
        {
            var raw = File.ReadAllText(_stateFile);
            Settings = JsonSerializer.Deserialize<AppSettings>(raw, _jsonOptions) ?? new();
        }
        catch { Settings = new(); }
    }

    public static void Save()
    {
        Directory.CreateDirectory(Path.GetDirectoryName(_stateFile)!);
        File.WriteAllText(_stateFile, JsonSerializer.Serialize(Settings, _jsonOptions));
    }

    public static bool BepInExInstalled =>
        BepInExDir is { } b && Directory.Exists(b);

    public static void EnsureSbFolder()
    {
        if (SbDir is not { } dir || Directory.Exists(dir)) return;
        Directory.CreateDirectory(dir);
        File.WriteAllText(Path.Combine(dir, "readme.md"),
            "This folder is used by Sev's Mod Manager and Several Bees to store settings and loadouts.\nDo not delete it.\n");
    }

    public static ModLoaderKind DetectLoaderKind()
    {
        if (GameDir is not { } dir) return ModLoaderKind.BepInEx;

        if (BepInExManager.IsInstalled(dir)) return ModLoaderKind.BepInEx;
        if (MelonLoaderManager.IsInstalled(dir)) return ModLoaderKind.MelonLoader;

        bool isIl2Cpp = File.Exists(Path.Combine(dir, "GameAssembly.dll"));
        return isIl2Cpp ? ModLoaderKind.MelonLoader : ModLoaderKind.BepInEx;
    }

    public static string LoaderName(ModLoaderKind kind) =>
        kind == ModLoaderKind.BepInEx ? "BepInEx" : "MelonLoader";

    public static bool IsLoaderInstalled(ModLoaderKind kind, string gameDir) =>
        kind == ModLoaderKind.BepInEx ? BepInExManager.IsInstalled(gameDir) : MelonLoaderManager.IsInstalled(gameDir);

    public static Task InstallLoaderAsync(ModLoaderKind kind, string gameDir, IProgress<(int percent, string status)> progress) =>
        kind == ModLoaderKind.BepInEx ? BepInExManager.InstallAsync(gameDir, progress) : MelonLoaderManager.InstallAsync(gameDir, progress);

    public static bool IsExeX64(string exePath)
    {
        try
        {
            using var fs = File.OpenRead(exePath);
            using var br = new BinaryReader(fs);

            if (br.ReadUInt16() != 0x5A4D) return true;

            fs.Seek(0x3C, SeekOrigin.Begin);
            int peOffset = br.ReadInt32();

            fs.Seek(peOffset, SeekOrigin.Begin);
            if (br.ReadUInt32() != 0x00004550) return true;

            ushort machine = br.ReadUInt16();
            return machine != 0x014c;
        }
        catch { return true; }
    }

    public static string? AutoDetectGame()
    {
        foreach (var path in CurrentPreset.DefaultPaths)
            if (File.Exists(path)) return path;
        return null;
    }
}
