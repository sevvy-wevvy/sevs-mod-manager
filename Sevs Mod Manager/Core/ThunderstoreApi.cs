using System.Text.Json;
using System.Net.Http;

namespace SevsModManager.Core;

internal static class ThunderstoreApi
{
    private static readonly HttpClient _http = new() { Timeout = TimeSpan.FromSeconds(30) };
    static ThunderstoreApi() => _http.DefaultRequestHeaders.UserAgent.ParseAdd("SevsModManager/1.0");

    public static async Task<List<SbMod>> ListPackagesAsync(string community)
    {
        var result = new List<SbMod>();
        try
        {
            var json = await _http.GetStringAsync($"https://thunderstore.io/c/{Uri.EscapeDataString(community)}/api/v1/package/");
            using var doc = JsonDocument.Parse(json);
            int id = 1;
            foreach (var pkg in doc.RootElement.EnumerateArray())
            {
                if (!pkg.TryGetProperty("versions", out var versions) || versions.GetArrayLength() == 0) continue;
                var latest = versions[0];
                bool deprecated = pkg.TryGetProperty("is_deprecated", out var dep) && dep.ValueKind == JsonValueKind.True;

                var dependencies = new List<string>();
                if (latest.TryGetProperty("dependencies", out var deps) && deps.ValueKind == JsonValueKind.Array)
                    foreach (var d in deps.EnumerateArray())
                        if (d.ValueKind == JsonValueKind.String) dependencies.Add(d.GetString() ?? "");

                result.Add(new SbMod
                {
                    Id           = id++,
                    Name         = GetStr(pkg, "name"),
                    DllName      = GetStr(pkg, "full_name"),
                    Author       = GetStr(pkg, "owner"),
                    Description  = GetStr(latest, "description"),
                    RepoUrl      = GetStr(pkg, "package_url"),
                    Upvotes      = pkg.TryGetProperty("rating_score", out var r) && r.ValueKind == JsonValueKind.Number ? r.GetInt32() : 0,
                    IsFeatured   = pkg.TryGetProperty("is_pinned", out var p) && p.ValueKind == JsonValueKind.True,
                    IsVerified   = !deprecated,
                    Source       = ModSource.Thunderstore,
                    DownloadUrl  = GetStr(latest, "download_url"),
                    Dependencies = dependencies,
                });
            }
        }
        catch {  }
        return result;
    }

    public static async Task<List<ThunderstoreCommunityInfo>> ListCommunitiesAsync()
    {
        var result = new List<ThunderstoreCommunityInfo>();
        try
        {

            var json = await _http.GetStringAsync("https://thunderstore.io/api/cyberstorm/community/?page_size=1000");
            using var doc = JsonDocument.Parse(json);
            foreach (var c in doc.RootElement.GetProperty("results").EnumerateArray())
                result.Add(new ThunderstoreCommunityInfo { Name = GetStr(c, "name"), Identifier = GetStr(c, "identifier") });
        }
        catch { }
        return result.OrderBy(c => c.Name, StringComparer.OrdinalIgnoreCase).ToList();
    }

    public static async Task<string?> GuessCommunityAsync(string gameName)
    {
        var communities = await ListCommunitiesAsync();
        string target = Normalize(gameName);
        var match = communities.FirstOrDefault(c => Normalize(c.Name) == target || Normalize(c.Identifier) == target);
        return match?.Identifier;
    }

    private static string Normalize(string s) => new(s.ToLowerInvariant().Where(char.IsLetterOrDigit).ToArray());

    private static string GetStr(JsonElement e, string prop) =>
        e.TryGetProperty(prop, out var v) && v.ValueKind == JsonValueKind.String ? v.GetString() ?? "" : "";
}
