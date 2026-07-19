using SevsModManager.Core;
using SevsModManager.UI.Controls;
using System.Drawing.Drawing2D;
using SevsModManager.Theme;

namespace SevsModManager.UI.Panels;

internal sealed class ModpacksPanel : UserControl
{
    private List<ModpackInfo> _packs = new();
    private ModpackInfo? _selected;

    private readonly ListBox _packList;
    private readonly ListBox _modList;
    private readonly RButton _saveBtn, _blankBtn, _applyBtn, _deleteBtn, _exportBtn, _importBtn, _refreshBtn, _openFolderBtn;
    private readonly Label   _detailName, _detailMeta, _modsLbl;
    private readonly Label   _statusLabel;
    private readonly Panel   _toolbar;
    private readonly Panel   _bodyWrap;
    private readonly RPanel  _detailPanel;
    private readonly RPanel  _leftPanel;

    public ModpacksPanel()
    {
        Dock = DockStyle.Fill;
        DoubleBuffered = true;

        _toolbar = new Panel { Dock = DockStyle.Top, Height = 44, Padding = new Padding(8, 0, 8, 6) };
        _saveBtn      = MakeBtn("+ Save Current");
        _blankBtn     = MakeBtn("Create Blank Pack");
        _refreshBtn   = MakeBtn("↺ Refresh");
        _applyBtn     = MakeBtn("Apply");
        _deleteBtn    = MakeBtn("Delete");
        _exportBtn    = MakeBtn("Export ↗");
        _importBtn    = MakeBtn("Import...");
        _openFolderBtn= MakeBtn("Open Folder ↗");

        _applyBtn.Enabled  = false;
        _deleteBtn.Enabled = false;
        _exportBtn.Enabled = false;

        _applyBtn.EnabledChanged  += (_, __) => ApplyTheme();
        _deleteBtn.EnabledChanged += (_, __) => ApplyTheme();
        _exportBtn.EnabledChanged += (_, __) => ApplyTheme();

        _saveBtn.Click       += async (_, __) => await DoSaveCurrent();
        _blankBtn.Click      += async (_, __) => await DoCreateBlank();
        _refreshBtn.Click    += (_, __) => Refresh_();
        _applyBtn.Click      += async (_, __) => await DoApply();
        _deleteBtn.Click     += (_, __) => DoDelete();
        _exportBtn.Click     += (_, __) => DoExport();
        _importBtn.Click     += (_, __) => DoImport();
        _openFolderBtn.Click += (_, __) => OpenFolder();

        var leftFlow = new FlowLayoutPanel
        {
            Dock = DockStyle.Left, AutoSize = true, FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false, Padding = Padding.Empty,
        };
        foreach (var b in new[] { _saveBtn, _blankBtn, _refreshBtn, _applyBtn, _deleteBtn })
        { b.Margin = new Padding(0, 0, 6, 0); leftFlow.Controls.Add(b); }

        var rightFlow = new FlowLayoutPanel
        {
            Dock = DockStyle.Right, AutoSize = true, FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false, Padding = Padding.Empty,
        };
        foreach (var b in new[] { _importBtn, _exportBtn, _openFolderBtn })
        { b.Margin = new Padding(6, 0, 0, 0); rightFlow.Controls.Add(b); }

        _toolbar.Controls.Add(leftFlow);
        _toolbar.Controls.Add(rightFlow);

        _packList = new ListBox { Dock = DockStyle.Fill, BorderStyle = BorderStyle.None, DrawMode = DrawMode.OwnerDrawFixed, ItemHeight = 52, IntegralHeight = false };
        _packList.DrawItem += DrawPackItem;
        _packList.SelectedIndexChanged += (_, __) => ShowDetail();

        bool r2 = AppState.Settings.Layout == AppLayout.R2Modman;

        _leftPanel = new RPanel { Dock = r2 ? DockStyle.Right : DockStyle.Left, Width = 260, CornerRadius = 0 };
        _leftPanel.Controls.Add(_packList);

        _detailPanel = new RPanel { Dock = DockStyle.Fill, Padding = new Padding(12), CornerRadius = 0 };
        _detailPanel.Tag = "surface";

        _detailName = new Label { Text = "", AutoSize = false, Dock = DockStyle.Top, Height = 28, Font = new Font("Segoe UI", 13f, FontStyle.Bold) };
        _detailMeta = new Label { Text = "", AutoSize = false, Dock = DockStyle.Top, Height = 22, Font = new Font("Segoe UI", 9f) };
        _detailMeta.Tag = "subtext";
        _modsLbl    = MakeLabel("Mods");

        _modList = new ListBox { Dock = DockStyle.Fill, BorderStyle = BorderStyle.None, SelectionMode = SelectionMode.None };

        var modsWrap = new Panel { Dock = DockStyle.Fill, Padding = new Padding(0, 6, 0, 0) };
        modsWrap.Controls.Add(_modList);
        modsWrap.Controls.Add(_modsLbl);

        _detailPanel.Controls.Add(modsWrap);
        _detailPanel.Controls.Add(_detailMeta);
        _detailPanel.Controls.Add(_detailName);

        _statusLabel = new Label { Dock = DockStyle.Bottom, Height = 22, Padding = new Padding(8, 0, 0, 0), TextAlign = ContentAlignment.MiddleLeft };

        var splitter = new Splitter { Dock = r2 ? DockStyle.Right : DockStyle.Left, Width = 1 };

        _bodyWrap = new Panel { Dock = DockStyle.Fill, Padding = Padding.Empty };
        _bodyWrap.Controls.Add(_detailPanel);
        _bodyWrap.Controls.Add(splitter);
        _bodyWrap.Controls.Add(_leftPanel);

        Controls.Add(_bodyWrap);
        Controls.Add(_toolbar);
        Controls.Add(_statusLabel);

        ThemeEngine.ThemeChanged += ApplyTheme;
        HandleCreated += (_, __) => ThemeEngine.ApplyScrollTheme(this);
        ApplyTheme();
        _detailPanel.Visible = false;
        Refresh_();
    }

    public void Refresh_()
    {
        if (AppState.GameDir == null) { _statusLabel.Text = "No game selected."; return; }

        _packs = ModpackManager.GetSavedPacks();
        _packList.BeginUpdate();
        _packList.Items.Clear();
        foreach (var p in _packs) _packList.Items.Add(p);
        _packList.EndUpdate();
        _statusLabel.Text = $"{_packs.Count} modpacks";

        if (_selected != null && !_packs.Any(p => p.FilePath == _selected.FilePath))
            ShowDetail(null);
    }

    private void ShowDetail(ModpackInfo? forced = null)
    {
        _selected = forced ?? (_packList.SelectedItem as ModpackInfo);
        _applyBtn.Enabled  = _selected != null;
        _deleteBtn.Enabled = _selected != null;
        _exportBtn.Enabled = _selected != null;

        _modList.Items.Clear();
        if (_selected == null)
        {
            _detailPanel.Visible = false;
            return;
        }
        _detailPanel.Visible = true;

        var m = _selected.Manifest;
        _detailName.Text = _selected.DisplayName;
        _detailMeta.Text = $"by {(string.IsNullOrWhiteSpace(m.Author) ? "unknown" : m.Author)}  ·  {m.CreatedUtc.ToLocalTime():MMM d, yyyy}  ·  {m.GameSlug}";
        _modsLbl.Text = $"Mods ({m.Mods.Count})";
        foreach (var mod in m.Mods.OrderBy(x => x)) _modList.Items.Add(mod);
    }

    private async Task DoSaveCurrent()
    {
        if (AppState.GameDir == null) { MessageBox.Show("No game path set.", "Error"); return; }
        var loaderKind = AppState.DetectLoaderKind();
        if (!AppState.IsLoaderInstalled(loaderKind, AppState.GameDir))
        {
            MessageBox.Show($"No {AppState.LoaderName(loaderKind)} folder found to save.", "Error");
            return;
        }

        using var dlg = new ModpackExportDialog(ModpackManager.GetPackRoots(), $"My Setup {DateTime.Now:yyyy-MM-dd}");
        if (dlg.ShowDialog() != DialogResult.OK) return;

        string name = dlg.PackName;
        var included = dlg.IncludedPaths;

        await RunBusy(() =>
        {
            var progress = new Progress<(int percent, string status)>(p => _statusLabel.Text = $"[{p.percent}%] {p.status}");
            ModpackManager.SaveCurrentAsPackSelective(name, Environment.UserName, included, progress);
        });
        Refresh_();
        _statusLabel.Text = $"\"{name}\" saved.";
    }

    private async Task RunBusy(Action work)
    {
        _toolbar.Enabled = false;
        try { await Task.Run(work); }
        finally { _toolbar.Enabled = true; }
    }

    private async Task RunBusyAsync(Func<Task> work)
    {
        _toolbar.Enabled = false;
        try { await work(); }
        finally { _toolbar.Enabled = true; }
    }

    private void SaveCurrentAsPackWithProgress(string name, string author)
    {
        var progress = new Progress<(int percent, string status)>(p => _statusLabel.Text = $"[{p.percent}%] {p.status}");
        ModpackManager.SaveCurrentAsPack(name, author, progress);
    }

    private async Task<bool> ConfirmSaveCurrentFirst(string action = "Applying a modpack")
    {
        var result = MessageBox.Show(
            $"{action} replaces everything in your current BepInEx folder. Save your current setup as a modpack first so you don't lose it?",
            "Save Current Setup First?", MessageBoxButtons.YesNoCancel, MessageBoxIcon.Warning);

        if (result == DialogResult.Cancel) return false;
        if (result == DialogResult.Yes)
        {
            string? name = PromptDialog.Show("Save Current Setup", "Modpack name:", $"My Setup {DateTime.Now:yyyy-MM-dd}");
            if (string.IsNullOrWhiteSpace(name)) return false;
            await RunBusy(() => SaveCurrentAsPackWithProgress(name, Environment.UserName));
        }
        return true;
    }

    private async Task DoCreateBlank()
    {
        if (AppState.GameDir == null) { MessageBox.Show("No game path set.", "Error"); return; }

        if (MessageBox.Show(
            "This deletes your current BepInEx folder (all installed mods) and leaves a blank one. Continue?",
            "Create Blank Pack", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes) return;

        if (!ModpackManager.CurrentSetupMatchesSavedPack())
            if (!await ConfirmSaveCurrentFirst("Creating a blank pack")) return;

        _toolbar.Enabled = false;
        try
        {
            var progress = new Progress<(int percent, string status)>(p => _statusLabel.Text = $"[{p.percent}%] {p.status}");
            await ModpackManager.CreateBlankSetup(progress);
            Refresh_();
            _statusLabel.Text = "Blank pack created.";
        }
        catch (Exception ex) { _statusLabel.Text = "Failed: " + ex.Message; }
        finally { _toolbar.Enabled = true; }
    }

    private async Task DoApply()
    {
        if (_selected == null) return;
        if (AppState.GameDir == null) { MessageBox.Show("No game path set.", "Error"); return; }

        if (MessageBox.Show($"Apply \"{_selected.DisplayName}\"? This replaces your current BepInEx folder.", "Confirm", MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;
        if (!await ConfirmSaveCurrentFirst()) return;

        try
        {
            string packPath = _selected.FilePath;
            string displayName = _selected.DisplayName;
            await RunBusyAsync(() =>
            {
                var progress = new Progress<(int percent, string status)>(p => _statusLabel.Text = $"[{p.percent}%] {p.status}");
                return ModpackManager.ApplyPack(packPath, progress);
            });
            Refresh_();
            _statusLabel.Text = $"{displayName} applied.";
        }
        catch (Exception ex) { _statusLabel.Text = "Apply failed: " + ex.Message; }
    }

    private void DoDelete()
    {
        if (_selected == null) return;
        if (MessageBox.Show($"Delete \"{_selected.DisplayName}\"? This only removes the saved pack, not your installed mods.", "Confirm", MessageBoxButtons.YesNo) != DialogResult.Yes) return;
        ModpackManager.DeletePack(_selected.FilePath);
        _selected = null;
        Refresh_();
        ShowDetail(null);
    }

    private void DoExport()
    {
        if (_selected == null) return;
        using var dlg = new SaveFileDialog
        {
            Title = "Export Modpack", Filter = "Sev's Mod Manager Pack|*.smmpack",
            FileName = Path.GetFileName(_selected.FilePath),
        };
        if (dlg.ShowDialog() != DialogResult.OK) return;

        try
        {
            ModpackManager.ExportPack(_selected.FilePath, dlg.FileName);
            _statusLabel.Text = $"Exported to {dlg.FileName}";
        }
        catch (Exception ex) { _statusLabel.Text = "Export failed: " + ex.Message; }
    }

    private void DoImport()
    {
        using var dlg = new OpenFileDialog { Title = "Import Modpack", Filter = "Sev's Mod Manager Pack|*.smmpack" };
        if (dlg.ShowDialog() != DialogResult.OK) return;

        try
        {
            ModpackManager.ImportPack(dlg.FileName);
            Refresh_();
            _statusLabel.Text = "Modpack imported.";
        }
        catch (Exception ex) { _statusLabel.Text = "Import failed: " + ex.Message; }
    }

    public async Task PromptInstallExternalPack(string path)
    {
        var manifest = ModpackManager.TryReadManifest(path);
        if (manifest == null)
        {
            MessageBox.Show("This doesn't look like a valid .smmpack file.", "Invalid Pack", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return;
        }

        if (!string.Equals(manifest.GameSlug, AppState.CurrentPreset.Slug, StringComparison.OrdinalIgnoreCase))
        {
            var proceedAnyway = MessageBox.Show(
                $"This modpack was made for \"{manifest.GameSlug}\", but you currently have \"{AppState.CurrentPreset.Slug}\" selected. Install anyway?",
                "Different Game", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
            if (proceedAnyway != DialogResult.Yes) return;
        }

        var install = MessageBox.Show(
            $"Install modpack \"{manifest.Name}\" by {manifest.Author} ({manifest.Mods.Count} mods)? This replaces your current BepInEx setup.",
            "Install Modpack", MessageBoxButtons.YesNo, MessageBoxIcon.Question);
        if (install != DialogResult.Yes) return;

        if (!await ConfirmSaveCurrentFirst()) return;

        try
        {
            string localPath = ModpackManager.ImportPack(path);
            await RunBusyAsync(() =>
            {
                var progress = new Progress<(int percent, string status)>(p => _statusLabel.Text = $"[{p.percent}%] {p.status}");
                return ModpackManager.ApplyPack(localPath, progress);
            });
            Refresh_();
            _statusLabel.Text = $"{manifest.Name} installed.";
        }
        catch (Exception ex) { _statusLabel.Text = "Install failed: " + ex.Message; }
    }

    private void OpenFolder()
    {
        string path = ModpackManager.PacksDir;
        if (!Directory.Exists(path)) Directory.CreateDirectory(path);
        System.Diagnostics.Process.Start("explorer.exe", path);
    }

    private void DrawPackItem(object? sender, DrawItemEventArgs e)
    {
        if (e.Index < 0 || e.Index >= _packList.Items.Count) return;
        var p = (ModpackInfo)_packList.Items[e.Index]!;
        bool sel = (e.State & DrawItemState.Selected) != 0;
        var t = ThemeEngine.Current;

        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        e.Graphics.FillRectangle(new SolidBrush(_packList.BackColor), e.Bounds);

        var card = new Rectangle(e.Bounds.Left + 6, e.Bounds.Top + 3, e.Bounds.Width - 12, e.Bounds.Height - 6);
        using (var path = RoundedGraphics.RoundedRect(card, 8))
        using (var brush = new SolidBrush(sel ? t.Highlight : t.Surface))
            e.Graphics.FillPath(brush, path);

        e.Graphics.DrawString(p.DisplayName, new Font("Segoe UI", 10f, FontStyle.Bold), new SolidBrush(t.Text), card.Left + 10, card.Top + 6);
        e.Graphics.DrawString($"{p.Manifest.Mods.Count} mods · {p.Manifest.CreatedUtc.ToLocalTime():MMM d}", new Font("Segoe UI", 8f), new SolidBrush(t.SubText), card.Left + 10, card.Top + 26);
    }

    private void ApplyTheme()
    {
        var t = ThemeEngine.Current;
        BackColor = t.Background;
        _toolbar.BackColor = t.Surface;
        _bodyWrap.BackColor = t.Background;
        _packList.BackColor = t.Background;
        _packList.ForeColor = t.Text;
        _leftPanel.BackColor = t.SurfaceAlt;
        _leftPanel.BorderColor = Color.Transparent;
        _detailPanel.BackColor = t.SurfaceAlt;
        _detailPanel.BorderColor = Color.Transparent;
        _detailName.BackColor = t.SurfaceAlt;
        _detailName.ForeColor = t.Text;
        _detailMeta.BackColor = t.SurfaceAlt;
        _detailMeta.ForeColor = t.SubText;
        _modsLbl.BackColor = t.SurfaceAlt;
        _modsLbl.ForeColor = t.SubText;
        _modList.BackColor = t.SurfaceAlt;
        _modList.ForeColor = t.Text;
        _statusLabel.BackColor = t.Background;
        _statusLabel.ForeColor = t.SubText;
        foreach (var b in new[] { _saveBtn, _blankBtn, _refreshBtn, _importBtn, _openFolderBtn })
            ThemeEngine.StyleGhostButton(b);

        if (_applyBtn.Enabled) ThemeEngine.StyleRButton(_applyBtn, accent: true);
        else ThemeEngine.StyleGhostButton(_applyBtn);
        ThemeEngine.StyleGhostButton(_deleteBtn);
        if (!_deleteBtn.Enabled) _deleteBtn.ForeColor = t.SubText;
        ThemeEngine.StyleGhostButton(_exportBtn);
        if (!_exportBtn.Enabled) _exportBtn.ForeColor = t.SubText;
        _packList.Invalidate();
        ThemeEngine.ApplyScrollTheme(this);
    }

    private static Label MakeLabel(string text) => new()
    {
        Text = text, AutoSize = false, Dock = DockStyle.Top, Height = 20,
        Font = new Font("Segoe UI", 8.5f, FontStyle.Regular),
        Padding = new Padding(2, 2, 0, 2),
    };

    private static RButton MakeBtn(string text) => new()
    {
        Text = text, Style = RButtonStyle.Outline, CornerRadius = 8,
        RoundedCorners = Corners.BottomLeft | Corners.BottomRight,
        AutoSize = true, Padding = new Padding(8, 2, 8, 2), Height = 38,
    };

    protected override void Dispose(bool disposing)
    {
        if (disposing) ThemeEngine.ThemeChanged -= ApplyTheme;
        base.Dispose(disposing);
    }
}
