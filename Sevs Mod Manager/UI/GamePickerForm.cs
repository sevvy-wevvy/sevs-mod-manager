using SevsModManager.Theme;
using System.Runtime.InteropServices;
using SevsModManager.Core;
using System.Drawing.Drawing2D;
using SevsModManager.UI.Controls;

namespace SevsModManager.UI;

internal sealed class GamePickerForm : Form
{
    private const int WindowRadius = 16;

    public bool Confirmed { get; private set; }

    private readonly AppLayout _layout;

    private Panel?  _titleBar;
    private Label?  _titleLabel;
    private readonly RPanel _content;
    private Point _dragStart;
    private bool  _dragging;

    public GamePickerForm()
    {
        _layout = AppState.Settings.Layout;
        Icon = AppIcons.Icon;
        StartPosition   = FormStartPosition.CenterScreen;
        DoubleBuffered  = true;
        Text            = "Select Game";

        _content = new RPanel
        {
            Dock = DockStyle.Fill, Padding = new Padding(28, 14, 28, 20),
            CornerRadius = _layout == AppLayout.SevsModManager ? 14 : 0,
            Corners = Corners.BottomLeft | Corners.BottomRight,
        };

        var heading = new Label
        {
            Text = "Choose a game to manage",
            Dock = DockStyle.Top,
            Height = 26,
            Font = new Font("Segoe UI", 11f, FontStyle.Bold),
            TextAlign = ContentAlignment.MiddleLeft,
        };

        const int rowHeight = 54;
        const int visibleRows = 6;
        const int listWidth = 380;

        var btnPanel = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            AutoSize = true,
            AutoSizeMode = AutoSizeMode.GrowAndShrink,
            Location = new Point(0, 0),
            Width = listWidth,
            Padding = Padding.Empty,
        };

        var scrollbar = new RScrollBar { Dock = DockStyle.Right, Width = 8, Visible = false };

        void UpdateScroll()
        {
            int contentHeight = btnPanel.PreferredSize.Height;
            int viewportHeight = rowHeight * visibleRows;
            scrollbar.Maximum = contentHeight;
            scrollbar.LargeChange = viewportHeight;
            scrollbar.Visible = contentHeight > viewportHeight;
            btnPanel.Location = new Point(0, -scrollbar.Value);
        }
        scrollbar.ValueChanged += (_, __) => btnPanel.Location = new Point(0, -scrollbar.Value);

        void HookWheel(Control c) => c.MouseWheel += (_, e) => scrollbar.Value -= e.Delta / 3;

        var visiblePresets = AppState.Presets.Where(p => p.Name != "Custom" && !AppState.Settings.HiddenPresets.Contains(p.Name)).ToList();
        foreach (var preset in visiblePresets)
        {
            var p = preset;
            var btn = MakePresetBtn(p.Name);
            btn.Click += (_, __) => PickPreset(p);
            HookWheel(btn);
            btnPanel.Controls.Add(btn);
        }

        foreach (var (name, path) in AppState.Settings.CustomGames)
        {
            var n = name; var pt = path;
            var btn = MakePresetBtn(n);
            btn.Tag = "secondary";
            btn.Click += (_, __) => Commit(pt, n);
            HookWheel(btn);
            btnPanel.Controls.Add(btn);
        }

        var customBtn = MakePresetBtn("Custom Game...");
        customBtn.Tag = "secondary";
        customBtn.Click += async (_, __) => await PickCustom();
        HookWheel(customBtn);
        btnPanel.Controls.Add(customBtn);

        var customSteamBtn = MakePresetBtn("Select Steam Game...");
        customSteamBtn.Tag = "secondary";
        customSteamBtn.Click += async (_, __) => await PickCustomViaSteamBrowser();
        HookWheel(customSteamBtn);
        btnPanel.Controls.Add(customSteamBtn);

        var viewport = new Panel { Dock = DockStyle.Top, Height = rowHeight * visibleRows };
        HookWheel(viewport);
        viewport.Controls.Add(btnPanel);
        viewport.Controls.Add(scrollbar);
        UpdateScroll();

        _content.Controls.Add(viewport);
        _content.Controls.Add(heading);
        Controls.Add(_content);

        if (_layout == AppLayout.SevsModManager) BuildCustomChrome();
        else BuildNativeChrome();

        Size = new Size(440, 38 + 50 + rowHeight * visibleRows + 20);

        ThemeEngine.ThemeChanged += ApplyTheme;
        ApplyTheme();
    }

    private void BuildCustomChrome()
    {
        FormBorderStyle = FormBorderStyle.None;

        _titleBar = new Panel { Dock = DockStyle.Top, Height = 38 };

        void TitleBarMouseDown(object? _, MouseEventArgs e) { if (e.Button == MouseButtons.Left) { _dragging = true; _dragStart = e.Location; } }
        void TitleBarMouseMove(object? _, MouseEventArgs e) { if (_dragging && e.Button == MouseButtons.Left) Location = new Point(Location.X + e.X - _dragStart.X, Location.Y + e.Y - _dragStart.Y); }
        void TitleBarMouseUp(object? _, MouseEventArgs __) => _dragging = false;

        _titleBar.MouseDown += TitleBarMouseDown;
        _titleBar.MouseMove += TitleBarMouseMove;
        _titleBar.MouseUp   += TitleBarMouseUp;

        var closeBtn = MakeLight(Color.FromArgb(255, 95, 86));
        closeBtn.Location = new Point(12, 13);
        closeBtn.Cursor   = Cursors.Hand;
        closeBtn.Click   += (_, __) => { Confirmed = false; Close(); };

        _titleLabel = new Label
        {
            Text = "Select Game",
            Dock = DockStyle.Fill,
            TextAlign = ContentAlignment.MiddleCenter,
            Font = new Font("Segoe UI", 10f),
        };
        _titleLabel.MouseDown += TitleBarMouseDown;
        _titleLabel.MouseMove += TitleBarMouseMove;
        _titleLabel.MouseUp   += TitleBarMouseUp;

        _titleBar.Controls.Add(_titleLabel);
        _titleBar.Controls.Add(closeBtn);
        Controls.Add(_titleBar);

        Load        += (_, __) => ApplyRoundedRegion();
        Resize      += (_, __) => ApplyRoundedRegion();
        SizeChanged += (_, __) => ApplyRoundedRegion();
    }

    private void BuildNativeChrome()
    {

        FormBorderStyle = FormBorderStyle.Sizable;
        MaximizeBox = false;
        bool r2 = _layout == AppLayout.R2Modman;
        HandleCreated += (_, __) =>
        {
            if (r2) ForceDarkTitleBar();
            if (r2) ForceSquareCorners(); else ForceRoundedCorners();
        };
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
        if (ClientSize.Width <= 0 || ClientSize.Height <= 0) return;
        using var path = RoundedGraphics.RoundedRect(new Rectangle(0, 0, ClientSize.Width, ClientSize.Height), WindowRadius);
        Region = new Region(path);
        Invalidate(true);
    }

    private void PickPreset(GamePreset preset)
    {

        string? found = preset.DefaultPaths.FirstOrDefault(File.Exists);

        if (found == null && preset.SteamAppId is { } appId && preset.DefaultPaths.Length > 0)
            found = SteamLocator.FindGamePath(appId, Path.GetFileName(preset.DefaultPaths[0]));

        if (found != null) { Commit(found, preset.Name); return; }

        if (AppState.Settings.KnownGamePaths.TryGetValue(preset.Name, out var remembered) && File.Exists(remembered))
        { Commit(remembered, preset.Name); return; }

        using var dlg = new OpenFileDialog
        {
            Title            = $"Locate {preset.Name} executable",
            Filter           = "Executable|*.exe",
            InitialDirectory = remembered is { Length: > 0 } ? Path.GetDirectoryName(remembered) : null,
        };
        if (dlg.ShowDialog() != DialogResult.OK) return;
        AppState.Settings.KnownGamePaths[preset.Name] = dlg.FileName;
        Commit(dlg.FileName, preset.Name);
    }

    private async Task PickCustom()
    {
        using var dlg = new OpenFileDialog
        {
            Title  = "Select Game Executable",
            Filter = "Executable|*.exe",
        };
        if (dlg.ShowDialog() != DialogResult.OK) return;

        await CommitCustomExe(dlg.FileName);
    }

    private async Task PickCustomViaSteamBrowser()
    {
        using var dlg = new SteamGameBrowserDialog();
        if (dlg.ShowDialog() != DialogResult.OK || dlg.SelectedExePath == null) return;

        await CommitCustomExe(dlg.SelectedExePath);
    }

    private async Task CommitCustomExe(string exePath)
    {
        string name = Path.GetFileNameWithoutExtension(exePath);
        AppState.Settings.CustomGames[name] = exePath;

        Cursor = Cursors.WaitCursor;
        try { await DetectModSourceAsync(name); }
        finally { Cursor = Cursors.Default; }

        Commit(exePath, name);
    }

    private async Task DetectModSourceAsync(string name)
    {
        if (AppState.Settings.ThunderstoreCommunities.ContainsKey(name) ||
            AppState.Settings.ManualOnlyGames.Contains(name))
            return;

        string slug = AppState.DeriveSlug(name);
        int gameId = await SbApi.RegisterGameAsync(slug);
        bool hasSbMods = gameId > 0 && (await SbApi.ListModsAsync("all", 1, gameId)).Count > 0;
        if (hasSbMods) return;

        string? guessed = await ThunderstoreApi.GuessCommunityAsync(name);
        if (guessed != null)
        {
            AppState.Settings.ThunderstoreCommunities[name] = guessed;
            return;
        }

        var communities = await ThunderstoreApi.ListCommunitiesAsync();
        if (communities.Count == 0) { AppState.Settings.ManualOnlyGames.Add(name); return; }

        using var picker = new ThunderstoreCommunityPickerForm(communities);
        if (picker.ShowDialog() == DialogResult.OK && picker.Selected != null)
            AppState.Settings.ThunderstoreCommunities[name] = picker.Selected.Identifier;
        else
            AppState.Settings.ManualOnlyGames.Add(name);
    }

    private void Commit(string path, string name)
    {
        AppState.Settings.GamePath = path;
        AppState.Settings.GameName = name;

        var preset = AppState.Presets.FirstOrDefault(p => p.Name == name);
        if (preset?.DefaultThunderstoreCommunity != null && !AppState.Settings.ThunderstoreCommunities.ContainsKey(name))
            AppState.Settings.ThunderstoreCommunities[name] = preset.DefaultThunderstoreCommunity;

        AppState.Save();

        AppState.EnsureSbFolder();
        DataBridge.LoadSettings();

        Confirmed = true;
        Close();
    }

    private void ApplyTheme()
    {
        var t = ThemeEngine.Current;
        BackColor = t.Background;

        if (_titleBar != null)
        {
            _titleBar.BackColor = t.Surface;
            if (_titleLabel != null) { _titleLabel.BackColor = t.Surface; _titleLabel.ForeColor = t.Text; }
        }

        _content.BackColor   = t.SurfaceAlt;
        _content.BorderColor = Color.Transparent;

        RecolorDeep(_content, t);
        Invalidate();
    }

    private static void RecolorDeep(Control c, ThemeColors t)
    {
        foreach (Control child in c.Controls)
        {
            if (child is RButton rBtn)
            {
                bool secondary = rBtn.Tag is "secondary";
                rBtn.Style = RButtonStyle.Solid;
                rBtn.FillColor = secondary ? Lighten(t.SurfaceAlt, 20) : t.Accent;
                rBtn.HoverFillColor = secondary ? Lighten(t.SurfaceAlt, 34) : Lighten(t.Accent, 18);
                rBtn.BorderColor = Color.Transparent;
                rBtn.HoverBorderColor = Color.Transparent;
                rBtn.ForeColor = secondary ? t.Text : t.AccentText;
                ThemeEngine.ApplyLayoutCornerStyle(rBtn);
            }
            else if (child is RScrollBar scrollbar)
            {
                scrollbar.ThumbColor = Lighten(t.SurfaceAlt, 40);
                scrollbar.ThumbHoverColor = Lighten(t.SurfaceAlt, 60);
            }
            else if (child.Tag is "subtext") { child.BackColor = t.SurfaceAlt; child.ForeColor = t.SubText; }
            else { child.BackColor = t.SurfaceAlt; child.ForeColor = t.Text; }

            RecolorDeep(child, t);
        }
    }

    private static Color Lighten(Color c, int amount) =>
        Color.FromArgb(Math.Min(255, c.R + amount), Math.Min(255, c.G + amount), Math.Min(255, c.B + amount));

    private static RButton MakePresetBtn(string text)
    {
        return new RButton
        {
            Text      = text,
            CornerRadius = 10,
            Width     = 380,
            Height    = 46,
            Font      = new Font("Segoe UI", 10f),
            Margin    = new Padding(0, 0, 0, 8),
        };
    }

    private static Panel MakeLight(Color c)
    {
        var p = new Panel { Width = 13, Height = 13, BackColor = Color.Transparent };
        p.Paint += (s, e) =>
        {
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            e.Graphics.FillEllipse(new SolidBrush(c), 0, 0, 12, 12);
        };
        return p;
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

    protected override void Dispose(bool disposing)
    {
        if (disposing) ThemeEngine.ThemeChanged -= ApplyTheme;
        base.Dispose(disposing);
    }
}
