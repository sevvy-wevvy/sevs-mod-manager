using System.Net.Http;

namespace SevsModManager.Core;

internal static class UpdateChecker
{
    private const string VersionUrl = "https://raw.githubusercontent.com/sevvy-wevvy/sevs-mod-manager/refs/heads/main/ver.txt";
    private static readonly HttpClient _http = new() { Timeout = TimeSpan.FromSeconds(10) };

    public static async Task<int?> GetLatestVersionAsync()
    {
        try
        {
            string bust = DateTime.UtcNow.ToString("yyyyMMddHHmmss");
            string text = await _http.GetStringAsync($"{VersionUrl}?date={bust}");
            return int.TryParse(text.Trim(), out int v) ? v : null;
        }
        catch { return null; }
    }
}
