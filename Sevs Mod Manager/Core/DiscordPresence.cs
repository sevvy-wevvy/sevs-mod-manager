using DiscordRPC;

namespace SevsModManager.Core;

internal static class DiscordPresence
{
    private const string ClientId = "1528920416075124946";
    private static DiscordRpcClient? _client;
    private static readonly Timestamps _startedAt = Timestamps.Now;
    private static string? _lastGameName;
    private static string? _lastGamePath;

    public static void Start()
    {
        if (_client != null) return;
        try
        {
            _client = new DiscordRpcClient(ClientId);
            _client.OnReady += (_, __) =>
            {
                if (_lastGameName != null && _lastGamePath != null) SendPresence(_lastGameName, _lastGamePath);
            };
            _client.Initialize();
        }
        catch { _client = null; }
    }

    public static void Update(string gameName, string gamePath)
    {
        _lastGameName = gameName;
        _lastGamePath = gamePath;
        if (_client == null) return;
        SendPresence(gameName, gamePath);
    }

    private static void SendPresence(string gameName, string gamePath)
    {
        if (_client == null) return;

        int? appId = AppState.CurrentPreset.SteamAppId;
        if (appId == null)
        {
            var match = AppState.Settings.SteamExePaths.FirstOrDefault(kv => string.Equals(kv.Value, gamePath, StringComparison.OrdinalIgnoreCase));
            if (match.Key != null && int.TryParse(match.Key, out int id)) appId = id;
        }

        try
        {
            _client.SetPresence(new RichPresence
            {
                Details = $"Modding {gameName}",
                Assets = new Assets
                {
                    LargeImageKey = "smm_icon",
                    LargeImageText = "Sev's Mod Manager",
                    SmallImageKey = appId is { } id ? $"https://cdn.cloudflare.steamstatic.com/steam/apps/{id}/capsule_231x87.jpg" : null,
                    SmallImageText = appId != null ? gameName : null,
                },
                Buttons = new[]
                {
                    new DiscordRPC.Button { Label = "Download Sev's Mod Manager", Url = "https://github.com/sevvy-wevvy/sevs-mod-manager" },
                },
                Timestamps = _startedAt,
            });
        }
        catch { }
    }

    public static void Stop()
    {
        try { _client?.Dispose(); } catch { }
        _client = null;
    }
}
