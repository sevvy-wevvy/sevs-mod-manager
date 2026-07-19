using System.Drawing.Drawing2D;

namespace SevsModManager.UI.Controls;

internal enum RButtonStyle { Solid, Outline, Ghost }

internal sealed class RButton : Control
{

    private int _cornerRadius = 8;
    public int CornerRadius { get => _cornerRadius; set { _cornerRadius = value; Invalidate(); } }

    private RButtonStyle _style = RButtonStyle.Outline;
    public RButtonStyle Style { get => _style; set { _style = value; Invalidate(); } }

    private Corners _roundedCorners = Corners.All;
    public Corners RoundedCorners { get => _roundedCorners; set { _roundedCorners = value; Invalidate(); } }

    private Color _fillColor = Color.Transparent;
    public Color FillColor { get => _fillColor; set { _fillColor = value; Invalidate(); } }

    private Color _hoverFillColor = Color.Transparent;
    public Color HoverFillColor { get => _hoverFillColor; set { _hoverFillColor = value; Invalidate(); } }

    private Color _borderColor = Color.Transparent;
    public Color BorderColor { get => _borderColor; set { _borderColor = value; Invalidate(); } }

    private Color _hoverBorderColor = Color.Transparent;
    public Color HoverBorderColor { get => _hoverBorderColor; set { _hoverBorderColor = value; Invalidate(); } }

    private float _borderWidth = 1.2f;
    public float BorderWidth { get => _borderWidth; set { _borderWidth = value; Invalidate(); } }

    private ContentAlignment _textAlign = ContentAlignment.MiddleCenter;
    public ContentAlignment TextAlign { get => _textAlign; set { _textAlign = value; Invalidate(); } }

    private float _hover;
    private bool _hovering, _pressed;
    private readonly System.Windows.Forms.Timer _timer;

    public RButton()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                  ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw |
                  ControlStyles.SupportsTransparentBackColor, true);
        BackColor = Color.Transparent;
        Cursor = Cursors.Hand;
        Size = new Size(96, 30);
        Font = new Font("Segoe UI", 9.5f);

        _timer = new System.Windows.Forms.Timer { Interval = 15 };
        _timer.Tick += (_, __) => StepAnimation();

        MouseEnter += (_, __) => { _hovering = true; _timer.Start(); };
        MouseLeave += (_, __) => { _hovering = false; _pressed = false; _timer.Start(); };
        MouseDown  += (_, __) => { _pressed = true; Invalidate(); };
        MouseUp    += (_, __) => { _pressed = false; Invalidate(); };
    }

    private void StepAnimation()
    {
        float target = _hovering ? 1f : 0f;
        const float step = 0.2f;
        if (Math.Abs(_hover - target) <= step) { _hover = target; _timer.Stop(); }
        else _hover += target > _hover ? step : -step;
        Invalidate();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;

        var rect = new Rectangle(0, 0, Width - 1, Height - 1);
        using var path = RoundedGraphics.RoundedRect(rect, CornerRadius, RoundedCorners);

        Color fill = RoundedGraphics.Lerp(FillColor, HoverFillColor, _hover);
        Color border = RoundedGraphics.Lerp(BorderColor, HoverBorderColor, _hover);

        if (!Enabled)
        {
            fill = RoundedGraphics.Darken(fill, 40);
            border = RoundedGraphics.Darken(border, 40);
        }
        else if (_pressed)
        {
            fill = RoundedGraphics.Darken(fill, 14);
        }

        if (fill.A > 0)
            using (var b = new SolidBrush(fill)) g.FillPath(b, path);

        if (Style != RButtonStyle.Solid && border.A > 0)
            using (var p = new Pen(border, BorderWidth)) g.DrawPath(p, path);

        var fore = Enabled ? ForeColor : RoundedGraphics.Darken(ForeColor, 90);
        var hAlign = TextAlign switch
        {
            ContentAlignment.TopLeft or ContentAlignment.MiddleLeft or ContentAlignment.BottomLeft => TextFormatFlags.Left,
            ContentAlignment.TopRight or ContentAlignment.MiddleRight or ContentAlignment.BottomRight => TextFormatFlags.Right,
            _ => TextFormatFlags.HorizontalCenter,
        };
        var textRect = new Rectangle(Padding.Left, 0, Math.Max(0, Width - Padding.Horizontal), Height);
        TextRenderer.DrawText(g, Text, Font, textRect, fore,
            hAlign | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding | TextFormatFlags.EndEllipsis);
    }

    protected override void OnTextChanged(EventArgs e)
    {
        base.OnTextChanged(e);
        if (AutoSize) Size = GetPreferredSize(Size.Empty);
        Invalidate();
    }

    protected override Size DefaultSize => new(96, 30);

    public override Size GetPreferredSize(Size proposedSize)
    {
        if (!AutoSize) return base.GetPreferredSize(proposedSize);
        var textSize = TextRenderer.MeasureText(Text, Font);
        int w = textSize.Width + Padding.Horizontal + 20;
        int h = Math.Max(textSize.Height + Padding.Vertical + 10, Height);
        return new Size(w, h);
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) _timer.Dispose();
        base.Dispose(disposing);
    }
}
