using SevsModManager.Core;
using SevsModManager.UI.Controls;
using System.Drawing.Drawing2D;
using SevsModManager.Theme;
using SevsModManager.UI;

namespace SevsModManager.UI.Panels;

internal sealed class ModpacksPanel : UserControl
{
    private List<ModpackInfo> _packs = new();
    private ModpackInfo? _selected;

    private readonly ListBox _packList;
    private readonly ListBox _modList;
    private readonly RButton _saveBtn, _blankBtn, _applyBtn, _deleteBtn, _exportBtn, _importBtn, _refreshBtn, _openFolderBtn;
    private readonly Label   _detailName, _detailMeta, _modsLbl, _filesLbl;
    private readonly TreeView _fileTree;
    private readonly Label   _statusLabel;
    private readonly RProgressStrip _progressStrip;
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
        _packList.MouseDoubleClick += (_, e) =>
        {
            int idx = _packList.IndexFromPoint(e.Location);
            if (idx < 0 || idx >= _packs.Count) return;
            SwitchToPackGame(_packs[idx]);
        };

        bool r2 = AppState.Settings.Layout == AppLayout.R2Modman;

        _leftPanel = new RPanel { Dock = r2 ? DockStyle.Right : DockStyle.Left, Width = 260, CornerRadius = 0 };
        _leftPanel.Controls.Add(_packList);

        _detailPanel = new RPanel { Dock = DockStyle.Fill, Padding = new Padding(12), CornerRadius = 0 };
        _detailPanel.Tag = "surface";

        _detailName = new Label { Text = "", AutoSize = false, Dock = DockStyle.Top, Height = 28, Font = new Font("Segoe UI", 13f, FontStyle.Bold) };
        _detailMeta = new Label { Text = "", AutoSize = false, Dock = DockStyle.Top, Height = 22, Font = new Font("Segoe UI", 9f) };
        _detailMeta.Tag = "subtext";
        _modsLbl    = MakeLabel("Mods");

        _modList = new ListBox { Dock = DockStyle.Top, Height = 120, BorderStyle = BorderStyle.None, SelectionMode = SelectionMode.None };

        var modsWrap = new Panel { Dock = DockStyle.Top, Height = 144, Padding = new Padding(0, 6, 0, 0) };
        modsWrap.Controls.Add(_modList);
        modsWrap.Controls.Add(_modsLbl);

        _filesLbl = MakeLabel("Files");
        _fileTree = new TreeView { Dock = DockStyle.Fill, BorderStyle = BorderStyle.None };
        _fileTree.HandleCreated += (_, __) => ThemeEngine.ApplyScrollTheme(_fileTree);

        var filesWrap = new Panel { Dock = DockStyle.Fill, Padding = new Padding(0, 6, 0, 0) };
        filesWrap.Controls.Add(_fileTree);
        filesWrap.Controls.Add(_filesLbl);

        _detailPanel.Controls.Add(filesWrap);
        _detailPanel.Controls.Add(modsWrap);
        _detailPanel.Controls.Add(_detailMeta);
        _detailPanel.Controls.Add(_detailName);

        _statusLabel = new Label { Dock = DockStyle.Bottom, Height = 22, Padding = new Padding(8, 0, 0, 0), TextAlign = ContentAlignment.MiddleLeft };
        _progressStrip = new RProgressStrip { Value = 0 };

        var splitter = new Splitter { Dock = r2 ? DockStyle.Right : DockStyle.Left, Width = 1 };

        _bodyWrap = new Panel { Dock = DockStyle.Fill, Padding = Padding.Empty };
        _bodyWrap.Controls.Add(_detailPanel);
        _bodyWrap.Controls.Add(splitter);
        _bodyWrap.Controls.Add(_leftPanel);

        Controls.Add(_bodyWrap);
        Controls.Add(_toolbar);
        Controls.Add(_progressStrip);
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
        _fileTree.Nodes.Clear();
        if (_selected == null)
        {
            _detailPanel.Visible = false;
            return;
        }
        _detailPanel.Visible = true;

        var m = _selected.Manifest;
        _detailName.Text = _selected.DisplayName;
        string loaderTag = m.LoaderKind is { } lk ? AppState.LoaderName(lk) : "any loader";
        _detailMeta.Text = $"by {(string.IsNullOrWhiteSpace(m.Author) ? "unknown" : m.Author)}  ·  {m.CreatedUtc.ToLocalTime():MMM d, yyyy}  ·  {m.GameSlug}  ·  {loaderTag}";
        _modsLbl.Text = $"Mods ({m.Mods.Count})";
        foreach (var mod in m.Mods.OrderBy(x => x)) _modList.Items.Add(mod);

        try
        {
            var entries = ModpackManager.GetPackEntries(_selected.FilePath);
            _filesLbl.Text = $"Files ({entries.Count})";
            PopulateFileTree(_fileTree, entries);
        }
        catch { _filesLbl.Text = "Files"; }
    }

    private static void PopulateFileTree(TreeView tree, List<string> entries)
    {
        var nodeMap = new Dictionary<string, TreeNode>();
        foreach (var entry in entries)
        {
            var parts = entry.Split('/');
            TreeNodeCollection parent = tree.Nodes;
            string pathSoFar = "";
            foreach (var part in parts)
            {
                pathSoFar = pathSoFar.Length == 0 ? part : pathSoFar + "/" + part;
                if (!nodeMap.TryGetValue(pathSoFar, out var node))
                {
                    node = new TreeNode(part);
                    parent.Add(node);
                    nodeMap[pathSoFar] = node;
                }
                parent = node.Nodes;
            }
        }
    }

    private void SwitchToPackGame(ModpackInfo pack)
    {
        string slug = pack.Manifest.GameSlug;
        if (string.IsNullOrEmpty(slug) || string.Equals(slug, AppState.CurrentGameSlug, StringComparison.OrdinalIgnoreCase))
        {
            ShowDetail(pack);
            return;
        }

        if (ResolveGameForSlug(slug) is not { } resolved) return;
        RegisterIfCustom(resolved);
        SwitchGame(resolved.Path, resolved.Name);
    }

    private static void RegisterIfCustom((string Path, string Name) resolved)
    {
        bool isPreset = AppState.Presets.Any(p => p.Name == resolved.Name && p.Name != "Custom");
        if (!isPreset) AppState.Settings.CustomGames[resolved.Name] = resolved.Path;
    }

    private static (string Path, string Name)? ResolveGameForSlug(string slug)
    {
        var preset = AppState.Presets.FirstOrDefault(p => p.Name != "Custom" && p.Slug.Equals(slug, StringComparison.OrdinalIgnoreCase));
        if (preset != null)
        {
            string? path = preset.DefaultPaths.FirstOrDefault(File.Exists);
            if (path == null && AppState.Settings.KnownGamePaths.TryGetValue(preset.Name, out var known) && File.Exists(known)) path = known;
            if (path == null && preset.SteamAppId is { } appId && preset.DefaultPaths.Length > 0)
                path = SteamLocator.FindGamePath(appId, Path.GetFileName(preset.DefaultPaths[0]));
            if (path != null) return (path, preset.Name);
        }

        var customMatch = AppState.Settings.CustomGames.FirstOrDefault(kv =>
            File.Exists(kv.Value) && AppState.DeriveSlug(Path.GetFileNameWithoutExtension(kv.Value)).Equals(slug, StringComparison.OrdinalIgnoreCase));
        if (customMatch.Key != null) return (customMatch.Value, customMatch.Key);

        foreach (var (appId, steamName) in SteamLocator.ListInstalledGames())
            foreach (var exe in SteamLocator.FindExeCandidates(appId))
                if (AppState.DeriveSlug(Path.GetFileNameWithoutExtension(exe)).Equals(slug, StringComparison.OrdinalIgnoreCase))
                    return (exe, preset?.Name ?? steamName);

        MessageBox.Show(
            $"Couldn't automatically find the game this modpack is for (\"{slug}\"). Pick its executable on the next screen.",
            "Locate Game", MessageBoxButtons.OK, MessageBoxIcon.Information);

        while (true)
        {
            using var fileDlg = new OpenFileDialog { Title = "Select Game Executable", Filter = "Executable|*.exe" };
            if (fileDlg.ShowDialog() != DialogResult.OK) return null;

            string picked = fileDlg.FileName;
            string pickedSlug = AppState.DeriveSlug(Path.GetFileNameWithoutExtension(picked));
            string name = preset?.Name ?? Path.GetFileNameWithoutExtension(picked);
            if (pickedSlug.Equals(slug, StringComparison.OrdinalIgnoreCase)) return (picked, name);

            var proceed = MessageBox.Show(
                $"\"{Path.GetFileName(picked)}\" doesn't look like the right game for this modpack. Continue with it anyway?",
                "Not Sure This Is Right", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
            if (proceed == DialogResult.Yes) return (picked, name);
        }
    }

    private void SwitchGame(string path, string name)
    {
        AppState.Settings.GamePath = path;
        AppState.Settings.GameName = name;
        AppState.Save();
        AppState.EnsureSbFolder();
        DataBridge.LoadSettings();
        Refresh_();
        _statusLabel.Text = $"Switched to {name}.";
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

        var progress = new Progress<(int percent, string status)>(p => { _statusLabel.Text = $"[{p.percent}%] {p.status}"; _progressStrip.Value = p.percent; });
        _toolbar.Enabled = false;
        try
        {
            await OperationQueue.RunAsync($"Save modpack \"{name}\"",
                p => { ModpackManager.SaveCurrentAsPackSelective(name, Environment.UserName, included, p); return Task.CompletedTask; }, progress);
            Refresh_();
            _statusLabel.Text = $"\"{name}\" saved.";
        }
        finally { _toolbar.Enabled = true; _progressStrip.Value = 0; }
    }

    private void SaveCurrentAsPackWithProgress(string name, string author, IProgress<(int percent, string status)> progress) =>
        ModpackManager.SaveCurrentAsPack(name, author, progress);

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
            var progress = new Progress<(int percent, string status)>(p => { _statusLabel.Text = $"[{p.percent}%] {p.status}"; _progressStrip.Value = p.percent; });
            _toolbar.Enabled = false;
            try
            {
                await OperationQueue.RunAsync($"Save modpack \"{name}\"",
                    p => { SaveCurrentAsPackWithProgress(name, Environment.UserName, p); return Task.CompletedTask; }, progress);
            }
            finally { _toolbar.Enabled = true; _progressStrip.Value = 0; }
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

        var progress = new Progress<(int percent, string status)>(p => { _statusLabel.Text = $"[{p.percent}%] {p.status}"; _progressStrip.Value = p.percent; });
        _toolbar.Enabled = false;
        try
        {
            await OperationQueue.RunAsync("Create blank pack", p => ModpackManager.CreateBlankSetup(p), progress);
            Refresh_();
            _statusLabel.Text = "Blank pack created.";
        }
        catch (Exception ex) { _statusLabel.Text = "Failed: " + ex.Message; }
        finally { _toolbar.Enabled = true; _progressStrip.Value = 0; }
    }

    private async Task DoApply()
    {
        if (_selected == null) return;
        if (AppState.GameDir == null) { MessageBox.Show("No game path set.", "Error"); return; }

        if (!ConfirmLoaderSwitch(_selected.Manifest)) return;
        if (MessageBox.Show($"Apply \"{_selected.DisplayName}\"? This replaces your current BepInEx folder.", "Confirm", MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;
        if (!await ConfirmSaveCurrentFirst()) return;

        string packPath = _selected.FilePath;
        string displayName = _selected.DisplayName;
        var manifest = _selected.Manifest;
        var progress = new Progress<(int percent, string status)>(p => { _statusLabel.Text = $"[{p.percent}%] {p.status}"; _progressStrip.Value = p.percent; });
        _toolbar.Enabled = false;
        try
        {
            await OperationQueue.RunAsync($"Apply \"{displayName}\"",
                p => { SwitchLoaderForPack(manifest, p); return ModpackManager.ApplyPack(packPath, p); }, progress);
            Refresh_();
            _statusLabel.Text = $"{displayName} applied.";
        }
        catch (Exception ex) { _statusLabel.Text = "Apply failed: " + ex.Message; }
        finally { _toolbar.Enabled = true; _progressStrip.Value = 0; }
    }

    private bool ConfirmLoaderSwitch(ModpackManifest manifest)
    {
        if (AppState.GameDir == null || manifest.LoaderKind is not { } packLoader) return true;
        var current = AppState.DetectLoaderKind();
        if (packLoader == current) return true;

        string currentName = AppState.LoaderName(current);
        string packName = AppState.LoaderName(packLoader);
        var proceed = MessageBox.Show(
            $"This modpack was made for {packName}, but this game currently has {currentName} installed. Installing it will delete {currentName} and its mods, then set up {packName} instead.\n\nContinue?",
            "Different Mod Loader", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
        return proceed == DialogResult.Yes;
    }

    private void SwitchLoaderForPack(ModpackManifest manifest, IProgress<(int percent, string status)>? progress = null)
    {
        if (AppState.GameDir == null || manifest.LoaderKind is not { } targetLoader) return;
        var current = AppState.DetectLoaderKind();
        if (current == targetLoader) return;

        progress?.Report((0, $"Removing {AppState.LoaderName(current)}..."));
        AppState.UninstallLoader(current, AppState.GameDir);
        AppState.Settings.LoaderOverrides[AppState.Settings.GameName] = targetLoader;
        AppState.Save();
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

        if (!string.IsNullOrEmpty(manifest.GameSlug) && !string.Equals(manifest.GameSlug, AppState.CurrentGameSlug, StringComparison.OrdinalIgnoreCase))
        {
            var switchFirst = MessageBox.Show(
                $"This modpack was made for \"{manifest.GameSlug}\", but you currently have \"{AppState.Settings.GameName}\" selected. Switch to the right game first?",
                "Different Game", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);

            if (switchFirst == DialogResult.Yes)
            {
                if (ResolveGameForSlug(manifest.GameSlug) is not { } resolved) return;
                RegisterIfCustom(resolved);
                SwitchGame(resolved.Path, resolved.Name);
            }
        }

        if (!ConfirmLoaderSwitch(manifest)) return;

        var install = MessageBox.Show(
            $"Install modpack \"{manifest.Name}\" by {manifest.Author} ({manifest.Mods.Count} mods)? This replaces your current BepInEx setup.",
            "Install Modpack", MessageBoxButtons.YesNo, MessageBoxIcon.Question);
        if (install != DialogResult.Yes) return;

        if (!await ConfirmSaveCurrentFirst()) return;

        var progress = new Progress<(int percent, string status)>(p => { _statusLabel.Text = $"[{p.percent}%] {p.status}"; _progressStrip.Value = p.percent; });
        _toolbar.Enabled = false;
        try
        {
            string localPath = ModpackManager.ImportPack(path);
            await OperationQueue.RunAsync($"Install \"{manifest.Name}\"",
                p => { SwitchLoaderForPack(manifest, p); return ModpackManager.ApplyPack(localPath, p); }, progress);
            Refresh_();
            _statusLabel.Text = $"{manifest.Name} installed.";
        }
        catch (Exception ex) { _statusLabel.Text = "Install failed: " + ex.Message; }
        finally { _toolbar.Enabled = true; _progressStrip.Value = 0; }
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

        string loaderSuffix = p.Manifest.LoaderKind is { } lk ? $" · {AppState.LoaderName(lk)}" : "";
        e.Graphics.DrawString(p.DisplayName, new Font("Segoe UI", 10f, FontStyle.Bold), new SolidBrush(t.Text), card.Left + 10, card.Top + 6);
        e.Graphics.DrawString($"{p.Manifest.Mods.Count} mods · {p.Manifest.CreatedUtc.ToLocalTime():MMM d}{loaderSuffix}", new Font("Segoe UI", 8f), new SolidBrush(t.SubText), card.Left + 10, card.Top + 26);
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
        _filesLbl.BackColor = t.SurfaceAlt;
        _filesLbl.ForeColor = t.SubText;
        _fileTree.BackColor = t.SurfaceAlt;
        _fileTree.ForeColor = t.Text;
        _statusLabel.BackColor = t.Background;
        _statusLabel.ForeColor = t.SubText;
        _progressStrip.TrackColor = t.SurfaceAlt;
        _progressStrip.FillColor = t.Accent;
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
