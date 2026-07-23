using SevsModManager.UI.Controls;
using SevsModManager.Core;
using SevsModManager.UI;
using System.Drawing.Drawing2D;
using SevsModManager.Theme;

namespace SevsModManager.UI.Panels;

internal sealed class ModsPanel : UserControl
{

    private List<SbMod> _mods = new();
    private HashSet<int> _upvoted = new();
    private SbMod? _selected;
    internal bool TutorialMode;
    internal Action? OnTutorialInstalled;
    internal Control InstallButtonControl => _installBtn;
    private string _currentTab = "all";
    private int _page = 1;
    private int _gameId;
    private bool _loading;

    private string? _token;
    private string? _username;
    private bool _loginPending;
    private string? _loginCode;

    private List<SbMod> _thunderstoreCache = new();
    private readonly Dictionary<string, Image?> _iconCache = new();
    private readonly HashSet<string> _iconLoading = new();

    private static bool UseThunderstore =>
        AppState.Settings.ThunderstoreCommunities.ContainsKey(AppState.Settings.GameName);
    private static string? ThunderstoreCommunitySlug =>
        AppState.Settings.ThunderstoreCommunities.TryGetValue(AppState.Settings.GameName, out var c) ? c : null;
    private static bool IsManualOnly =>
        AppState.Settings.ManualOnlyGames.Contains(AppState.Settings.GameName);

    private readonly Panel     _topBar;
    private readonly Panel     _bodyWrap;
    private readonly RDropdown _tabDropdown;
    private readonly RButton   _sourceBtn;
    private readonly RTextBox  _search;
    private readonly RButton   _loginBtn;
    private readonly ListBox   _list;
    private readonly RPanel    _listPanel;
    private readonly RPanel    _detail;
    private readonly Label     _detailName, _detailAuthor, _detailDesc, _detailStatus;
    private readonly RButton   _installBtn, _uninstallBtn, _upvoteBtn, _ghBtn, _changelogBtn;
    private readonly RButton   _loadMoreBtn;
    private readonly RButton   _installExternalBtn;
    private readonly Label     _statusLabel;
    private readonly RProgressStrip _progressStrip;

    private static readonly string[] _tabs = { "all", "featured", "verified", "unverified" };

    public ModsPanel()
    {
        Dock = DockStyle.Fill;
        DoubleBuffered = true;

        _topBar = new Panel { Dock = DockStyle.Top, Height = 44, Padding = new Padding(8, 0, 8, 6) };

        _tabDropdown = new RDropdown
        {
            Width = 130, Height = 38,
            CornerRadius = 8,
            RoundedCorners = Corners.BottomLeft | Corners.BottomRight,
        };
        foreach (var t in _tabs) _tabDropdown.Items.Add(char.ToUpper(t[0]) + t[1..]);
        _tabDropdown.SelectedIndex = 0;
        _tabDropdown.SelectedIndexChanged += (_, __) =>
        {
            if (_tabDropdown.SelectedIndex >= 0) SwitchTab(_tabs[_tabDropdown.SelectedIndex]);
        };

        _sourceBtn = new RButton
        {
            Style = RButtonStyle.Outline, CornerRadius = 8,
            RoundedCorners = Corners.BottomLeft | Corners.BottomRight,
            AutoSize = true, Height = 38, Padding = new Padding(10, 0, 10, 0),
        };
        _sourceBtn.Click += async (_, __) => await ToggleThunderstoreQuick();

        var leftFlow = new FlowLayoutPanel
        {
            Dock = DockStyle.Left, AutoSize = true, FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false, Padding = Padding.Empty,
        };
        _tabDropdown.Margin = new Padding(0, 0, 6, 0);
        _sourceBtn.Margin = new Padding(0);
        leftFlow.Controls.Add(_tabDropdown);
        leftFlow.Controls.Add(_sourceBtn);

        _installExternalBtn = new RButton
        {
            Text = "Install External Mod",
            Style = RButtonStyle.Outline,
            CornerRadius = 8,
            AutoSize = true,
            Height = 28,
            Padding = new Padding(10, 2, 10, 2),
        };
        _installExternalBtn.Click += (_, __) => InstallExternalMod();

        _search = new RTextBox
        {
            PlaceholderText = "Search mods...",
            Width = 160, Height = 38,
            CornerRadius = 8,
            RoundedCorners = Corners.BottomLeft | Corners.BottomRight,
        };
        _search.TextChanged += (_, __) => FilterList();

        _loginBtn = new RButton
        {
            Text = "Login",
            Width = 70, Height = 38,
            Style = RButtonStyle.Outline,
            CornerRadius = 8,
            RoundedCorners = Corners.BottomLeft | Corners.BottomRight,
        };
        _loginBtn.Click += async (_, __) => await HandleLoginButtonClick();

        var rightFlow = new FlowLayoutPanel
        {
            Dock = DockStyle.Right, AutoSize = true, FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false, Padding = Padding.Empty,
        };
        _search.Margin = new Padding(0, 0, 6, 0);
        _loginBtn.Margin = new Padding(0);
        rightFlow.Controls.Add(_search);
        rightFlow.Controls.Add(_loginBtn);

        _topBar.Controls.Add(_installExternalBtn);
        _topBar.Controls.Add(leftFlow);
        _topBar.Controls.Add(rightFlow);

        void CenterInstallExternalBtn()
        {
            _installExternalBtn.Left = (_topBar.ClientSize.Width - _installExternalBtn.Width) / 2;
            _installExternalBtn.Top  = (_topBar.ClientSize.Height - _installExternalBtn.Height) / 2;
        }
        _topBar.Resize += (_, __) => CenterInstallExternalBtn();
        _installExternalBtn.SizeChanged += (_, __) => CenterInstallExternalBtn();
        CenterInstallExternalBtn();

        _list = new ListBox
        {
            Dock = DockStyle.Fill,
            DrawMode = DrawMode.OwnerDrawFixed,
            ItemHeight = 56,
            BorderStyle = BorderStyle.None,
            ScrollAlwaysVisible = false,
            IntegralHeight = false,
        };
        _list.DrawItem += DrawModItem;
        _list.SelectedIndexChanged += (_, __) => ShowDetail(_list.SelectedIndex >= 0 ? _mods.ElementAtOrDefault(_list.SelectedIndex) : null);

        _list.HandleCreated += (_, __) => ThemeEngine.ApplyScrollTheme(this);

        _list.MouseDown += (_, e) =>
        {
            if (e.Button != MouseButtons.Right) return;
            int idx = _list.IndexFromPoint(e.Location);
            if (idx >= 0) _list.SelectedIndex = idx;
        };
        _list.ContextMenuStrip = BuildModContextMenu();

        _loadMoreBtn = MakeBtn("Load More", tag: null);
        _loadMoreBtn.Dock = DockStyle.Bottom;
        _loadMoreBtn.Height = 32;
        _loadMoreBtn.Click += async (_, __) => await LoadMoreAsync();

        _statusLabel = new Label
        {
            Dock = DockStyle.Bottom, Height = 22,
            TextAlign = ContentAlignment.MiddleLeft,
            Padding = new Padding(6, 0, 0, 0),
            Text = "Loading...",
        };
        _statusLabel.Tag = "subtext";

        _progressStrip = new RProgressStrip { Value = 0 };

        bool r2 = AppState.Settings.Layout == AppLayout.R2Modman;

        _listPanel = new RPanel { Dock = r2 ? DockStyle.Right : DockStyle.Left, Width = 380, CornerRadius = 0, Padding = new Padding(2) };
        _listPanel.Controls.Add(_list);
        _listPanel.Controls.Add(_loadMoreBtn);
        _listPanel.Controls.Add(_progressStrip);
        _listPanel.Controls.Add(_statusLabel);

        _detail = new RPanel { Dock = DockStyle.Fill, Padding = new Padding(16), CornerRadius = 0 };
        _detail.Tag = "surface";

        _detailName   = MakeLabel("", 15, bold: true);
        _detailAuthor = MakeLabel("", 10);
        _detailAuthor.Tag = "subtext";
        _detailDesc   = MakeLabel("", 10) ;
        _detailDesc.Tag = "subtext";
        _detailDesc.Dock = DockStyle.Fill;
        _detailDesc.AutoSize = false;
        _detailStatus = MakeLabel("", 10, bold: true);
        _detailStatus.Dock = DockStyle.Right;
        _detailStatus.AutoSize = false;
        _detailStatus.Width = 150;
        _detailStatus.TextAlign = ContentAlignment.MiddleRight;

        _installBtn   = MakeBtn("Install",          "accent");
        _uninstallBtn = MakeBtn("Uninstall",         null);
        _upvoteBtn    = MakeBtn("▲ Upvote",          null);
        _ghBtn        = MakeBtn("GitHub ↗",          null);
        _changelogBtn = MakeBtn("Changelog ↗",       null);

        _installBtn.Click   += async (_, __) => await DoInstall();
        _uninstallBtn.Click += (_, __) => DoUninstall();
        _upvoteBtn.Click    += async (_, __) => await DoUpvote();
        _ghBtn.Click        += (_, __) => { if (_selected != null) OpenUrl(_selected.RepoUrl); };
        _changelogBtn.Click += (_, __) => { if (_selected != null) OpenUrl(_selected.RepoUrl.TrimEnd('/') + "/changelog/"); };

        var btnRow = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            Anchor = AnchorStyles.Left,
        };
        foreach (var b in new[] { _installBtn, _upvoteBtn, _uninstallBtn, _ghBtn, _changelogBtn })
        {
            b.Margin = new Padding(0, 0, 6, 6);
            btnRow.Controls.Add(b);
        }

        var actionRow = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1, AutoSize = false };
        actionRow.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        actionRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        actionRow.Controls.Add(btnRow, 0, 0);
        actionRow.Controls.Add(_detailStatus, 1, 0);

        var detailLayout = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 1,
            AutoSize = false,
            RowCount = 4,
        };
        detailLayout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        detailLayout.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        detailLayout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        detailLayout.RowStyles.Add(new RowStyle(SizeType.Absolute, 60));
        detailLayout.Controls.Add(_detailName,   0, 0);
        detailLayout.Controls.Add(_detailAuthor, 0, 1);
        detailLayout.Controls.Add(_detailDesc,   0, 2);
        detailLayout.Controls.Add(actionRow,     0, 3);
        detailLayout.Dock = DockStyle.Fill;
        _detail.Controls.Add(detailLayout);

        var splitter = new Splitter { Dock = r2 ? DockStyle.Right : DockStyle.Left, Width = 1, BackColor = Color.FromArgb(55, 55, 55) };

        _bodyWrap = new Panel { Dock = DockStyle.Fill, Padding = Padding.Empty };
        _bodyWrap.Controls.Add(_detail);
        _bodyWrap.Controls.Add(splitter);
        _bodyWrap.Controls.Add(_listPanel);

        Controls.Add(_bodyWrap);
        Controls.Add(_topBar);

        ThemeEngine.ThemeChanged += ApplyTheme;
        HandleCreated += (_, __) => ThemeEngine.ApplyScrollTheme(this);
        ApplyTheme();

        _detail.Visible = false;
        SwitchTab("all");
    }

    public async Task InitAsync()
    {
        if (IsManualOnly) { _loginBtn.Visible = false; UpdateSourceBtn(); await LoadAsync(); return; }

        if (!UseThunderstore)
        {
            _token = DataBridge.LoadToken();
            SbApi.Token = _token;

            if (!string.IsNullOrEmpty(_token))
            {
                _username = await SbApi.ValidateTokenAsync(_token);
                if (_username == null) { _token = null; SbApi.Token = null; DataBridge.DeleteToken(); }
            }
            UpdateLoginBtn();

            _gameId = await SbApi.RegisterGameAsync(AppState.CurrentGameSlug);
        }
        _loginBtn.Visible = !UseThunderstore;
        UpdateSourceBtn();
        await LoadAsync();
    }

    public void RefreshModSource()
    {
        _loginBtn.Visible = !IsManualOnly && !UseThunderstore;
        UpdateSourceBtn();
        _page = 1;
        _mods.Clear();
        if (UseThunderstore) _thunderstoreCache.Clear();
        _ = LoadAsync();
    }

    private void UpdateSourceBtn() =>
        _sourceBtn.Text = IsManualOnly ? "Source: None (manual)" : UseThunderstore ? $"Source: Thunderstore ({ThunderstoreCommunitySlug})" : "Source: Sev's Mod Manager";

    private async Task ToggleThunderstoreQuick()
    {
        string game = AppState.Settings.GameName;

        if (UseThunderstore)
        {
            AppState.Settings.ThunderstoreCommunities.Remove(game);
            AppState.Save();
            _statusLabel.Text = $"{game} now uses Sev's Mod Manager's own mod list.";
        }
        else
        {
            _statusLabel.Text = "Looking up Thunderstore community...";
            string? community = await ThunderstoreApi.GuessCommunityAsync(game);
            if (community == null)
            {
                _statusLabel.Text = "Couldn't guess a community, pick one.";
                var communities = await ThunderstoreApi.ListCommunitiesAsync();
                if (communities.Count == 0) { _statusLabel.Text = "Couldn't reach Thunderstore right now."; return; }

                using var picker = new ThunderstoreCommunityPickerForm(communities);
                if (picker.ShowDialog() != DialogResult.OK || picker.Selected == null) { _statusLabel.Text = "Ready."; return; }
                community = picker.Selected.Identifier;
            }
            AppState.Settings.ThunderstoreCommunities[game] = community;
            AppState.Save();
            _statusLabel.Text = $"Using Thunderstore community \"{community}\".";
        }

        RefreshModSource();
    }

    private async Task LoadAsync()
    {
        if (_loading) return;
        _loading = true;

        if (IsManualOnly)
        {
            _mods.Clear();
            RebuildList();
            _statusLabel.Text = "No mod library for this game. Manage mods manually via Installed.";
            _loading = false;
            return;
        }

        _statusLabel.Text = "Loading...";
        _loadMoreBtn.Visible = false;

        try
        {
            if (UseThunderstore)
            {
                string? community = ThunderstoreCommunitySlug;
                if (community == null) { _statusLabel.Text = "No Thunderstore community configured."; _loading = false; return; }

                if (_thunderstoreCache.Count == 0)
                    _thunderstoreCache = await ThunderstoreApi.ListPackagesAsync(community);

                IEnumerable<SbMod> filtered = _currentTab switch
                {
                    "featured"   => _thunderstoreCache.Where(m => m.IsFeatured),
                    "verified"   => _thunderstoreCache.Where(m => m.IsVerified),
                    "unverified" => _thunderstoreCache.Where(m => !m.IsVerified),
                    _            => _thunderstoreCache,
                };
                _mods = filtered
                    .OrderByDescending(m => m.IsFeatured ? 2 : m.IsVerified ? 1 : 0)
                    .ThenByDescending(m => m.Upvotes)
                    .ToList();

                _loadMoreBtn.Visible = false;
            }
            else
            {
                var mods = await SbApi.ListModsAsync(_currentTab, _page, _gameId);
                var combined = _page == 1 ? mods : _mods.Concat(mods).ToList();

                _mods = combined
                    .OrderByDescending(m => m.IsFeatured ? 2 : m.IsVerified ? 1 : 0)
                    .ThenByDescending(m => m.Upvotes)
                    .ToList();

                if (!string.IsNullOrEmpty(_token))
                    _upvoted = await SbApi.GetUserUpvotesAsync(_mods);

                _loadMoreBtn.Visible = mods.Count >= 15;
            }

            if (!UseThunderstore)
            {
                var existence = await Task.WhenAll(_mods.Select(async m => (mod: m, ok: await SbApi.UrlExistsAsync(DllUrl(m)))));
                _mods = existence.Where(x => x.ok).Select(x => x.mod).ToList();
            }

            RebuildList();
            _statusLabel.Text = $"{_mods.Count} mods";
        }
        catch (Exception ex)
        {
            _statusLabel.Text = "Load failed: " + ex.Message;
        }
        _loading = false;
    }

    private async Task LoadMoreAsync() { _page++; await LoadAsync(); }

    private void SwitchTab(string tab)
    {
        _currentTab = tab; _page = 1; _mods.Clear();
        _ = LoadAsync();
    }

    private void RebuildList()
    {
        var filter = _search.Text.Trim().ToLowerInvariant();
        var filtered = string.IsNullOrEmpty(filter)
            ? _mods
            : _mods.Where(m => m.Name.ToLowerInvariant().Contains(filter) || m.Author.ToLowerInvariant().Contains(filter)).ToList();

        _list.BeginUpdate();
        _list.Items.Clear();
        foreach (var m in filtered) _list.Items.Add(m);
        _list.EndUpdate();
    }

    private void FilterList() => RebuildList();

    private void ShowDetail(SbMod? mod)
    {
        _selected = mod;
        if (mod == null) { _detail.Visible = false; return; }
        _detail.Visible = true;

        _detailName.Text   = mod.Name;
        _detailAuthor.Text = $"by @{mod.Author}  ·  ▲ {mod.Upvotes}";
        _detailDesc.Text   = mod.Description.Length > 0 ? mod.Description : "(no description)";

        bool installed = IsInstalled(mod);
        bool disabled  = !installed && IsDisabled(mod);

        bool squareGlyph = AppState.Settings.Layout is AppLayout.R2Modman or AppLayout.MonkeModManager;
        string dotOn  = squareGlyph ? "■" : "●";
        string dotOff = squareGlyph ? "□" : "○";
        _detailStatus.Text = installed ? $"{dotOn} Installed" : disabled ? $"{dotOn} Disabled" : $"{dotOff} Not installed";
        _detailStatus.ForeColor = installed ? Color.FromArgb(39, 201, 63) : disabled ? Color.FromArgb(255, 189, 46) : ThemeEngine.Current.SubText;

        _installBtn.Visible   = !installed && !disabled;
        _uninstallBtn.Visible = installed || disabled;

        _upvoteBtn.Visible = mod.Source == ModSource.OurApi;
        _upvoteBtn.Text = _upvoted.Contains(mod.Id) ? "▲ Upvoted" : "▲ Upvote";
        _upvoteBtn.ForeColor = _upvoted.Contains(mod.Id) ? Color.FromArgb(124, 58, 237) : ThemeEngine.Current.Text;

        _ghBtn.Text = mod.Source == ModSource.Thunderstore ? "Thunderstore ↗" : "GitHub ↗";
        _changelogBtn.Visible = mod.Source == ModSource.Thunderstore && !string.IsNullOrEmpty(mod.RepoUrl);
        _installBtn.Text = mod.IsVerified ? "Install" : "Install (Unverified)";
    }

    internal void ShowTutorialMod()
    {
        TutorialMode = true;
        ShowDetail(new SbMod
        {
            Id = -1, Name = "Example Mod", DllName = "TutorialExampleMod",
            Description = "A demo mod for the tutorial. Nothing real gets downloaded.",
            Author = "Tutorial", Upvotes = 0, IsVerified = true, IsFeatured = false,
            Source = ModSource.OurApi,
        });
    }

    internal void EndTutorialMod()
    {
        TutorialMode = false;
        OnTutorialInstalled = null;
        ShowDetail(null);
    }

    private async Task RunTutorialInstall()
    {
        _installBtn.Enabled = false;
        for (int p = 0; p <= 100; p += 20)
        {
            _progressStrip.Value = p;
            _statusLabel.Text = $"[{p}%] Installing Example Mod...";
            await Task.Delay(110);
        }
        _statusLabel.Text = "Example Mod installed.";
        _progressStrip.Value = 0;
        _installBtn.Enabled = true;
        _installBtn.Visible = false;
        _uninstallBtn.Visible = true;
        OnTutorialInstalled?.Invoke();
    }

    private async Task DoInstall()
    {
        if (_selected == null) return;
        if (TutorialMode) { await RunTutorialInstall(); return; }

        if (!_selected.IsVerified)
        {
            var proceed = MessageBox.Show(
                $"\"{_selected.Name}\" is unverified. It hasn't been reviewed and could contain malicious code. Install anyway?",
                "Unverified Mod", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
            if (proceed != DialogResult.Yes) return;
        }

        var loaderKind = AppState.DetectLoaderKind();
        if (AppState.GameDir == null) { _statusLabel.Text = "No game path set."; return; }
        if (!AppState.IsLoaderInstalled(loaderKind, AppState.GameDir))
        {
            string loaderName = AppState.LoaderName(loaderKind);
            var install = MessageBox.Show(
                $"{loaderName} isn't installed for this game yet, so mods won't do anything without it. Install {loaderName} now?",
                $"{loaderName} Required", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
            if (install != DialogResult.Yes) return;

            _statusLabel.Text = $"Installing {loaderName}...";
            var loaderProg = new Progress<(int percent, string status)>(p => { _statusLabel.Text = $"[{p.percent}%] {p.status}"; _progressStrip.Value = p.percent; });
            try { await OperationQueue.RunAsync($"Install {loaderName}", p => AppState.InstallLoaderAsync(loaderKind, AppState.GameDir, p), loaderProg); }
            catch (Exception ex) { _statusLabel.Text = $"{loaderName} install failed: " + ex.Message; _progressStrip.Value = 0; return; }
        }

        var prog = new Progress<(int, string)>(r => { _statusLabel.Text = $"[{r.Item1}%] {r.Item2}"; _progressStrip.Value = r.Item1; });
        try
        {
            if (_selected.Source == ModSource.Thunderstore)
            {
                if (string.IsNullOrEmpty(_selected.DownloadUrl)) { _statusLabel.Text = "This package has no download available."; return; }
                await OperationQueue.RunAsync($"Install {_selected.Name}", p => ModInstaller.InstallThunderstoreAsync(_selected.DownloadUrl, _selected.DllName, p), prog);
                await InstallDependenciesAsync(_selected, new HashSet<string>(StringComparer.OrdinalIgnoreCase) { _selected.DllName });
            }
            else
            {
                var url = DllUrl(_selected);
                if (url == null) { _statusLabel.Text = "This mod has no download available."; return; }
                await OperationQueue.RunAsync($"Install {_selected.Name}", p => ModInstaller.InstallAsync(url, _selected.DllName.Replace(".dll", ""), p), prog);
            }
            if (_selected.Version is { Length: > 0 }) DataBridge.SetModVersion(ModKey(_selected), _selected.Version);
            ShowDetail(_selected);
        }
        catch (Exception ex) { _statusLabel.Text = "Install failed: " + ex.Message; }
        finally { _progressStrip.Value = 0; }
    }

    private async Task InstallDependenciesAsync(SbMod mod, HashSet<string> visited)
    {
        foreach (var dep in mod.Dependencies)
        {
            int lastDash = dep.LastIndexOf('-');
            if (lastDash < 0) continue;
            string depFullName = dep[..lastDash];

            if (!visited.Add(depFullName)) continue;
            if (depFullName.Contains("BepInExPack", StringComparison.OrdinalIgnoreCase)) continue;
            if (depFullName.Contains("MelonLoader", StringComparison.OrdinalIgnoreCase)) continue;

            var depMod = _thunderstoreCache.FirstOrDefault(m => m.DllName.Equals(depFullName, StringComparison.OrdinalIgnoreCase));
            if (depMod == null || string.IsNullOrEmpty(depMod.DownloadUrl)) continue;
            if (IsInstalled(depMod)) continue;

            _statusLabel.Text = $"Installing dependency {depMod.Name}...";
            var prog = new Progress<(int percent, string status)>(r => { _statusLabel.Text = $"[{r.percent}%] {depMod.Name}: {r.status}"; _progressStrip.Value = r.percent; });
            await OperationQueue.RunAsync($"Install dependency {depMod.Name}", p => ModInstaller.InstallThunderstoreAsync(depMod.DownloadUrl, depMod.DllName, p), prog);
            if (depMod.Version is { Length: > 0 }) DataBridge.SetModVersion(ModKey(depMod), depMod.Version);

            await InstallDependenciesAsync(depMod, visited);
        }
    }

    private static string? DllUrl(SbMod m) =>
        m.Source == ModSource.Thunderstore
            ? m.DownloadUrl
            : string.IsNullOrEmpty(m.RepoUrl) || string.IsNullOrEmpty(m.DllName)
                ? null
                : m.RepoUrl.TrimEnd('/') + "/releases/latest/download/" + m.DllName;

    private void DoUninstall()
    {
        if (_selected == null) return;

        var dependents = _thunderstoreCache
            .Where(m => !m.DllName.Equals(_selected.DllName, StringComparison.OrdinalIgnoreCase))
            .Where(m => m.Dependencies.Any(d => StripDepVersion(d).Equals(_selected.DllName, StringComparison.OrdinalIgnoreCase)))
            .Where(IsInstalled)
            .Select(m => m.Name)
            .ToList();

        if (dependents.Count > 0)
        {
            var proceed = MessageBox.Show(
                $"{string.Join(", ", dependents)} depend{(dependents.Count == 1 ? "s" : "")} on this mod and may break without it. Uninstall anyway?",
                "Other Mods Depend On This", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
            if (proceed != DialogResult.Yes) return;
        }

        ModInstaller.Uninstall(ModKey(_selected));
        DataBridge.DeleteModVersion(ModKey(_selected));
        ShowDetail(_selected);
    }

    private static string StripDepVersion(string dep)
    {
        int lastDash = dep.LastIndexOf('-');
        return lastDash >= 0 ? dep[..lastDash] : dep;
    }

    private static string ModKey(SbMod mod) =>
        mod.Source == ModSource.Thunderstore
            ? (AppState.DetectLoaderKind() == ModLoaderKind.MelonLoader ? ThunderstoreMelonDllName(mod.DllName) : mod.DllName)
            : mod.DllName.Replace(".dll", "");

    private ContextMenuStrip BuildModContextMenu()
    {
        var menu = new ContextMenuStrip { BackColor = Color.FromArgb(30, 30, 30), ForeColor = Color.White };
        menu.Opening += (_, e) =>
        {
            if (_selected == null) { e.Cancel = true; return; }
            menu.Items.Clear();

            bool installed = IsInstalled(_selected);
            bool disabled = !installed && IsDisabled(_selected);

            var installItem = menu.Items.Add(installed || disabled ? "Uninstall" : "Install");
            installItem.Click += async (_, __) => { if (installed || disabled) DoUninstall(); else await DoInstall(); };

            if (!string.IsNullOrEmpty(_selected.RepoUrl))
            {
                menu.Items.Add(_selected.Source == ModSource.Thunderstore ? "Open Thunderstore Page" : "Open GitHub Page")
                    .Click += (_, __) => OpenUrl(_selected.RepoUrl);
                menu.Items.Add("Copy Link").Click += (_, __) =>
                {
                    try { Clipboard.SetText(_selected.RepoUrl); } catch { }
                };
            }
        };
        return menu;
    }

    private static string ThunderstoreMelonDllName(string fullName)
    {
        int dash = fullName.IndexOf('-');
        return dash >= 0 ? fullName[(dash + 1)..] : fullName;
    }

    private async Task DoUpvote()
    {
        if (_selected == null || _selected.Source != ModSource.OurApi) return;
        if (string.IsNullOrEmpty(_token)) { await HandleLogin(); return; }
        bool now = await SbApi.ToggleUpvoteAsync(_selected.Id);
        if (now) { _upvoted.Add(_selected.Id); _selected.Upvotes++; }
        else     { _upvoted.Remove(_selected.Id); _selected.Upvotes = Math.Max(0, _selected.Upvotes - 1); }
        ShowDetail(_selected);
        _list.Invalidate();
    }

    private async Task HandleLoginButtonClick()
    {
        if (!string.IsNullOrEmpty(_token)) { DoLogout(); return; }
        await HandleLogin();
    }

    private void DoLogout()
    {
        _token = null;
        _username = null;
        SbApi.Token = null;
        DataBridge.DeleteToken();
        _upvoted.Clear();
        UpdateLoginBtn();
        _list.Invalidate();
        if (_selected != null) ShowDetail(_selected);
        _statusLabel.Text = "Logged out.";
    }

    private async Task HandleLogin()
    {
        if (_loginPending && !string.IsNullOrEmpty(_loginCode)) { OpenUrl($"https://3gv.org/link?code={_loginCode}"); return; }
        _loginPending = true;
        _loginBtn.Text = "...";

        _loginCode = await SbApi.GenerateLoginCodeAsync();
        if (string.IsNullOrEmpty(_loginCode)) { _loginPending = false; _loginBtn.Text = "Login"; return; }

        OpenUrl($"https://3gv.org/link?code={_loginCode}");
        _loginBtn.Text = _loginCode;
        _statusLabel.Text = $"Enter code {_loginCode} at 3gv.org/link";

        _ = Task.Run(async () =>
        {
            for (int i = 0; i < 60; i++)
            {
                await Task.Delay(3000);
                string? token = await SbApi.PollLoginCodeAsync(_loginCode!);
                if (!string.IsNullOrEmpty(token))
                {
                    string? user = await SbApi.ValidateTokenAsync(token);
                    if (!string.IsNullOrEmpty(user))
                    {
                        _token = token; _username = user;
                        SbApi.Token = token;
                        DataBridge.SaveToken(token);
                        var upvotes = await SbApi.GetUserUpvotesAsync(_mods);
                        Invoke(() =>
                        {
                            _loginPending = false; _loginCode = null;
                            _upvoted = upvotes;
                            UpdateLoginBtn();
                            _list.Invalidate();
                            if (_selected != null) ShowDetail(_selected);
                            _statusLabel.Text = $"Logged in as @{user}";
                        });
                        return;
                    }
                }
            }
            Invoke(() => { _loginPending = false; _loginCode = null; UpdateLoginBtn(); _statusLabel.Text = "Login timed out."; });
        });
    }

    private void UpdateLoginBtn()
    {
        _loginBtn.Text = string.IsNullOrEmpty(_username) ? "Login" : $"@{_username}";
    }

    private void InstallExternalMod()
    {
        using var dlg = new OpenFileDialog { Title = "Select Mod DLL", Filter = "DLL files|*.dll" };
        if (dlg.ShowDialog() != DialogResult.OK) return;

        try
        {
            ModInstaller.InstallLocal(dlg.FileName);
            _statusLabel.Text = $"{Path.GetFileNameWithoutExtension(dlg.FileName)} installed.";
        }
        catch (Exception ex)
        {
            _statusLabel.Text = "Install failed: " + ex.Message;
        }
    }

    private void DrawModItem(object? sender, DrawItemEventArgs e)
    {
        if (e.Index < 0 || e.Index >= _list.Items.Count) return;
        var mod = (SbMod)_list.Items[e.Index]!;
        bool sel = (e.State & DrawItemState.Selected) != 0;
        var t = ThemeEngine.Current;

        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        e.Graphics.FillRectangle(new SolidBrush(t.Surface), e.Bounds);
        if (sel)
        {
            var hi = new Rectangle(e.Bounds.Left + 4, e.Bounds.Top + 2, e.Bounds.Width - 8, e.Bounds.Height - 4);
            using var path = RoundedGraphics.RoundedRect(hi, 8);
            using var brush = new SolidBrush(t.Highlight);
            e.Graphics.FillPath(brush, path);
        }

        string badge = mod.IsFeatured ? "Featured" : mod.IsVerified ? "Verified" : "Unverified";
        var badgeColor = mod.IsFeatured ? Color.FromArgb(255, 189, 46)
                       : mod.IsVerified ? Color.FromArgb(39, 201, 63)
                                        : Color.FromArgb(136, 136, 136);
        var badgeFont = new Font("Segoe UI", 7f, FontStyle.Regular);
        var nameFont  = new Font("Segoe UI", 10f, FontStyle.Bold);
        var subFont   = new Font("Segoe UI", 8f);

        var icon = GetModIcon(mod);
        int textRight = e.Bounds.Right - 12;
        if (icon != null)
        {
            const int iconSize = 40;
            var iconRect = new Rectangle(e.Bounds.Right - iconSize - 12, e.Bounds.Top + (e.Bounds.Height - iconSize) / 2, iconSize, iconSize);
            using var path = RoundedGraphics.RoundedRect(iconRect, 6);
            var oldClip = e.Graphics.Clip;
            e.Graphics.SetClip(path, System.Drawing.Drawing2D.CombineMode.Intersect);
            e.Graphics.DrawImage(icon, iconRect);
            e.Graphics.Clip = oldClip;
            textRight = iconRect.Left - 8;
        }

        int x = e.Bounds.Left + 12, y = e.Bounds.Top + 8;
        int textWidth = Math.Max(0, textRight - x);
        e.Graphics.DrawString(badge, badgeFont, new SolidBrush(badgeColor), x, y);
        using (var nameFormat = new StringFormat { Trimming = StringTrimming.EllipsisCharacter, FormatFlags = StringFormatFlags.NoWrap })
            e.Graphics.DrawString(mod.Name, nameFont, new SolidBrush(t.Text), new RectangleF(x, y + 14, textWidth, 20), nameFormat);
        e.Graphics.DrawString($"@{mod.Author}  ·  ▲{mod.Upvotes}", subFont, new SolidBrush(t.SubText), x, y + 32);

        if (DataBridge.GetModVersion(mod.RepoUrl) is { Length: > 0 })
        {
            var dot = new SolidBrush(Color.FromArgb(255, 95, 86));
            int dotX = icon != null ? e.Bounds.Right - 56 : e.Bounds.Right - 16;
            e.Graphics.FillEllipse(dot, dotX, e.Bounds.Top + 8, 8, 8);
        }

        e.Graphics.DrawLine(new Pen(t.Border), e.Bounds.Left, e.Bounds.Bottom - 1, e.Bounds.Right, e.Bounds.Bottom - 1);
    }

    private Image? GetModIcon(SbMod mod)
    {
        if (string.IsNullOrEmpty(mod.IconUrl)) return null;
        if (_iconCache.TryGetValue(mod.IconUrl, out var img)) return img;
        if (_iconLoading.Add(mod.IconUrl)) _ = LoadIconAsync(mod.IconUrl);
        return null;
    }

    private async Task LoadIconAsync(string url)
    {
        var bytes = await ThunderstoreApi.DownloadBytesAsync(url);
        try { _iconCache[url] = bytes != null ? Image.FromStream(new MemoryStream(bytes)) : null; }
        catch { _iconCache[url] = null; }
        _iconLoading.Remove(url);
        if (IsHandleCreated) _list.Invalidate();
    }

    private bool IsInstalled(SbMod mod)
    {
        if (AppState.ModsInstallDir is not { } p) return false;
        if (mod.Source == ModSource.Thunderstore)
        {
            if (AppState.DetectLoaderKind() == ModLoaderKind.MelonLoader)
                return File.Exists(Path.Combine(p, ThunderstoreMelonDllName(mod.DllName) + ".dll"));
            string dir = Path.Combine(p, mod.DllName);
            return Directory.Exists(dir) && Directory.EnumerateFiles(dir, "*.dll", SearchOption.AllDirectories).Any();
        }
        return File.Exists(Path.Combine(p, mod.DllName));
    }

    private bool IsDisabled(SbMod mod)
    {
        if (AppState.ModsInstallDir is not { } p) return false;
        if (mod.Source == ModSource.Thunderstore)
        {
            if (AppState.DetectLoaderKind() == ModLoaderKind.MelonLoader)
                return File.Exists(Path.Combine(p, ThunderstoreMelonDllName(mod.DllName) + ".dll.disabled"));
            string dir = Path.Combine(p, mod.DllName);
            return Directory.Exists(dir) && Directory.EnumerateFiles(dir, "*.dll.disabled", SearchOption.AllDirectories).Any();
        }
        return File.Exists(Path.Combine(p, mod.DllName + ".disabled"));
    }

    private static void OpenUrl(string url)
    {
        try { System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo { FileName = url, UseShellExecute = true }); }
        catch { }
    }

    private void ApplyTheme()
    {
        var t = ThemeEngine.Current;
        BackColor     = t.Background;
        _topBar.BackColor = t.Surface;
        _bodyWrap.BackColor = t.Background;
        _listPanel.BackColor = t.SurfaceAlt;
        _listPanel.BorderColor = Color.Transparent;
        _list.BackColor   = t.Surface;
        _list.ForeColor   = t.Text;
        _detail.BackColor = t.SurfaceAlt;
        _detail.BorderColor = Color.Transparent;
        _statusLabel.ForeColor = t.SubText;
        _statusLabel.BackColor = t.SurfaceAlt;
        _progressStrip.TrackColor = t.SurfaceAlt;
        _progressStrip.FillColor = t.Accent;
        ThemeEngine.StyleRButton(_loadMoreBtn);
        ThemeEngine.StyleRButton(_loginBtn);
        ThemeEngine.StyleRButton(_installExternalBtn);
        _search.BackColor = t.SurfaceAlt;
        _search.ForeColor = t.Text;
        ThemeEngine.ApplyLayoutCornerStyle(_search);
        ThemeEngine.StyleRButton(_installBtn, accent: true);
        foreach (var b in new[] { _uninstallBtn, _upvoteBtn, _ghBtn, _changelogBtn })
            ThemeEngine.StyleRButton(b);
        _detailName.BackColor   = t.SurfaceAlt;
        _detailName.ForeColor   = t.Text;
        _detailAuthor.BackColor = t.SurfaceAlt;
        _detailAuthor.ForeColor = t.SubText;
        _detailDesc.BackColor   = t.SurfaceAlt;
        _detailDesc.ForeColor   = t.SubText;
        _detailStatus.BackColor = t.SurfaceAlt;

        _tabDropdown.FillColor = Color.Transparent;
        _tabDropdown.HoverFillColor = t.SurfaceAlt;
        _tabDropdown.BorderColor = Color.Transparent;
        _tabDropdown.ForeColor = t.Text;
        ThemeEngine.ApplyLayoutCornerStyle(_tabDropdown);

        ThemeEngine.StyleGhostButton(_sourceBtn);

        _list.Invalidate();
        ThemeEngine.ApplyScrollTheme(this);
    }

    private static RButton MakeBtn(string text, string? tag)
    {
        return new RButton
        {
            Text = text,
            Style = RButtonStyle.Outline,
            CornerRadius = 8,
            AutoSize = true,
            Padding = new Padding(10, 4, 10, 4),
            Tag = tag,
        };
    }

    private static Label MakeLabel(string text, float size, bool bold = false)
    {
        return new Label
        {
            Text = text,
            Font = new Font("Segoe UI", size, bold ? FontStyle.Bold : FontStyle.Regular),
            AutoSize = true,
            Padding = new Padding(0, 4, 0, 4),
        };
    }

    protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
    {
        if (keyData == (Keys.Control | Keys.F)) { _search.Inner.Focus(); return true; }
        if (keyData == Keys.Delete && _selected != null && _uninstallBtn.Visible) { DoUninstall(); return true; }
        return base.ProcessCmdKey(ref msg, keyData);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) ThemeEngine.ThemeChanged -= ApplyTheme;
        base.Dispose(disposing);
    }
}
