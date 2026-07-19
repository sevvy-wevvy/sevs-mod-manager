using System.Text;

namespace SevsModManager.Core;

internal static class MelonPreferencesConfig
{
    private static string? FilePath =>
        AppState.GameDir is { } d ? Path.Combine(d, "UserData", "MelonPreferences.cfg") : null;

    public static List<(string DisplayName, string FilePath)> ListConfigFiles()
    {
        var result = new List<(string, string)>();
        if (FilePath is not { } path || !File.Exists(path)) return result;

        foreach (var section in ParseAllSections(path))
            result.Add((section.Name, "melon:" + section.Name));

        return result.OrderBy(x => x.Item1, StringComparer.OrdinalIgnoreCase).ToList();
    }

    public static ModConfigFile Parse(string virtualPath)
    {
        string categoryName = virtualPath.StartsWith("melon:") ? virtualPath["melon:".Length..] : virtualPath;
        var file = new ModConfigFile { FilePath = virtualPath, DisplayName = categoryName };

        if (FilePath is { } path && File.Exists(path))
        {
            var section = ParseAllSections(path).FirstOrDefault(s => s.Name == categoryName);
            if (section != null) file.Sections.Add(section);
        }
        return file;
    }

    public static void Save(ModConfigFile file)
    {
        if (FilePath is not { } path || file.Sections.Count == 0) return;

        var all = File.Exists(path) ? ParseAllSections(path) : new List<ConfigSection>();
        var edited = file.Sections[0];

        int idx = all.FindIndex(s => s.Name == edited.Name);
        if (idx >= 0) all[idx] = edited; else all.Add(edited);

        var sb = new StringBuilder();
        foreach (var section in all)
        {
            sb.Append('[').Append(section.Name).Append(']').Append('\n');
            foreach (var e in section.Entries)
            {
                if (e.Description.Count > 0) sb.Append("# ").Append(string.Join(' ', e.Description)).Append('\n');
                sb.Append(e.Key).Append(" = ").Append(FormatTomlValue(e)).Append('\n');
            }
            sb.Append('\n');
        }

        File.WriteAllText(path, sb.ToString());
    }

    private static List<ConfigSection> ParseAllSections(string path)
    {
        var sections = new List<ConfigSection>();
        ConfigSection? section = null;
        var pendingDesc = new List<string>();

        foreach (var raw in File.ReadAllLines(path))
        {
            string trimmed = raw.Trim();
            if (trimmed.Length == 0) continue;

            if (trimmed.StartsWith('[') && trimmed.EndsWith(']'))
            {
                if (section != null) sections.Add(section);
                section = new ConfigSection { Name = trimmed[1..^1] };
                pendingDesc = new List<string>();
                continue;
            }

            if (trimmed.StartsWith('#'))
            {
                pendingDesc.Add(trimmed[1..].Trim());
                continue;
            }

            int eq = trimmed.IndexOf('=');
            if (eq < 0 || section == null) continue;

            string key = trimmed[..eq].Trim();
            string rawValue = trimmed[(eq + 1)..].Trim();

            section.Entries.Add(new ConfigEntry
            {
                Key = key,
                Value = ParseTomlValue(rawValue, out string type),
                SettingType = type,
                Description = pendingDesc,
            });
            pendingDesc = new List<string>();
        }
        if (section != null) sections.Add(section);
        return sections;
    }

    private static string ParseTomlValue(string raw, out string type)
    {
        if (raw is "true" or "false") { type = "Boolean"; return raw; }

        if (raw.Length >= 2 && raw[0] == '"' && raw[^1] == '"')
        {
            type = "String";
            return raw[1..^1].Replace("\\\"", "\"").Replace("\\\\", "\\");
        }

        if (raw.StartsWith('[') && raw.EndsWith(']')) { type = "Array"; return raw; }

        if (long.TryParse(raw, out _)) { type = "Int32"; return raw; }
        if (double.TryParse(raw, out _)) { type = "Single"; return raw; }

        type = "String";
        return raw;
    }

    private static string FormatTomlValue(ConfigEntry e) => e.SettingType switch
    {
        "Boolean" => e.Value.Equals("true", StringComparison.OrdinalIgnoreCase) ? "true" : "false",
        "Int32" or "Single" => e.Value,
        "Array" => e.Value,
        _ => "\"" + e.Value.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\"",
    };
}
