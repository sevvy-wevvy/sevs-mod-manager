using SevsModManager.UI.Panels;
using System.Runtime.InteropServices;
using SevsModManager.Core;
using SevsModManager.UI.Controls;
using System.Drawing.Drawing2D;
using SevsModManager.Theme;

namespace SevsModManager.UI;

internal sealed class MainForm : Form
{
    private const int WindowRadius = 14;
    private const int TabRadius    = 8;

    private static readonly string[] NavLabels = { "Mods", "Installed", "Logs", "Loadouts", "Modpacks", "Config", "Settings" };

    private readonly AppLayout _layout;

    private Panel?  _titleBar;
    private Label?  _titleLabel;
    private Label?  _gameLabel;
    private Panel?  _titleGameIcon;
    private Panel?  _gameBox;
    private Panel?  _titleIconBox;
    private RButton? _playBtn;
    private System.Windows.Forms.Timer? _gameRunningTimer;
    private bool _gameRunning;
    private DateTime? _launchedAt;

    private Panel?      _tabStrip;
    private RButton[]?  _navButtons;
    private int         _activeNavIndex;
    private float _underlineX, _underlineW, _targetX, _targetW;
    private System.Windows.Forms.Timer? _underlineTimer;

    private Panel? _sidebar;
    private Label? _sidebarMascotLabel;
    private PictureBox? _sidebarMascotBox;
    private Panel? _sidebarBottomRow;

    private readonly ModsPanel      _modsPanel;
    private readonly InstalledPanel _installedPanel;
    private readonly LogsPanel      _logsPanel;
    private readonly LoadoutsPanel  _loadoutsPanel;
    private readonly ModpacksPanel  _modpacksPanel;
    private readonly ConfigPanel    _configPanel;
    private readonly SettingsPanel  _settingsPanel;
    private readonly Control[]  _navPanels;
    private readonly Action?[]  _navActivate;

    private readonly Panel  _statusBar;
    private readonly Label  _statusLabel;
    private Panel?  _resizeHandle;

    private Point _dragStart;
    private bool  _dragging;

    private readonly string? _pendingPackPath;

    public MainForm(string? pendingPackPath = null)
    {
        _pendingPackPath = pendingPackPath;
        _layout = AppState.Settings.Layout;
        Icon = AppIcons.Icon;

        StartPosition = FormStartPosition.CenterScreen;

        int extra = _layout == AppLayout.R2Modman ? 200 : 0;
        Size          = new Size(1100 + extra, 720);
        MinimumSize   = new Size(860 + extra, 560);
        DoubleBuffered = true;
        Text          = "Sev's Mod Manager";

        _modsPanel      = new ModsPanel     { Visible = true };
        _installedPanel = new InstalledPanel{ Visible = false };
        _logsPanel      = new LogsPanel     { Visible = false };
        _loadoutsPanel  = new LoadoutsPanel { Visible = false };
        _modpacksPanel  = new ModpacksPanel { Visible = false };
        _configPanel    = new ConfigPanel   { Visible = false };
        _settingsPanel  = new SettingsPanel { Visible = false };

        _navPanels = new Control[] { _modsPanel, _installedPanel, _logsPanel, _loadoutsPanel, _modpacksPanel, _configPanel, _settingsPanel };
        _navActivate = new Action?[]
        {
            () => _modsPanel.RefreshModSource(),
            () => _installedPanel.Refresh_(),
            () => _logsPanel.Refresh_(),
            () => _loadoutsPanel.Refresh_(),
            () => _modpacksPanel.Refresh_(),
            () => _configPanel.Refresh_(),
            () => _settingsPanel.Reload(),
        };

        var content = new Panel { Dock = DockStyle.Fill };
        foreach (var p in _navPanels) content.Controls.Add(p);

        _statusBar   = new Panel { Dock = DockStyle.Bottom, Height = 24, Padding = new Padding(8, 0, 8, 0) };
        _statusLabel = new Label { Dock = DockStyle.Left, AutoSize = true, TextAlign = ContentAlignment.MiddleLeft, Font = new Font("Segoe UI", 8f) };
        _statusBar.Controls.Add(_statusLabel);
        if (_layout == AppLayout.SevsModManager)
        {
            _resizeHandle = MakeResizeHandle();
            _statusBar.Controls.Add(_resizeHandle);
        }

        Controls.Add(content);
        Controls.Add(_statusBar);

        AllowDrop = true;
        DragEnter += MainForm_DragEnter;
        DragDrop  += MainForm_DragDrop;

        switch (_layout)
        {
            case AppLayout.R2Modman:        BuildR2ModmanChrome(); break;
            case AppLayout.MonkeModManager:  BuildMmmChrome(); break;
            default:                        BuildSmmChrome(); break;
        }

        _gameRunningTimer = new System.Windows.Forms.Timer { Interval = 1000 };
        _gameRunningTimer.Tick += (_, __) => CheckGameRunning();
        _gameRunningTimer.Start();

        DiscordPresence.Start();

        ThemeEngine.ThemeChanged += ApplyTheme;
        ApplyTheme();

        Load += async (_, __) =>
        {
            SetStatus("Starting...");
            UpdateGameLabel();
            CheckGameRunning();
            if (_navButtons != null) HighlightNav(0, animate: false);

            SetStatus("Loading mod catalog...");
            await _modsPanel.InitAsync();
            SetStatus("Ready.");

            if (_pendingPackPath != null)
            {
                _modpacksPanel.Refresh_();
                SwitchToIndex(3);
                await _modpacksPanel.PromptInstallExternalPack(_pendingPackPath);
            }

            CheckWhatsNew();
            await CheckForUpdatesAsync();
        };
    }

    private static void CheckWhatsNew()
    {
        if (AppState.Settings.AppVersionSeen > 0 && AppState.Settings.AppVersionSeen < Program.CurrentVersion)
            UpdateDialogs.ShowWhatsNew("Thanks for updating! Check the GitHub releases page for what's changed in this version.");

        AppState.Settings.AppVersionSeen = Program.CurrentVersion;
        AppState.Save();
    }

    private static async Task CheckForUpdatesAsync()
    {
        if (!AppState.Settings.AutoCheckUpdates) return;

        int? latest = await UpdateChecker.GetLatestVersionAsync();
        if (latest is not { } v || v <= Program.CurrentVersion) return;

        bool getIt = UpdateDialogs.ShowUpdateAvailable(
            $"A new version of Sev's Mod Manager is available (v{v}, you're on v{Program.CurrentVersion}).");
        if (getIt)
        {
            try { System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo("https://github.com/sevvy-wevvy/sevs-mod-manager/releases/latest") { UseShellExecute = true }); }
            catch { }
        }
    }

    private void BuildSmmChrome()
    {
        FormBorderStyle = FormBorderStyle.None;

        _titleBar = new Panel { Dock = DockStyle.Top, Height = 40 };
        _titleBar.MouseDown += TitleBar_MouseDown;
        _titleBar.MouseMove += TitleBar_MouseMove;
        _titleBar.MouseUp   += (_, __) => _dragging = false;

        var closeBtn = MakeLight(Color.FromArgb(255, 95,  86));
        var minBtn   = MakeLight(Color.FromArgb(255, 189, 46));
        var maxBtn   = MakeLight(Color.FromArgb(39,  201, 63));

        var lights = new Panel { Width = 78, Height = 40, Dock = DockStyle.Left };
        lights.MouseDown += TitleBar_MouseDown;
        lights.MouseMove += TitleBar_MouseMove;
        lights.MouseUp   += (_, __) => _dragging = false;

        int lx = 14;
        foreach (var btn in new[] { closeBtn, minBtn, maxBtn })
        {
            btn.Location = new Point(lx, 13);
            btn.Cursor   = Cursors.Hand;
            lights.Controls.Add(btn);
            lx += 22;
        }

        closeBtn.Click += (_, __) => Close();
        minBtn.Click   += (_, __) => WindowState = FormWindowState.Minimized;
        maxBtn.Click   += (_, __) =>
            WindowState = WindowState == FormWindowState.Maximized ? FormWindowState.Normal : FormWindowState.Maximized;

        _titleLabel = new Label
        {
            Text = "Sev's Mod Manager",
            Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleCenter,
            Font = new Font("Segoe UI", 10f, FontStyle.Regular),
        };
        _titleLabel.MouseDown += TitleBar_MouseDown;
        _titleLabel.MouseMove += TitleBar_MouseMove;
        _titleLabel.MouseUp   += (_, __) => _dragging = false;

        _gameBox = new Panel { Dock = DockStyle.Right, Width = 200 };

        const int titleIconSize = 20;
        _titleIconBox = new Panel { Dock = DockStyle.Right, Width = 32 };
        _titleGameIcon = MakeCircularIcon(titleIconSize);
        _titleGameIcon.Location = new Point((32 - titleIconSize) / 2, (40 - titleIconSize) / 2);
        _titleIconBox.Controls.Add(_titleGameIcon);

        _gameLabel = new Label
        {
            Text = "",
            Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleRight,
            Font = new Font("Segoe UI", 8.5f),
            Padding = new Padding(0, 0, 8, 0),
            Cursor = Cursors.Hand,
        };
        _gameLabel.Click += (_, __) => ShowQuickSwitchMenu(_gameLabel);
        _titleIconBox.Cursor = Cursors.Hand;
        _titleIconBox.Click += (_, __) => ShowQuickSwitchMenu(_titleIconBox);

        _gameBox.Controls.Add(_gameLabel);
        _gameBox.Controls.Add(_titleIconBox);

        _titleBar.Controls.Add(_gameBox);
        _titleBar.Controls.Add(_titleLabel);
        _titleBar.Controls.Add(lights);

        BuildTabStrip();
        Controls.Add(_titleBar);

        Load        += (_, __) => ApplyRoundedRegion();
        Resize      += (_, __) => ApplyRoundedRegion();
        SizeChanged += (_, __) => ApplyRoundedRegion();
    }

    private void BuildMmmChrome()
    {
        FormBorderStyle = FormBorderStyle.Sizable;
        HandleCreated += (_, __) => ForceRoundedCorners();
        BuildTabStrip();
    }

    private void BuildTabStrip()
    {
        _tabStrip = new Panel { Dock = DockStyle.Top, Height = 38 };
        _tabStrip.Paint += TabStrip_Paint;

        _navButtons = NavLabels.Select(MakeTabBtn).ToArray();
        for (int i = 0; i < _navButtons.Length; i++)
        {
            int idx = i;
            _navButtons[i].Click += (_, __) => SwitchToIndex(idx);
        }

        var tabFlow = new FlowLayoutPanel { Dock = DockStyle.Left, AutoSize = true, FlowDirection = FlowDirection.LeftToRight, WrapContents = false, Padding = new Padding(8, 0, 0, 0) };
        foreach (var b in _navButtons) tabFlow.Controls.Add(b);
        _tabStrip.Controls.Add(tabFlow);

        int tabHeight = _layout == AppLayout.MonkeModManager ? 38 : 32;
        int vpad = (38 - tabHeight) / 2;
        var playWrap = new Panel { Dock = DockStyle.Right, Width = 130, Padding = new Padding(8, vpad, 8, vpad) };
        _playBtn = MakePlayBtn();
        _playBtn.Dock = DockStyle.Fill;
        playWrap.Controls.Add(_playBtn);
        _tabStrip.Controls.Add(playWrap);

        _underlineTimer = new System.Windows.Forms.Timer { Interval = 12 };
        _underlineTimer.Tick += (_, __) => StepUnderlineAnim();

        var tabSep = new Panel { Dock = DockStyle.Top, Height = 1 };
        tabSep.Tag = "border_panel";

        Controls.Add(tabSep);
        Controls.Add(_tabStrip);
    }

    private void BuildR2ModmanChrome()
    {
        FormBorderStyle = FormBorderStyle.Sizable;

        HandleCreated += (_, __) => { ForceDarkTitleBar(); ForceSquareCorners(); };

        _sidebar = new Panel { Dock = DockStyle.Left, Width = 200 };

        _navButtons = NavLabels.Select(MakeSidebarBtn).ToArray();
        for (int i = 0; i < _navButtons.Length; i++)
        {
            int idx = i;
            _navButtons[i].Click += (_, __) => SwitchToIndex(idx);
        }

        var navHost = new Panel { Dock = DockStyle.Top, Height = _navButtons.Length * 38 + 8, Padding = new Padding(8, 4, 8, 4) };
        foreach (var b in _navButtons.Reverse()) navHost.Controls.Add(b);

        _sidebarMascotBox = new PictureBox
        {
            Image = AppIcons.TryExtractGameIcon(AppState.Settings.GamePath) ?? AppIcons.Png,
            SizeMode = PictureBoxSizeMode.Zoom, Width = 32, Height = 32, Location = new Point(12, 8),
            Cursor = Cursors.Hand,
        };
        _sidebarMascotLabel = new Label
        {
            Text = AppState.Settings.GameName, AutoSize = true, Font = new Font("Segoe UI", 8.5f),
            Location = new Point(52, 16), Cursor = Cursors.Hand,
        };
        _sidebarBottomRow = new Panel { Dock = DockStyle.Bottom, Height = 48 };
        _sidebarMascotBox.Click += (_, __) => ShowQuickSwitchMenu(_sidebarMascotBox);
        _sidebarMascotLabel.Click += (_, __) => ShowQuickSwitchMenu(_sidebarMascotLabel);
        _sidebarBottomRow.Controls.Add(_sidebarMascotBox);
        _sidebarBottomRow.Controls.Add(_sidebarMascotLabel);

        var playRow = new Panel { Dock = DockStyle.Bottom, Height = 44, Padding = new Padding(8, 4, 8, 4) };
        _playBtn = MakePlayBtn();
        _playBtn.Dock = DockStyle.Fill;
        playRow.Controls.Add(_playBtn);

        _sidebar.Controls.Add(navHost);
        _sidebar.Controls.Add(playRow);
        _sidebar.Controls.Add(_sidebarBottomRow);

        Controls.Add(_sidebar);
    }

    [DllImport("dwmapi.dll")]
    private static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int attrValue, int attrSize);

    private void ForceDarkTitleBar()
    {
        const int DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
        int useDark = 1;
        DwmSetWindowAttribute(Handle, DWMWA_USE_IMMERSIVE_DARK_MODE, ref useDark, sizeof(int));
    }

    private void ForceSquareCorners()
    {
        const int DWMWA_WINDOW_CORNER_PREFERENCE = 33;
        const int DWMWCP_DONOTROUND = 1;
        int pref = DWMWCP_DONOTROUND;
        DwmSetWindowAttribute(Handle, DWMWA_WINDOW_CORNER_PREFERENCE, ref pref, sizeof(int));
    }

    private void ForceRoundedCorners()
    {
        const int DWMWA_WINDOW_CORNER_PREFERENCE = 33;
        const int DWMWCP_ROUND = 2;
        int pref = DWMWCP_ROUND;
        DwmSetWindowAttribute(Handle, DWMWA_WINDOW_CORNER_PREFERENCE, ref pref, sizeof(int));
    }

    private void ApplyRoundedRegion()
    {
        if (FormBorderStyle != FormBorderStyle.None) return;
        if (WindowState == FormWindowState.Maximized) { Region = null; return; }
        if (ClientSize.Width <= 0 || ClientSize.Height <= 0) return;
        using var path = RoundedGraphics.RoundedRect(new Rectangle(0, 0, ClientSize.Width, ClientSize.Height), WindowRadius);
        Region = new Region(path);

        Invalidate(true);
    }

    private void SwitchToIndex(int idx)
    {
        for (int i = 0; i < _navPanels.Length; i++) _navPanels[i].Visible = i == idx;
        _navActivate[idx]?.Invoke();
        HighlightNav(idx);
    }

    private void HighlightNav(int idx, bool animate = true)
    {
        _activeNavIndex = idx;
        if (_navButtons == null) return;

        if (_layout == AppLayout.R2Modman)
        {
            var t = ThemeEngine.Current;
            for (int i = 0; i < _navButtons.Length; i++)
            {
                bool active = i == idx;
                _navButtons[i].FillColor = active ? t.Accent : Color.Transparent;
                _navButtons[i].HoverFillColor = active ? t.Accent : t.SurfaceAlt;
                _navButtons[i].ForeColor = active ? t.AccentText : t.Text;
            }
            return;
        }

        foreach (var b in _navButtons)
        {
            bool active = b == _navButtons[idx];
            b.Font = new Font("Segoe UI", 9.5f, active ? FontStyle.Bold : FontStyle.Regular);
        }

        _targetX = _navButtons[idx].Bounds.Left + 6;
        _targetW = _navButtons[idx].Bounds.Width - 12;

        if (!animate) { _underlineX = _targetX; _underlineW = _targetW; }
        _underlineTimer!.Start();
        _tabStrip!.Invalidate();
    }

    private void StepUnderlineAnim()
    {
        const float step = 0.25f;
        bool doneX = Math.Abs(_underlineX - _targetX) < 0.5f;
        bool doneW = Math.Abs(_underlineW - _targetW) < 0.5f;
        _underlineX += (_targetX - _underlineX) * step;
        _underlineW += (_targetW - _underlineW) * step;
        if (doneX && doneW) { _underlineX = _targetX; _underlineW = _targetW; _underlineTimer!.Stop(); }
        _tabStrip!.Invalidate();
    }

    private void TabStrip_Paint(object? sender, PaintEventArgs e)
    {
        if (_navButtons == null || _underlineW <= 0) return;
        var t = ThemeEngine.Current;
        e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
        var rect = new RectangleF(_underlineX, _tabStrip!.Height - 4, _underlineW, 3);
        using var path = RoundedGraphics.RoundedRect(Rectangle.Round(rect), 2);
        using var brush = new SolidBrush(t.Accent);
        e.Graphics.FillPath(brush, path);
    }

    private void TitleBar_MouseDown(object? s, MouseEventArgs e)
    {
        if (e.Button != MouseButtons.Left) return;
        _dragging  = true;
        _dragStart = e.Location;
    }

    private void TitleBar_MouseMove(object? s, MouseEventArgs e)
    {
        if (!_dragging || e.Button != MouseButtons.Left) return;
        if (WindowState == FormWindowState.Maximized) WindowState = FormWindowState.Normal;
        Location = new Point(Location.X + e.X - _dragStart.X, Location.Y + e.Y - _dragStart.Y);
    }

    protected override CreateParams CreateParams
    {
        get
        {
            var cp = base.CreateParams;
            if (FormBorderStyle == FormBorderStyle.None) cp.ClassStyle |= 0x20000;
            return cp;
        }
    }

    protected override void WndProc(ref Message m)
    {
        const int WM_NCHITTEST = 0x84;
        const int grip = 8;
        if (FormBorderStyle == FormBorderStyle.None && m.Msg == WM_NCHITTEST && WindowState == FormWindowState.Normal)
        {
            base.WndProc(ref m);
            if ((int)m.Result == 1)
            {
                var p = PointToClient(Cursor.Position);
                bool r = p.X >= ClientSize.Width - grip;
                bool b = p.Y >= ClientSize.Height - grip;
                bool l = p.X < grip;
                bool t = p.Y < grip;
                if      (b && r) m.Result = (IntPtr)17;
                else if (b && l) m.Result = (IntPtr)16;
                else if (t && r) m.Result = (IntPtr)14;
                else if (t && l) m.Result = (IntPtr)13;
                else if (r)      m.Result = (IntPtr)11;
                else if (b)      m.Result = (IntPtr)15;
                else if (l)      m.Result = (IntPtr)10;
            }
            return;
        }
        base.WndProc(ref m);
    }

    private void ApplyTheme()
    {
        var t = ThemeEngine.Current;
        BackColor = t.Background;

        if (_titleBar != null)
        {
            _titleBar.BackColor = t.Surface;
            if (_titleLabel != null) { _titleLabel.BackColor = t.Surface; _titleLabel.ForeColor = t.Text; }
            if (_gameBox != null) _gameBox.BackColor = t.Surface;
            if (_titleIconBox != null) _titleIconBox.BackColor = t.Surface;
            if (_gameLabel != null) { _gameLabel.BackColor = t.Surface; _gameLabel.ForeColor = t.SubText; }
        }

        if (_tabStrip != null && _navButtons != null)
        {
            _tabStrip.BackColor = t.Surface;
            foreach (var b in _navButtons)
            {
                bool mmm = _layout == AppLayout.MonkeModManager;
                b.Style = RButtonStyle.Ghost;
                b.FillColor = Color.Transparent;
                b.HoverFillColor = t.SurfaceAlt;
                b.BorderColor = mmm ? t.Border : Color.Transparent;
                b.HoverBorderColor = mmm ? t.Border : Color.Transparent;
                b.BorderWidth = mmm ? 2f : 1.2f;
                b.ForeColor = t.Text;
            }
        }

        if (_sidebar != null)
        {
            _sidebar.BackColor = t.Surface;
            if (_sidebarBottomRow != null) _sidebarBottomRow.BackColor = t.Surface;
            if (_sidebarMascotLabel != null)
            {
                _sidebarMascotLabel.BackColor = t.Surface;
                _sidebarMascotLabel.ForeColor = t.Text;
            }
            HighlightNav(_activeNavIndex, animate: false);
        }

        _statusBar.BackColor   = t.SurfaceAlt;
        _statusLabel.BackColor = t.SurfaceAlt;
        _statusLabel.ForeColor = t.SubText;
        _resizeHandle?.Invalidate();

        foreach (Control c in Controls)
            if (c.Tag is "border_panel") c.BackColor = t.Border;

        UpdatePlayButton();

        _tabStrip?.Invalidate();
        Invalidate();
    }

    private void SetStatus(string msg) => _statusLabel.Text = msg;

    private void UpdateGameLabel()
    {
        if (_gameLabel != null)
        {
            _gameLabel.Text = AppState.Settings.GamePath.Length > 0
                ? AppState.Settings.GameName
                : "No game selected";
        }
        if (_titleGameIcon != null)
        {
            bool hasGame = AppState.Settings.GamePath.Length > 0;
            _titleGameIcon.Visible = hasGame;
            _titleGameIcon.Tag = hasGame ? AppIcons.TryExtractGameIcon(AppState.Settings.GamePath) ?? AppIcons.Png : null;
            _titleGameIcon.Invalidate();
        }
        if (_sidebarMascotLabel != null)
            _sidebarMascotLabel.Text = AppState.Settings.GameName;
        if (_sidebarMascotBox != null)
            _sidebarMascotBox.Image = AppIcons.TryExtractGameIcon(AppState.Settings.GamePath) ?? AppIcons.Png;

        if (AppState.Settings.GamePath.Length > 0)
            DiscordPresence.Update(AppState.Settings.GameName, AppState.Settings.GamePath);
    }

    private RButton MakePlayBtn()
    {
        var btn = new RButton
        {
            Text = "▶ Play", CornerRadius = 8, Width = 74, Height = 28,
            Font = new Font("Segoe UI", 9f, FontStyle.Bold),
            Cursor = Cursors.Hand, Style = RButtonStyle.Solid,
        };
        btn.Click += (_, __) => OnPlayClick();
        return btn;
    }

    private void MainForm_DragEnter(object? sender, DragEventArgs e)
    {
        bool hasDll = e.Data?.GetDataPresent(DataFormats.FileDrop) == true &&
            ((string[])e.Data.GetData(DataFormats.FileDrop)!).Any(f => f.EndsWith(".dll", StringComparison.OrdinalIgnoreCase));
        e.Effect = hasDll ? DragDropEffects.Copy : DragDropEffects.None;
    }

    private void MainForm_DragDrop(object? sender, DragEventArgs e)
    {
        if (e.Data?.GetData(DataFormats.FileDrop) is not string[] files) return;
        if (AppState.GameDir == null) { SetStatus("No game selected, can't install."); return; }

        int count = 0;
        foreach (var f in files.Where(f => f.EndsWith(".dll", StringComparison.OrdinalIgnoreCase)))
        {
            try { ModInstaller.InstallLocal(f); count++; }
            catch { }
        }

        if (count == 0) return;
        SetStatus($"Installed {count} dropped .dll file{(count == 1 ? "" : "s")}.");
        _installedPanel.Refresh_();
    }

    private void ShowQuickSwitchMenu(Control anchor)
    {
        var menu = new ContextMenuStrip { BackColor = Color.FromArgb(30, 30, 30), ForeColor = Color.White };

        foreach (var preset in AppState.Presets.Where(p => p.Name != "Custom"))
        {
            string? path = preset.DefaultPaths.FirstOrDefault(File.Exists);
            if (path == null && AppState.Settings.KnownGamePaths.TryGetValue(preset.Name, out var known) && File.Exists(known)) path = known;
            if (path == null) continue;

            var resolved = path;
            var item = menu.Items.Add(preset.Name);
            item.Enabled = !preset.Name.Equals(AppState.Settings.GameName, StringComparison.OrdinalIgnoreCase);
            item.Click += (_, __) => QuickSwitchGame(resolved, preset.Name);
        }

        foreach (var (name, path) in AppState.Settings.CustomGames)
        {
            if (!File.Exists(path)) continue;
            var n = name; var pt = path;
            var item = menu.Items.Add(n);
            item.Enabled = !n.Equals(AppState.Settings.GameName, StringComparison.OrdinalIgnoreCase);
            item.Click += (_, __) => QuickSwitchGame(pt, n);
        }

        if (menu.Items.Count == 0) menu.Items.Add("No other games set up yet").Enabled = false;
        menu.Show(anchor, new Point(0, anchor.Height));
    }

    private void QuickSwitchGame(string path, string name)
    {
        AppState.Settings.GamePath = path;
        AppState.Settings.GameName = name;
        AppState.Save();
        AppState.EnsureSbFolder();
        DataBridge.LoadSettings();
        UpdateGameLabel();
        _navActivate[_activeNavIndex]?.Invoke();
        SetStatus($"Switched to {name}.");
    }

    private void OnPlayClick()
    {
        if (_gameRunning) StopGame();
        else LaunchGame();
    }

    private void LaunchGame()
    {
        string path = AppState.Settings.GamePath;
        if (string.IsNullOrEmpty(path) || !File.Exists(path)) return;

        int? appId = AppState.CurrentPreset.SteamAppId;
        if (appId == null)
        {
            var match = AppState.Settings.SteamExePaths.FirstOrDefault(kv => string.Equals(kv.Value, path, StringComparison.OrdinalIgnoreCase));
            if (match.Key != null && int.TryParse(match.Key, out int id)) appId = id;
        }

        try
        {
            if (appId is { } steamAppId)
                System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo($"steam://run/{steamAppId}") { UseShellExecute = true });
            else
                System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(path) { UseShellExecute = true, WorkingDirectory = Path.GetDirectoryName(path) });
            _launchedAt = DateTime.UtcNow;
        }
        catch { }
    }

    private static void StopGame()
    {
        string procName = Path.GetFileNameWithoutExtension(AppState.Settings.GamePath);
        if (procName.Length == 0) return;
        foreach (var p in System.Diagnostics.Process.GetProcessesByName(procName))
            try { p.Kill(); } catch { }
    }

    private void CheckGameRunning()
    {
        bool running = IsGameProcessRunning();
        if (running == _gameRunning) return;

        if (!running && _gameRunning && _launchedAt is { } launched && DateTime.UtcNow - launched < TimeSpan.FromSeconds(10))
        {
            int logsIdx = Array.IndexOf(NavLabels, "Logs");
            var check = MessageBox.Show(
                $"{AppState.Settings.GameName} closed just a few seconds after launching. It may have crashed. Check the log?",
                "Game Closed Quickly", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
            if (check == DialogResult.Yes && logsIdx >= 0) SwitchToIndex(logsIdx);
        }
        _launchedAt = null;

        _gameRunning = running;
        UpdatePlayButton();
    }

    private static bool IsGameProcessRunning()
    {
        string procName = Path.GetFileNameWithoutExtension(AppState.Settings.GamePath);
        if (procName.Length == 0) return false;
        try { return System.Diagnostics.Process.GetProcessesByName(procName).Length > 0; }
        catch { return false; }
    }

    private void UpdatePlayButton()
    {
        if (_playBtn == null) return;
        var t = ThemeEngine.Current;
        bool mmm = _layout == AppLayout.MonkeModManager;
        Color tint = _gameRunning ? Color.FromArgb(220, 60, 60) : Color.FromArgb(60, 190, 100);
        _playBtn.Text = _gameRunning ? "■ Stop" : "▶ Play";
        _playBtn.Style = mmm ? RButtonStyle.Outline : RButtonStyle.Solid;
        _playBtn.FillColor = RoundedGraphics.Lerp(t.SurfaceAlt, tint, 0.3f);
        _playBtn.HoverFillColor = RoundedGraphics.Lerp(t.SurfaceAlt, tint, 0.45f);
        _playBtn.BorderColor = mmm ? RoundedGraphics.Lerp(t.Border, tint, 0.5f) : Color.Transparent;
        _playBtn.HoverBorderColor = mmm ? tint : Color.Transparent;
        _playBtn.BorderWidth = 2f;
        _playBtn.ForeColor = t.Text;
        ThemeEngine.ApplyLayoutCornerStyle(_playBtn);
    }

    private static Panel MakeCircularIcon(int size)
    {
        var p = new Panel { Width = size, Height = size, BackColor = Color.Transparent };
        p.Paint += (s, e) =>
        {
            if (p.Tag is not Image img) return;
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            using var path = new GraphicsPath();
            path.AddEllipse(0, 0, size - 1, size - 1);
            var oldClip = e.Graphics.Clip;
            e.Graphics.SetClip(path, CombineMode.Intersect);
            e.Graphics.DrawImage(img, 0, 0, size, size);
            e.Graphics.Clip = oldClip;
        };
        return p;
    }

    private static Panel MakeLight(Color c)
    {
        bool hovering = false;
        float glow = 0f;
        var timer = new System.Windows.Forms.Timer { Interval = 15 };

        var p = new Panel { Width = 13, Height = 13, BackColor = Color.Transparent };
        p.Paint += (s, e) =>
        {
            var ctrl = (Panel)s!;
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            if (glow > 0.01f)
            {
                int r = (int)(2 * glow);
                using var glowBrush = new SolidBrush(Color.FromArgb((int)(60 * glow), Color.White));
                e.Graphics.FillEllipse(glowBrush, -r, -r, 12 + r * 2, 12 + r * 2);
            }
            using var b = new SolidBrush(c);
            e.Graphics.FillEllipse(b, 0, 0, 12, 12);
        };
        timer.Tick += (_, __) =>
        {
            float target = hovering ? 1f : 0f;
            if (Math.Abs(glow - target) < 0.08f) { glow = target; timer.Stop(); }
            else glow += target > glow ? 0.2f : -0.2f;
            p.Invalidate();
        };
        p.MouseEnter += (_, __) => { hovering = true; timer.Start(); };
        p.MouseLeave += (_, __) => { hovering = false; timer.Start(); };
        return p;
    }

    private static Panel MakeResizeHandle()
    {
        var p = new Panel { Width = 20, Dock = DockStyle.Right, Cursor = Cursors.SizeNWSE, BackColor = Color.Transparent };
        p.Paint += (s, e) =>
        {
            var ctrl = (Panel)s!;
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            using var pen = new Pen(ThemeEngine.Current.Border, 1.4f);
            int m = 6;
            e.Graphics.DrawLine(pen, ctrl.Width - m, ctrl.Height - m - 8, ctrl.Width - m - 8, ctrl.Height - m);
        };
        return p;
    }

    private RButton MakeTabBtn(string text)
    {
        bool mmm = _layout == AppLayout.MonkeModManager;
        return new RButton
        {
            Text = text, Style = RButtonStyle.Ghost,
            AutoSize = false, Width = 90, Height = mmm ? 38 : 32,
            Font = new Font("Segoe UI", 9.5f), Cursor = Cursors.Hand,
            CornerRadius = mmm ? 0 : TabRadius,
            Margin = new Padding(0),
        };
    }

    private static RButton MakeSidebarBtn(string text)
    {
        return new RButton
        {
            Text = text, Style = RButtonStyle.Ghost, CornerRadius = 0,
            TextAlign = ContentAlignment.MiddleLeft,
            Padding = new Padding(14, 0, 8, 0),
            Dock = DockStyle.Top, Height = 36,
            Font = new Font("Segoe UI", 9.5f), Cursor = Cursors.Hand,
        };
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) { ThemeEngine.ThemeChanged -= ApplyTheme; _gameRunningTimer?.Dispose(); DiscordPresence.Stop(); }
        base.Dispose(disposing);
    }
}
