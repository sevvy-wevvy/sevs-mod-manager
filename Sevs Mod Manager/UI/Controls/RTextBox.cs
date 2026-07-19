using System.Drawing.Drawing2D;

namespace SevsModManager.UI.Controls;

internal sealed class RTextBox : Panel
{
    public TextBox Inner { get; }

    private int _cornerRadius = 8;
    public int CornerRadius
    {
        get => _cornerRadius;
        set { _cornerRadius = value; UpdateRegion(); }
    }

    private Corners _roundedCorners = Corners.All;

    public Corners RoundedCorners
    {
        get => _roundedCorners;
        set { _roundedCorners = value; UpdateRegion(); }
    }

#pragma warning disable CS8764, CS8765
    public override string Text
    {
        get => Inner.Text;
        set => Inner.Text = value ?? "";
    }
#pragma warning restore CS8764, CS8765

    public string PlaceholderText
    {
        get => Inner.PlaceholderText;
        set => Inner.PlaceholderText = value;
    }

    public new event EventHandler? TextChanged { add => Inner.TextChanged += value; remove => Inner.TextChanged -= value; }

    public RTextBox()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                  ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
        Height = 28;
        Padding = new Padding(10, 0, 10, 0);

        Inner = new TextBox
        {
            BorderStyle = BorderStyle.None,
            Anchor = AnchorStyles.Left | AnchorStyles.Right | AnchorStyles.Top,
            Font = new Font("Segoe UI", 9.5f),
        };
        Controls.Add(Inner);
        PositionInner();
    }

    private void PositionInner()
    {

        if (Inner == null) return;
        Inner.Left = Padding.Left;
        Inner.Width = Math.Max(0, Width - Padding.Horizontal);
        Inner.Top = Math.Max(0, (Height - Inner.PreferredHeight) / 2);
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        var rect = new Rectangle(0, 0, Width - 1, Height - 1);
        using var path = RoundedGraphics.RoundedRect(rect, CornerRadius, RoundedCorners);
        using var b = new SolidBrush(BackColor);
        g.FillPath(b, path);
    }

    protected override void OnResize(EventArgs e)
    {
        base.OnResize(e);
        UpdateRegion();
        PositionInner();
    }

    private void UpdateRegion()
    {
        if (Width > 0 && Height > 0)
        {
            using var path = RoundedGraphics.RoundedRect(new Rectangle(0, 0, Width, Height), _cornerRadius, _roundedCorners);
            Region = new Region(path);
        }
        Invalidate();
    }

    public override Color ForeColor
    {
        get => Inner.ForeColor;
        set => Inner.ForeColor = value;
    }

    public override Color BackColor
    {
        get => Inner.BackColor;
        set { base.BackColor = value; Inner.BackColor = value; Invalidate(); }
    }
}
