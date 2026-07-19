using System.Drawing.Drawing2D;

namespace SevsModManager.UI.Controls;

internal sealed class RScrollBar : Control
{
    private int _maximum;
    public int Maximum { get => _maximum; set { _maximum = Math.Max(0, value); ClampValue(); Invalidate(); } }

    private int _largeChange = 1;
    public int LargeChange { get => _largeChange; set { _largeChange = Math.Max(1, value); ClampValue(); Invalidate(); } }

    private int _value;
    public int Value
    {
        get => _value;
        set
        {
            int clamped = Math.Clamp(value, 0, Math.Max(0, _maximum - _largeChange));
            if (clamped == _value) return;
            _value = clamped;
            Invalidate();
            ValueChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    public event EventHandler? ValueChanged;

    public Color TrackColor { get; set; } = Color.Transparent;
    public Color ThumbColor { get; set; } = Color.FromArgb(90, 90, 90);
    public Color ThumbHoverColor { get; set; } = Color.FromArgb(120, 120, 120);

    private bool _hovering, _dragging;
    private int _dragStartY, _dragStartValue;

    public RScrollBar()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                  ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
        Width = 8;
        Cursor = Cursors.Default;

        MouseEnter += (_, __) => { _hovering = true; Invalidate(); };
        MouseLeave += (_, __) => { _hovering = false; Invalidate(); };
        MouseDown  += OnMouseDown;
        MouseMove  += OnMouseMove;
        MouseUp    += (_, __) => _dragging = false;
    }

    private bool NeedsScroll => _maximum > _largeChange;

    private RectangleF ThumbBounds()
    {
        if (!NeedsScroll) return RectangleF.Empty;
        float trackHeight = Height;
        float thumbHeight = Math.Max(24, trackHeight * _largeChange / _maximum);
        float range = trackHeight - thumbHeight;
        float thumbY = range <= 0 ? 0 : range * _value / Math.Max(1, _maximum - _largeChange);
        return new RectangleF(0, thumbY, Width, thumbHeight);
    }

    private void OnMouseDown(object? sender, MouseEventArgs e)
    {
        if (!NeedsScroll || e.Button != MouseButtons.Left) return;
        var thumb = ThumbBounds();
        if (thumb.Contains(e.X, e.Y))
        {
            _dragging = true;
            _dragStartY = e.Y;
            _dragStartValue = _value;
        }
        else
        {
            Value += e.Y < thumb.Top ? -_largeChange : _largeChange;
        }
    }

    private void OnMouseMove(object? sender, MouseEventArgs e)
    {
        if (!_dragging) return;
        float trackHeight = Height;
        var thumb = ThumbBounds();
        float range = trackHeight - thumb.Height;
        if (range <= 0) return;

        int maxValue = Math.Max(0, _maximum - _largeChange);
        int delta = (int)Math.Round((e.Y - _dragStartY) / range * maxValue);
        Value = _dragStartValue + delta;
    }

    private void ClampValue() => Value = _value;

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;

        if (TrackColor.A > 0)
            using (var b = new SolidBrush(TrackColor)) g.FillRectangle(b, ClientRectangle);

        if (!NeedsScroll) return;

        var thumb = ThumbBounds();
        thumb.Inflate(-1, 0);
        using var path = RoundedGraphics.RoundedRect(Rectangle.Round(thumb), Width / 2);
        using var thumbBrush = new SolidBrush(_hovering || _dragging ? ThumbHoverColor : ThumbColor);
        g.FillPath(thumbBrush, path);
    }
}
