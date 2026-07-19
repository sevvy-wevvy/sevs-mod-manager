using System.Drawing.Drawing2D;

namespace SevsModManager.UI.Controls;

internal class RPanel : Panel
{
    public int CornerRadius { get; set; } = 10;
    public Color BorderColor { get; set; } = Color.Transparent;
    public float BorderWidth { get; set; } = 1f;

    public Corners Corners { get; set; } = Corners.All;

    public RPanel()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                  ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw |
                  ControlStyles.SupportsTransparentBackColor, true);
        DoubleBuffered = true;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;

        var rect = new Rectangle(0, 0, Width - 1, Height - 1);
        using var path = RoundedGraphics.RoundedRect(rect, CornerRadius, Corners);

        using (var b = new SolidBrush(BackColor)) g.FillPath(b, path);
        if (BorderColor.A > 0 && BorderWidth > 0)
            using (var p = new Pen(BorderColor, BorderWidth)) g.DrawPath(p, path);
    }

    protected override void OnResize(EventArgs e)
    {
        base.OnResize(e);
        using var path = RoundedGraphics.RoundedRect(new Rectangle(0, 0, Width, Height), CornerRadius, Corners);
        Region = new Region(path);
        Invalidate();
    }

    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);
        using var path = RoundedGraphics.RoundedRect(new Rectangle(0, 0, Width, Height), CornerRadius, Corners);
        Region = new Region(path);
    }
}
