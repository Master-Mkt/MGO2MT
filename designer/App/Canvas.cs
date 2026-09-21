using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using FPoint = System.Drawing.PointF;
using FRect = System.Drawing.RectangleF;
namespace MgoUi;

internal sealed class Canvas : Control
{
    [System.ComponentModel.DesignerSerializationVisibility(System.ComponentModel.DesignerSerializationVisibility.Hidden)]
    public LayoutDocument Document { get; set; } = Samples.Create();
    [System.ComponentModel.DesignerSerializationVisibility(System.ComponentModel.DesignerSerializationVisibility.Hidden)]
    public NativeRenderer Renderer { get; set; } = null!;
    [System.ComponentModel.DesignerSerializationVisibility(System.ComponentModel.DesignerSerializationVisibility.Hidden)]
    public string AssetRoot { get; set; } = "";
    [System.ComponentModel.DesignerSerializationVisibility(System.ComponentModel.DesignerSerializationVisibility.Hidden)]
    public string Screen { get; set; } = "title";
    [System.ComponentModel.DesignerSerializationVisibility(System.ComponentModel.DesignerSerializationVisibility.Hidden)]
    public string State { get; set; } = "default";
    [System.ComponentModel.DesignerSerializationVisibility(System.ComponentModel.DesignerSerializationVisibility.Hidden)]
    public HashSet<string> Flags { get; set; } = [];
    [System.ComponentModel.DesignerSerializationVisibility(System.ComponentModel.DesignerSerializationVisibility.Hidden)]
    public string Bindings { get; set; } = "{}";
    [System.ComponentModel.DesignerSerializationVisibility(System.ComponentModel.DesignerSerializationVisibility.Hidden)]
    public bool Grid { get; set; } = true;
    [System.ComponentModel.DesignerSerializationVisibility(System.ComponentModel.DesignerSerializationVisibility.Hidden)]
    public bool Snap { get; set; } = true;
    [System.ComponentModel.DesignerSerializationVisibility(System.ComponentModel.DesignerSerializationVisibility.Hidden)]
    public Element? Selected { get; set; }
    public event Action? SelectionChanged;
    public event Action? EditCompleted;
    public event Action<string, PointF>? ToolDropped;
    public event Action<string>? Diagnostic;
    [System.ComponentModel.DesignerSerializationVisibility(System.ComponentModel.DesignerSerializationVisibility.Hidden)]
    public bool PreviewMode { get; set; }
    public event Action<PointF, bool>? ActionPointer;
    public event Func<Keys,bool>? ActionKey;
    private readonly HashSet<Keys> heldKeys=[];
    private Bitmap? image;
    private bool dirty = true, dragging;
    private PointF origin; private RectF start; private int handle = -1;
    private readonly Cursor moveCursor = Cursors.SizeAll;
    public Canvas() { DoubleBuffered = true; BackColor = Color.FromArgb(24, 28, 34); TabStop = true; AllowDrop = true; SetStyle(ControlStyles.ResizeRedraw, true); DragEnter += (_, e) => { if (e.Data?.GetDataPresent(DataFormats.Text) == true) e.Effect = DragDropEffects.Copy; }; DragDrop += (_, e) => { var s = e.Data?.GetData(DataFormats.Text) as string; if (s is "panel" or "text" or "image" or "button" or "dropdown") { var p = PointToClient(new Point(e.X, e.Y)); ToolDropped?.Invoke(s, View.ToDesign(p.X, p.Y)); } }; }
    public void RefreshPreview() { dirty = true; Invalidate(); }
    private Viewport View => Viewport.Contain(Math.Max(1, Width), Math.Max(1, Height));
    protected override void OnResize(EventArgs e) { base.OnResize(e); dirty = true; }
    protected override void Dispose(bool disposing) { if (disposing) image?.Dispose(); base.Dispose(disposing); }
    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e); var g = e.Graphics; g.Clear(BackColor); var v = View;
        if (dirty && Renderer is not null) { dirty = false; image?.Dispose(); image = null; try { if (Renderer.Load(Document, AssetRoot)) { var bytes = Renderer.Render(Width, Height, Screen, State, Flags, Bindings); if (bytes is not null) { for (int i = 0; i < bytes.Length; i += 4) (bytes[i], bytes[i + 2]) = (bytes[i + 2], bytes[i]); image = new Bitmap(Width, Height, PixelFormat.Format32bppArgb); var data = image.LockBits(new Rectangle(0, 0, Width, Height), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb); try { for (int row = 0; row < Height; row++) Marshal.Copy(bytes, row * Width * 4, data.Scan0 + row * data.Stride, Width * 4); } finally { image.UnlockBits(data); } } } Diagnostic?.Invoke(Renderer.Error); } catch (Exception ex) { Diagnostic?.Invoke(ex.Message); } }
        if (image is not null) g.DrawImageUnscaled(image, 0, 0); else { using var brush = new SolidBrush(Color.FromArgb(38, 47, 57)); g.FillRectangle(brush, v.Left, v.Top, v.Width, v.Height); TextRenderer.DrawText(g, "共有描画を準備しています。DLL の配置と素材フォルダーを確認してください。", Font, new Rectangle((int)v.Left + 22, (int)v.Top + 24, (int)v.Width - 44, 70), Color.LightSteelBlue, TextFormatFlags.WordBreak); }
        if (Grid) { using var pen = new Pen(Color.FromArgb(35, 154, 183, 199)); for (int x = 0; x <= 1280; x += 40) g.DrawLine(pen, v.Left + x * v.Scale, v.Top, v.Left + x * v.Scale, v.Top + v.Height); for (int y = 0; y <= 720; y += 40) g.DrawLine(pen, v.Left, v.Top + y * v.Scale, v.Left + v.Width, v.Top + y * v.Scale); }
        using (var border = new Pen(Color.FromArgb(77, 94, 109))) g.DrawRectangle(border, v.Left, v.Top, v.Width, v.Height);
        if (!PreviewMode && Selected is not null && Selected.Screen == Screen) { var b = v.ToPixel(LayoutRules.Bounds(Selected)); using var pen = new Pen(Color.FromArgb(64, 209, 184), 2); g.DrawRectangle(pen, b.X, b.Y, b.Width, b.Height); foreach (var p in Handles(b)) { using var fill = new SolidBrush(Color.FromArgb(64, 209, 184)); g.FillRectangle(fill, p.X - 4, p.Y - 4, 8, 8); } var f = LayoutRules.Factor(Selected.Anchor); float ax = v.Left + 1280 * f.X * v.Scale, ay = v.Top + 720 * f.Y * v.Scale; g.DrawLine(pen, ax - 7, ay, ax + 7, ay); g.DrawLine(pen, ax, ay - 7, ax, ay + 7); }
    }
    internal void ExerciseDrag(PointF from, PointF to) { var v = View; var a = v.ToPixel(new(from.X, from.Y, 0, 0)); var b = v.ToPixel(new(to.X, to.Y, 0, 0)); OnMouseDown(new(MouseButtons.Left, 1, (int)Math.Round(a.X), (int)Math.Round(a.Y), 0)); OnMouseMove(new(MouseButtons.Left, 0, (int)Math.Round(b.X), (int)Math.Round(b.Y), 0)); OnMouseUp(new(MouseButtons.Left, 1, (int)Math.Round(b.X), (int)Math.Round(b.Y), 0)); }
    private static FPoint[] Handles(RectF b) => [new(b.X, b.Y), new(b.X + b.Width / 2, b.Y), new(b.X + b.Width, b.Y), new(b.X + b.Width, b.Y + b.Height / 2), new(b.X + b.Width, b.Y + b.Height), new(b.X + b.Width / 2, b.Y + b.Height), new(b.X, b.Y + b.Height), new(b.X, b.Y + b.Height / 2)];
    protected override void OnMouseDown(MouseEventArgs e)
    {
        base.OnMouseDown(e); if (e.Button != MouseButtons.Left) return; Focus(); var v = View; if (PreviewMode) { Capture = true; ActionPointer?.Invoke(v.ToDesign(e.X, e.Y), true); return; } handle = -1; if (Selected is not null) { var handles = Handles(v.ToPixel(LayoutRules.Bounds(Selected))); for (int i = 0; i < handles.Length; i++) if (Math.Abs(handles[i].X - e.X) <= 7 && Math.Abs(handles[i].Y - e.Y) <= 7) { handle = i; break; } }
        var p = v.ToDesign(e.X, e.Y); if (handle < 0) Selected = LayoutRules.Hit(Document, p, Screen, State, Flags); SelectionChanged?.Invoke(); if (Selected is null) { Invalidate(); return; }
        origin = p; start = LayoutRules.Bounds(Selected); dragging = true; Capture = true; Invalidate();
    }
    protected override void OnMouseMove(MouseEventArgs e)
    {
        base.OnMouseMove(e); if (PreviewMode) { ActionPointer?.Invoke(View.ToDesign(e.X, e.Y), (e.Button & MouseButtons.Left) != 0); return; } if (!dragging || Selected is null) return; var p = View.ToDesign(e.X, e.Y); var dx = p.X - origin.X; var dy = p.Y - origin.Y; float Round(float x) => Snap ? (float)Math.Round(x / 8) * 8 : x;
        float x = start.X, y = start.Y, right = start.X + start.Width, bottom = start.Y + start.Height; if (handle < 0) { x = Round(x + dx); y = Round(y + dy); right = x + start.Width; bottom = y + start.Height; Cursor = moveCursor; } else { if (handle is 0 or 6 or 7) x = Math.Min(right - 8, Round(x + dx)); if (handle is 2 or 3 or 4) right = Math.Max(x + 8, Round(right + dx)); if (handle is 0 or 1 or 2) y = Math.Min(bottom - 8, Round(y + dy)); if (handle is 4 or 5 or 6) bottom = Math.Max(y + 8, Round(bottom + dy)); }
        x = Math.Clamp(x, -6000, 6000); y = Math.Clamp(y, -6000, 6000); LayoutRules.SetBounds(Selected, new(x, y, Math.Clamp(right - x, 8, Selected.Kind == "text" ? 1280 : 4096), Math.Clamp(bottom - y, 8, Selected.Kind == "text" ? 720 : 4096))); RefreshPreview();
    }
    protected override void OnMouseUp(MouseEventArgs e) { base.OnMouseUp(e); if (PreviewMode) { ActionPointer?.Invoke(View.ToDesign(e.X, e.Y), false); Capture = false; return; } if (!dragging) return; dragging = false; Capture = false; Cursor = Cursors.Default; EditCompleted?.Invoke(); }
    protected override void OnMouseCaptureChanged(EventArgs e) { base.OnMouseCaptureChanged(e); if (dragging && !Capture) { dragging = false; EditCompleted?.Invoke(); } }
    protected override void OnMouseLeave(EventArgs e) { base.OnMouseLeave(e); if (PreviewMode && !Capture) ActionPointer?.Invoke(new(-1, -1), false); }
    protected override void OnKeyUp(KeyEventArgs e) { heldKeys.Remove(e.KeyCode); base.OnKeyUp(e); }
    protected override void OnLostFocus(EventArgs e) { heldKeys.Clear(); base.OnLostFocus(e); }
    protected override bool IsInputKey(Keys keyData) => (PreviewMode && (keyData & Keys.KeyCode) is Keys.Tab or Keys.Enter or Keys.Space or Keys.Escape) || keyData is Keys.Left or Keys.Right or Keys.Up or Keys.Down || base.IsInputKey(keyData);
    protected override void OnKeyDown(KeyEventArgs e) { base.OnKeyDown(e); if (PreviewMode) { bool first=heldKeys.Add(e.KeyCode); if ((first || e.KeyCode is Keys.Up or Keys.Down or Keys.Left or Keys.Right) && ActionKey?.Invoke(e.KeyCode) == true) { e.Handled=true; e.SuppressKeyPress=true; } return; } if (Selected is null) return; var d = e.Shift ? 8 : 1; var b = LayoutRules.Bounds(Selected); switch (e.KeyCode) { case Keys.Left: b = b with { X = b.X - d }; break; case Keys.Right: b = b with { X = b.X + d }; break; case Keys.Up: b = b with { Y = b.Y - d }; break; case Keys.Down: b = b with { Y = b.Y + d }; break; default: return; } LayoutRules.SetBounds(Selected, b); EditCompleted?.Invoke(); e.Handled = true; }
}


