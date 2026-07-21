using SevsModManager.Theme;
using SevsModManager.Core;
using System.Drawing.Drawing2D;
using SevsModManager.UI.Controls;

namespace SevsModManager.UI.Panels;

internal sealed class InstalledPanel : UserControl
{
    private List<InstalledMod> _mods = new();
    private List<InstalledMod> _filtered = new();
    private Dictionary<string, SbMod> _latestByKey = new(StringComparer.OrdinalIgnoreCase);
    private List<string> _conflicts = new();

    private readonly ListView  _list;
    private readonly RButton   _refreshBtn, _updateAllBtn, _conflictsBtn;
    private readonly RButton   _openFolderBtn, _enableAllBtn, _disableAllBtn, _uninstallAllBtn;
    private readonly RTextBox  _search;
    private readonly Label     _statusLabel;
    private readonly Panel     _toolbar;
    private readonly Panel     _bodyWrap;
    private readonly RPanel    _listCard;

    public InstalledPanel()
    {
        Dock = DockStyle.Fill;
        DoubleBuffered = true;

        _toolbar = new Panel { Dock = DockStyle.Top, Height = 44, Padding = new Padding(8, 0, 8, 6) };

        _refreshBtn     = MakeBtn("↺ Refresh");
        _updateAllBtn   = MakeBtn("Update All");
        _conflictsBtn   = MakeBtn("⚠ Conflicts");
        _openFolderBtn  = MakeBtn("Open Plugins ↗");
        _enableAllBtn   = MakeBtn("Enable All Mods");
        _disableAllBtn  = MakeBtn("Disable All Mods");
        _uninstallAllBtn= MakeBtn("Uninstall All");
        _updateAllBtn.Visible = false;
        _conflictsBtn.Visible = false;

        _refreshBtn.Click     += (_, __) => Refresh_();
        _updateAllBtn.Click   += async (_, __) => await DoUpdateAll();
        _conflictsBtn.Click   += (_, __) => ShowConflicts();
        _openFolderBtn.Click  += (_, __) => OpenFolder();
        _enableAllBtn.Click   += (_, __) => DoEnableAll();
        _disableAllBtn.Click  += (_, __) => DoDisableAll();
        _uninstallAllBtn.Click+= (_, __) => DoUninstallAll();

        _search = new RTextBox
        {
            PlaceholderText = "Search installed...",
            Width = 180, Height = 38, CornerRadius = 8,
            RoundedCorners = Corners.BottomLeft | Corners.BottomRight,
        };
        _search.TextChanged += (_, __) => PopulateList();

        var leftFlow = new FlowLayoutPanel
        {
            Dock = DockStyle.Left, AutoSize = true, FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false, Padding = Padding.Empty,
        };
        foreach (var b in new[] { _refreshBtn, _updateAllBtn, _conflictsBtn })
        {
            b.Margin = new Padding(0, 0, 6, 0);
            leftFlow.Controls.Add(b);
        }

        var rightFlow = new FlowLayoutPanel
        {
            Dock = DockStyle.Right, AutoSize = true, FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false, Padding = Padding.Empty,
        };
        foreach (var b in new[] { _openFolderBtn, _enableAllBtn, _disableAllBtn, _uninstallAllBtn })
        {
            b.Margin = new Padding(6, 0, 0, 0);
            rightFlow.Controls.Add(b);
        }
        _search.Margin = new Padding(6, 0, 0, 0);
        rightFlow.Controls.Add(_search);

        _toolbar.Controls.Add(leftFlow);
        _toolbar.Controls.Add(rightFlow);

        _list = new ListView
        {
            Dock = DockStyle.Fill,
            View = View.Details,
            FullRowSelect = true,
            GridLines = false,
            BorderStyle = BorderStyle.None,
            OwnerDraw = true,
        };
        _list.Columns.Add("Name", 240);
        _list.Columns.Add("Status", 90);
        _list.Columns.Add("Version", 120);
        _list.Columns.Add("Size", 70);
        _list.Columns.Add("", 32);
        _list.Columns.Add("", 32);
        _list.DrawColumnHeader += DrawHeader;
        _list.DrawItem += DrawItem;
        _list.DrawSubItem += (_, e) => e.DrawDefault = false;
        _list.MouseDown += List_MouseDown;
        _list.HandleCreated += (_, __) => ThemeEngine.StripVisualStyle(_list);
        _list.Resize += (_, __) => StretchLastColumn();

        _statusLabel = new Label
        {
            Dock = DockStyle.Bottom, Height = 22,
            TextAlign = ContentAlignment.MiddleLeft,
            Padding = new Padding(8, 0, 0, 0),
        };

        _listCard = new RPanel { Dock = DockStyle.Fill, CornerRadius = 0, Padding = new Padding(2) };
        _listCard.Controls.Add(_list);

        _bodyWrap = new Panel { Dock = DockStyle.Fill, Padding = Padding.Empty };
        _bodyWrap.Controls.Add(_listCard);

        Controls.Add(_bodyWrap);
        Controls.Add(_toolbar);
        Controls.Add(_statusLabel);

        ThemeEngine.ThemeChanged += ApplyTheme;
        HandleCreated += (_, __) => { ThemeEngine.ApplyScrollTheme(this); StretchLastColumn(); };
        ApplyTheme();
        Refresh_();
    }

    private void StretchLastColumn()
    {
        if (_list.Columns.Count == 0) return;
        int used = 0;
        for (int i = 1; i < _list.Columns.Count; i++) used += _list.Columns[i].Width;
        int nameWidth = _list.ClientSize.Width - used;
        var nameCol = _list.Columns[0];
        if (nameWidth > nameCol.Width) nameCol.Width = nameWidth;
    }

    public void Refresh_()
    {
        DataBridge.LoadSettings();
        _mods = ModInstaller.GetInstalled();
        ApplyLatestVersions();
        FindConflicts();
        PopulateList();
        UpdateStatusText();

        _openFolderBtn.Text = AppState.DetectLoaderKind() == ModLoaderKind.MelonLoader ? "Open Mods ↗" : "Open Plugins ↗";
        _ = RefreshLatestVersionsAsync();
    }

    private void FindConflicts()
    {
        _conflicts.Clear();
        var byFile = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);

        foreach (var m in _mods)
        {
            var dlls = m.IsFolder
                ? Directory.EnumerateFiles(m.FilePath, "*.dll", SearchOption.AllDirectories)
                : new[] { m.FilePath };

            foreach (var f in dlls)
            {
                string name = Path.GetFileName(f);
                if (!byFile.TryGetValue(name, out var owners)) byFile[name] = owners = new List<string>();
                if (!owners.Contains(m.Name)) owners.Add(m.Name);
            }
        }

        foreach (var (file, owners) in byFile)
            if (owners.Count > 1)
                _conflicts.Add($"{file} is present in both {string.Join(" and ", owners)}");
    }

    private void ShowConflicts()
    {
        if (_conflicts.Count == 0) return;
        MessageBox.Show(string.Join("\n", _conflicts), "Possible Mod Conflicts", MessageBoxButtons.OK, MessageBoxIcon.Warning);
    }

    private void UpdateStatusText()
    {
        int updates = _mods.Count(m => m.UpdateAvailable);
        long totalSize = _mods.Sum(m => m.SizeBytes);
        _statusLabel.Text = $"{_mods.Count} mods  ·  {_mods.Count(m => m.Enabled)} enabled, {_mods.Count(m => !m.Enabled)} disabled  ·  {FormatSize(totalSize)} total" +
            (updates > 0 ? $"  ·  {updates} update{(updates == 1 ? "" : "s")} available" : "");
        _updateAllBtn.Visible = updates > 0;
        _conflictsBtn.Visible = _conflicts.Count > 0;
        _conflictsBtn.Text = _conflicts.Count == 1 ? "⚠ 1 Conflict" : $"⚠ {_conflicts.Count} Conflicts";
    }

    private async Task RefreshLatestVersionsAsync()
    {
        if (!AppState.Settings.ThunderstoreCommunities.TryGetValue(AppState.Settings.GameName, out var community)) return;

        var latest = await ThunderstoreApi.ListPackagesAsync(community);
        bool melon = AppState.DetectLoaderKind() == ModLoaderKind.MelonLoader;
        _latestByKey = latest
            .Where(m => m.Version is { Length: > 0 })
            .GroupBy(m => melon ? MelonKey(m.DllName) : m.DllName, StringComparer.OrdinalIgnoreCase)
            .ToDictionary(g => g.Key, g => g.First(), StringComparer.OrdinalIgnoreCase);

        ApplyLatestVersions();
        PopulateList();
        UpdateStatusText();
    }

    private static string MelonKey(string fullName)
    {
        int dash = fullName.IndexOf('-');
        return dash >= 0 ? fullName[(dash + 1)..] : fullName;
    }

    private void ApplyLatestVersions()
    {
        foreach (var m in _mods)
            m.LatestVersion = _latestByKey.TryGetValue(m.Name, out var latest) ? latest.Version : null;
    }

    private async Task DoUpdateAll()
    {
        var outdated = _mods.Where(m => m.UpdateAvailable && _latestByKey.ContainsKey(m.Name)).ToList();
        if (outdated.Count == 0) return;

        _updateAllBtn.Enabled = false;
        foreach (var m in outdated)
        {
            var latest = _latestByKey[m.Name];
            if (string.IsNullOrEmpty(latest.DownloadUrl)) continue;

            _statusLabel.Text = $"Updating {m.Name}...";
            try
            {
                await ModInstaller.InstallThunderstoreAsync(latest.DownloadUrl, latest.DllName, new Progress<(int, string)>(_ => { }));
                DataBridge.SetModVersion(m.Name, latest.Version!);
            }
            catch { }
        }
        _updateAllBtn.Enabled = true;
        Refresh_();
    }

    private void PopulateList()
    {
        string filter = _search.Text.Trim();
        _filtered = string.IsNullOrEmpty(filter)
            ? _mods
            : _mods.Where(m => m.Name.Contains(filter, StringComparison.OrdinalIgnoreCase)).ToList();

        _list.BeginUpdate();
        _list.Items.Clear();
        foreach (var m in _filtered)
        {
            var item = new ListViewItem(m.Name) { Tag = m };
            item.SubItems.Add(m.Enabled ? "Enabled" : "Disabled");
            item.SubItems.Add(m.UpdateAvailable ? $"{m.KnownVersion} → {m.LatestVersion}" : m.KnownVersion ?? "-");
            item.SubItems.Add(FormatSize(m.SizeBytes));
            _list.Items.Add(item);
        }
        _list.EndUpdate();
    }

    private static string FormatSize(long bytes)
    {
        if (bytes >= 1024 * 1024) return $"{bytes / (1024.0 * 1024.0):0.#} MB";
        if (bytes >= 1024) return $"{bytes / 1024.0:0.#} KB";
        return $"{bytes} B";
    }

    private void List_MouseDown(object? sender, MouseEventArgs e)
    {
        var hit = _list.HitTest(e.Location);
        if (hit.Item?.Tag is not InstalledMod m) return;

        int toggleLeft = 0;
        for (int i = 0; i < _list.Columns.Count - 2; i++) toggleLeft += _list.Columns[i].Width;
        int toggleRight = toggleLeft + _list.Columns[_list.Columns.Count - 2].Width;
        int deleteRight = toggleRight + _list.Columns[_list.Columns.Count - 1].Width;

        if (e.X >= toggleLeft && e.X < toggleRight)
        {
            if (m.Enabled) ModInstaller.Disable(m.Name); else ModInstaller.Enable(m.Name);
            Refresh_();
        }
        else if (e.X >= toggleRight && e.X < deleteRight)
        {
            UninstallMod(m);
        }
    }

    private void UninstallMod(InstalledMod m)
    {
        bool melon = AppState.DetectLoaderKind() == ModLoaderKind.MelonLoader;
        var dependents = _latestByKey.Values
            .Where(latest => !(melon ? MelonKey(latest.DllName) : latest.DllName).Equals(m.Name, StringComparison.OrdinalIgnoreCase))
            .Where(latest => latest.Dependencies.Any(d => (melon ? MelonKey(StripDepVersion(d)) : StripDepVersion(d)).Equals(m.Name, StringComparison.OrdinalIgnoreCase)))
            .Where(latest => _mods.Any(im => im.Name.Equals(melon ? MelonKey(latest.DllName) : latest.DllName, StringComparison.OrdinalIgnoreCase)))
            .Select(latest => latest.Name)
            .ToList();

        string warning = dependents.Count > 0
            ? $"\n\n{string.Join(", ", dependents)} depend{(dependents.Count == 1 ? "s" : "")} on this and may break without it."
            : "";
        if (MessageBox.Show($"Uninstall {m.Name}?{warning}", "Confirm", MessageBoxButtons.YesNo,
            dependents.Count > 0 ? MessageBoxIcon.Warning : MessageBoxIcon.Question) != DialogResult.Yes) return;

        ModInstaller.Uninstall(m.Name); Refresh_();
    }

    private static string StripDepVersion(string dep)
    {
        int lastDash = dep.LastIndexOf('-');
        return lastDash >= 0 ? dep[..lastDash] : dep;
    }

    private void DoEnableAll()
    {
        if (_mods.Count == 0) return;
        foreach (var m in _mods) ModInstaller.Enable(m.Name);
        Refresh_();
        _statusLabel.Text = "All mods enabled.";
    }

    private void DoDisableAll()
    {
        if (_mods.Count == 0) return;
        foreach (var m in _mods) ModInstaller.Disable(m.Name);
        Refresh_();
        _statusLabel.Text = "All mods disabled.";
    }

    private void DoUninstallAll()
    {
        if (_mods.Count == 0) return;
        if (MessageBox.Show($"Uninstall all {_mods.Count} mods?", "Confirm", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes) return;
        foreach (var m in _mods) ModInstaller.Uninstall(m.Name);
        Refresh_();
    }

    private void OpenFolder()
    {
        string? path = AppState.ModsInstallDir;
        if (path == null) return;
        if (!Directory.Exists(path)) Directory.CreateDirectory(path);
        System.Diagnostics.Process.Start("explorer.exe", path);
    }

    private void DrawHeader(object? sender, DrawListViewColumnHeaderEventArgs e)
    {
        var t = ThemeEngine.Current;
        e.Graphics.FillRectangle(new SolidBrush(t.SurfaceAlt), e.Bounds);
        e.Graphics.DrawString(e.Header!.Text, new Font("Segoe UI", 9f, FontStyle.Regular), new SolidBrush(t.SubText),
            e.Bounds.Left + 6, e.Bounds.Top + (e.Bounds.Height - 14) / 2);

        e.Graphics.DrawLine(new Pen(t.Border), e.Bounds.Left, e.Bounds.Bottom - 1, e.Bounds.Right, e.Bounds.Bottom - 1);
    }

    private void DrawItem(object? sender, DrawListViewItemEventArgs e)
    {
        e.DrawDefault = false;

        var t = ThemeEngine.Current;
        var item = e.Item!;
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        e.Graphics.FillRectangle(new SolidBrush(e.ItemIndex % 2 == 0 ? t.Surface : t.Background), e.Bounds);
        if (item.Selected)
        {
            var hi = new Rectangle(e.Bounds.Left + 2, e.Bounds.Top + 1, e.Bounds.Width - 4, e.Bounds.Height - 2);
            using var path = RoundedGraphics.RoundedRect(hi, 6);
            using var brush = new SolidBrush(t.Highlight);
            e.Graphics.FillPath(brush, path);
        }

        var font = new Font("Segoe UI", 9f);
        int x = e.Bounds.Left;
        for (int col = 0; col < _list.Columns.Count && col < item.SubItems.Count; col++)
        {
            int width = _list.Columns[col].Width;
            string text = item.SubItems[col].Text;
            Color fg = col == 1 ? (text == "Enabled" ? Color.FromArgb(39, 201, 63) : Color.FromArgb(255, 95, 86))
                     : col == 2 && item.Tag is InstalledMod { UpdateAvailable: true } ? Color.FromArgb(255, 189, 46)
                     : t.Text;
            var cellRect = new Rectangle(x, e.Bounds.Top, width, e.Bounds.Height);
            e.Graphics.DrawString(text, font, new SolidBrush(fg), cellRect.Left + 6, cellRect.Top + (cellRect.Height - 14) / 2);
            x += width;
        }

        if (item.Tag is InstalledMod m)
        {
            int toggleColIdx = _list.Columns.Count - 2;
            int deleteColIdx = _list.Columns.Count - 1;
            var toggleRect = new Rectangle(x, e.Bounds.Top, _list.Columns[toggleColIdx].Width, e.Bounds.Height);
            var deleteRect = new Rectangle(x + _list.Columns[toggleColIdx].Width, e.Bounds.Top, _list.Columns[deleteColIdx].Width, e.Bounds.Height);
            DrawCheckbox(e.Graphics, toggleRect, m.Enabled, t);
            DrawTrashIcon(e.Graphics, deleteRect, Color.FromArgb(255, 95, 86));
        }

        e.Graphics.DrawLine(new Pen(t.Border), e.Bounds.Left, e.Bounds.Bottom - 1, e.Bounds.Right, e.Bounds.Bottom - 1);
    }

    private static void DrawCheckbox(Graphics g, Rectangle area, bool @checked, ThemeColors t)
    {
        const int size = 16;
        var rect = new Rectangle(area.Left + (area.Width - size) / 2, area.Top + (area.Height - size) / 2, size, size);
        using var path = RoundedGraphics.RoundedRect(rect, 4);
        if (@checked)
        {
            using (var brush = new SolidBrush(t.Accent)) g.FillPath(brush, path);
            using var pen = new Pen(Color.White, 1.6f) { StartCap = LineCap.Round, EndCap = LineCap.Round, LineJoin = LineJoin.Round };
            g.DrawLines(pen, new[]
            {
                new Point(rect.Left + 3, rect.Top + 8),
                new Point(rect.Left + 7, rect.Top + 12),
                new Point(rect.Right - 3, rect.Top + 4),
            });
        }
        else
        {
            using var pen = new Pen(t.SubText, 1.4f);
            g.DrawPath(pen, path);
        }
    }

    private static void DrawTrashIcon(Graphics g, Rectangle area, Color color)
    {
        const int w = 12, h = 11;
        int left = area.Left + (area.Width - w) / 2;
        int top  = area.Top + (area.Height - h) / 2 + 2;
        using var pen = new Pen(color, 1.3f) { LineJoin = LineJoin.Round };

        g.DrawLine(pen, left - 1, top, left + w + 1, top);
        g.DrawLine(pen, left + 3, top, left + 3, top - 2);
        g.DrawLine(pen, left + w - 3, top, left + w - 3, top - 2);
        g.DrawLine(pen, left + 3, top - 2, left + w - 3, top - 2);

        using var path = RoundedGraphics.RoundedRect(new Rectangle(left, top + 2, w, h - 2), 2);
        g.DrawPath(pen, path);

        g.DrawLine(pen, left + w / 3, top + 4, left + w / 3, top + h - 2);
        g.DrawLine(pen, left + w - w / 3, top + 4, left + w - w / 3, top + h - 2);
    }

    private void ApplyTheme()
    {
        var t = ThemeEngine.Current;
        BackColor = t.Background;
        _toolbar.BackColor = t.Surface;
        _bodyWrap.BackColor = t.Background;
        _listCard.BackColor = t.SurfaceAlt;
        _listCard.BorderColor = Color.Transparent;
        _list.BackColor = t.Surface;
        _list.ForeColor = t.Text;
        _statusLabel.BackColor = t.Background;
        _statusLabel.ForeColor = t.SubText;
        foreach (var b in new[] { _refreshBtn, _openFolderBtn, _enableAllBtn, _disableAllBtn, _uninstallAllBtn })
            ThemeEngine.StyleGhostButton(b);
        ThemeEngine.StyleRButton(_updateAllBtn, accent: true);
        ThemeEngine.StyleGhostButton(_conflictsBtn);
        _conflictsBtn.ForeColor = Color.FromArgb(255, 189, 46);
        _search.BackColor = t.SurfaceAlt;
        _search.ForeColor = t.Text;
        ThemeEngine.ApplyLayoutCornerStyle(_search);
        _list.Invalidate();
        ThemeEngine.ApplyScrollTheme(this);
    }

    private static RButton MakeBtn(string text)
    {
        return new RButton
        {
            Text = text, Style = RButtonStyle.Outline, CornerRadius = 8,
            RoundedCorners = Corners.BottomLeft | Corners.BottomRight,
            AutoSize = true, Padding = new Padding(8, 2, 8, 2), Height = 38,
        };
    }

    protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
    {
        if (keyData == (Keys.Control | Keys.F)) { _search.Inner.Focus(); return true; }
        if (keyData == Keys.Delete && _list.SelectedItems.Count > 0 && _list.SelectedItems[0].Tag is InstalledMod m)
        {
            UninstallMod(m);
            return true;
        }
        return base.ProcessCmdKey(ref msg, keyData);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) ThemeEngine.ThemeChanged -= ApplyTheme;
        base.Dispose(disposing);
    }
}
