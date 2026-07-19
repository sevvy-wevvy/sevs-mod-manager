using SevsModManager.UI.Controls;
using SevsModManager.Theme;

namespace SevsModManager.UI;

internal static class PromptDialog
{
    public static string? Show(string title, string label, string defaultValue = "")
    {
        var t = ThemeEngine.Current;

        using var form = new Form
        {
            Text = title,
            FormBorderStyle = FormBorderStyle.FixedDialog,
            StartPosition = FormStartPosition.CenterParent,
            ClientSize = new Size(340, 130),
            MaximizeBox = false,
            MinimizeBox = false,
            ShowInTaskbar = false,
            BackColor = t.Background,
        };

        var lbl = new Label { Text = label, AutoSize = true, Location = new Point(16, 16), ForeColor = t.Text };
        var box = new TextBox
        {
            Location = new Point(16, 40), Width = 300, Text = defaultValue,
            BackColor = t.SurfaceAlt, ForeColor = t.Text, BorderStyle = BorderStyle.FixedSingle,
        };

        var ok = new RButton
        {
            Text = "OK", CornerRadius = 6, Width = 75, Height = 30, Location = new Point(160, 82),
        };
        var cancel = new RButton
        {
            Text = "Cancel", CornerRadius = 6, Width = 75, Height = 30, Location = new Point(241, 82),
        };
        ThemeEngine.StyleRButton(ok, accent: true);
        ThemeEngine.StyleRButton(cancel);

        DialogResult result = DialogResult.Cancel;
        ok.Click     += (_, __) => { result = DialogResult.OK; form.Close(); };
        cancel.Click  += (_, __) => { result = DialogResult.Cancel; form.Close(); };
        box.KeyDown  += (_, e) =>
        {
            if (e.KeyCode == Keys.Enter)  { e.SuppressKeyPress = true; result = DialogResult.OK; form.Close(); }
            if (e.KeyCode == Keys.Escape) { e.SuppressKeyPress = true; result = DialogResult.Cancel; form.Close(); }
        };

        form.Controls.Add(lbl);
        form.Controls.Add(box);
        form.Controls.Add(ok);
        form.Controls.Add(cancel);
        form.Shown += (_, __) => { box.Focus(); box.SelectAll(); };

        form.ShowDialog();
        return result == DialogResult.OK ? box.Text.Trim() : null;
    }
}
