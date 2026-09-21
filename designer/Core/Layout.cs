using System.Text.Json;
using System.Text.Json.Serialization;
namespace MgoUi;

public sealed class LayoutDocument
{
    public string Format { get; set; } = "MGO2MT.UI_LAYOUT.1";
    public int Width { get; set; } = 1280;
    public int Height { get; set; } = 720;
    public List<Element> Elements { get; set; } = [];
}
public sealed class Element
{
    public string Id { get; set; } = "element";
    public string Screen { get; set; } = "title";
    public string State { get; set; } = "*";
    public List<string> FlagsAll { get; set; } = [];
    public List<string> FlagsNone { get; set; } = [];
    public string Kind { get; set; } = "panel";
    public float X { get; set; }
    public float Y { get; set; }
    public float Width { get; set; } = 240;
    public float Height { get; set; } = 80;
    public string Anchor { get; set; } = "top-left";
    public int Z { get; set; }
    public bool Visible { get; set; } = true;
    public int[] Color { get; set; } = [255, 255, 255, 255];
    public string Texture { get; set; } = "";
    public string Text { get; set; } = "";
    public float FontSize { get; set; } = 28;
    public string Font { get; set; } = "Yu Gothic";
    public string Bind { get; set; } = "";
    public List<UiAction> OnClick { get; set; } = [];
    public List<UiAction> OnChange { get; set; } = [];
    public int DelayMs { get; set; }
    public int[]? HoverColor { get; set; }
    public int[]? PressedColor { get; set; }
    public int[]? TextColor { get; set; }
    public int[]? ShadowColor { get; set; }
    public float ShadowX { get; set; }
    public float ShadowY { get; set; }
    public List<SelectOption> Options { get; set; } = [];
    public string Selected { get; set; } = "";
}
public sealed class SelectOption { public string Value { get; set; } = ""; public string Label { get; set; } = ""; }
public sealed class UiAction
{
    public string Type { get; set; } = "emitEvent";
    public string? Sound { get; set; }
    public float? Volume { get; set; }
    public bool? WaitForEnd { get; set; }
    public string? Name { get; set; }
    public string? Value { get; set; }
    public string? Target { get; set; }
    public bool? Visible { get; set; }
    public int? Ms { get; set; }
}
public sealed class ProjectDocument
{
    public string Format { get; set; } = "MGO2MT.UI_PROJECT.1";
    public string AssetRoot { get; set; } = "";
    public LayoutDocument Layout { get; set; } = new();
}
public readonly record struct PointF(float X, float Y);
public readonly record struct RectF(float X, float Y, float Width, float Height)
{
    public bool Contains(PointF p) => p.X >= X && p.Y >= Y && p.X <= X + Width && p.Y <= Y + Height;
}
public static class LayoutRules
{
    public static readonly string[] Screens = ["title", "menu", "lobby", "briefing", "hud", "result"];
    public static readonly string[] Anchors = ["top-left", "top-center", "top-right", "middle-left", "center", "middle-right", "bottom-left", "bottom-center", "bottom-right"];
    public static PointF Factor(string anchor)
    {
        var i = Array.IndexOf(Anchors, anchor); if (i < 0) throw new InvalidDataException("不明なアンカーです: " + anchor);
        return new((i % 3) * .5f, (i / 3) * .5f);
    }
    public static RectF Bounds(Element e) { var f = Factor(e.Anchor); return new(f.X * (1280 - e.Width) + e.X, f.Y * (720 - e.Height) + e.Y, e.Width, e.Height); }
    public static void SetBounds(Element e, RectF b) { var f = Factor(e.Anchor); e.Width = b.Width; e.Height = b.Height; e.X = b.X - f.X * (1280 - b.Width); e.Y = b.Y - f.Y * (720 - b.Height); }
    public static void SetAnchorPreservePosition(Element e, string anchor) { var b = Bounds(e); e.Anchor = anchor; SetBounds(e, b); }
    public static bool Shown(Element e, string screen, string state, IReadOnlySet<string> flags) => e.Visible && e.Screen == screen && (e.State == "*" || e.State == state) && e.FlagsAll.All(flags.Contains) && !e.FlagsNone.Any(flags.Contains);
    public static Element? Hit(LayoutDocument d, PointF p, string screen, string state, IReadOnlySet<string> flags) => d.Elements.Select((e, i) => (e, i)).Where(x => Shown(x.e, screen, state, flags) && Bounds(x.e).Contains(p)).OrderByDescending(x => x.e.Z).ThenByDescending(x => x.i).Select(x => x.e).FirstOrDefault();
    public static void Validate(LayoutDocument d)
    {
        if (d.Format != "MGO2MT.UI_LAYOUT.1" || d.Width != 1280 || d.Height != 720) throw new InvalidDataException("1280 × 720 の UI_LAYOUT.1 が必要です。");
        if (d.Elements.Count > 4096) throw new InvalidDataException("要素数は4096以内です。");
        var ids = new HashSet<string>(StringComparer.Ordinal); long textBytes=0; double paintWork=0;
        foreach (var e in d.Elements)
        {
            if (string.IsNullOrWhiteSpace(e.Id) || e.Id.Length > 128 || !ids.Add(e.Id)) throw new InvalidDataException("要素IDが空、長すぎる、または重複しています。");
            if (!Screens.Contains(e.Screen) || !new[] { "panel", "image", "text", "button", "dropdown" }.Contains(e.Kind) || !Anchors.Contains(e.Anchor)) throw new InvalidDataException("画面、種類、アンカーが不正です。");
            if (!float.IsFinite(e.X) || !float.IsFinite(e.Y) || Math.Abs(e.X) > 8192 || Math.Abs(e.Y) > 8192 || !float.IsFinite(e.Width) || !float.IsFinite(e.Height) || e.Width <= 0 || e.Height <= 0 || e.Width > 4096 || e.Height > 4096) throw new InvalidDataException("座標またはサイズが範囲外です。");
            if (!float.IsFinite(e.FontSize) || e.FontSize < 1 || e.FontSize > 256) throw new InvalidDataException("文字サイズは1〜256です。");
            if (e.Color.Length != 4 || e.Color.Any(v => v < 0 || v > 255)) throw new InvalidDataException("色はRGBAの各0〜255です。");
            if (e.State.Length > 128 || System.Text.Encoding.UTF8.GetByteCount(e.Text) > 4096 || System.Text.Encoding.UTF8.GetByteCount(e.Font) > 128 || e.Bind.Length > 128) throw new InvalidDataException("文字列が長すぎます。");
            if (e.FlagsAll.Count > 64 || e.FlagsNone.Count > 64 || e.FlagsAll.Concat(e.FlagsNone).Any(v => string.IsNullOrWhiteSpace(v) || v.Length > 128) || e.FlagsAll.Intersect(e.FlagsNone).Any()) throw new InvalidDataException("表示条件が不正、または矛盾しています。");
            if (e.Texture.Length > 0) ValidateRelative(e.Texture);
            if (e.Z < -100000 || e.Z > 100000 || string.IsNullOrEmpty(e.State)) throw new InvalidDataException("重なり順または状態が不正です。");
            var control = e.Kind is "button" or "dropdown";
            if (e.Kind != "text" && !control && (e.Text.Length > 0 || e.Bind.Length > 0)) throw new InvalidDataException("テキストと変数はテキスト・ボタン・プルダウンで指定してください。");
            if (e.Kind != "image" && !control && e.Texture.Length > 0) throw new InvalidDataException("画像は画像部品・ボタン・プルダウンで指定してください。");
            if ((e.Kind == "text" || control) && (e.Width > 1280 || e.Height > 720 || e.Font.Length == 0)) throw new InvalidDataException("文字の領域は1280 × 720以内、フォント名を指定してください。");
            if(e.Kind=="text"||control) textBytes+=(long)Math.Ceiling(e.Width)*(long)Math.Ceiling(e.Height)*4;
            paintWork+=Math.Min(e.Width,1280)*Math.Min(e.Height,720)*(1+(control?1:0)+(e.ShadowColor is {Length:4}&&e.ShadowColor[3]>0?1:0));
            if (e.DelayMs < 0 || e.DelayMs > 60000) throw new InvalidDataException("待ち時間は0〜60000 msです。");
            foreach (var color in new[] { e.HoverColor, e.PressedColor, e.TextColor, e.ShadowColor }) if (color is not null && (color.Length != 4 || color.Any(c => c < 0 || c > 255))) throw new InvalidDataException("色はRGBAの各0〜255です。");
            if (!float.IsFinite(e.ShadowX) || !float.IsFinite(e.ShadowY) || Math.Abs(e.ShadowX) > 256 || Math.Abs(e.ShadowY) > 256) throw new InvalidDataException("影の位置は−256〜256です。");
            if (e.Kind == "dropdown")
            {
                if (e.Options.Count is < 1 or > 64 || e.Options.Select(o => o.Value).Distinct(StringComparer.Ordinal).Count() != e.Options.Count || e.Options.Any(o => string.IsNullOrWhiteSpace(o.Value) || o.Value.Any(char.IsControl) || System.Text.Encoding.UTF8.GetByteCount(o.Value) > 128 || string.IsNullOrWhiteSpace(o.Label) || System.Text.Encoding.UTF8.GetByteCount(o.Label) > 512)) throw new InvalidDataException("プルダウンの項目は1〜64個、値は重複せず128バイト、表示名は512バイト以内です。");
                if (e.Selected.Length > 0 && !e.Options.Any(o => o.Value == e.Selected)) throw new InvalidDataException("初期選択の値が項目にありません。");
            }
            else if (e.Options.Count != 0 || e.Selected.Length > 0 || e.OnChange.Count != 0) throw new InvalidDataException("選択項目・初期選択・変更時の動作はプルダウンで指定してください。");
            if (e.OnClick.Count > 32 || e.OnChange.Count > 32) throw new InvalidDataException("1つの操作に設定する動作は32個までです。");
            foreach (var action in e.OnClick.Concat(e.OnChange)) AudioRules.Validate(action);
        }
        var actions = d.Elements.SelectMany(e => e.OnClick.Concat(e.OnChange)).ToArray();
        if(textBytes>32*1024*1024)throw new InvalidDataException("文字の描画領域の合計が32 MiBを超えています。部品を小さくしてください。");
        if(paintWork>32.0*1280*720)throw new InvalidDataException("描画量の上限を超えています。大きな部品や影の数を減らしてください。");
        if(d.Elements.Where(e=>e.Texture.Length>0).Select(e=>e.Texture).Distinct(StringComparer.OrdinalIgnoreCase).Count()>512)throw new InvalidDataException("参照する画像は512枚までです。");
        if (actions.Length > 8192) throw new InvalidDataException("レイアウト全体の動作は8192個までです。");
        if (actions.Where(a => a.Type == "setVariable").Select(a => a.Name).Concat(d.Elements.Where(e => e.Kind == "dropdown" && e.Bind.Length > 0).Select(e => e.Bind)).Distinct().Count() > 128) throw new InvalidDataException("変数は128個までです。");
        foreach (var action in actions) if (action.Type == "setVisible" && !ids.Contains(action.Target!)) throw new InvalidDataException("表示を切り替える部品がありません: " + action.Target);
    }
    public static void ValidateRelative(string path)
    {
        if (System.Text.Encoding.UTF8.GetByteCount(path)>512 || path.Any(char.IsControl) || Path.IsPathRooted(path) || path.Contains(':') || path.Contains('\\') || path.Split('/').Any(x => x is "" or "." or "..")) throw new InvalidDataException("素材は素材フォルダー内の相対パス（512バイト以内）で指定してください。");
    }
    public static string AssetPath(string root, string relative)
    {
        ValidateRelative(relative); var basePath=Path.GetFullPath(root); var full=Path.GetFullPath(Path.Combine(basePath,relative)); var prefix=basePath.TrimEnd(Path.DirectorySeparatorChar)+Path.DirectorySeparatorChar;
        if (!full.StartsWith(prefix,StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("素材フォルダー外のパスです。");
        // The chosen root may itself be a user-selected junction. References below
        // it must be ordinary files/directories, so import/export cannot escape it.
        var current=basePath;
        foreach(var component in relative.Split('/'))
        {
            current=Path.Combine(current,component);
            try { if((File.GetAttributes(current)&FileAttributes.ReparsePoint)!=0) throw new InvalidDataException("素材の相対パス内にリンクやジャンクションは使用できません。"); }
            catch(FileNotFoundException) { } catch(DirectoryNotFoundException) { }
        }
        return full;
    }
}
public readonly record struct Viewport(float Scale, float Left, float Top, float Width, float Height)
{
    public static Viewport Contain(int width, int height) { var s = Math.Max(.0001f, Math.Min(width / 1280f, height / 720f)); return new(s, (width - 1280 * s) / 2, (height - 720 * s) / 2, 1280 * s, 720 * s); }
    public PointF ToDesign(float x, float y) => new((x - Left) / Scale, (y - Top) / Scale);
    public RectF ToPixel(RectF b) => new(Left + b.X * Scale, Top + b.Y * Scale, b.Width * Scale, b.Height * Scale);
}
public static class Serialization
{
    public static readonly JsonSerializerOptions Options = new() { PropertyNamingPolicy = JsonNamingPolicy.CamelCase, WriteIndented = true, UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow, DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull };
    private static string Bounded(string json) { if(System.Text.Encoding.UTF8.GetByteCount(json)>1024*1024) throw new InvalidDataException("JSONは1 MiB以内です。部品や文字を減らしてください。"); return json; }
    public static string Encode(LayoutDocument d) { LayoutRules.Validate(d); return Bounded(JsonSerializer.Serialize(d, Options)); }
    private static void MigrateLayout(LayoutDocument d) { if(d.Format=="MGO2WIN.UI_LAYOUT.1")d.Format="MGO2MT.UI_LAYOUT.1"; }
    public static LayoutDocument Decode(string json) { Bounded(json); var d = JsonSerializer.Deserialize<LayoutDocument>(json, Options) ?? throw new InvalidDataException("空のJSONです。"); MigrateLayout(d); LayoutRules.Validate(d); return d; }
    public static LayoutDocument Clone(LayoutDocument d) => Decode(Encode(d));
    public static ProjectDocument ReadProject(string file) { if(new FileInfo(file).Length>1024*1024) throw new InvalidDataException("JSONは1 MiB以内です。"); var json = Bounded(File.ReadAllText(file)); using var probe = JsonDocument.Parse(json); if (probe.RootElement.GetProperty("format").GetString() is "MGO2MT.UI_LAYOUT.1" or "MGO2WIN.UI_LAYOUT.1") return new() { AssetRoot = Path.GetDirectoryName(Path.GetFullPath(file))!, Layout = Decode(json) }; var p = JsonSerializer.Deserialize<ProjectDocument>(json, Options) ?? throw new InvalidDataException("空のプロジェクトです。"); if(p.Format=="MGO2WIN.UI_PROJECT.1")p.Format="MGO2MT.UI_PROJECT.1"; if (p.Format != "MGO2MT.UI_PROJECT.1") throw new InvalidDataException("プロジェクトの形式が違います。"); MigrateLayout(p.Layout); LayoutRules.Validate(p.Layout); p.AssetRoot = Path.GetFullPath(p.AssetRoot, Path.GetDirectoryName(Path.GetFullPath(file))!); return p; }
    public static void WriteProject(string file, ProjectDocument p) { LayoutRules.Validate(p.Layout); AtomicWrite(file, Bounded(JsonSerializer.Serialize(p, Options))); }
    public static void AtomicWrite(string file, string text) { var path = Path.GetFullPath(file); Directory.CreateDirectory(Path.GetDirectoryName(path)!); var tmp = path + "." + Guid.NewGuid().ToString("N") + ".tmp"; try { File.WriteAllText(tmp, text, new System.Text.UTF8Encoding(false)); File.Move(tmp, path, true); } finally { if (File.Exists(tmp)) File.Delete(tmp); } }
}
public sealed class History
{
    private readonly List<string> snapshots = []; private int cursor;
    public LayoutDocument Current => Serialization.Decode(snapshots[cursor]);
    public bool CanUndo => cursor > 0; public bool CanRedo => cursor + 1 < snapshots.Count;
    public void Reset(LayoutDocument d) { snapshots.Clear(); snapshots.Add(Serialization.Encode(d)); cursor = 0; }
    public void Commit(LayoutDocument d) { var s = Serialization.Encode(d); if (s == snapshots[cursor]) return; snapshots.RemoveRange(cursor + 1, snapshots.Count - cursor - 1); snapshots.Add(s); cursor++; if (snapshots.Count > 151) { snapshots.RemoveAt(0); cursor--; } }
    public LayoutDocument Undo() { if (CanUndo) cursor--; return Serialization.Decode(snapshots[cursor]); }
    public LayoutDocument Redo() { if (CanRedo) cursor++; return Serialization.Decode(snapshots[cursor]); }
}
public static class Samples
{
    public static LayoutDocument Create() { var d = new LayoutDocument(); foreach (var screen in LayoutRules.Screens) { d.Elements.Add(new() { Id = screen + "_bar", Screen = screen, X = 48, Y = 42, Width = 1184, Height = 94, Color = [20, 34, 45, 228], Z = 0 }); d.Elements.Add(new() { Id = screen + "_heading", Screen = screen, X = 76, Y = 65, Width = 900, Height = 48, Kind = "text", Text = screen switch { "title" => "METAL GEAR ONLINE", "menu" => "メニュー", "lobby" => "ロビー", "briefing" => "装備を選択", "hud" => "TEAM DEATHMATCH", _ => "ラウンド結果" }, FontSize = 32, Z = 1, Color = [221, 235, 238, 255] }); } d.Elements.Add(new() { Id = "title_start", Screen = "title", Anchor = "center", X = 0, Y = 40, Width = 420, Height = 62, Kind = "text", Text = "PRESS START", FontSize = 40, Z = 2, Color = [210, 230, 235, 255] }); d.Elements.Add(new() { Id = "title_hint", Screen = "title", Anchor = "bottom-center", X = 0, Y = -38, Width = 610, Height = 32, Kind = "text", Text = "画像を取り込み、ドラッグして配置してください", FontSize = 20, Z = 3, Color = [152, 178, 190, 255] }); return d; }
}



