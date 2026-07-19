using System.Text;
using System.Globalization;

namespace SevsModManager.Core;

internal static class DataBridge
{
    private static string SettingsDir  => Path.Combine(AppState.SbDir ?? "", "settings");
    private static string LoadoutsDir  => Path.Combine(AppState.SbDir ?? "", "loadouts");
    private static string SettingsPath => Path.Combine(SettingsDir, "settings.json");
    private static string AuthPath     => Path.Combine(SettingsDir, "auth.json");
    private static string PluginVerPath=> Path.Combine(SettingsDir, "plugin_versions.json");
    private static string ModVerPath   => Path.Combine(SettingsDir, "mod_versions.json");
    public static  string LoadoutsPath => Path.Combine(LoadoutsDir, "loadouts.json");

    private static Dictionary<string, string> _cfg = new();

    public static bool HasSbFolder => AppState.SbDir is { } d && Directory.Exists(d);

    public static void LoadSettings()
    {
        _cfg = File.Exists(SettingsPath) ? LoadFlat(SettingsPath) : new();
    }

    public static bool GetBool(string key, bool def)
    {
        if (!_cfg.TryGetValue(key, out var v)) return def;
        return v == "true" ? true : v == "false" ? false : def;
    }

    public static void SetBool(string key, bool val) { _cfg[key] = val ? "true" : "false"; FlushSettings(); }

    public static float GetFloat(string key, float def)
    {
        if (!_cfg.TryGetValue(key, out var v)) return def;
        return float.TryParse(v, NumberStyles.Float, CultureInfo.InvariantCulture, out var r) ? r : def;
    }

    public static void SetFloat(string key, float val) { _cfg[key] = val.ToString(CultureInfo.InvariantCulture); FlushSettings(); }

    public static string? GetString(string key) => _cfg.TryGetValue(key, out var v) ? v : null;
    public static string GetString(string key, string def) => _cfg.TryGetValue(key, out var v) ? v : def;

    public static void SetString(string key, string val) { _cfg[key] = val; FlushSettings(); }

    public static string? LoadToken()
    {
        if (!File.Exists(AuthPath)) return null;
        try { return ParseStr(File.ReadAllText(AuthPath), "token"); } catch { return null; }
    }

    public static void SaveToken(string token)
    {
        EnsureDir(SettingsDir);
        File.WriteAllText(AuthPath, $"{{\"token\":\"{Esc(token)}\"}}");
    }

    public static void DeleteToken() { if (File.Exists(AuthPath)) File.Delete(AuthPath); }

    public static string GetModVersion(string repoUrl)
    {
        var d = LoadKv(ModVerPath);
        return d.TryGetValue(repoUrl, out var v) ? v : "";
    }

    public static void SetModVersion(string repoUrl, string tag)
    {
        var d = LoadKv(ModVerPath); d[repoUrl] = tag; SaveKv(ModVerPath, d);
    }

    public static void DeleteModVersion(string repoUrl)
    {
        var d = LoadKv(ModVerPath); if (d.Remove(repoUrl)) SaveKv(ModVerPath, d);
    }

    public static List<ModLoadout> LoadLoadouts()
    {
        if (!File.Exists(LoadoutsPath)) return new();
        try { return ParseLoadoutsJson(File.ReadAllText(LoadoutsPath)); }
        catch { return new(); }
    }

    public static void SaveLoadouts(List<ModLoadout> loadouts)
    {
        EnsureDir(LoadoutsDir);
        var sb = new StringBuilder("[");
        for (int i = 0; i < loadouts.Count; i++)
        {
            var l = loadouts[i];
            sb.Append('{');
            sb.Append($"\"Number\":{l.Number},");
            if (!string.IsNullOrEmpty(l.Name)) sb.Append($"\"Name\":\"{Esc(l.Name)}\",");
            sb.Append($"\"EnabledMods\":[{string.Join(",", l.EnabledMods.Select(m => $"\"{Esc(m)}\""  ))}],");
            sb.Append($"\"DisabledMods\":[{string.Join(",", l.DisabledMods.Select(m => $"\"{Esc(m)}\""  ))}],");
            sb.Append($"\"MissingMods\":[{string.Join(",", l.MissingMods.Select(m => $"\"{Esc(m)}\""))}]");
            sb.Append('}');
            if (i < loadouts.Count - 1) sb.Append(',');
        }
        sb.Append(']');
        File.WriteAllText(LoadoutsPath, sb.ToString());
    }

    private static void FlushSettings()
    {
        EnsureDir(SettingsDir);
        var sb = new StringBuilder("{");
        bool first = true;
        foreach (var kv in _cfg)
        {
            if (!first) sb.Append(',');
            bool isBool = kv.Value is "true" or "false";
            bool isNum = !isBool && float.TryParse(kv.Value, NumberStyles.Float, CultureInfo.InvariantCulture, out _);
            if (isBool || isNum) sb.Append($"\"{Esc(kv.Key)}\":{kv.Value}");
            else                 sb.Append($"\"{Esc(kv.Key)}\":\"{Esc(kv.Value)}\"");
            first = false;
        }
        sb.Append('}');
        File.WriteAllText(SettingsPath, sb.ToString());
    }

    private static Dictionary<string, string> LoadFlat(string path)
    {
        var d = new Dictionary<string, string>();
        if (!File.Exists(path)) return d;
        try
        {
            string json = File.ReadAllText(path).Trim();
            int i = json.IndexOf('{') + 1;
            while (i < json.Length)
            {
                while (i < json.Length && json[i] != '"' && json[i] != '}') i++;
                if (i >= json.Length || json[i] == '}') break;
                string? key = ReadStr(json, ref i);
                while (i < json.Length && json[i] != ':') i++;
                i++;
                while (i < json.Length && json[i] == ' ') i++;
                string val;
                if (i < json.Length && json[i] == '"')
                {
                    val = ReadStr(json, ref i) ?? "";
                }
                else
                {
                    int end = i;
                    while (end < json.Length && json[end] != ',' && json[end] != '}') end++;
                    val = json.Substring(i, end - i).Trim();
                    i = end;
                }
                if (key != null) d[key] = val;
            }
        }
        catch { }
        return d;
    }

    private static Dictionary<string, string> LoadKv(string path)
    {
        var d = new Dictionary<string, string>();
        if (!File.Exists(path)) return d;
        try
        {
            string json = File.ReadAllText(path).Trim();
            int i = json.IndexOf('{') + 1;
            while (i < json.Length)
            {
                while (i < json.Length && json[i] != '"' && json[i] != '}') i++;
                if (i >= json.Length || json[i] == '}') break;
                string? key = ReadStr(json, ref i);
                while (i < json.Length && json[i] != ':') i++;
                i++;
                while (i < json.Length && json[i] == ' ') i++;
                string? val = ReadStr(json, ref i);
                if (key != null && val != null) d[key] = val;
            }
        }
        catch { }
        return d;
    }

    private static void SaveKv(string path, Dictionary<string, string> dict)
    {
        EnsureDir(Path.GetDirectoryName(path)!);
        var sb = new StringBuilder("{");
        bool first = true;
        foreach (var kv in dict)
        {
            if (!first) sb.Append(',');
            sb.Append($"\"{Esc(kv.Key)}\":\"{Esc(kv.Value)}\"");
            first = false;
        }
        sb.Append('}');
        File.WriteAllText(path, sb.ToString());
    }

    private static List<ModLoadout> ParseLoadoutsJson(string json)
    {
        var result = new List<ModLoadout>();
        json = json.Trim();
        if (!json.StartsWith('[')) return result;

        int depth = 0, objStart = -1;
        var objects = new List<string>();
        for (int i = 0; i < json.Length; i++)
        {
            if (json[i] == '{') { if (depth == 0) objStart = i; depth++; }
            else if (json[i] == '}') { depth--; if (depth == 0 && objStart >= 0) { objects.Add(json.Substring(objStart, i - objStart + 1)); objStart = -1; } }
        }

        foreach (var obj in objects)
        {
            result.Add(new ModLoadout
            {
                Number      = ParseInt(obj, "Number"),
                Name        = ParseStr(obj, "Name") ?? "",
                EnabledMods = ParseStrArray(obj, "EnabledMods"),
                DisabledMods= ParseStrArray(obj, "DisabledMods"),
                MissingMods = ParseStrArray(obj, "MissingMods"),
            });
        }
        return result;
    }

    private static int ParseInt(string json, string key)
    {
        string search = $"\"{key}\":";
        int idx = json.IndexOf(search);
        if (idx < 0) return 0;
        int start = idx + search.Length;
        int end = start;
        while (end < json.Length && (char.IsDigit(json[end]) || json[end] == '-')) end++;
        int.TryParse(json.Substring(start, end - start), out int val);
        return val;
    }

    private static List<string> ParseStrArray(string json, string key)
    {
        var result = new List<string>();
        string search = $"\"{key}\":[";
        int idx = json.IndexOf(search);
        if (idx < 0) return result;
        int start = idx + search.Length;
        int end = json.IndexOf(']', start);
        if (end < 0) return result;
        string inner = json.Substring(start, end - start).Trim();
        if (string.IsNullOrEmpty(inner)) return result;

        bool inStr = false;
        var cur = new StringBuilder();
        for (int i = 0; i < inner.Length; i++)
        {
            char c = inner[i];
            if (c == '"' && (i == 0 || inner[i - 1] != '\\'))
            {
                inStr = !inStr;
                if (!inStr && cur.Length > 0) { result.Add(cur.ToString()); cur.Clear(); }
                continue;
            }
            if (inStr) cur.Append(c);
        }
        return result;
    }

    private static string? ReadStr(string json, ref int i)
    {
        if (i >= json.Length || json[i] != '"') return null;
        i++;
        var sb = new StringBuilder();
        while (i < json.Length)
        {
            char c = json[i++];
            if (c == '\\' && i < json.Length) { sb.Append(json[i++]); continue; }
            if (c == '"') break;
            sb.Append(c);
        }
        return sb.ToString();
    }

    private static string? ParseStr(string json, string key)
    {
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

    private static void EnsureDir(string path) { if (!Directory.Exists(path)) Directory.CreateDirectory(path); }
    private static string Esc(string s) => s.Replace("\\", "\\\\").Replace("\"", "\\\"");
}
