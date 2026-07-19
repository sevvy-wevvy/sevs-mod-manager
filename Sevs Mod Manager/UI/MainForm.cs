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

    private static readonly string[] NavLabels = { "Mods", "Installed", "Loadouts", "Modpacks", "Config", "Settings" };

    private readonly AppLayout _layout;

    private Panel?  _titleBar;
    private Label?  _titleLabel;
    private Label?  _gameLabel;

    private Panel?      _tabStrip;
    private RButton[]?  _navButtons;
    private int         _activeNavIndex;
    private float _underlineX, _underlineW, _targetX, _targetW;
    private System.Windows.Forms.Timer? _underlineTimer;

    private Panel? _sidebar;
    private Label? _sidebarGameLabel;
    private Label? _sidebarMascotLabel;
    private PictureBox? _sidebarMascotBox;
    private Panel? _sidebarBottomRow;

    private readonly ModsPanel      _modsPanel;
    private readonly InstalledPanel _installedPanel;
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
        _loadoutsPanel  = new LoadoutsPanel { Visible = false };
        _modpacksPanel  = new ModpacksPanel { Visible = false };
        _configPanel    = new ConfigPanel   { Visible = false };
        _settingsPanel  = new SettingsPanel { Visible = false };

        _navPanels = new Control[] { _modsPanel, _installedPanel, _loadoutsPanel, _modpacksPanel, _configPanel, _settingsPanel };
        _navActivate = new Action?[]
        {
            () => _modsPanel.RefreshModSource(),
            () => _installedPanel.Refresh_(),
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

        switch (_layout)
        {
            case AppLayout.R2Modman:        BuildR2ModmanChrome(); break;
            case AppLayout.MonkeModManager:  BuildMmmChrome(); break;
            default:                        BuildSmmChrome(); break;
        }

        ThemeEngine.ThemeChanged += ApplyTheme;
        ApplyTheme();

        Load += async (_, __) =>
        {
            SetStatus("Starting...");
            UpdateGameLabel();
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
        };
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

        _gameLabel = new Label
        {
            Text = "",
            Dock = DockStyle.Right,
            Width = 200,
            TextAlign = ContentAlignment.MiddleRight,
            Font = new Font("Segoe UI", 8.5f),
            Padding = new Padding(0, 0, 12, 0),
        };

        _titleBar.Controls.Add(_gameLabel);
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

        _sidebarGameLabel = new Label
        {
            Text = AppState.Settings.GameName.ToUpperInvariant(),
            Dock = DockStyle.Top, Height = 34,
            Font = new Font("Segoe UI", 8f, FontStyle.Bold),
            TextAlign = ContentAlignment.MiddleLeft,
            Padding = new Padding(16, 0, 0, 0),
        };

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
        };
        _sidebarMascotLabel = new Label { Text = AppState.Settings.GameName, AutoSize = true, Font = new Font("Segoe UI", 8.5f), Location = new Point(52, 16) };
        _sidebarBottomRow = new Panel { Dock = DockStyle.Bottom, Height = 48 };
        _sidebarBottomRow.Controls.Add(_sidebarMascotBox);
        _sidebarBottomRow.Controls.Add(_sidebarMascotLabel);

        _sidebar.Controls.Add(navHost);
        _sidebar.Controls.Add(_sidebarGameLabel);
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
            if (_sidebarGameLabel != null)
            {
                _sidebarGameLabel.BackColor = t.Surface;
                _sidebarGameLabel.ForeColor = t.SubText;
            }
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

        _tabStrip?.Invalidate();
        Invalidate();
    }

    private void SetStatus(string msg) => _statusLabel.Text = msg;

    private void UpdateGameLabel()
    {
        if (_gameLabel != null)
        {
            _gameLabel.Text = AppState.Settings.GamePath.Length > 0
                ? $"{AppState.Settings.GameName}  ·  {Path.GetFileName(AppState.Settings.GamePath)}"
                : "No game selected";
        }
        if (_sidebarGameLabel != null)
            _sidebarGameLabel.Text = AppState.Settings.GameName.ToUpperInvariant();
        if (_sidebarMascotLabel != null)
            _sidebarMascotLabel.Text = AppState.Settings.GameName;
        if (_sidebarMascotBox != null)
            _sidebarMascotBox.Image = AppIcons.TryExtractGameIcon(AppState.Settings.GamePath) ?? AppIcons.Png;
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
        if (disposing) ThemeEngine.ThemeChanged -= ApplyTheme;
        base.Dispose(disposing);
    }
}
