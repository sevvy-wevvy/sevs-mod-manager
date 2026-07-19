using System.Drawing.Drawing2D;

namespace SevsModManager.UI.Controls;

    [Flags]
    internal enum Corners
    {
        None        = 0,
        TopLeft     = 1,
        TopRight    = 2,
        BottomRight = 4,
        BottomLeft  = 8,
        All         = TopLeft | TopRight | BottomRight | BottomLeft,
    }

internal static class RoundedGraphics
{
    public static GraphicsPath RoundedRect(Rectangle rect, int radius, Corners corners = Corners.All)
    {
        var path = new GraphicsPath();
        if (rect.Width <= 0 || rect.Height <= 0) return path;
        if (radius <= 0 || corners == Corners.None) { path.AddRectangle(rect); return path; }

        int d = radius * 2;
        d = Math.Max(0, Math.Min(d, Math.Min(rect.Width, rect.Height)));
        if (d == 0) { path.AddRectangle(rect); return path; }

        path.StartFigure();

        if (corners.HasFlag(Corners.TopLeft)) path.AddArc(rect.X, rect.Y, d, d, 180, 90);
        else path.AddLine(rect.X, rect.Y, rect.X, rect.Y);

        if (corners.HasFlag(Corners.TopRight)) path.AddArc(rect.Right - d, rect.Y, d, d, 270, 90);
        else path.AddLine(rect.Right, rect.Y, rect.Right, rect.Y);

        if (corners.HasFlag(Corners.BottomRight)) path.AddArc(rect.Right - d, rect.Bottom - d, d, d, 0, 90);
        else path.AddLine(rect.Right, rect.Bottom, rect.Right, rect.Bottom);

        if (corners.HasFlag(Corners.BottomLeft)) path.AddArc(rect.X, rect.Bottom - d, d, d, 90, 90);
        else path.AddLine(rect.X, rect.Bottom, rect.X, rect.Bottom);

        path.CloseFigure();
        return path;
    }

    public static Color Lerp(Color a, Color b, float t)
    {
        t = Math.Clamp(t, 0f, 1f);
        return Color.FromArgb(
            a.A + (int)((b.A - a.A) * t),
            a.R + (int)((b.R - a.R) * t),
            a.G + (int)((b.G - a.G) * t),
            a.B + (int)((b.B - a.B) * t));
    }

    public static Color Darken(Color c, int amount) =>
        Color.FromArgb(c.A, Math.Max(0, c.R - amount), Math.Max(0, c.G - amount), Math.Max(0, c.B - amount));

    public static Color Lighten(Color c, int amount) =>
        Color.FromArgb(c.A, Math.Min(255, c.R + amount), Math.Min(255, c.G + amount), Math.Min(255, c.B + amount));
}
