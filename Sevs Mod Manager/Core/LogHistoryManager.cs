namespace SevsModManager.Core;

internal record LogSession(string FilePath, DateTime Timestamp)
{
    public string DisplayName => Timestamp.ToString("MMM d, yyyy h:mm tt");
}

internal static class LogHistoryManager
{
    private const int MaxSessions = 10;
    private const string TimestampFormat = "yyyy-MM-dd_HH-mm-ss";

    public static string? HistoryDir => AppState.SbDir is { } d ? Path.Combine(d, "LogHistory") : null;

    public static List<LogSession> ListSessions()
    {
        var result = new List<LogSession>();
        if (HistoryDir is not { } dir || !Directory.Exists(dir)) return result;

        foreach (var f in Directory.GetFiles(dir, "*.log"))
        {
            string name = Path.GetFileNameWithoutExtension(f);
            if (DateTime.TryParseExact(name, TimestampFormat, null, System.Globalization.DateTimeStyles.None, out var ts))
                result.Add(new LogSession(f, ts));
        }
        return result.OrderByDescending(s => s.Timestamp).ToList();
    }

    public static void ArchiveSession(DateTime startedAt, IReadOnlyList<string> lines)
    {
        if (HistoryDir is not { } dir || lines.Count == 0) return;
        Directory.CreateDirectory(dir);

        string path = Path.Combine(dir, startedAt.ToString(TimestampFormat) + ".log");
        try { File.WriteAllLines(path, lines); }
        catch { return; }

        Prune(dir);
    }

    private static void Prune(string dir)
    {
        var files = Directory.GetFiles(dir, "*.log")
            .Select(f => new FileInfo(f))
            .OrderByDescending(f => f.LastWriteTimeUtc)
            .ToList();

        foreach (var extra in files.Skip(MaxSessions))
            try { extra.Delete(); } catch { }
    }
}
