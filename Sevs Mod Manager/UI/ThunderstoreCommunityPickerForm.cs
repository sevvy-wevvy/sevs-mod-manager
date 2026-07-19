using SevsModManager.UI.Controls;
using SevsModManager.Core;
using SevsModManager.Theme;

namespace SevsModManager.UI;

internal sealed class ThunderstoreCommunityPickerForm : Form
{
    public ThunderstoreCommunityInfo? Selected { get; private set; }

    private readonly List<ThunderstoreCommunityInfo> _all;
    private readonly RTextBox _search;
    private readonly ListBox _list;

    public ThunderstoreCommunityPickerForm(List<ThunderstoreCommunityInfo> communities)
    {
        _all = communities;
        Icon = AppIcons.Icon;
        Text = "Choose Thunderstore Community";
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;
        MinimizeBox = false;
        StartPosition = FormStartPosition.CenterParent;
        ClientSize = new Size(420, 470);

        var t = ThemeEngine.Current;
        BackColor = t.Background;

        var label = new Label
        {
            Text = "Search for your game's Thunderstore community:",
            AutoSize = true, Location = new Point(16, 14),
            ForeColor = t.Text, BackColor = t.Background,
        };

        _search = new RTextBox
        {
            PlaceholderText = "Search communities...",
            Location = new Point(16, 40), Width = 388, Height = 30,
            BackColor = t.SurfaceAlt, ForeColor = t.Text, CornerRadius = 6,
        };
        _search.TextChanged += (_, __) => Filter();

        _list = new ListBox
        {
            Location = new Point(16, 80), Width = 388, Height = 320,
            BackColor = t.Surface, ForeColor = t.Text, BorderStyle = BorderStyle.FixedSingle,
        };
        _list.DoubleClick += (_, __) => Confirm();

        var okBtn = new RButton { Text = "Select", Width = 100, Height = 32, Location = new Point(200, 420) };
        var cancelBtn = new RButton { Text = "Cancel", Width = 100, Height = 32, Location = new Point(304, 420) };
        ThemeEngine.StyleRButton(okBtn, accent: true);
        ThemeEngine.StyleRButton(cancelBtn);
        okBtn.Click += (_, __) => Confirm();
        cancelBtn.Click += (_, __) => { DialogResult = DialogResult.Cancel; Close(); };

        Controls.Add(label);
        Controls.Add(_search);
        Controls.Add(_list);
        Controls.Add(okBtn);
        Controls.Add(cancelBtn);

        HandleCreated += (_, __) => ThemeEngine.ApplyScrollTheme(this);
        Filter();
    }

    private void Filter()
    {
        string q = _search.Text.Trim().ToLowerInvariant();
        var filtered = string.IsNullOrEmpty(q)
            ? _all
            : _all.Where(c => c.Name.ToLowerInvariant().Contains(q) || c.Identifier.ToLowerInvariant().Contains(q)).ToList();

        _list.BeginUpdate();
        _list.Items.Clear();
        foreach (var c in filtered) _list.Items.Add(c);
        _list.EndUpdate();
    }

    private void Confirm()
    {
        if (_list.SelectedItem is ThunderstoreCommunityInfo c)
        {
            Selected = c;
            DialogResult = DialogResult.OK;
            Close();
        }
    }
}
