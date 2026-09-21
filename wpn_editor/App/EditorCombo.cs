namespace Mgo2Mt.WpnEditor;
// Native ComboBox WM_PRINT omits the selection text on some Windows themes.
// Draw it in the print path as well, using the same actual selection value.
internal sealed class EditorCombo:ComboBox
{
    protected override void WndProc(ref Message m)
    {
        base.WndProc(ref m);
        if((m.Msg==0x317||m.Msg==0x318)&&m.WParam!=0&&SelectedIndex>=0)
        {
            using var g=Graphics.FromHdc(m.WParam);var r=new Rectangle(3,2,Math.Max(1,Width-SystemInformation.VerticalScrollBarWidth-6),Math.Max(1,Height-4));using var b=new SolidBrush(BackColor);g.FillRectangle(b,r);TextRenderer.DrawText(g,Items[SelectedIndex]?.ToString(),Font,r,ForeColor,TextFormatFlags.Left|TextFormatFlags.VerticalCenter|TextFormatFlags.EndEllipsis);
        }
    }
}
