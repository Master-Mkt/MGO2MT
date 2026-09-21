using System.Buffers.Binary;
using System.Text.Json.Nodes;
namespace Mgo2Mt.WpnEditor;

public static class WeaponIcons
{
    public static (int Width,int Height) ValidatePng(ReadOnlySpan<byte> bytes)
    {
        if(bytes.Length is <33 or >4194304||!bytes[..8].SequenceEqual(new byte[]{137,80,78,71,13,10,26,10})||!bytes.Slice(12,4).SequenceEqual("IHDR"u8))throw new InvalidDataException("アイコンは4 MiB以内のPNGで指定してください。");
        uint w=BinaryPrimitives.ReadUInt32BigEndian(bytes.Slice(16,4)),h=BinaryPrimitives.ReadUInt32BigEndian(bytes.Slice(20,4));
        if(w is 0 or >1024||h is 0 or >512)throw new InvalidDataException("アイコンの大きさは1024×512以内で指定してください。");
        return ((int)w,(int)h);
    }
    public static string? Override(JsonObject weapon)=>weapon["visual"]?["iconPath"]?.GetValue<string>() is {Length:>0} p?p:null;
    public static void Set(Document document,int id,string? relative)
    {
        var visual=document.Weapon(id)["visual"]!.AsObject();
        if(string.IsNullOrWhiteSpace(relative)){visual.Remove("iconPath");return;}
        Paths.Resolve(document.Root,relative);if(!relative.EndsWith(".png",StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("アイコンのパスはPNGを指定してください。");visual["iconPath"]=relative;
    }
    public static string? Resolve(Document document,int id)
    {
        if(Override(document.Weapon(id)) is {} p){Paths.Resolve(document.Root,p);return p;}
        var index=Paths.Resolve(document.Root,"weapon-icons/index.tsv");if(!File.Exists(index))return null;
        if(new FileInfo(index).Length>16384)throw new InvalidDataException("原アイコン一覧が上限を超えています。");
        var rows=File.ReadAllLines(index);if(rows.Length==0||rows[0] is not ("MGO2MT_WEAPON_ICONS\t1" or "MGO2WIN_WEAPON_ICONS\t1"))throw new InvalidDataException("原アイコン一覧の形式に対応していません。");
        var ids=new HashSet<int>();string? found=null;
        foreach(var line in rows.Skip(1)){var r=line.Split('\t');if(r.Length!=3||r[0]!="ICON"||!int.TryParse(r[1],out int key)||key is <0 or >511||!ids.Add(key)||ids.Count>128||r[2].Length is <5 or >80||!r[2].EndsWith(".png",StringComparison.OrdinalIgnoreCase)||r[2].Contains("..",StringComparison.Ordinal)||r[2].Any(c=>!(char.IsAsciiLetterOrDigit(c)||c is '_' or '-' or '.')))throw new InvalidDataException("原アイコン一覧の行が不正です。");if(key==id)found="weapon-icons/"+r[2];}
        if(found is not null)Paths.Resolve(document.Root,found);return found;
    }
}
