using System.Drawing.Drawing2D;

namespace SevsModManager.UI.Controls;

internal sealed class RFlowPanel : FlowLayoutPanel
{
    public int CornerRadius { get; set; } = 10;
    public Color BorderColor { get; set; } = Color.Transparent;

    public RFlowPanel()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                  ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        var rect = new Rectangle(0, 0, Width - 1, Height - 1);
        using var path = RoundedGraphics.RoundedRect(rect, CornerRadius);
        using (var b = new SolidBrush(BackColor)) g.FillPath(b, path);
        if (BorderColor.A > 0)
            using (var p = new Pen(BorderColor, 1f)) g.DrawPath(p, path);
    }

    protected override void OnResize(EventArgs e)
    {
        base.OnResize(e);
        if (Width > 0 && Height > 0)
        {
            using var path = RoundedGraphics.RoundedRect(new Rectangle(0, 0, Width, Height), CornerRadius);
            Region = new Region(path);
        }
        Invalidate();
    }
}
