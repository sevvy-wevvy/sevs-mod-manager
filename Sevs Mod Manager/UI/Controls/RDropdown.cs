using System.Drawing.Drawing2D;

namespace SevsModManager.UI.Controls;

internal sealed class RDropdown : Control
{
    public List<string> Items { get; } = new();

    private int _selectedIndex = -1;
    public int SelectedIndex
    {
        get => _selectedIndex;
        set { if (_selectedIndex == value) return; _selectedIndex = value; Invalidate(); SelectedIndexChanged?.Invoke(this, EventArgs.Empty); }
    }
    public event EventHandler? SelectedIndexChanged;

    private int _cornerRadius = 8;
    public int CornerRadius { get => _cornerRadius; set { _cornerRadius = value; Invalidate(); } }

    private Corners _roundedCorners = Corners.All;
    public Corners RoundedCorners { get => _roundedCorners; set { _roundedCorners = value; Invalidate(); } }

    private Color _fillColor = Color.Transparent;
    public Color FillColor { get => _fillColor; set { _fillColor = value; Invalidate(); } }

    private Color _hoverFillColor = Color.Transparent;
    public Color HoverFillColor { get => _hoverFillColor; set { _hoverFillColor = value; Invalidate(); } }

    private Color _borderColor = Color.Transparent;
    public Color BorderColor { get => _borderColor; set { _borderColor = value; Invalidate(); } }

    private float _hover;
    private bool _hovering;
    private RDropdownPopup? _popup;
    private readonly System.Windows.Forms.Timer _timer;

    public RDropdown()
    {
        SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint |
                  ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw |
                  ControlStyles.SupportsTransparentBackColor, true);
        BackColor = Color.Transparent;
        Cursor = Cursors.Hand;
        Size = new Size(130, 30);
        Font = new Font("Segoe UI", 9.5f);

        _timer = new System.Windows.Forms.Timer { Interval = 15 };
        _timer.Tick += (_, __) => StepAnimation();
        MouseEnter += (_, __) => { _hovering = true; _timer.Start(); };
        MouseLeave += (_, __) => { _hovering = false; _timer.Start(); };
        Click += (_, __) => ToggleOpen();
    }

    private void StepAnimation()
    {
        float target = _hovering ? 1f : 0f;
        const float step = 0.2f;
        if (Math.Abs(_hover - target) <= step) { _hover = target; _timer.Stop(); }
        else _hover += target > _hover ? step : -step;
        Invalidate();
    }

    private void ToggleOpen()
    {
        if (_popup != null) { _popup.Close(); return; }

        var popup = new RDropdownPopup(Items, _selectedIndex) { Width = Math.Max(Width, 140) };
        popup.Location = PointToScreen(new Point(0, Height + 2));
        popup.ItemChosen += idx => SelectedIndex = idx;
        popup.FormClosed += (_, __) => _popup = null;
        _popup = popup;
        popup.Show();
    }

    protected override void OnPaint(PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;

        var rect = new Rectangle(0, 0, Width - 1, Height - 1);
        using var path = RoundedGraphics.RoundedRect(rect, _cornerRadius, _roundedCorners);

        Color fill = RoundedGraphics.Lerp(_fillColor, _hoverFillColor, _hover);
        if (fill.A > 0) using (var b = new SolidBrush(fill)) g.FillPath(b, path);
        if (_borderColor.A > 0) using (var p = new Pen(_borderColor, 1.2f)) g.DrawPath(p, path);

        string text = _selectedIndex >= 0 && _selectedIndex < Items.Count ? Items[_selectedIndex] : "";
        var textRect = new Rectangle(10, 0, Width - 28, Height);
        TextRenderer.DrawText(g, text, Font, textRect, ForeColor,
            TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding | TextFormatFlags.EndEllipsis);

        using var caretPen = new Pen(ForeColor, 1.6f) { StartCap = LineCap.Round, EndCap = LineCap.Round, LineJoin = LineJoin.Round };
        int cx = Width - 16, cy = Height / 2 - 1;
        g.DrawLines(caretPen, new[] { new Point(cx - 4, cy - 2), new Point(cx, cy + 2), new Point(cx + 4, cy - 2) });
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing) { _timer.Dispose(); _popup?.Close(); }
        base.Dispose(disposing);
    }
}

internal sealed class RDropdownPopup : Form
{
    public event Action<int>? ItemChosen;

    private const int RowHeight = 28;
    private const int MaxVisibleRows = 12;

    private readonly List<Label> _rows = new();

    public RDropdownPopup(List<string> items, int current)
    {
        FormBorderStyle = FormBorderStyle.None;
        StartPosition = FormStartPosition.Manual;
        ShowInTaskbar = false;
        TopMost = true;

        int fullHeight = items.Count * RowHeight + 4;
        int maxHeight  = MaxVisibleRows * RowHeight + 4;
        Height = Math.Min(fullHeight, maxHeight);
        AutoScroll = fullHeight > maxHeight;

        var t = Theme.ThemeEngine.Current;
        BackColor = t.Border;

        var flow = new FlowLayoutPanel
        {
            FlowDirection = FlowDirection.TopDown, WrapContents = false, Padding = new Padding(1),
            AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, Location = new Point(0, 0),
        };

        for (int i = 0; i < items.Count; i++)
        {
            int idx = i;
            var row = new Label
            {
                Text = items[i], Height = RowHeight - 1,
                TextAlign = ContentAlignment.MiddleLeft, Padding = new Padding(10, 0, 0, 0),
                ForeColor = t.Text, BackColor = idx == current ? t.Highlight : t.SurfaceAlt,
                Cursor = Cursors.Hand,
            };
            row.Click += (_, __) => { ItemChosen?.Invoke(idx); Close(); };
            row.MouseEnter += (_, __) => row.BackColor = t.Highlight;
            row.MouseLeave += (_, __) => row.BackColor = idx == current ? t.Highlight : t.SurfaceAlt;
            _rows.Add(row);
            flow.Controls.Add(row);
        }

        Controls.Add(flow);
        Resize += (_, __) => UpdateRowWidths();
        UpdateRowWidths();
        Deactivate += (_, __) => Close();
    }

    private void UpdateRowWidths()
    {
        int w = Width - 2 - (AutoScroll ? SystemInformation.VerticalScrollBarWidth : 0);
        foreach (var r in _rows) r.Width = Math.Max(0, w);
    }
}
