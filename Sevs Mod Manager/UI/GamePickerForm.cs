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

        var headingDivider = new Panel { Dock = DockStyle.Top, Height = 1, Margin = new Padding(0), Tag = "border_panel" };
        var headingSpacer = new Panel { Dock = DockStyle.Top, Height = 12 };

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

        var mode = AppState.Settings.GamePickerDisplay;
        var visiblePresets = AppState.Presets.Where(p => p.Name != "Custom" && !AppState.Settings.HiddenPresets.Contains(p.Name)).ToList();

        if (mode == GamePickerDisplay.Icons)
        {
            const int tilesPerRow = 3;
            const int tileOuterWidth = 92 + 12;
            int gridWidth = tilesPerRow * tileOuterWidth;

            var grid = new FlowLayoutPanel
            {
                FlowDirection = FlowDirection.LeftToRight,
                WrapContents = true,
                AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink,
                MaximumSize = new Size(gridWidth, 0),
                Margin = new Padding(Math.Max(0, (listWidth - gridWidth) / 2), 0, 0, 8),
            };

            foreach (var preset in visiblePresets)
            {
                var p = preset;
                var tile = MakeGameTile(p.Name, ResolveIcon(ResolvePresetPath(p)), GameAccentColor(p.Name), () => PickPreset(p));
                HookWheel(tile);
                grid.Controls.Add(tile);
            }

            foreach (var (name, path) in AppState.Settings.CustomGames)
            {
                var n = name; var pt = path;
                var tile = MakeGameTile(n, ResolveIcon(pt), GameAccentColor(n), () => Commit(pt, n));
                HookWheel(tile);
                grid.Controls.Add(tile);
            }

            btnPanel.Controls.Add(grid);
        }
        else
        {
            foreach (var preset in visiblePresets)
            {
                var p = preset;
                var btn = MakePresetBtn(p.Name);
                btn.Tag = new CustomGameTag(p.Name);
                if (mode == GamePickerDisplay.Both) AttachIcon(btn, ResolveIcon(ResolvePresetPath(p)));
                btn.Click += (_, __) => PickPreset(p);
                HookWheel(btn);
                btnPanel.Controls.Add(btn);
            }

            foreach (var (name, path) in AppState.Settings.CustomGames)
            {
                var n = name; var pt = path;
                var btn = MakePresetBtn(n);
                btn.Tag = new CustomGameTag(n);
                if (mode == GamePickerDisplay.Both) AttachIcon(btn, ResolveIcon(pt));
                btn.Click += (_, __) => Commit(pt, n);
                HookWheel(btn);
                btnPanel.Controls.Add(btn);
            }
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
        _content.Controls.Add(headingSpacer);
        _content.Controls.Add(headingDivider);
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

    private async void PickPreset(GamePreset preset)
    {
        string? found = preset.DefaultPaths.FirstOrDefault(File.Exists);

        if (found == null && preset.SteamAppId is { } appId && preset.DefaultPaths.Length > 0)
            found = SteamLocator.FindGamePath(appId, Path.GetFileName(preset.DefaultPaths[0]));

        if (found != null) { await CommitPreset(found, preset); return; }

        if (AppState.Settings.KnownGamePaths.TryGetValue(preset.Name, out var remembered) && File.Exists(remembered))
        { await CommitPreset(remembered, preset); return; }

        using var dlg = new OpenFileDialog
        {
            Title            = $"Locate {preset.Name} executable",
            Filter           = "Executable|*.exe",
            InitialDirectory = remembered is { Length: > 0 } ? Path.GetDirectoryName(remembered) : null,
        };
        if (dlg.ShowDialog() != DialogResult.OK) return;
        AppState.Settings.KnownGamePaths[preset.Name] = dlg.FileName;
        await CommitPreset(dlg.FileName, preset);
    }

    private async Task CommitPreset(string path, GamePreset preset)
    {
        if (preset.Name == "Gorilla Tag") { Commit(path, preset.Name); return; }

        Cursor = Cursors.WaitCursor;
        try
        {
            if (preset.Name == "ULTRAKILL" && preset.DefaultThunderstoreCommunity != null)
            {
                if (!AppState.Settings.ThunderstoreCommunities.ContainsKey(preset.Name))
                    AppState.Settings.ThunderstoreCommunities[preset.Name] = preset.DefaultThunderstoreCommunity;
            }
            else
            {
                await DetectModSourceAsync(preset.Name, preset.DefaultThunderstoreCommunity);
            }
        }
        finally { Cursor = Cursors.Default; }

        Commit(path, preset.Name);
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

    private async Task DetectModSourceAsync(string name, string? fallbackCommunity = null)
    {
        if (AppState.Settings.ThunderstoreCommunities.ContainsKey(name) ||
            AppState.Settings.ManualOnlyGames.Contains(name))
            return;

        string slug = AppState.DeriveSlug(name);
        int gameId = await SbApi.RegisterGameAsync(slug);
        bool hasSbMods = gameId > 0 && (await SbApi.ListModsAsync("all", 1, gameId)).Count > 0;
        if (hasSbMods) return;

        string? guessed = await ThunderstoreApi.GuessCommunityAsync(name) ?? fallbackCommunity;
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
                bool secondary = rBtn.Tag is "secondary" or CustomGameTag;
                rBtn.Style = RButtonStyle.Solid;
                Color fill = secondary ? Lighten(t.SurfaceAlt, 20) : t.Accent;
                Color hoverFill = secondary ? Lighten(t.SurfaceAlt, 34) : Lighten(t.Accent, 18);
                if (rBtn.Tag is CustomGameTag cgt)
                {
                    var unique = GameAccentColor(cgt.Name);
                    fill = RoundedGraphics.Lerp(fill, unique, 0.35f);
                    hoverFill = RoundedGraphics.Lerp(hoverFill, unique, 0.35f);
                }
                rBtn.FillColor = fill;
                rBtn.HoverFillColor = hoverFill;
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
            else if (child.Tag is "overlay") { child.ForeColor = t.Text; }
            else if (child.Tag is CustomGameTag) { }
            else if (child.Tag is "border_panel") { child.BackColor = t.Border; }
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

    private sealed record CustomGameTag(string Name);

    private static string? ResolvePresetPath(GamePreset preset)
    {
        if (AppState.Settings.KnownGamePaths.TryGetValue(preset.Name, out var known) && File.Exists(known)) return known;
        return preset.DefaultPaths.FirstOrDefault(File.Exists);
    }

    private static Image? ResolveIcon(string? path) =>
        path is { Length: > 0 } p && File.Exists(p) ? AppIcons.TryExtractGameIcon(p) : null;

    private static void AttachIcon(RButton btn, Image? img)
    {
        if (img == null) return;
        const int size = 26, pad = 10;
        btn.Padding = new Padding(16, btn.Padding.Top, size + pad, btn.Padding.Bottom);
        btn.TextAlign = ContentAlignment.MiddleLeft;

        var iconBox = new Panel
        {
            Width = size, Height = size, BackColor = Color.Transparent,
            Location = new Point(btn.Width - size - pad / 2, (btn.Height - size) / 2),
            Anchor = AnchorStyles.Top | AnchorStyles.Right,
            Tag = "overlay",
        };
        iconBox.Paint += (_, e) => DrawCircularIcon(e.Graphics, img, size);
        btn.Controls.Add(iconBox);
    }

    private static Control MakeGameTile(string name, Image? icon, Color accent, Action onClick)
    {
        const int size = 92, labelHeight = 20;

        var wrapper = new Panel
        {
            Width = size, Height = size + labelHeight, Margin = new Padding(6),
            BackColor = Color.Transparent, Tag = "overlay",
        };

        bool hovering = false;
        var tile = new Panel
        {
            Size = new Size(size, size), Location = new Point(0, 0),
            Cursor = Cursors.Hand, Tag = new CustomGameTag(name),
        };
        tile.Paint += (_, e) =>
        {
            int radius = AppState.Settings.Layout == AppLayout.SevsModManager ? 12 : 0;
            e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
            using var path = RoundedGraphics.RoundedRect(new Rectangle(0, 0, size - 1, size - 1), radius);
            var oldClip = e.Graphics.Clip;
            e.Graphics.SetClip(path, CombineMode.Intersect);

            if (icon != null)
            {
                e.Graphics.DrawImage(icon, 0, 0, size, size);
                if (hovering)
                    using (var overlay = new SolidBrush(Color.FromArgb(50, 255, 255, 255)))
                        e.Graphics.FillRectangle(overlay, 0, 0, size, size);
            }
            else
            {
                using var brush = new SolidBrush(hovering ? RoundedGraphics.Lighten(accent, 20) : accent);
                e.Graphics.FillRectangle(brush, 0, 0, size, size);
            }

            e.Graphics.Clip = oldClip;
        };
        tile.Click += (_, __) => onClick();
        tile.MouseEnter += (_, __) => { hovering = true; tile.Invalidate(); };
        tile.MouseLeave += (_, __) => { hovering = false; tile.Invalidate(); };

        var label = new Label
        {
            Text = name, AutoEllipsis = true, TextAlign = ContentAlignment.MiddleCenter,
            Location = new Point(0, size + 2), Size = new Size(size, labelHeight - 2),
            Font = new Font("Segoe UI", 7.5f), BackColor = Color.Transparent, Tag = "overlay",
            Cursor = Cursors.Hand,
        };
        label.Click += (_, __) => onClick();

        wrapper.Controls.Add(tile);
        wrapper.Controls.Add(label);
        return wrapper;
    }

    private static void DrawCircularIcon(Graphics g, Image? img, int size)
    {
        if (img == null) return;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        int radius = AppState.Settings.Layout == AppLayout.SevsModManager ? size / 2 : 0;
        using var path = RoundedGraphics.RoundedRect(new Rectangle(0, 0, size - 1, size - 1), radius);
        var oldClip = g.Clip;
        g.SetClip(path, CombineMode.Intersect);
        g.DrawImage(img, 0, 0, size, size);
        g.Clip = oldClip;
    }

    private static Color GameAccentColor(string name)
    {
        int hash = 0;
        foreach (char c in name) hash = hash * 31 + c;
        float hue = Math.Abs(hash) % 360;
        return FromHsl(hue, 0.55f, 0.55f);
    }

    private static Color FromHsl(float h, float s, float l)
    {
        float c = (1 - Math.Abs(2 * l - 1)) * s;
        float x = c * (1 - Math.Abs(h / 60f % 2 - 1));
        float m = l - c / 2;
        var (r, g, b) = h switch
        {
            < 60  => (c, x, 0f),
            < 120 => (x, c, 0f),
            < 180 => (0f, c, x),
            < 240 => (0f, x, c),
            < 300 => (x, 0f, c),
            _     => (c, 0f, x),
        };
        return Color.FromArgb((int)((r + m) * 255), (int)((g + m) * 255), (int)((b + m) * 255));
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
