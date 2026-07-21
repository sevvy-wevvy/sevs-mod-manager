using SevsModManager.UI;
using SevsModManager.Core;
using SevsModManager.Theme;

namespace SevsModManager;

internal static class Program
{
    internal const int CurrentVersion = 1;

    [STAThread]
    static void Main(string[] args)
    {
        ApplicationConfiguration.Initialize();
        Application.SetHighDpiMode(HighDpiMode.SystemAware);

        FileAssociation.EnsureRegistered();

        bool skipPicker = args.Any(a => a.Equals("--skip-picker", StringComparison.OrdinalIgnoreCase));

        string? pendingPackPath = args.Length > 0 && File.Exists(args[0]) &&
            Path.GetExtension(args[0]).Equals(".smmpack", StringComparison.OrdinalIgnoreCase)
            ? args[0] : null;

        AppState.Load();

        if (!skipPicker) AppState.Settings.GamePath = "";

        ThemeEngine.Apply(
            AppState.Settings.Theme,
            AppState.Settings.CustomBackground,
            AppState.Settings.CustomAccent,
            sbAccentHex: null
        );

        if (string.IsNullOrEmpty(AppState.Settings.GamePath) || !File.Exists(AppState.Settings.GamePath))
        {
            using var picker = new GamePickerForm();
            picker.ShowDialog();
            if (!picker.Confirmed)
                return;
        }

        if (AppState.Settings.Theme == ThemeMode.SyncedSB && DataBridge.HasSbFolder)
        {
            DataBridge.LoadSettings();
            ThemeEngine.Apply(ThemeMode.SyncedSB, sbAccentHex: DataBridge.GetString("Theme1"));
        }

        Application.Run(new MainForm(pendingPackPath));
    }

    internal static void RestartApp(bool skipPicker)
    {
        string exe = Environment.ProcessPath
            ?? System.Diagnostics.Process.GetCurrentProcess().MainModule?.FileName
            ?? Application.ExecutablePath;

        var psi = new System.Diagnostics.ProcessStartInfo(exe);
        if (skipPicker) psi.ArgumentList.Add("--skip-picker");
        System.Diagnostics.Process.Start(psi);
        Environment.Exit(0);
    }
}
