using SevsModManager.UI.Controls;
using System.Net.Http;
using SevsModManager.Theme;
using SevsModManager.Core;

namespace SevsModManager.UI;

internal sealed class SteamGameBrowserDialog : Form
{
    public string? SelectedExePath { get; private set; }
    public string? SelectedName { get; private set; }

    private static readonly HttpClient _http = new() { Timeout = TimeSpan.FromSeconds(10) };
    static SteamGameBrowserDialog() => _http.DefaultRequestHeaders.UserAgent.ParseAdd("SevsModManager/1.0");

    private static readonly string CoverCacheDir =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "SevsModManager", "SteamCovers");

    private const int CardWidth = 120, CoverHeight = 160, CardHeight = 190;

    private readonly Panel _viewport;
    private readonly FlowLayoutPanel _grid;
    private readonly RScrollBar _scrollbar;
    private readonly Label _statusLabel;
    private readonly ThemeColors _t;

    public SteamGameBrowserDialog()
    {
        _t = ThemeEngine.Current;

        Text = "Select Steam Game";
        FormBorderStyle = FormBorderStyle.FixedDialog;
        StartPosition = FormStartPosition.CenterParent;
        ClientSize = new Size(560, 460);
        MaximizeBox = false;
        MinimizeBox = false;
        ShowInTaskbar = false;
        BackColor = _t.Background;

        _statusLabel = new Label
        {
            Text = "Scanning Steam library...", Dock = DockStyle.Top, Height = 30,
            Padding = new Padding(12, 8, 0, 0), BackColor = _t.Background, ForeColor = _t.SubText,
        };

        _grid = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.LeftToRight, WrapContents = true,
            AutoSize = false,
            Location = new Point(0, 0), BackColor = _t.Background, Padding = new Padding(8),
        };

        _scrollbar = new RScrollBar
        {
            Dock = DockStyle.Right, Width = 8, Visible = false,
            ThumbColor = Lighten(_t.SurfaceAlt, 40), ThumbHoverColor = Lighten(_t.SurfaceAlt, 60),
        };
        _scrollbar.ValueChanged += (_, __) => _grid.Location = new Point(0, -_scrollbar.Value);

        _viewport = new Panel { Dock = DockStyle.Fill, BackColor = _t.Background };
        _viewport.MouseWheel += (_, e) => _scrollbar.Value -= e.Delta / 3;
        _grid.MouseWheel += (_, e) => _scrollbar.Value -= e.Delta / 3;
        _viewport.Controls.Add(_grid);
        _viewport.Controls.Add(_scrollbar);

        var cancelBtn = new RButton { Text = "Cancel", Width = 100, Height = 32 };
        ThemeEngine.StyleGhostButton(cancelBtn);
        cancelBtn.Click += (_, __) => { DialogResult = DialogResult.Cancel; Close(); };

        var bottomBar = new Panel { Dock = DockStyle.Bottom, Height = 46, BackColor = _t.Background };
        cancelBtn.Location = new Point(16, 7);
        bottomBar.Controls.Add(cancelBtn);

        Controls.Add(_viewport);
        Controls.Add(bottomBar);
        Controls.Add(_statusLabel);

        Resize += (_, __) => UpdateScroll();

        Load += async (_, __) => await PopulateAsync();
    }

    private async Task PopulateAsync()
    {
        List<(int AppId, string Name)> games;
        try
        {
            games = await Task.Run(SteamLocator.ListInstalledGames);
        }
        catch (Exception ex)
        {
            _statusLabel.Text = "Couldn't scan Steam library: " + ex.Message;
            return;
        }

        _statusLabel.Text = games.Count == 0 ? "No Steam games found." : $"{games.Count} games found. Click one to select it.";

        _grid.SuspendLayout();
        foreach (var (appId, name) in games)
            _grid.Controls.Add(BuildCard(appId, name));
        _grid.ResumeLayout();
        UpdateScroll();

        try { Directory.CreateDirectory(CoverCacheDir); } catch { }
        foreach (Control card in _grid.Controls)
            if (card.Tag is (int cardAppId, PictureBox pic))
                _ = LoadCoverAsync(cardAppId, pic);
    }

    private void UpdateScroll()
    {
        _grid.Width = Math.Max(1, _viewport.Width - _scrollbar.Width);
        int contentHeight = _grid.GetPreferredSize(new Size(_grid.Width, 0)).Height;
        _grid.Height = contentHeight;

        int viewportHeight = _viewport.Height;
        _scrollbar.Maximum = contentHeight;
        _scrollbar.LargeChange = Math.Max(1, viewportHeight);
        _scrollbar.Visible = contentHeight > viewportHeight;
        _grid.Location = new Point(0, -_scrollbar.Value);
    }

    private Control BuildCard(int appId, string name)
    {
        var card = new RPanel
        {
            Width = CardWidth, Height = CardHeight, Margin = new Padding(6), Cursor = Cursors.Hand,
            CornerRadius = 8, BackColor = _t.SurfaceAlt, BorderColor = Color.Transparent,
        };

        var pic = new PictureBox
        {
            Width = CardWidth - 8, Height = CoverHeight, Location = new Point(4, 4),
            SizeMode = PictureBoxSizeMode.Zoom, BackColor = Color.Transparent,
            Image = AppIcons.Png,
        };

        var nameLbl = new Label
        {
            Text = name, Width = CardWidth - 8, Height = CardHeight - CoverHeight - 8,
            Location = new Point(4, CoverHeight + 4), TextAlign = ContentAlignment.TopCenter,
            AutoEllipsis = true, ForeColor = _t.Text, BackColor = Color.Transparent,
            Font = new Font("Segoe UI", 8.5f),
        };

        card.Controls.Add(pic);
        card.Controls.Add(nameLbl);
        card.Tag = (appId, pic);

        void Select() => _ = SelectGameAsync(appId, name);
        card.Click += (_, __) => Select();
        pic.Click  += (_, __) => Select();
        nameLbl.Click += (_, __) => Select();

        return card;
    }

    private static async Task LoadCoverAsync(int appId, PictureBox pic)
    {
        string cachePath = Path.Combine(CoverCacheDir, $"{appId}.jpg");
        try
        {
            if (!File.Exists(cachePath))
            {
                byte[] data = await _http.GetByteArrayAsync($"https://cdn.akamai.steamstatic.com/steam/apps/{appId}/library_600x900.jpg");
                await File.WriteAllBytesAsync(cachePath, data);
            }
            if (pic.IsDisposed) return;
            using var stream = File.OpenRead(cachePath);
            pic.Image = Image.FromStream(stream);
        }
        catch { }
    }

    private async Task SelectGameAsync(int appId, string name)
    {
        _statusLabel.Text = $"Locating {name}...";
        Enabled = false;

        string? exePath = AppState.Settings.SteamExePaths.TryGetValue(appId.ToString(), out var cached) && File.Exists(cached)
            ? cached
            : null;

        if (exePath == null)
        {
            var candidates = await Task.Run(() => SteamLocator.FindExeCandidates(appId));
            exePath = candidates.Count == 1 ? candidates[0]
                : candidates.FirstOrDefault(c => string.Equals(Path.GetFileNameWithoutExtension(c), Path.GetFileName(Path.GetDirectoryName(c)), StringComparison.OrdinalIgnoreCase))
                  ?? candidates.FirstOrDefault();

            if (exePath != null)
            {
                AppState.Settings.SteamExePaths[appId.ToString()] = exePath;
                AppState.Save();
            }
        }

        Enabled = true;
        if (exePath == null)
        {
            MessageBox.Show($"Couldn't find an executable for \"{name}\".", "Not Found", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            _statusLabel.Text = "Click a game to select it.";
            return;
        }

        SelectedExePath = exePath;
        SelectedName = name;
        DialogResult = DialogResult.OK;
        Close();
    }

    private static Color Lighten(Color c, int amount) =>
        Color.FromArgb(Math.Min(255, c.R + amount), Math.Min(255, c.G + amount), Math.Min(255, c.B + amount));
}
