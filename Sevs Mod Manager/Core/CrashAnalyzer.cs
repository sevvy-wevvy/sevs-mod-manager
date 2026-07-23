namespace SevsModManager.Core;

internal record CrashSuspect(string ModName, string Reason);

internal static class CrashAnalyzer
{
    private static readonly string[] ExceptionMarkers = { "Exception", "Error", "Fatal" };
    private const int StackTraceWindow = 40;

    public static CrashSuspect? Analyze(IReadOnlyList<string> lines)
    {
        var installedNames = ModInstaller.GetInstalled()
            .Select(m => m.Name)
            .Where(n => n.Length >= 3)
            .ToList();
        if (installedNames.Count == 0) return null;

        int exceptionAt = -1;
        for (int i = 0; i < lines.Count; i++)
        {
            if (ExceptionMarkers.Any(marker => lines[i].Contains(marker, StringComparison.OrdinalIgnoreCase)))
            { exceptionAt = i; break; }
        }
        if (exceptionAt < 0) return null;

        int windowEnd = Math.Min(lines.Count, exceptionAt + StackTraceWindow);
        for (int i = exceptionAt; i < windowEnd; i++)
        {
            foreach (var mod in installedNames)
            {
                if (lines[i].Contains(mod, StringComparison.OrdinalIgnoreCase))
                    return new CrashSuspect(mod, $"\"{mod}\" appears in the stack trace near a reported error/exception. This is a best-effort guess, not a certainty.");
            }
        }
        return null;
    }
}
