using Microsoft.Win32;
using System.Runtime.InteropServices;

namespace SevsModManager.Core;

internal static class FileAssociation
{
    private const string ProgId = "SevsModManager.smmpack";
    private const string Ext = ".smmpack";

    public static void EnsureRegistered()
    {
        try
        {
            string exePath = Environment.ProcessPath
                ?? System.Diagnostics.Process.GetCurrentProcess().MainModule?.FileName
                ?? "";
            if (exePath.Length == 0) return;

            string desiredCommand = $"\"{exePath}\" \"%1\"";

            using var cmdKey = Registry.CurrentUser.CreateSubKey($@"Software\Classes\{ProgId}\shell\open\command");
            if (cmdKey.GetValue("") as string == desiredCommand) return;

            using var extKey = Registry.CurrentUser.CreateSubKey($@"Software\Classes\{Ext}");
            extKey.SetValue("", ProgId);

            using var progIdKey = Registry.CurrentUser.CreateSubKey($@"Software\Classes\{ProgId}");
            progIdKey.SetValue("", "Sev's Mod Manager Pack");

            using var iconKey = Registry.CurrentUser.CreateSubKey($@"Software\Classes\{ProgId}\DefaultIcon");
            iconKey.SetValue("", $"\"{exePath}\",0");

            cmdKey.SetValue("", desiredCommand);

            SHChangeNotify(0x08000000, 0x0000, IntPtr.Zero, IntPtr.Zero);
        }
        catch {  }
    }

    [DllImport("shell32.dll")]
    private static extern void SHChangeNotify(int wEventId, int uFlags, IntPtr dwItem1, IntPtr dwItem2);
}
