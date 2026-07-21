using SevsModManager.Theme;
using SevsModManager.Core;
using SevsModManager.UI.Controls;

namespace SevsModManager.UI.Panels;

internal sealed class LogsPanel : UserControl
{
    private readonly RichTextBox _logBox;
    private readonly Panel _toolbar;
    private readonly RButton _clearBtn;
    private readonly System.Windows.Forms.Timer _timer;

    private FileStream? _logStream;
    private StreamReader? _logReader;
    private string? _currentLogPath;
    private bool _wasRunning;

    public LogsPanel()
    {
        Dock = DockStyle.Fill;
        DoubleBuffered = true;

        _logBox = new RichTextBox
        {
            Dock = DockStyle.Fill, ReadOnly = true, BorderStyle = BorderStyle.None,
            Font = new Font("Consolas", 9f), WordWrap = true, DetectUrls = false,
        };

        _toolbar = new Panel { Dock = DockStyle.Bottom, Height = 44, Padding = new Padding(8) };
        _clearBtn = new RButton { Text = "Clear", Dock = DockStyle.Right, Width = 90, CornerRadius = 8 };
        _clearBtn.Click += (_, __) => _logBox.Clear();
        _toolbar.Controls.Add(_clearBtn);

        Controls.Add(_logBox);
        Controls.Add(_toolbar);

        ThemeEngine.ThemeChanged += ApplyTheme;
        HandleCreated += (_, __) => ThemeEngine.ApplyScrollTheme(this);
        ApplyTheme();

        _timer = new System.Windows.Forms.Timer { Interval = 1000 };
        _timer.Tick += (_, __) => Tick();
        _timer.Start();

        ShowIdleMessage();
    }

    public void Refresh_() { }

    private void Tick()
    {
        bool running = IsGameRunning();
        if (running && !_wasRunning) OnGameStarted();
        else if (!running && _wasRunning) OnGameStopped();
        else if (running) PollLog();
        _wasRunning = running;
    }

    private static bool IsGameRunning()
    {
        string procName = Path.GetFileNameWithoutExtension(AppState.Settings.GamePath);
        if (procName.Length == 0) return false;
        try { return System.Diagnostics.Process.GetProcessesByName(procName).Length > 0; }
        catch { return false; }
    }

    private static string? ResolveLogPath()
    {
        if (AppState.GameDir is not { } dir) return null;
        return AppState.DetectLoaderKind() == ModLoaderKind.MelonLoader
            ? Path.Combine(dir, "MelonLoader", "Latest.log")
            : Path.Combine(dir, "BepInEx", "LogOutput.log");
    }

    private void OnGameStarted()
    {
        _currentLogPath = ResolveLogPath();
        _logBox.Clear();
        AppendLine($"{AppState.Settings.GameName} is running, watching its log file.");
        AppendLine("");
        PollLog();
    }

    private void OnGameStopped()
    {
        AppendLine("");
        AppendLine("Game closed.");
        CloseLogStream();
    }

    private void PollLog()
    {
        if (_logReader == null)
        {
            if (_currentLogPath == null || !File.Exists(_currentLogPath)) return;
            try
            {
                _logStream = new FileStream(_currentLogPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
                _logReader = new StreamReader(_logStream);
            }
            catch { return; }
        }

        string? line;
        while ((line = _logReader.ReadLine()) != null) AppendLine(line);
    }

    private void CloseLogStream()
    {
        _logReader?.Dispose();
        _logStream?.Dispose();
        _logReader = null;
        _logStream = null;
        _currentLogPath = null;
    }

    private void AppendLine(string line)
    {
        _logBox.AppendText(line + Environment.NewLine);
        _logBox.SelectionStart = _logBox.TextLength;
        _logBox.ScrollToCaret();
    }

    private void ShowIdleMessage()
    {
        _logBox.Clear();
        string name = AppState.Settings.GameName;
        AppendLine($"No game running right now, so there's nothing to show. Launch {name} and this tab will start following its log live.");
    }

    private void ApplyTheme()
    {
        var t = ThemeEngine.Current;
        BackColor = t.Background;
        _toolbar.BackColor = t.Surface;
        _logBox.BackColor = t.Background;
        _logBox.ForeColor = t.Text;
        ThemeEngine.StyleGhostButton(_clearBtn);
        ThemeEngine.ApplyScrollTheme(this);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            ThemeEngine.ThemeChanged -= ApplyTheme;
            _timer.Dispose();
            CloseLogStream();
        }
        base.Dispose(disposing);
    }
}
