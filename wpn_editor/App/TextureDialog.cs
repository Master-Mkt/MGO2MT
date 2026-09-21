namespace Mgo2Mt.WpnEditor;
internal sealed class TextureDialog:Form
{
    public uint Selected{get;private set;}
    public TextureDialog(IReadOnlyDictionary<uint,string> textures)
    {
        Text="原画像を選択";Size=new(720,580);Font=new("Yu Gothic UI",10);StartPosition=FormStartPosition.CenterParent;
        var list=new ListBox{Dock=DockStyle.Left,Width=220};var picture=new PictureBox{Dock=DockStyle.Fill,SizeMode=PictureBoxSizeMode.Zoom,BackColor=Color.FromArgb(35,43,55)};var bottom=new Button{Text="この画像を使う",Dock=DockStyle.Bottom,Height=42};Controls.Add(picture);Controls.Add(list);Controls.Add(bottom);
        foreach(var key in textures.Keys.Order())list.Items.Add(key.ToString("X6"));list.SelectedIndexChanged+=(_,_)=>{if(list.SelectedItem is not string s)return;Selected=Convert.ToUInt32(s,16);picture.Image?.Dispose();picture.Image=Native.Image(textures[Selected],Selected);};bottom.Click+=(_,_)=>{if(list.SelectedIndex>=0)DialogResult=DialogResult.OK;};if(list.Items.Count>0)list.SelectedIndex=0;FormClosed+=(_,_)=>picture.Image?.Dispose();
    }
}
