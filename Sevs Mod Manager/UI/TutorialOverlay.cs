using SevsModManager.Theme;
using SevsModManager.UI.Controls;
using SevsModManager.Core;

namespace SevsModManager.UI;

internal sealed class TutorialOverlay
{
    private const int TotalSteps = 11;
    private static readonly (string Title, string Body)[] Steps =
    {
        ("Hey, welcome!",
         "Let's take a look around the app together!"),
        ("Getting around",
         "These tabs are your main navigation. Mods for browsing and installing, Installed for managing what is on your system, Modpacks for saving and sharing setups, Config for tweaking settings, Logs for watching your game, and Settings for everything else."),
        ("The Play button",
         "This launches your game. Once you have some mods set up, click here anytime to play."),
        ("What a mod loader does",
         "Mods cannot run on their own. They need a mod loader, BepInEx or MelonLoader, installed first. Sev's Mod Manager figures out which one your game needs and installs it automatically the first time you install a mod."),
        ("Try it: click Mods",
         "Go ahead and click the Mods tab above."),
        ("Installing a mod",
         "This is the real Mods screen with a sample entry dropped in. Click Install and watch the progress bar, that is exactly what a real install looks like. Nothing actually downloads here."),
        ("Try it: click Installed",
         "Now click the Installed tab above."),
        ("Enable and disable",
         "You can turn a mod off without removing it. This is handy for figuring out which mod is causing a problem. Click the toggle next to Example Mod to try it."),
        ("Removing a mod",
         "When you are done with a mod, click the trash icon to remove it completely. Try it on Example Mod."),
        ("One more thing",
         "Settings has your themes, loader options, and this tutorial again if you ever want to redo it."),
        ("You're all set",
         "That is the whole loop. Explore and have fun modding."),
    };

    private readonly MainForm _form;
    private readonly List<Control> _active = new();
    private int _step;
    private bool _modsDemoActive;
    private bool _installedDemoActive;
    private RButton? _gateTarget;
    private EventHandler? _gateHandler;
    private EventHandler? _resizeHandler;

    public TutorialOverlay(MainForm form) => _form = form;

    public void Start()
    {
        _resizeHandler = (_, __) => SafeRender();
        _form.Resize += _resizeHandler;
        SafeRender();
    }

    private void SafeRender()
    {
        try { Render(); }
        catch (Exception ex)
        {
            Cleanup();
            MessageBox.Show("The tutorial hit a problem and closed: " + ex.Message, "Tutorial", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void Finish()
    {
        Cleanup();
        AppState.Settings.HasSeenTutorial = true;
        AppState.Save();
    }

    private void Cleanup()
    {
        if (_gateTarget != null && _gateHandler != null) _gateTarget.Click -= _gateHandler;
        _gateTarget = null;
        _gateHandler = null;

        _form.ModsPanelControl.EndTutorialMod();
        _form.InstalledPanelControl.EndTutorialMod();
        _modsDemoActive = false;
        _installedDemoActive = false;

        foreach (var c in _active) { _form.Controls.Remove(c); c.Dispose(); }
        _active.Clear();

        if (_resizeHandler != null) _form.Resize -= _resizeHandler;
        _resizeHandler = null;
    }

    private void Render()
    {
        foreach (var c in _active) { _form.Controls.Remove(c); c.Dispose(); }
        _active.Clear();
        if (_gateTarget != null && _gateHandler != null) _gateTarget.Click -= _gateHandler;
        _gateTarget = null;
        _gateHandler = null;

        UpdateDemoModes();

        var t = ThemeEngine.Current;
        var full = _form.ClientRectangle;

        Rectangle? hole = GetTargetHole(_step);
        foreach (var rect in ComputeMasks(full, hole))
        {
            var mask = new Panel { Bounds = rect, BackColor = Color.FromArgb(255, 14, 14, 18) };
            _form.Controls.Add(mask);
            mask.BringToFront();
            _active.Add(mask);
        }

        BuildCallout(t, hole);

        var clickTarget = GetClickGateTarget(_step);
        if (clickTarget != null)
        {
            _gateTarget = clickTarget;
            _gateHandler = (_, __) => { _step++; SafeRender(); };
            clickTarget.Click += _gateHandler;
        }

        _form.PerformLayout();
        foreach (var c in _active) c.Invalidate(true);
    }

    private void UpdateDemoModes()
    {
        bool wantMods = _step == 5;
        bool wantInstalled = _step is 7 or 8;

        if (wantMods && !_modsDemoActive)
        {
            _form.ModsPanelControl.OnTutorialInstalled = () => { _step++; SafeRender(); };
            _form.ModsPanelControl.ShowTutorialMod();
            _modsDemoActive = true;
        }
        else if (!wantMods && _modsDemoActive)
        {
            _form.ModsPanelControl.EndTutorialMod();
            _modsDemoActive = false;
        }

        if (wantInstalled && !_installedDemoActive)
        {
            _form.InstalledPanelControl.OnTutorialToggled = () => { if (_step == 7) { _step++; SafeRender(); } };
            _form.InstalledPanelControl.OnTutorialDeleted = () => { _step++; SafeRender(); };
            _form.InstalledPanelControl.ShowTutorialMod(enabled: true);
            _installedDemoActive = true;
        }
        else if (!wantInstalled && _installedDemoActive)
        {
            _form.InstalledPanelControl.EndTutorialMod();
            _installedDemoActive = false;
        }
    }

    private bool CanAdvance(int step) => step switch
    {
        4 => false,
        5 => false,
        6 => false,
        7 => false,
        8 => false,
        _ => true,
    };

    private Rectangle? GetTargetHole(int step) => step switch
    {
        1 => GetNavStripBounds(),
        2 => _form.PlayButtonControl is { } pb ? Inflate(TranslateToForm(pb), 6) : null,
        4 => _form.NavButtons is { Length: > 0 } n0 ? Inflate(TranslateToForm(n0[0]), 6) : null,
        5 => Inflate(TranslateToForm(_form.ModsPanelControl.InstallButtonControl), 6),
        6 => _form.NavButtons is { Length: > 1 } n1 ? Inflate(TranslateToForm(n1[1]), 6) : null,
        7 or 8 => Inflate(TranslateToForm(_form.InstalledPanelControl.ListControl), 4),
        9 => _form.NavButtons is { Length: > 6 } n6 ? Inflate(TranslateToForm(n6[6]), 6) : null,
        _ => null,
    };

    private RButton? GetClickGateTarget(int step) => step switch
    {
        4 => _form.NavButtons is { Length: > 0 } n0 ? n0[0] : null,
        6 => _form.NavButtons is { Length: > 1 } n1 ? n1[1] : null,
        _ => null,
    };

    private Rectangle? GetNavStripBounds()
    {
        var buttons = _form.NavButtons;
        if (buttons == null || buttons.Length == 0) return null;

        var rects = buttons.Select(TranslateToForm).ToList();
        int left = rects.Min(r => r.Left), top = rects.Min(r => r.Top);
        int right = rects.Max(r => r.Right), bottom = rects.Max(r => r.Bottom);
        return Inflate(Rectangle.FromLTRB(left, top, right, bottom), 6);
    }

    private Rectangle TranslateToForm(Control c)
    {
        var screenTopLeft = c.Parent!.PointToScreen(c.Location);
        var clientTopLeft = _form.PointToClient(screenTopLeft);
        return new Rectangle(clientTopLeft, c.Size);
    }

    private static Rectangle Inflate(Rectangle r, int amount)
    {
        r.Inflate(amount, amount);
        return r;
    }

    private static List<Rectangle> ComputeMasks(Rectangle full, Rectangle? hole)
    {
        if (hole is not { } h) return new List<Rectangle> { full };

        var result = new List<Rectangle>();
        if (h.Top > full.Top) result.Add(new Rectangle(full.Left, full.Top, full.Width, h.Top - full.Top));
        if (h.Bottom < full.Bottom) result.Add(new Rectangle(full.Left, h.Bottom, full.Width, full.Bottom - h.Bottom));
        if (h.Left > full.Left) result.Add(new Rectangle(full.Left, h.Top, h.Left - full.Left, h.Height));
        if (h.Right < full.Right) result.Add(new Rectangle(h.Right, h.Top, full.Right - h.Right, h.Height));
        return result;
    }

    private void BuildCallout(ThemeColors t, Rectangle? avoid)
    {
        const int w = 380, h = 170;
        var full = _form.ClientRectangle;
        int x, y;

        if (avoid is { } r)
        {
            x = Math.Clamp(r.Left + (r.Width - w) / 2, 12, Math.Max(12, full.Width - w - 12));
            y = r.Bottom + 14 + h <= full.Height ? r.Bottom + 14 : Math.Max(12, r.Top - 14 - h);
        }
        else
        {
            x = (full.Width - w) / 2;
            y = (full.Height - h) / 2;
        }

        var panel = new RPanel { Bounds = new Rectangle(x, y, w, h), CornerRadius = 12, BackColor = t.Surface };

        var title = new Label
        {
            Text = Steps[_step].Title, Location = new Point(18, 14), Width = w - 36, Height = 24,
            Font = new Font("Segoe UI", 11.5f, FontStyle.Bold), ForeColor = t.Text, BackColor = Color.Transparent,
        };
        var body = new Label
        {
            Text = Steps[_step].Body, Location = new Point(18, 44), Width = w - 36, Height = 76,
            Font = new Font("Segoe UI", 9f), ForeColor = t.SubText, BackColor = Color.Transparent,
        };

        var skip = new RButton { Text = "Skip", Width = 80, Height = 30, Location = new Point(18, h - 42) };
        var back = new RButton { Text = "Back", Width = 80, Height = 30, Location = new Point(w - 180, h - 42), Visible = _step > 0 };
        var next = new RButton
        {
            Text = _step == TotalSteps - 1 ? "Finish" : "Next", Width = 80, Height = 30,
            Location = new Point(w - 92, h - 42), Enabled = CanAdvance(_step),
        };
        ThemeEngine.StyleGhostButton(skip);
        ThemeEngine.StyleGhostButton(back);
        ThemeEngine.StyleRButton(next, accent: true);

        skip.Click += (_, __) => Finish();
        back.Click += (_, __) => { _step--; SafeRender(); };
        next.Click += (_, __) =>
        {
            if (_step >= TotalSteps - 1) Finish();
            else { _step++; SafeRender(); }
        };

        panel.Controls.Add(title);
        panel.Controls.Add(body);
        panel.Controls.Add(skip);
        panel.Controls.Add(back);
        panel.Controls.Add(next);

        _form.Controls.Add(panel);
        panel.BringToFront();
        _active.Add(panel);
    }
}
