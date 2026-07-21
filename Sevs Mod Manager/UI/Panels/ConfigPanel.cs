using SevsModManager.UI.Controls;
using SevsModManager.Theme;
using SevsModManager.Core;

namespace SevsModManager.UI.Panels;

internal sealed class ConfigPanel : UserControl
{
    private readonly ListBox _fileList;
    private readonly RPanel  _leftPanel;
    private readonly Panel   _contentHost;
    private readonly Panel   _bodyWrap;
    private readonly Panel   _toolbar;
    private readonly RButton _saveBtn, _reloadBtn;
    private readonly Label   _statusLabel, _titleLabel;

    private List<(string DisplayName, string FilePath)> _files = new();
    private ModConfigFile? _current;
    private readonly List<(ConfigEntry Entry, Func<string> Read)> _liveWidgets = new();

    public ConfigPanel()
    {
        Dock = DockStyle.Fill;
        DoubleBuffered = true;

        _toolbar = new Panel { Dock = DockStyle.Top, Height = 44, Padding = new Padding(8, 0, 8, 6) };
        _saveBtn   = MakeBtn("Save");
        _reloadBtn = MakeBtn("↺ Reload");
        _saveBtn.Enabled = false;
        _saveBtn.Click   += (_, __) => DoSave();
        _reloadBtn.Click += (_, __) => Refresh_();

        var toolFlow = new FlowLayoutPanel { Dock = DockStyle.Left, AutoSize = true, FlowDirection = FlowDirection.LeftToRight, WrapContents = false };
        foreach (var b in new[] { _saveBtn, _reloadBtn }) { b.Margin = new Padding(0, 0, 6, 0); toolFlow.Controls.Add(b); }
        _toolbar.Controls.Add(toolFlow);

        _fileList = new ListBox { Dock = DockStyle.Fill, BorderStyle = BorderStyle.None };
        _fileList.SelectedIndexChanged += (_, __) => ShowSelectedFile();

        bool r2 = AppState.Settings.Layout == AppLayout.R2Modman;

        _leftPanel = new RPanel { Dock = r2 ? DockStyle.Right : DockStyle.Left, Width = 260, CornerRadius = 0, Padding = new Padding(2) };
        _leftPanel.Controls.Add(_fileList);

        _titleLabel = new Label
        {
            Dock = DockStyle.Top, Height = 30, Text = "",
            Font = new Font("Segoe UI", 12f, FontStyle.Bold), Padding = new Padding(4, 4, 0, 0),
        };

        _contentHost = new Panel { Dock = DockStyle.Fill, AutoScroll = true, Padding = new Padding(12, 4, 12, 12) };

        var detailWrap = new Panel { Dock = DockStyle.Fill };
        detailWrap.Controls.Add(_contentHost);
        detailWrap.Controls.Add(_titleLabel);

        var splitter = new Splitter { Dock = r2 ? DockStyle.Right : DockStyle.Left, Width = 1 };

        _bodyWrap = new Panel { Dock = DockStyle.Fill };
        _bodyWrap.Controls.Add(detailWrap);
        _bodyWrap.Controls.Add(splitter);
        _bodyWrap.Controls.Add(_leftPanel);

        _statusLabel = new Label { Dock = DockStyle.Bottom, Height = 22, Padding = new Padding(8, 0, 0, 0), TextAlign = ContentAlignment.MiddleLeft };
        _statusLabel.Tag = "subtext";

        Controls.Add(_bodyWrap);
        Controls.Add(_toolbar);
        Controls.Add(_statusLabel);

        ThemeEngine.ThemeChanged += ApplyTheme;
        HandleCreated += (_, __) => ThemeEngine.ApplyScrollTheme(this);
        ApplyTheme();
        Refresh_();
    }

    public void Refresh_()
    {
        if (AppState.GameDir == null) { _statusLabel.Text = "No game selected."; return; }

        string? keepPath = _current?.FilePath;
        _files = AppState.DetectLoaderKind() == ModLoaderKind.MelonLoader
            ? MelonPreferencesConfig.ListConfigFiles()
            : BepInExConfig.ListConfigFiles();

        _fileList.BeginUpdate();
        _fileList.Items.Clear();
        foreach (var f in _files) _fileList.Items.Add(f.DisplayName);
        _fileList.EndUpdate();

        _statusLabel.Text = $"{_files.Count} config files";

        int idx = keepPath != null ? _files.FindIndex(f => f.FilePath == keepPath) : -1;
        _fileList.SelectedIndex = idx >= 0 ? idx : (_files.Count > 0 ? 0 : -1);
        if (_files.Count == 0) ShowSelectedFile();
    }

    private void ShowSelectedFile()
    {
        _contentHost.Controls.Clear();
        _liveWidgets.Clear();
        _current = null;
        _saveBtn.Enabled = false;

        int idx = _fileList.SelectedIndex;
        if (idx < 0 || idx >= _files.Count)
        {
            _titleLabel.Text = "";
            if (_files.Count == 0)
            {
                var theme = ThemeEngine.Current;
                var msg = new Label
                {
                    Text = "No configs found, maybe try installing some mods and starting your game!",
                    Dock = DockStyle.Top, AutoSize = false, Height = 60,
                    Font = new Font("Segoe UI", 10f), Padding = new Padding(4, 8, 4, 0),
                    ForeColor = theme.SubText, BackColor = theme.SurfaceAlt,
                };
                _contentHost.Controls.Add(msg);
            }
            return;
        }

        var (displayName, filePath) = _files[idx];
        _titleLabel.Text = displayName;

        try
        {
            _current = AppState.DetectLoaderKind() == ModLoaderKind.MelonLoader
                ? MelonPreferencesConfig.Parse(filePath)
                : BepInExConfig.Parse(filePath);
        }
        catch (Exception ex)
        {
            _statusLabel.Text = "Failed to parse config: " + ex.Message;
            return;
        }

        var t = ThemeEngine.Current;
        for (int i = _current.Sections.Count - 1; i >= 0; i--)
            _contentHost.Controls.Add(MakeSectionGroup(_current.Sections[i], t));

        _saveBtn.Enabled = _current.Sections.Any(s => s.Entries.Count > 0);
        ThemeEngine.ApplyScrollTheme(this);
    }

    private const int RowHeight = 54;
    private const int SectionTitleHeight = 30;

    private Control MakeSectionGroup(ConfigSection section, ThemeColors t)
    {
        var group = new Panel
        {
            Dock = DockStyle.Top,
            Height = SectionTitleHeight + section.Entries.Count * RowHeight + 10,
            BackColor = t.SurfaceAlt,
        };

        for (int i = section.Entries.Count - 1; i >= 0; i--)
            group.Controls.Add(MakeEntryRow(section.Entries[i], t));

        var title = new Label
        {
            Text = section.Name, Dock = DockStyle.Top, Height = SectionTitleHeight,
            Font = new Font("Segoe UI", 10f, FontStyle.Bold), Padding = new Padding(0, 6, 0, 0),
            ForeColor = t.Text, BackColor = t.SurfaceAlt,
        };
        group.Controls.Add(title);

        return group;
    }

    private Control MakeEntryRow(ConfigEntry entry, ThemeColors t)
    {
        var row = new Panel { Dock = DockStyle.Top, Height = RowHeight, Padding = new Padding(0, 4, 0, 4), BackColor = t.SurfaceAlt };

        var label = new Label
        {
            Text = entry.Key, Dock = DockStyle.Top, Height = 18, AutoSize = false,
            Font = new Font("Segoe UI", 9.5f, FontStyle.Bold), ForeColor = t.Text, BackColor = t.SurfaceAlt,
        };
        string? desc = entry.Description.Count > 0 ? string.Join(" ", entry.Description) : null;
        if (desc != null) new ToolTip().SetToolTip(label, desc);

        row.Controls.Add(BuildWidget(entry, t));
        row.Controls.Add(label);
        return row;
    }

    private Control BuildWidget(ConfigEntry entry, ThemeColors t)
    {
        if (entry.SettingType.Equals("Boolean", StringComparison.OrdinalIgnoreCase))
        {
            var chk = new CheckBox
            {
                Dock = DockStyle.Top, Height = 26, AutoSize = false,
                Text = entry.Value,
                Checked = entry.Value.Equals("true", StringComparison.OrdinalIgnoreCase),
                ForeColor = t.Text, BackColor = t.SurfaceAlt,
            };
            chk.CheckedChanged += (_, __) => chk.Text = chk.Checked ? "true" : "false";
            _liveWidgets.Add((entry, () => chk.Checked ? "true" : "false"));
            return chk;
        }

        if (entry.Range != null)
        {
            bool isInt = entry.SettingType.Contains("Int", StringComparison.OrdinalIgnoreCase) ||
                         entry.SettingType.Equals("Byte", StringComparison.OrdinalIgnoreCase) ||
                         entry.SettingType.Equals("SByte", StringComparison.OrdinalIgnoreCase);
            var (min, max) = entry.Range.Value;

            var wrap = new Panel { Dock = DockStyle.Top, Height = 30, BackColor = t.SurfaceAlt };
            var valueLbl = new Label { Dock = DockStyle.Right, Width = 60, TextAlign = ContentAlignment.MiddleRight, ForeColor = t.Text, BackColor = t.SurfaceAlt };
            var slider = new TrackBar
            {
                Dock = DockStyle.Fill, TickStyle = TickStyle.None,
                Minimum = isInt ? (int)min : 0,
                Maximum = isInt ? (int)max : 1000,
            };

            double.TryParse(entry.Value, out double startVal);
            slider.Value = isInt
                ? Math.Clamp((int)startVal, slider.Minimum, slider.Maximum)
                : Math.Clamp((int)Math.Round((startVal - min) / (max - min) * 1000), 0, 1000);
            valueLbl.Text = entry.Value;

            slider.ValueChanged += (_, __) =>
            {
                double actual = isInt ? slider.Value : min + slider.Value / 1000.0 * (max - min);
                valueLbl.Text = isInt ? actual.ToString("0") : actual.ToString("0.###");
            };

            wrap.Controls.Add(slider);
            wrap.Controls.Add(valueLbl);

            _liveWidgets.Add((entry, () =>
            {
                double actual = isInt ? slider.Value : min + slider.Value / 1000.0 * (max - min);
                return isInt ? actual.ToString("0") : actual.ToString("0.###");
            }));
            return wrap;
        }

        if (entry.AcceptableValues is { Count: > 0 })
        {
            var dd = new RDropdown
            {
                Dock = DockStyle.Top, Height = 30, CornerRadius = 6,
                ForeColor = t.Text, FillColor = t.Surface, HoverFillColor = t.Border, BorderColor = Color.Transparent,
            };
            ThemeEngine.ApplyLayoutCornerStyle(dd);
            foreach (var v in entry.AcceptableValues) dd.Items.Add(v);
            int sel = entry.AcceptableValues.FindIndex(v => v.Equals(entry.Value, StringComparison.OrdinalIgnoreCase));
            dd.SelectedIndex = sel >= 0 ? sel : 0;
            _liveWidgets.Add((entry, () => dd.SelectedIndex >= 0 ? entry.AcceptableValues[dd.SelectedIndex] : entry.Value));
            return dd;
        }

        var txt = new RTextBox
        {
            Dock = DockStyle.Top, Height = 30, CornerRadius = 6, Text = entry.Value,
            ForeColor = t.Text, BackColor = t.Surface,
        };
        ThemeEngine.ApplyLayoutCornerStyle(txt);
        _liveWidgets.Add((entry, () => txt.Text));
        return txt;
    }

    private void DoSave()
    {
        if (_current == null) return;
        foreach (var (entry, read) in _liveWidgets) entry.Value = read();

        try
        {
            if (AppState.DetectLoaderKind() == ModLoaderKind.MelonLoader)
                MelonPreferencesConfig.Save(_current);
            else
                BepInExConfig.Save(_current);
            _statusLabel.Text = $"Saved {_current.DisplayName}.";
        }
        catch (Exception ex) { _statusLabel.Text = "Save failed: " + ex.Message; }
    }

    private void ApplyTheme()
    {
        var t = ThemeEngine.Current;
        BackColor = t.Background;
        _toolbar.BackColor = t.Surface;
        _bodyWrap.BackColor = t.Background;
        _fileList.BackColor = t.Background;
        _fileList.ForeColor = t.Text;
        _leftPanel.BackColor = t.SurfaceAlt;
        _leftPanel.BorderColor = Color.Transparent;
        _contentHost.BackColor = t.SurfaceAlt;
        _titleLabel.BackColor = t.SurfaceAlt;
        _titleLabel.ForeColor = t.Text;
        _statusLabel.BackColor = t.Background;
        _statusLabel.ForeColor = t.SubText;

        ThemeEngine.StyleRButton(_saveBtn, accent: true);
        ThemeEngine.StyleGhostButton(_reloadBtn);
        ThemeEngine.ApplyScrollTheme(this);
    }

    private static RButton MakeBtn(string text) => new()
    {
        Text = text, Style = RButtonStyle.Outline, CornerRadius = 8,
        RoundedCorners = Corners.BottomLeft | Corners.BottomRight,
        AutoSize = true, Padding = new Padding(10, 2, 10, 2), Height = 38,
    };

    protected override void Dispose(bool disposing)
    {
        if (disposing) ThemeEngine.ThemeChanged -= ApplyTheme;
        base.Dispose(disposing);
    }
}
