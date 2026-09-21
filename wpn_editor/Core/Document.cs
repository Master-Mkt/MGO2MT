using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
namespace Mgo2Mt.WpnEditor;

public sealed class Document
{
    public JsonObject Gameplay { get; private set; }
    public JsonObject Effects { get; private set; }
    public string GameplayPath { get; }
    public string Root => Path.GetDirectoryName(GameplayPath)!;
    public string EffectsPath => Paths.Resolve(Root, EffectsRelative);
    public string EffectsRelative => Gameplay["resources"]?["weaponEffectsManifest"]?.GetValue<string>() is { Length:>0 } s ? s : "weapon_effects.json";
    public bool EffectsEnabled => Gameplay["resources"]?["weaponEffectsManifest"]?.GetValue<string>() is { Length:>0 };
    private string saved, originalGameplayHash, originalEffectsHash;
    private readonly Stack<string> undo = [], redo = [];
    private readonly HashSet<string> imports = new(StringComparer.OrdinalIgnoreCase);
    public bool Dirty => State()!=saved;
    public bool CanUndo=>undo.Count>0;
    public bool CanRedo=>redo.Count>0;
    public bool MigratedLegacyFormat { get; private set; }
    public JsonArray Weapons => Gameplay["weapons"]!.AsArray();
    public static readonly JsonSerializerOptions JsonOptions=new(){WriteIndented=true,Encoder=System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping};
    public static JsonObject Parse(string text,int maximumBytes=1048576)
    {
        if(Encoding.UTF8.GetByteCount(text)>maximumBytes)throw new InvalidDataException("設定の大きさが上限を超えています。");
        using var parsed=JsonDocument.Parse(text,new JsonDocumentOptions{MaxDepth=32});
        void Check(JsonElement e){if(e.ValueKind==JsonValueKind.Object){var keys=new HashSet<string>(StringComparer.Ordinal);foreach(var p in e.EnumerateObject()){if(!keys.Add(p.Name))throw new InvalidDataException("同じ設定名が重複しています: "+p.Name);Check(p.Value);}}else if(e.ValueKind==JsonValueKind.Array)foreach(var p in e.EnumerateArray())Check(p);}
        Check(parsed.RootElement);return JsonNode.Parse(text) as JsonObject??throw new InvalidDataException("設定の先頭はオブジェクトにしてください。");
    }
    public static Document Open(string path)
    {
        path=Path.GetFullPath(path);if(new FileInfo(path).Length>1024*1024)throw new InvalidDataException("設定は1 MiBを超えています。");
        var g=Parse(File.ReadAllText(path,new UTF8Encoding(false,true)));
        if(g["format"]?.GetValue<string>() is not ("MGO2MT.Gameplay" or "MGO2WIN.Gameplay")||g["version"]?.GetValue<int>()!=1||g["weapons"] is not JsonArray wa||wa.Count is <1 or >128)throw new InvalidDataException("対応する gameplay.json ではありません。");
        var ids=new HashSet<int>();foreach(var w in wa){var id=w?["id"]?.GetValue<int>()??-1;if(id is <0 or >511||!ids.Add(id))throw new InvalidDataException("武器IDが不正または重複しています。");}
        var relative=g["resources"]?["weaponEffectsManifest"]?.GetValue<string>() is {Length:>0} r?r:"weapon_effects.json";
        var fx=Paths.Resolve(Path.GetDirectoryName(path)!,relative);
        if(File.Exists(fx)&&new FileInfo(fx).Length>1048576)throw new InvalidDataException("エフェクト設定は1 MiBを超えています。");
        var e=File.Exists(fx)?Parse(File.ReadAllText(fx,new UTF8Encoding(false,true))):NewEffects();
        var document=new Document(path,g,e);
        if(g["format"]?.GetValue<string>()=="MGO2WIN.Gameplay"||e["format"]?.GetValue<string>()=="MGO2WIN.WeaponEffects")
        {
            document.MigratedLegacyFormat=true;
            g["format"]="MGO2MT.Gameplay";if(e["format"]?.GetValue<string>()=="MGO2WIN.WeaponEffects")e["format"]="MGO2MT.WeaponEffects";
        }
        return document;
    }
    public static JsonObject NewEffects()=>new(){["format"]="MGO2MT.WeaponEffects",["version"]=1,["weapons"]=new JsonArray()};
    public void ValidateCurrent(Action<string,string?> validate)
    {
        string g=Path.Combine(Root,".wpn-check-"+Guid.NewGuid().ToString("N")+".json"),e=Path.Combine(Root,".wpn-check-"+Guid.NewGuid().ToString("N")+".json");
        try{File.WriteAllText(g,Gameplay.ToJsonString(),new UTF8Encoding(false));if(File.Exists(EffectsPath)||EffectsEnabled)File.WriteAllText(e,Effects.ToJsonString(),new UTF8Encoding(false));validate(g,File.Exists(e)?e:null);}
        finally{if(File.Exists(g))File.Delete(g);if(File.Exists(e))File.Delete(e);}
    }
    public string ImportIcon(byte[] png)
    {
        WeaponIcons.ValidatePng(png);
        var hash=Convert.ToHexStringLower(SHA256.HashData(png));var relative="weapon_editor_assets/icons/"+hash+".png";var dest=Paths.Resolve(Root,relative);
        Directory.CreateDirectory(Path.GetDirectoryName(dest)!);dest=Paths.Resolve(Root,relative);
        if(!File.Exists(dest)){using var f=new FileStream(dest,FileMode.CreateNew,FileAccess.Write,FileShare.None);f.Write(png);f.Flush(true);}else if(Hash(dest)!=hash)throw new IOException("アイコン名と内容が一致しません。");
        imports.Add(relative);return relative;
    }
    private Document(string path,JsonObject g,JsonObject e){GameplayPath=path;Gameplay=g;Effects=e;saved=State();originalGameplayHash=Hash(path);originalEffectsHash=Hash(EffectsPath);}
    public JsonObject Weapon(int id)=>Weapons.OfType<JsonObject>().Single(w=>w["id"]!.GetValue<int>()==id);
    private string State()=>new JsonObject{["gameplay"]=Gameplay.DeepClone(),["effects"]=Effects.DeepClone()}.ToJsonString();
    private void Restore(string state){var root=Parse(state,4194304);Gameplay=root["gameplay"]!.AsObject();Effects=root["effects"]!.AsObject();}
    public void Change(Action action){var before=State();try{action();if(Encoding.UTF8.GetByteCount(Gameplay.ToJsonString())>1024*1024||Encoding.UTF8.GetByteCount(Effects.ToJsonString())>1048576)throw new InvalidDataException("設定の大きさが上限を超えます。");var after=State();if(before==after)return;undo.Push(before);if(undo.Count>80){var rows=undo.Take(80).Reverse().ToArray();undo.Clear();foreach(var row in rows)undo.Push(row);}redo.Clear();}catch{Restore(before);throw;}}
    public void Undo(){if(!CanUndo)return;redo.Push(State());Restore(undo.Pop());}
    public void Redo(){if(!CanRedo)return;undo.Push(State());Restore(redo.Pop());}
    public void EnableEffects(){Gameplay["resources"]!.AsObject()["weaponEffectsManifest"]=EffectsRelative;}
    public string Import(string input,bool audio)
    {
        var ext=Path.GetExtension(input).ToLowerInvariant();var allowed=audio?new[]{".wav"}:new[]{".png",".jpg",".jpeg",".bmp",".dds",".gif",".tif",".tiff",".ico"};
        if(!allowed.Contains(ext))throw new InvalidDataException("対応しない素材形式です。");
        var info=new FileInfo(input);if(!info.Exists||info.Length is <=0 or >67108864)throw new InvalidDataException("素材がない、空、または64 MiBを超えています。");
        var bytes=File.ReadAllBytes(input);var hash=Convert.ToHexStringLower(SHA256.HashData(bytes));var relative="weapon_editor_assets/"+(audio?"audio/":"textures/")+hash+ext;var dest=Paths.Resolve(Root,relative);
        Directory.CreateDirectory(Path.GetDirectoryName(dest)!);dest=Paths.Resolve(Root,relative);
        if(!File.Exists(dest)){using var f=new FileStream(dest,FileMode.CreateNew,FileAccess.Write,FileShare.None);f.Write(bytes);f.Flush(true);}else if(Hash(dest)!=hash)throw new IOException("素材名と内容が一致しません。");
        imports.Add(relative);return relative;
    }
    public void Save(Action<string,string?> validate,string? hostRoot=null)
    {
        if(Hash(GameplayPath)!=originalGameplayHash||Hash(EffectsPath)!=originalEffectsHash)throw new IOException("別の操作で設定が更新されています。開き直してから編集してください。");
        var g=Encoding.UTF8.GetBytes(Gameplay.ToJsonString(JsonOptions)+"\n");var e=Encoding.UTF8.GetBytes(Effects.ToJsonString(JsonOptions)+"\n");
        if(g.Length>1048576||e.Length>1048576)throw new InvalidDataException("整形後の設定が1 MiBを超えています。項目数を減らしてください。");
        var pending=new Dictionary<string,byte[]>(StringComparer.OrdinalIgnoreCase){[GameplayPath]=g};if(EffectsEnabled)pending[EffectsPath]=e;
        AddManifest(Root,pending,imports);
        if(hostRoot is not null)
        {
            if(string.IsNullOrWhiteSpace(hostRoot))throw new IOException("HOSTのdataフォルダーを指定してください。");
            hostRoot=Path.GetFullPath(hostRoot);if(!Directory.Exists(hostRoot)||!File.Exists(Path.Combine(hostRoot,"gameplay.json")))throw new IOException("HOSTのdataフォルダーと既存gameplay.jsonを選択してください。");
            var hp=Paths.Resolve(hostRoot,"gameplay.json");if(!string.Equals(hp,GameplayPath,StringComparison.OrdinalIgnoreCase)){pending[hp]=g;AddManifest(hostRoot,pending,[]);}
        }
        var staged=new Dictionary<string,string>();var old=new Dictionary<string,byte[]?>();var committed=new List<string>();
        try
        {
            foreach(var item in pending){var dir=Path.GetDirectoryName(item.Key)!;Directory.CreateDirectory(dir);var temp=Path.Combine(dir,".wpn-edit-"+Guid.NewGuid().ToString("N")+".json");using(var stream=new FileStream(temp,FileMode.CreateNew,FileAccess.Write,FileShare.None)){stream.Write(item.Value);stream.Flush(true);}staged[item.Key]=temp;old[item.Key]=File.Exists(item.Key)?File.ReadAllBytes(item.Key):null;}
            validate(staged[GameplayPath],EffectsEnabled?staged[EffectsPath]:null);
            foreach(var item in staged){if(File.Exists(item.Key))File.Replace(item.Value,item.Key,item.Key+".bak",true);else File.Move(item.Value,item.Key);committed.Add(item.Key);}
            saved=State();originalGameplayHash=Hash(GameplayPath);originalEffectsHash=Hash(EffectsPath);imports.Clear();
        }
        catch
        {
            foreach(var p in committed.AsEnumerable().Reverse()){if(old[p] is { } b){var rollback=p+".rollback-"+Guid.NewGuid().ToString("N");File.WriteAllBytes(rollback,b);File.Move(rollback,p,true);}else File.Delete(p);}throw;
        }
        finally{foreach(var temp in staged.Values)if(File.Exists(temp))File.Delete(temp);}
    }
    private static void AddManifest(string root,Dictionary<string,byte[]> changes,IEnumerable<string> assets)
    {
        var manifest=Paths.Resolve(root,"assets.sha256");if(!File.Exists(manifest))return;
        var replacements=new Dictionary<string,string>(StringComparer.Ordinal);foreach(var item in changes){if(!Paths.Contains(root,item.Key))continue;var relative=Path.GetRelativePath(root,item.Key).Replace('\\','/');if(!Path.IsPathRooted(relative)&&!relative.StartsWith("../",StringComparison.Ordinal)&&relative!="assets.sha256")replacements[relative]=Convert.ToHexStringLower(SHA256.HashData(item.Value));}
        foreach(var relative in assets)replacements[relative]=Hash(Paths.Resolve(root,relative));
        var lines=new List<string>();foreach(var line in File.ReadAllLines(manifest)){if(line.Length>66&&line[64]==' '&&line[65]==' '&&replacements.Remove(line[66..],out var hash))lines.Add(hash+"  "+line[66..]);else lines.Add(line);}
        lines.AddRange(replacements.OrderBy(k=>k.Key,StringComparer.Ordinal).Select(k=>k.Value+"  "+k.Key));changes[manifest]=Encoding.UTF8.GetBytes(string.Join('\n',lines)+"\n");
    }
    public static string Hash(string p)=>File.Exists(p)?Convert.ToHexStringLower(SHA256.HashData(File.ReadAllBytes(p))):"";
}

public static class Paths
{
    public static bool Contains(string root,string path)=>Path.GetFullPath(path).StartsWith(Path.TrimEndingDirectorySeparator(Path.GetFullPath(root))+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase);
    public static string Resolve(string root,string relative)
    {
        if(string.IsNullOrWhiteSpace(relative)||relative.Length>240||Path.IsPathRooted(relative)||relative.IndexOfAny(['\\',':','<','>','"','|','?','*'])>=0||relative.Any(char.IsControl))throw new InvalidDataException("素材はdataからの相対パスで指定してください。");
        var parts=relative.Split('/');if(parts.Any(s=>s.Length==0||s is "." or ".."||s.EndsWith('.')||s.EndsWith(' ')||System.Text.RegularExpressions.Regex.IsMatch(s,"^(CON|PRN|AUX|NUL|CONIN\\$|CONOUT\\$|COM[1-9¹²³]|LPT[1-9¹²³])(?:\\.|$)",System.Text.RegularExpressions.RegexOptions.IgnoreCase)))throw new InvalidDataException("素材のパスに使用できない名前があります。");
        var current=Path.GetFullPath(root);foreach(var part in parts){current=Path.Combine(current,part);if((File.Exists(current)||Directory.Exists(current))&&(File.GetAttributes(current)&FileAttributes.ReparsePoint)!=0)throw new InvalidDataException("素材パス内のリンクは使用できません。");}return current;
    }
}
