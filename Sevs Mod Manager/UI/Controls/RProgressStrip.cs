using System.Drawing.Drawing2D;

namespace SevsModManager.UI.Controls;

internal sealed class RProgressStrip : Control
{
    private int _value;
    private Color _trackColor = Color.Transparent;
    private Color _fillColor = Color.FromArgb(124, 58, 237);

    public int Value
    {
        get => _value;
        set { _value = Math.Clamp(value, 0, 100); Invalidate(); }
    }

    public Color TrackColor { get => _trackColor; set { _trackColor = value; Invalidate(); } }
    public Color FillColor  { get => _fillColor;  set { _fillColor  = value; Invalidate(); } }

    public RProgressStrip()
    {
        SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer, true);
        Height = 3;
        Dock = DockStyle.Bottom;
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        e.Graphics.SmoothingMode = SmoothingMode.None;
        if (_trackColor != Color.Transparent)
            e.Graphics.FillRectangle(new SolidBrush(_trackColor), 0, 0, Width, Height);

        if (_value <= 0) return;
        int fillWidth = Width * _value / 100;
        e.Graphics.FillRectangle(new SolidBrush(_fillColor), 0, 0, fillWidth, Height);
    }
}
