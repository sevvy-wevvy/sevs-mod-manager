using System.Runtime.InteropServices;

namespace SevsModManager.Theme;

internal struct ThemeColors
{
    public Color Background;
    public Color Surface;
    public Color SurfaceAlt;
    public Color Border;
    public Color Text;
    public Color SubText;
    public Color Accent;
    public Color AccentText;

    public Color Highlight
    {
        get
        {
            bool dark = Background.R + Background.G + Background.B < 384;
            int amt = dark ? 22 : -22;
            return Color.FromArgb(
                Math.Clamp(SurfaceAlt.R + amt, 0, 255),
                Math.Clamp(SurfaceAlt.G + amt, 0, 255),
                Math.Clamp(SurfaceAlt.B + amt, 0, 255));
        }
    }
}

internal static class ThemeEngine
{
    public static ThemeColors Current { get; private set; } = Black();
    public static event Action? ThemeChanged;

    public static void Apply(Core.ThemeMode mode, string? customBg = null, string? customAccent = null, string? sbAccentHex = null)
    {
        Current = mode switch
        {
            Core.ThemeMode.White     => White(),
            Core.ThemeMode.Custom    => BuildCustom(customBg, customAccent),
            Core.ThemeMode.SyncedSB  => BuildSynced(sbAccentHex),
            Core.ThemeMode.R2Modman  => R2Modman(),
            _                        => Black(),
        };
        ThemeChanged?.Invoke();
    }

    public static void Recolor(Control root)
    {
        ApplyControl(root);
        foreach (Control c in root.Controls)
            Recolor(c);
    }

    public static ThemeColors Black() => new()
    {
        Background = Color.FromArgb(20, 20, 20),
        Surface    = Color.FromArgb(30, 30, 30),
        SurfaceAlt = Color.FromArgb(62, 62, 62),
        Border     = Color.FromArgb(80, 80, 80),
        Text       = Color.FromArgb(232, 230, 239),
        SubText    = Color.FromArgb(160, 160, 160),
        Accent     = Color.FromArgb(124, 58, 237),
        AccentText = Color.White,
    };

    public static ThemeColors White() => new()
    {
        Background = Color.FromArgb(248, 248, 248),
        Surface    = Color.White,
        SurfaceAlt = Color.FromArgb(238, 238, 238),
        Border     = Color.FromArgb(210, 210, 210),
        Text       = Color.FromArgb(20, 20, 20),
        SubText    = Color.FromArgb(100, 100, 100),
        Accent     = Color.FromArgb(124, 58, 237),
        AccentText = Color.White,
    };

    public static ThemeColors R2Modman() => new()
    {
        Background = Color.FromArgb(27, 29, 39),
        Surface    = Color.FromArgb(35, 37, 47),
        SurfaceAlt = Color.FromArgb(46, 48, 61),
        Border     = Color.FromArgb(58, 61, 77),
        Text       = Color.FromArgb(228, 228, 232),
        SubText    = Color.FromArgb(154, 156, 173),
        Accent     = Color.FromArgb(88, 101, 242),
        AccentText = Color.White,
    };

    private static ThemeColors BuildCustom(string? bg, string? accent)
    {
        var base_ = Black();
        if (TryHex(bg, out var bgColor))
        {
            base_.Background = bgColor;
            base_.Surface    = Lighten(bgColor, 10);
            base_.SurfaceAlt = Lighten(bgColor, 20);
            base_.Border     = Lighten(bgColor, 35);
        }
        if (TryHex(accent, out var accentColor))
            base_.Accent = accentColor;
        return base_;
    }

    private static ThemeColors BuildSynced(string? sbHex)
    {
        var base_ = Black();
        if (TryHex(sbHex, out var accent))
            base_.Accent = accent;
        return base_;
    }

    private static void ApplyControl(Control c)
    {

        if (c.Tag is string skip && skip == "swatch") return;

        Color parentBg = c.Parent?.BackColor ?? Current.Background;

        c.BackColor = c.Tag is string tag && tag == "surface"    ? Current.SurfaceAlt
                    : c.Tag is string tag2 && tag2 == "surfacealt" ? Current.SurfaceAlt
                    : c.Tag is string tag3 && tag3 == "accent"     ? Current.Accent
                    : parentBg;

        c.ForeColor = c.Tag is string tag4 && tag4 == "subtext"  ? Current.SubText
                    : c.Tag is string tag5 && tag5 == "accent"    ? Current.AccentText
                    : Current.Text;

        if (c is TextBox tb) { tb.BackColor = Current.SurfaceAlt; tb.ForeColor = Current.Text; tb.BorderStyle = BorderStyle.FixedSingle; }
        if (c is ListBox lb) { lb.BackColor = Current.Surface;    lb.ForeColor = Current.Text; }
        if (c is ListView lv){ lv.BackColor = Current.Surface;    lv.ForeColor = Current.Text; }
        if (c is ComboBox cb){ cb.BackColor = Current.SurfaceAlt; cb.ForeColor = Current.Text; }
        if (c is CheckBox chk) chk.ForeColor = Current.Text;
        if (c is RadioButton rb) rb.ForeColor = Current.Text;
        if (c is GroupBox gb) gb.ForeColor = Current.SubText;
        if (c is Panel p and not (UI.Controls.RPanel or UI.Controls.RFlowPanel))
            p.BackColor = c.Tag is string s && s == "surface" ? Current.SurfaceAlt : parentBg;

        if (c is UI.Controls.RButton rBtn) StyleRButton(rBtn);
        if (c is UI.Controls.RPanel rPanel)
        {
            rPanel.BackColor = c.Tag is string rs && rs == "surface" ? Current.SurfaceAlt : parentBg;
            rPanel.BorderColor = Color.Transparent;
        }
        if (c is UI.Controls.RFlowPanel rFlow)
        {
            rFlow.BackColor = c.Tag is string fs && fs == "surface" ? Current.SurfaceAlt : parentBg;
            rFlow.BorderColor = Color.Transparent;
        }
    }

    public static void StyleRButton(UI.Controls.RButton btn, bool accent = false)
    {
        accent = accent || btn.Tag is string t && t == "accent";
        if (accent)
        {
            btn.Style = UI.Controls.RButtonStyle.Solid;
            btn.FillColor = Current.Accent;
            btn.HoverFillColor = Lighten(Current.Accent, 18);
            btn.BorderColor = Current.Accent;
            btn.HoverBorderColor = Current.Accent;
            btn.ForeColor = Current.AccentText;
        }
        else
        {
            bool onDark = Current.Background.GetBrightness() < 0.5f;
            btn.Style = UI.Controls.RButtonStyle.Solid;
            btn.FillColor = onDark ? Lighten(Current.SurfaceAlt, 20) : Darken(Current.SurfaceAlt, 10);
            btn.HoverFillColor = onDark ? Lighten(Current.SurfaceAlt, 34) : Darken(Current.SurfaceAlt, 20);
            btn.BorderColor = Color.Transparent;
            btn.HoverBorderColor = Color.Transparent;
            btn.ForeColor = Current.Text;
        }
        ApplyLayoutCornerStyle(btn);
    }

    public static void StyleGhostButton(UI.Controls.RButton btn)
    {
        btn.Style = UI.Controls.RButtonStyle.Ghost;
        btn.FillColor = Color.Transparent;
        btn.HoverFillColor = Current.SurfaceAlt;
        btn.BorderColor = Color.Transparent;
        btn.HoverBorderColor = Color.Transparent;
        btn.ForeColor = Current.Text;
        ApplyLayoutCornerStyle(btn);
    }

    public static void ApplyLayoutCornerStyle(UI.Controls.RButton btn)
    {
        if (Core.AppState.Settings.Layout != Core.AppLayout.SevsModManager) btn.CornerRadius = 0;
    }

    public static void ApplyLayoutCornerStyle(UI.Controls.RTextBox box)
    {
        if (Core.AppState.Settings.Layout != Core.AppLayout.SevsModManager) box.CornerRadius = 0;
    }

    public static void ApplyLayoutCornerStyle(UI.Controls.RDropdown dd)
    {
        if (Core.AppState.Settings.Layout != Core.AppLayout.SevsModManager) dd.CornerRadius = 0;
    }

    [DllImport("uxtheme.dll", CharSet = CharSet.Unicode)]
    private static extern int SetWindowTheme(IntPtr hwnd, string? pszSubAppName, string? pszSubIdList);

    public static void StripVisualStyle(Control c)
    {
        if (c.IsHandleCreated) SetWindowTheme(c.Handle, "", "");
    }

    public static void ApplyScrollTheme(Control root)
    {
        bool dark = Current.Background.GetBrightness() < 0.5f;
        ApplyScrollControl(root, dark);
        foreach (Control c in root.Controls)
            ApplyScrollTheme(c);
    }

    private static void ApplyScrollControl(Control c, bool dark)
    {
        if (!c.IsHandleCreated) return;

        bool scrollable = c is ListBox or TextBox { Multiline: true } or RichTextBox or TreeView or ListView
                       || (c is Panel { AutoScroll: true });
        if (!scrollable) return;
        SetWindowTheme(c.Handle, dark ? "DarkMode_Explorer" : "Explorer", null);
    }

    public static bool TryHex(string? hex, out Color color)
    {
        color = Color.Empty;
        if (string.IsNullOrEmpty(hex)) return false;
        hex = hex.TrimStart('#');
        if (hex.Length == 6 && int.TryParse(hex, System.Globalization.NumberStyles.HexNumber, null, out int rgb))
        {
            color = Color.FromArgb(0xFF, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
            return true;
        }
        return false;
    }

    private static Color Lighten(Color c, int amount) =>
        Color.FromArgb(Math.Min(255, c.R + amount), Math.Min(255, c.G + amount), Math.Min(255, c.B + amount));

    private static Color Darken(Color c, int amount) =>
        Color.FromArgb(Math.Max(0, c.R - amount), Math.Max(0, c.G - amount), Math.Max(0, c.B - amount));

    public static string ToHex(Color c) => $"#{c.R:X2}{c.G:X2}{c.B:X2}";
}
