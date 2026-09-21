using System.Drawing.Imaging;
using System.Text.Json.Nodes;
namespace Mgo2Mt.WpnEditor;

internal sealed partial class MainForm
{
    private readonly TextBox iconPath=new(){Dock=DockStyle.Top};
    private readonly Label iconInfo=new(){Dock=DockStyle.Top,Height=66,Padding=new(4),AutoEllipsis=true};
    private readonly IconPreview iconPreview=new(){Dock=DockStyle.Fill};
    private void BuildIconPage(TabControl tabs)
    {
        var page=Page(tabs,"アイコン",iconPreview);
        var bar=new FlowLayoutPanel{Dock=DockStyle.Top,Height=82,Padding=new(3)};
        Add(bar,"画像を取り込む…",ImportIcon,155);
        Add(bar,"パスを適用",()=>Guard(()=>ApplyIconPath(iconPath.Text.Trim())),125);
        Add(bar,"原アイコンに戻す",()=>Guard(()=>ApplyIconPath(null)),155);
        bar.Controls.Add(new Label{Text="dataからの相対パス。PNG・1024×512・4 MiB以内。",AutoSize=true});
        page.Controls.Add(iconInfo);page.Controls.Add(bar);page.Controls.Add(iconPath);
        page.Controls.Add(new Label{Text="ゲームの装備選択で使うアイコン",Dock=DockStyle.Top,Height=30,Padding=new(4)});
        iconPath.KeyDown+=(_,e)=>{if(e.KeyCode==Keys.Enter){e.SuppressKeyPress=true;Guard(()=>ApplyIconPath(iconPath.Text.Trim()));}};
    }
    private static Bitmap ReadIcon(string full)
    {
        var size=new FileInfo(full).Length;if(size>4194304)throw new InvalidDataException("アイコンは4 MiB以内にしてください。");
        WeaponIcons.ValidatePng(File.ReadAllBytes(full));return Native.Image(full);
    }
    private void ApplyIconPath(string? relative)
    {
        if(doc is null)return;
        if(!string.IsNullOrWhiteSpace(relative)){using var checkedImage=ReadIcon(Paths.Resolve(doc.Root,relative));}
        Change(()=>WeaponIcons.Set(doc,weaponId,relative));
    }
    private void ImportIcon()
    {
        if(doc is null)return;using var dialog=new OpenFileDialog{Title="装備アイコンの画像を選択",Filter="画像|*.png;*.dds;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff;*.ico"};
        if(dialog.ShowDialog(this)==DialogResult.OK)Guard(()=>ImportIconFile(dialog.FileName));
    }
    private void ImportIconFile(string file)
    {
        if(doc is null)return;if(new FileInfo(file).Length>67108864)throw new InvalidDataException("取り込む画像は64 MiB以内にしてください。");
        using var image=Native.Image(file);if(image.Width>1024||image.Height>512)throw new InvalidDataException("アイコンは1024×512以内の画像を選んでください。サイズは自動変更しません。");
        using var data=new MemoryStream();image.Save(data,ImageFormat.Png);string path=doc.ImportIcon(data.ToArray());ApplyIconPath(path);
    }
    private void RefreshIcon()
    {
        iconPreview.Image=null;if(doc is null)return;iconPath.Text=WeaponIcons.Override(doc.Weapon(weaponId))??"";
        try
        {
            string? relative=WeaponIcons.Resolve(doc,weaponId);if(relative is null){iconInfo.Text="指定なし：この武器の原アイコンは登録されていません。";return;}
            var image=ReadIcon(Paths.Resolve(doc.Root,relative));iconPreview.Image=image;
            iconInfo.Text=(iconPath.Text.Length>0?"個別指定のアイコン":"従来の原アイコン")+Environment.NewLine+image.Width+" × "+image.Height+" px　透明度を含めて表示";
        }
        catch(Exception ex){iconInfo.Text="表示できません："+ex.Message;}
    }
    private void ValidateIcons()
    {
        if(doc is null)return;var checkedPaths=new HashSet<string>(StringComparer.OrdinalIgnoreCase);long pixels=0;
        foreach(var weapon in doc.Weapons.OfType<JsonObject>())if(WeaponIcons.Override(weapon) is {} path)
        {
            if(!path.EndsWith(".png",StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("アイコンのパスはPNGを指定してください。");
            if(checkedPaths.Add(path)){using var image=ReadIcon(Paths.Resolve(doc.Root,path));pixels+=(long)image.Width*image.Height;if(pixels>8*1024*1024)throw new InvalidDataException("個別アイコンの画像量が上限を超えています。");}
        }
    }
}

internal sealed class IconPreview:Control
{
    private Bitmap? image;
    [System.ComponentModel.DesignerSerializationVisibility(System.ComponentModel.DesignerSerializationVisibility.Hidden)]
    public Bitmap? Image{get=>image;set{image?.Dispose();image=value;Invalidate();}}
    public IconPreview(){DoubleBuffered=true;BackColor=Color.FromArgb(39,46,55);}
    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);using var a=new SolidBrush(Color.FromArgb(49,57,65));using var b=new SolidBrush(Color.FromArgb(64,72,80));
        for(int y=0;y<Height;y+=16)for(int x=0;x<Width;x+=16)e.Graphics.FillRectangle(((x/16+y/16)&1)==0?a:b,x,y,16,16);
        if(image is null){TextRenderer.DrawText(e.Graphics,"アイコンのプレビュー",Font,ClientRectangle,Color.White,TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter);return;}
        float scale=Math.Min((Width-32f)/image.Width,(Height-32f)/image.Height);if(scale<=0)return;
        int w=Math.Max(1,(int)Math.Round(image.Width*scale)),h=Math.Max(1,(int)Math.Round(image.Height*scale));
        e.Graphics.InterpolationMode=System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;e.Graphics.DrawImage(image,new Rectangle((Width-w)/2,(Height-h)/2,w,h));
    }
    protected override void Dispose(bool disposing){if(disposing){image?.Dispose();image=null;}base.Dispose(disposing);}
}
