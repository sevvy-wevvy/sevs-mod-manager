using System.Text;
using System.Net.Http.Headers;
using System.Net.Http;

namespace SevsModManager.Core;

internal static class SbApi
{
    private const string ApiBase  = "https://sevvy-wevvy.com/mods/sb/api.php";
    private const string LinkBase = "https://3gv.org/link/";

    private static readonly HttpClient _http = new(new HttpClientHandler { AllowAutoRedirect = true })
    {
        Timeout = TimeSpan.FromSeconds(15),
    };

    static SbApi() => _http.DefaultRequestHeaders.UserAgent.ParseAdd("SevsModManager/1.0");

    public static string? Token { get; set; }

    public static async Task<int> RegisterGameAsync(string slug)
    {
        var res = await GetAsync("register_game", new() { ["slug"] = slug });
        return JsonInt(res, "id");
    }

    public static async Task<List<SbMod>> ListModsAsync(string tab, int page, int gameId = 0)
    {
        var q = new Dictionary<string, string> { ["tab"] = tab, ["page"] = page.ToString() };
        if (gameId > 0) q["game_id"] = gameId.ToString();
        var json = await GetAsync("list_mods", q);
        return ParseModList(json);
    }

    public static async Task<HashSet<int>> GetUserUpvotesAsync(List<SbMod> mods)
    {
        var set = new HashSet<int>();
        if (string.IsNullOrEmpty(Token)) return set;
        var tasks = mods.Select(async m =>
        {
            var res = await GetAsync("get_user_upvote", new() { ["mod_id"] = m.Id.ToString() });
            if (JsonBool(res, "upvoted")) set.Add(m.Id);
        });
        await Task.WhenAll(tasks);
        return set;
    }

    public static async Task<bool> ToggleUpvoteAsync(int modId)
    {
        var res = await PostAsync("toggle_upvote", new() { ["mod_id"] = modId.ToString() });
        return JsonBool(res, "upvoted");
    }

    public static async Task<bool> UrlExistsAsync(string? url)
    {
        if (string.IsNullOrEmpty(url)) return true;
        try
        {
            using var req = new HttpRequestMessage(HttpMethod.Get, url);
            using var res = await _http.SendAsync(req, HttpCompletionOption.ResponseHeadersRead);
            return res.StatusCode != System.Net.HttpStatusCode.NotFound;
        }
        catch { return true; }
    }

    public static async Task<string?> ValidateTokenAsync(string token)
    {
        var res = await PostAsync("validate_token", new() { ["token"] = token });
        return res != null && JsonBool(res, "valid") ? JsonStr(res, "username") : null;
    }

    public static async Task<string?> GenerateLoginCodeAsync()
    {
        try
        {
            var res = await _http.GetStringAsync(LinkBase + "?action=generate");
            return JsonStr(res, "code");
        }
        catch { return null; }
    }

    public static async Task<string?> PollLoginCodeAsync(string code)
    {
        try
        {
            var res = await _http.GetStringAsync($"{LinkBase}?action=status&code={Uri.EscapeDataString(code)}");
            return JsonStr(res, "token");
        }
        catch { return null; }
    }

    public static async Task<string?> GetLatestTagAsync(string owner, string repo, int assetIndex = 0)
    {
        try
        {
            _http.DefaultRequestHeaders.Accept.Clear();
            _http.DefaultRequestHeaders.Accept.Add(new MediaTypeWithQualityHeaderValue("application/vnd.github+json"));
            var json = await _http.GetStringAsync($"https://api.github.com/repos/{owner}/{repo}/releases/latest");
            _http.DefaultRequestHeaders.Accept.Clear();
            return JsonStr(json, "tag_name");
        }
        catch { return null; }
    }

    private static async Task<string?> GetAsync(string action, Dictionary<string, string>? query = null)
    {
        try
        {
            var url = ApiBase + "?action=" + Uri.EscapeDataString(action);
            if (query != null)
                foreach (var kv in query)
                    url += $"&{Uri.EscapeDataString(kv.Key)}={Uri.EscapeDataString(kv.Value)}";
            if (!string.IsNullOrEmpty(Token))
                url += $"&token={Uri.EscapeDataString(Token)}";
            return await _http.GetStringAsync(url);
        }
        catch { return null; }
    }

    private static async Task<string?> PostAsync(string action, Dictionary<string, string> fields)
    {
        try
        {
            var data = new Dictionary<string, string>(fields) { ["action"] = action };
            if (!string.IsNullOrEmpty(Token)) data["token"] = Token!;
            var res = await _http.PostAsync(ApiBase, new FormUrlEncodedContent(data));
            return await res.Content.ReadAsStringAsync();
        }
        catch { return null; }
    }

    private static string? JsonStr(string? json, string key)
    {
        if (json == null) return null;
        string search = $"\"{key}\":\"";
        int idx = json.IndexOf(search);
        if (idx < 0) return null;
        int start = idx + search.Length;
        var sb = new StringBuilder();
        for (int i = start; i < json.Length; i++)
        {
            if (json[i] == '\\' && i + 1 < json.Length) { sb.Append(json[++i]); continue; }
            if (json[i] == '"') break;
            sb.Append(json[i]);
        }
        return sb.ToString();
    }

    private static bool JsonBool(string? json, string key)
    {
        if (json == null) return false;
        string search = $"\"{key}\":";
        int idx = json.IndexOf(search);
        if (idx < 0) return false;
        return json.Substring(idx + search.Length).TrimStart().StartsWith("true");
    }

    private static int JsonInt(string? json, string key)
    {
        if (json == null) return 0;
        string search = $"\"{key}\":";
        int idx = json.IndexOf(search);
        if (idx < 0) return 0;
        string tail = json.Substring(idx + search.Length).TrimStart();
        int i = 0;
        while (i < tail.Length && (char.IsDigit(tail[i]) || tail[i] == '-')) i++;
        int.TryParse(tail[..i], out int val);
        return val;
    }

    private static List<SbMod> ParseModList(string? json)
    {
        var result = new List<SbMod>();
        if (json == null) return result;
        int modsStart = json.IndexOf("\"mods\":[");
        if (modsStart >= 0) json = json[(modsStart + 8)..];

        int depth = 0, objStart = -1;
        for (int i = 0; i < json.Length; i++)
        {
            if (json[i] == '{') { if (depth == 0) objStart = i; depth++; }
            else if (json[i] == '}')
            {
                depth--;
                if (depth == 0 && objStart >= 0)
                {
                    string obj = json.Substring(objStart, i - objStart + 1);
                    result.Add(new SbMod
                    {
                        Id          = JsonInt(obj, "id"),
                        Name        = JsonStr(obj, "name")            ?? "",
                        DllName     = JsonStr(obj, "dll_name")        ?? "",
                        RepoUrl     = JsonStr(obj, "repo_url")        ?? "",
                        Description = JsonStr(obj, "description")     ?? "",
                        Author      = JsonStr(obj, "author_username") ?? "",
                        Upvotes     = JsonInt(obj, "upvotes"),
                        IsVerified  = JsonInt(obj, "is_verified") == 1,
                        IsFeatured  = JsonInt(obj, "is_featured") == 1,
                    });
                    objStart = -1;
                }
            }
        }
        return result;
    }

}
