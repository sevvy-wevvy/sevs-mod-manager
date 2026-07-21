namespace SevsModManager.Core;

internal enum ModSource { OurApi, Thunderstore }

internal enum ModLoaderKind { BepInEx, MelonLoader }

internal class SbMod
{
    public int Id;
    public string Name = "";
    public string DllName = "";
    public string RepoUrl = "";
    public string Description = "";
    public string Author = "";
    public int Upvotes;
    public bool IsVerified;
    public bool IsFeatured;
    public ModSource Source = ModSource.OurApi;

    public string? DownloadUrl;
    public string? IconUrl;
    public string? Version;
    public List<string> Dependencies = new();
}

internal class InstalledMod
{
    public string Name = "";
    public string FilePath = "";
    public bool Enabled;
    public string? KnownVersion;
    public string? LatestVersion;
    public bool IsFolder;
    public long SizeBytes;
    public bool UpdateAvailable => KnownVersion is { Length: > 0 } && LatestVersion is { Length: > 0 } && KnownVersion != LatestVersion;
}

internal class ThunderstoreCommunityInfo
{
    public string Name = "";
    public string Identifier = "";

    public override string ToString() => Name;
}

internal class ModLoadout
{
    public int Number;
    public string Name = "";
    public List<string> EnabledMods = new();
    public List<string> DisabledMods = new();
    public List<string> MissingMods = new();

    public string DisplayName => string.IsNullOrWhiteSpace(Name) ? $"Loadout {Number}" : Name;
}

internal class ModpackManifest
{
    public string Name { get; set; } = "";
    public string Author { get; set; } = "";
    public string GameSlug { get; set; } = "";
    public DateTime CreatedUtc { get; set; }
    public List<string> Mods { get; set; } = new();
}

internal class ModpackInfo
{
    public string FilePath = "";
    public ModpackManifest Manifest = new();
    public string DisplayName => string.IsNullOrWhiteSpace(Manifest.Name) ? Path.GetFileNameWithoutExtension(FilePath) : Manifest.Name;
}

internal enum ThemeMode { Black, White, Custom, SyncedSB, R2Modman }

internal enum AppLayout { SevsModManager, R2Modman, MonkeModManager }

internal enum GamePickerDisplay { Both, Text, Icons }

internal class AppSettings
{
    public string GamePath = "";
    public string GameName = "Gorilla Tag";
    public ThemeMode Theme = ThemeMode.Black;
    public string CustomBackground = "#141414";
    public string CustomAccent = "#7C3AED";
    public bool AutoCheckUpdates = true;
    public int AppVersionSeen;
    public AppLayout Layout = AppLayout.SevsModManager;
    public GamePickerDisplay GamePickerDisplay = GamePickerDisplay.Both;

    public Dictionary<string, string> KnownGamePaths = new();

    public Dictionary<string, string> CustomGames = new();

    public Dictionary<string, string> ThunderstoreCommunities = new();

    public HashSet<string> HiddenPresets = new();

    public HashSet<string> ManualOnlyGames = new();

    public Dictionary<string, string> SteamExePaths = new();
}

internal record GamePreset(string Name, string Slug, string[] DefaultPaths, string? DefaultThunderstoreCommunity = null, int? SteamAppId = null);
