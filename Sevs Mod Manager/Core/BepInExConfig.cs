using System.Text;

namespace SevsModManager.Core;

internal sealed class ConfigEntry
{
    public string Key = "";
    public string Value = "";
    public List<string> Description = new();
    public string SettingType = "String";
    public string? DefaultValue;
    public (double Min, double Max)? Range;
    public List<string>? AcceptableValues;
}

internal sealed class ConfigSection
{
    public string Name = "";
    public List<ConfigEntry> Entries = new();
}

internal sealed class ModConfigFile
{
    public string FilePath = "";
    public string DisplayName = "";
    public List<string> Header = new();
    public List<ConfigSection> Sections = new();
}

internal static class BepInExConfig
{
    public static List<(string DisplayName, string FilePath)> ListConfigFiles()
    {
        var result = new List<(string, string)>();
        string? bepDir = AppState.BepInExDir;
        if (bepDir == null) return result;

        string configDir = Path.Combine(bepDir, "config");
        if (!Directory.Exists(configDir)) return result;

        foreach (var f in Directory.GetFiles(configDir, "*.cfg", SearchOption.TopDirectoryOnly))
            result.Add((Path.GetFileNameWithoutExtension(f), f));

        return result.OrderBy(x => x.Item1, StringComparer.OrdinalIgnoreCase).ToList();
    }

    public static ModConfigFile Parse(string path)
    {
        var file = new ModConfigFile { FilePath = path, DisplayName = Path.GetFileNameWithoutExtension(path) };
        var lines = File.ReadAllLines(path);

        ConfigSection? section = null;
        var pendingDesc = new List<string>();
        string? pendingType = null;
        string? pendingDefault = null;
        (double, double)? pendingRange = null;
        List<string>? pendingValues = null;
        bool sawSection = false;

        void ClearPending()
        {
            pendingDesc = new List<string>();
            pendingType = null;
            pendingDefault = null;
            pendingRange = null;
            pendingValues = null;
        }

        foreach (var raw in lines)
        {
            string line = raw.TrimEnd();
            string trimmed = line.Trim();

            if (trimmed.Length == 0) continue;

            if (trimmed.StartsWith('[') && trimmed.EndsWith(']'))
            {
                if (section != null) file.Sections.Add(section);
                section = new ConfigSection { Name = trimmed[1..^1] };
                sawSection = true;
                ClearPending();
                continue;
            }

            if (trimmed.StartsWith("## "))
            {
                if (!sawSection) file.Header.Add(trimmed[3..]);
                else pendingDesc.Add(trimmed[3..]);
                continue;
            }

            if (trimmed.StartsWith("# Setting type:"))
            {
                pendingType = trimmed["# Setting type:".Length..].Trim();
                continue;
            }

            if (trimmed.StartsWith("# Default value:"))
            {
                pendingDefault = trimmed["# Default value:".Length..].Trim();
                continue;
            }

            if (trimmed.StartsWith("# Acceptable value range:"))
            {
                var text = trimmed["# Acceptable value range:".Length..].Trim();
                var tokens = text.Split(' ', StringSplitOptions.RemoveEmptyEntries);
                if (tokens.Length == 4 && double.TryParse(tokens[1], out var min) && double.TryParse(tokens[3], out var max))
                    pendingRange = (min, max);
                continue;
            }

            if (trimmed.StartsWith("# Acceptable values:"))
            {
                var text = trimmed["# Acceptable values:".Length..].Trim();
                pendingValues = text.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries).ToList();
                continue;
            }

            if (trimmed.StartsWith('#')) continue;

            int eq = trimmed.IndexOf('=');
            if (eq < 0 || section == null) continue;

            var entry = new ConfigEntry
            {
                Key = trimmed[..eq].Trim(),
                Value = trimmed[(eq + 1)..].Trim(),
                Description = pendingDesc,
                SettingType = pendingType ?? "String",
                DefaultValue = pendingDefault,
                Range = pendingRange,
                AcceptableValues = pendingValues,
            };
            section.Entries.Add(entry);
            ClearPending();
        }

        if (section != null) file.Sections.Add(section);
        return file;
    }

    public static void Save(ModConfigFile file)
    {
        var sb = new StringBuilder();
        foreach (var h in file.Header) sb.Append("## ").Append(h).Append('\n');
        if (file.Header.Count > 0) sb.Append('\n');

        foreach (var section in file.Sections)
        {
            sb.Append('[').Append(section.Name).Append(']').Append('\n').Append('\n');

            foreach (var e in section.Entries)
            {
                foreach (var d in e.Description) sb.Append("## ").Append(d).Append('\n');
                sb.Append("# Setting type: ").Append(e.SettingType).Append('\n');
                if (e.DefaultValue != null) sb.Append("# Default value: ").Append(e.DefaultValue).Append('\n');
                if (e.Range != null) sb.Append("# Acceptable value range: From ").Append(e.Range.Value.Min).Append(" to ").Append(e.Range.Value.Max).Append('\n');
                if (e.AcceptableValues != null) sb.Append("# Acceptable values: ").Append(string.Join(", ", e.AcceptableValues)).Append('\n');
                sb.Append(e.Key).Append(" = ").Append(e.Value).Append('\n').Append('\n');
            }
        }

        File.WriteAllText(file.FilePath, sb.ToString());
    }
}
