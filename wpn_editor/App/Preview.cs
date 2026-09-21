using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Text.Json.Nodes;
namespace Mgo2Mt.WpnEditor;
internal sealed class Preview:Control
{
    public Document? Document;
    public int WeaponId=25;
    public string Channel="muzzle";
    public int TimeMs;
    public double ViewScale=.6;
    public string Error{get;private set;}="";
    public int SampleCount{get;private set;}
    private readonly Dictionary<uint,Bitmap> images=[];
    private readonly Dictionary<uint,string> originals=[];
    public Preview(){DoubleBuffered=true;BackColor=Color.FromArgb(17,23,32);ForeColor=Color.FromArgb(203,221,231);Dock=DockStyle.Fill;ResizeRedraw=true;}
    public void ReloadImages(){ClearImages();originals.Clear();Error="";if(Document is null)return;var bundles=new HashSet<string>(StringComparer.OrdinalIgnoreCase);foreach(string key in new[]{"effectsManifest","damageEffectsManifest"})if(Document.Gameplay["resources"]?[key]?.GetValue<string>() is {Length:>0} relative)bundles.Add(Paths.Resolve(Document.Root,relative));var fx=Path.Combine(Document.Root,"fx");if(Directory.Exists(fx))foreach(var p in Directory.GetFiles(fx,"*.gwfx").Take(32))bundles.Add(p);foreach(var p in bundles.Where(File.Exists)){try{foreach(var key in Native.Keys(p))originals.TryAdd(key,p);}catch(Exception ex){Error=ex.Message;}}Invalidate();}
    public void ClearImages(){foreach(var b in images.Values)b.Dispose();images.Clear();}
    public IReadOnlyDictionary<uint,string> OriginalImages=>originals;
    private Bitmap? GetImage(uint key)
    {
        if(images.TryGetValue(key,out var b))return b;if(Document is null)return null;
        if(key>0xffffff){var relative=Native.TexturePath(key);if(relative.Length==0)return null;b=Native.Image(Paths.Resolve(Document.Root,relative));}
        else if(originals.TryGetValue(key,out var bundle))b=Native.Image(bundle,key);else return null;
        if(images.Count>=64){images.First().Value.Dispose();images.Remove(images.First().Key);}images[key]=b;return b;
    }
    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);var g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;g.InterpolationMode=InterpolationMode.HighQualityBilinear;float ox=Width*.5f,oy=Height*.57f;
        using var pen=new Pen(Color.FromArgb(29,43,57));for(int x=0;x<Width;x+=40)g.DrawLine(pen,x,0,x,Height);for(int y=0;y<Height;y+=40)g.DrawLine(pen,0,y,Width,y);
        using var axes=new Pen(Color.FromArgb(69,111,134));g.DrawLine(axes,ox-60,oy,ox+60,oy);g.DrawLine(axes,ox,oy-60,ox,oy+60);
        using var title=new Font("Yu Gothic UI",15,FontStyle.Bold);g.DrawString("EFFECT PREVIEW",title,Brushes.White,22,20);g.DrawString("2D 表示  •  ゲームと共通の粒子計算",Font,Brushes.LightSlateGray,24,54);
        if(Document is null){g.DrawString("gameplay.json を開いてください",title,Brushes.LightSlateGray,35,Height/2);return;}
        try
        {
            DrawLight(g,ox,oy);
            var samples=Native.Sample(Document.Effects.ToJsonString(),WeaponId,Channel,TimeMs);SampleCount=samples.Length;Error="";
            foreach(var p in samples)
            {
                var image=GetImage(p.Texture);if(image is null){Error="参照画像を読み込めません: "+p.Texture.ToString("X6");continue;}
                float x=ox+(p.X+p.Z*.45f)*(float)ViewScale,y=oy-p.Y*(float)ViewScale-p.Z*.16f*(float)ViewScale;
                float w=Math.Min(5000,p.Radius*2*p.StretchX*(float)ViewScale),h=Math.Min(5000,p.Radius*2*p.StretchY*(float)ViewScale);if(w<=0||h<=0)continue;
                var state=g.Save();g.TranslateTransform(x,y);g.RotateTransform(p.Rotation*180/MathF.PI);
                using var attr=new ImageAttributes();attr.SetColorMatrix(new ColorMatrix{Matrix00=p.R,Matrix11=p.G,Matrix22=p.B,Matrix33=p.A,Matrix44=1});
                g.DrawImage(image,new Rectangle(-(int)w/2,-(int)h/2,(int)Math.Ceiling(w),(int)Math.Ceiling(h)),p.U0*image.Width,p.V0*image.Height,(p.U1-p.U0)*image.Width,(p.V1-p.V0)*image.Height,GraphicsUnit.Pixel,attr);g.Restore(state);
            }
            DrawReticle(g,ox,oy);
            using var text=new SolidBrush(Color.FromArgb(164,201,218));g.DrawString($"{TimeMs:N0} ms   /   {samples.Length} particles   /   ×{ViewScale:F2}",Font,text,24,Height-55);
            if(samples.Length==0)g.DrawString("この時点の粒子はありません。層・時間・参照画像を確認できます。",Font,text,24,Height-32);
            if(Error.Length>0)g.DrawString(Error,Font,Brushes.Salmon,new RectangleF(24,85,Width-45,90));
        }
        catch(Exception ex){Error=ex.Message;g.DrawString("プレビューを更新できません\n"+ex.Message,Font,Brushes.Salmon,new RectangleF(24,90,Width-48,Height-140));}
    }
    private void DrawLight(Graphics g,float x,float y)
    {
        string eventName=Channel=="explosion"?"explosion":"shot";var row=Document!.Effects["weapons"]?.AsArray().OfType<JsonObject>().FirstOrDefault(w=>w["id"]?.GetValue<int>()==WeaponId);var light=row?["lights"]?[eventName]??Document.Effects["defaults"]?["lights"]?[eventName];if(light is not JsonObject l||l["enabled"]?.GetValue<bool>()==false)return;
        double N(string key,double fallback)=>l[key] is {} n?Fields.Number(n):fallback;
        double delay=N("delayMs",0),duration=N("durationMs",80),t=(TimeMs-delay)/duration;if(t<0||t>=1)return;
        double alpha=Curve(l["intensityCurve"] as JsonArray,t)*N("intensity",2);var c=l["color"] as JsonArray??new JsonArray(1,.64,.25);var radius=(float)Math.Min(350,N("radius",2600)*ViewScale*.13);
        using var path=new GraphicsPath();path.AddEllipse(x-radius,y-radius,radius*2,radius*2);using var brush=new PathGradientBrush(path){CenterColor=Color.FromArgb((int)(Math.Clamp(alpha/8,0,.85)*255),(int)(Fields.Number(c[0]!)*255),(int)(Fields.Number(c[1]!)*255),(int)(Fields.Number(c[2]!)*255)),SurroundColors=[Color.Transparent]};g.FillPath(brush,path);
    }
    private void DrawReticle(Graphics g,float x,float y)
    {
        var row=Document!.Effects["weapons"]?.AsArray().OfType<JsonObject>().FirstOrDefault(w=>w["id"]?.GetValue<int>()==WeaponId);var value=row?["reticle"]??Document.Effects["defaults"]?["reticle"];if(value is not JsonObject r||r["enabled"]?.GetValue<bool>()==false)return;
        double N(string key,double fallback)=>r[key] is {} n?Fields.Number(n):fallback;var c=r["color"] as JsonArray??new JsonArray(1,.65,.17,1);var color=Color.FromArgb((int)(Fields.Number(c[3]!)*255),(int)(Fields.Number(c[0]!)*255),(int)(Fields.Number(c[1]!)*255),(int)(Fields.Number(c[2]!)*255));float gap=(float)N("minGap",6),length=(float)N("length",10),thickness=(float)N("thickness",2);using var pen=new Pen(color,thickness);g.DrawLine(pen,x-gap-length,y,x-gap,y);g.DrawLine(pen,x+gap,y,x+gap+length,y);g.DrawLine(pen,x,y-gap-length,x,y-gap);g.DrawLine(pen,x,y+gap,x,y+gap+length);if(r["centerDot"]?.GetValue<bool>()!=false){using var brush=new SolidBrush(color);g.FillEllipse(brush,x-thickness/2,y-thickness/2,thickness,thickness);}
    }
    public static double Curve(JsonArray? a,double t){if(a is null)return 1-t;double prevT=0,prevV=Fields.Number(a[0]![1]!);foreach(var p in a.OfType<JsonArray>()){double time=Fields.Number(p[0]!),v=Fields.Number(p[1]!);if(t<=time)return time==prevT?v:prevV+(v-prevV)*(t-prevT)/(time-prevT);prevT=time;prevV=v;}return prevV;}
    protected override void Dispose(bool disposing){if(disposing)foreach(var b in images.Values)b.Dispose();base.Dispose(disposing);}
}
