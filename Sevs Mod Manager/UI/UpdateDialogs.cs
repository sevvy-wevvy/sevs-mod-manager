using SevsModManager.UI.Controls;
using SevsModManager.Theme;

namespace SevsModManager.UI;

internal static class UpdateDialogs
{
    public static bool ShowUpdateAvailable(string message)
    {
        var t = ThemeEngine.Current;

        using var form = new Form
        {
            Text = "Update Available", FormBorderStyle = FormBorderStyle.FixedDialog,
            StartPosition = FormStartPosition.CenterScreen, ClientSize = new Size(360, 160),
            MaximizeBox = false, MinimizeBox = false, ShowInTaskbar = false, BackColor = t.Background,
        };

        var lbl = new Label
        {
            Text = message, Location = new Point(16, 16), Size = new Size(328, 90),
            ForeColor = t.Text, Font = new Font("Segoe UI", 9.5f),
        };

        var getBtn = new RButton { Text = "Get It", CornerRadius = 8, Width = 100, Height = 32, Location = new Point(148, 116) };
        var laterBtn = new RButton { Text = "Not Now", CornerRadius = 8, Width = 100, Height = 32, Location = new Point(252, 116) };
        ThemeEngine.StyleRButton(getBtn, accent: true);
        ThemeEngine.StyleRButton(laterBtn);

        bool getIt = false;
        getBtn.Click += (_, __) => { getIt = true; form.Close(); };
        laterBtn.Click += (_, __) => form.Close();

        form.Controls.Add(lbl);
        form.Controls.Add(getBtn);
        form.Controls.Add(laterBtn);
        form.ShowDialog();
        return getIt;
    }

    public static void ShowWhatsNew(string message)
    {
        var t = ThemeEngine.Current;

        using var form = new Form
        {
            Text = "What's New", FormBorderStyle = FormBorderStyle.FixedDialog,
            StartPosition = FormStartPosition.CenterScreen, ClientSize = new Size(380, 220),
            MaximizeBox = false, MinimizeBox = false, ShowInTaskbar = false, BackColor = t.Background,
        };

        var title = new Label
        {
            Text = "What's New in Sev's Mod Manager", Location = new Point(16, 14), AutoSize = true,
            ForeColor = t.Text, Font = new Font("Segoe UI", 11f, FontStyle.Bold),
        };
        var lbl = new Label
        {
            Text = message, Location = new Point(16, 44), Size = new Size(348, 130),
            ForeColor = t.SubText, Font = new Font("Segoe UI", 9.5f),
        };

        var okBtn = new RButton { Text = "Nice", CornerRadius = 8, Width = 100, Height = 32, Location = new Point(264, 176) };
        ThemeEngine.StyleRButton(okBtn, accent: true);
        okBtn.Click += (_, __) => form.Close();

        form.Controls.Add(title);
        form.Controls.Add(lbl);
        form.Controls.Add(okBtn);
        form.ShowDialog();
    }
}
