using System.ComponentModel;
namespace MgoUi;

public sealed class AnchorConverter : StringConverter { public override bool GetStandardValuesSupported(ITypeDescriptorContext? c) => true; public override bool GetStandardValuesExclusive(ITypeDescriptorContext? c) => true; public override StandardValuesCollection GetStandardValues(ITypeDescriptorContext? c) => new(LayoutRules.Anchors); }
public sealed class ScreenConverter : StringConverter { public override bool GetStandardValuesSupported(ITypeDescriptorContext? c) => true; public override bool GetStandardValuesExclusive(ITypeDescriptorContext? c) => true; public override StandardValuesCollection GetStandardValues(ITypeDescriptorContext? c) => new(LayoutRules.Screens); }
public sealed class ElementProperties(Element e, LayoutDocument? document = null) : ICustomTypeDescriptor
{
    [Category("1 識別"), DisplayName("ID")] public string Id { get => e.Id; set { var old=e.Id; e.Id=value; if(document is not null) foreach(var action in document.Elements.SelectMany(x=>x.OnClick.Concat(x.OnChange))) if(action.Type=="setVisible"&&action.Target==old)action.Target=value; } }
    [Category("1 識別"), DisplayName("種類"), ReadOnly(true)] public string Kind => e.Kind;
    [Category("1 識別"), DisplayName("画面"), TypeConverter(typeof(ScreenConverter))] public string Screen { get => e.Screen; set => e.Screen = value; }
    [Category("2 配置"), DisplayName("アンカー"), Description("基準点を変更しても見た目の位置を保持します。"), TypeConverter(typeof(AnchorConverter))] public string Anchor { get => e.Anchor; set => LayoutRules.SetAnchorPreservePosition(e, value); }
    [Category("2 配置"), DisplayName("X"), Description("アンカーからの横方向オフセット（設計座標）")] public float X { get => e.X; set => e.X = value; }
    [Category("2 配置"), DisplayName("Y"), Description("アンカーからの縦方向オフセット（設計座標）")] public float Y { get => e.Y; set => e.Y = value; }
    [Category("2 配置"), DisplayName("幅")] public float Width { get => e.Width; set => e.Width = value; }
    [Category("2 配置"), DisplayName("高さ")] public float Height { get => e.Height; set => e.Height = value; }
    [Category("2 配置"), DisplayName("重なり順"), Description("大きな値ほど手前になります。")] public int Z { get => e.Z; set => e.Z = value; }
    [Category("3 表示条件"), DisplayName("表示")] public bool Visible { get => e.Visible; set => e.Visible = value; }
    [Category("3 表示条件"), DisplayName("状態"), Description("* なら全状態。その他は状態名と完全一致。")] public string State { get => e.State; set => e.State = value; }
    [Category("3 表示条件"), DisplayName("必要フラグ"), Description("すべてONなら表示。カンマで区切ります。")] public string All { get => string.Join(", ", e.FlagsAll); set => e.FlagsAll = Split(value); }
    [Category("3 表示条件"), DisplayName("除外フラグ"), Description("一つでもONなら非表示。カンマで区切ります。")] public string None { get => string.Join(", ", e.FlagsNone); set => e.FlagsNone = Split(value); }
    [Category("4 色"), DisplayName("R 赤 (0–255)")] public int Red { get => e.Color[0]; set => e.Color[0] = value; }
    [Category("4 色"), DisplayName("G 緑 (0–255)")] public int Green { get => e.Color[1]; set => e.Color[1] = value; }
    [Category("4 色"), DisplayName("B 青 (0–255)")] public int Blue { get => e.Color[2]; set => e.Color[2] = value; }
    [Category("4 色"), DisplayName("A 不透明度 (0–255)")] public int Alpha { get => e.Color[3]; set => e.Color[3] = value; }
    [Category("5 内容"), DisplayName("テクスチャ"), Description("素材フォルダーからの相対パス。例 textures/icon.dds")] public string Texture { get => e.Texture; set => e.Texture = value.Replace('\\', '/'); }
    [Category("5 内容"), DisplayName("テキスト")] public string Text { get => e.Text; set => e.Text = value; }
    [Category("5 内容"), DisplayName("文字サイズ")] public float FontSize { get => e.FontSize; set => e.FontSize = value; }
    [Category("5 内容"), DisplayName("フォント")] public string Font { get => e.Font; set => e.Font = value; }
    [Category("5 内容"), DisplayName("変数名"), Description("例 player_name。プレビュー変数に同じ名前を設定します。空なら固定文字列。")] public string Bind { get => e.Bind; set => e.Bind = value; }
    [Category("6 動作"), DisplayName("Delay (ms)"), Description("押した後、または選択を変更した後、動作を開始するまでの時間。0〜60000 ms。1000 ms = 1秒。")] public int DelayMs { get => e.DelayMs; set => e.DelayMs = value; }
    [Category("6 動作"), DisplayName("動作の数"), ReadOnly(true), Description("下の「音・動作を設定」で編集します。プルダウンは選択変更時に実行します。")] public int ActionCount => e.Kind == "dropdown" ? e.OnChange.Count : e.OnClick.Count;
    [Category("7 ボタンの色"), DisplayName("カーソルが重なった色")] public Color HoverColor { get => FromRgba(e.HoverColor ?? e.Color); set => e.HoverColor = ToRgba(value); }
    [Category("7 ボタンの色"), DisplayName("押したときの色")] public Color PressedColor { get => FromRgba(e.PressedColor ?? e.Color); set => e.PressedColor = ToRgba(value); }
    [Category("7 ボタンの色"), DisplayName("文字色")] public Color TextColor { get => FromRgba(e.TextColor ?? [255,255,255,255]); set => e.TextColor = ToRgba(value); }
    [Category("8 影"), DisplayName("影を表示")] public bool HasShadow { get => e.ShadowColor is { Length: 4 } && e.ShadowColor[3] > 0; set { e.ShadowColor = value ? [0,0,0,140] : null; if (value && e.ShadowX == 0 && e.ShadowY == 0) { e.ShadowX = 4; e.ShadowY = 4; } } }
    [Category("8 影"), DisplayName("影の色")] public Color ShadowColor { get => FromRgba(e.ShadowColor ?? [0,0,0,140]); set => e.ShadowColor = [value.R,value.G,value.B,e.ShadowColor?[3] ?? 140]; }
    [Category("8 影"), DisplayName("影の不透明度 (0–255)")] public int ShadowAlpha { get => e.ShadowColor?[3] ?? 0; set { e.ShadowColor ??= [0,0,0,0]; e.ShadowColor[3] = value; } }
    [Category("8 影"), DisplayName("影の横位置")] public float ShadowX { get => e.ShadowX; set => e.ShadowX = value; }
    [Category("8 影"), DisplayName("影の縦位置")] public float ShadowY { get => e.ShadowY; set => e.ShadowY = value; }
    [Category("9 選択項目"), DisplayName("項目数"), ReadOnly(true), Description("下の「選択項目」で選択肢と初期値を編集します。")] public int OptionCount => e.Options.Count;
    private static Color FromRgba(int[] c) => Color.FromArgb(c[3],c[0],c[1],c[2]);
    private static int[] ToRgba(Color c) => [c.R,c.G,c.B,c.A];
    AttributeCollection ICustomTypeDescriptor.GetAttributes() => TypeDescriptor.GetAttributes(typeof(ElementProperties));
    string? ICustomTypeDescriptor.GetClassName() => null;
    string? ICustomTypeDescriptor.GetComponentName() => e.Id;
    TypeConverter ICustomTypeDescriptor.GetConverter() => new TypeConverter();
    EventDescriptor? ICustomTypeDescriptor.GetDefaultEvent() => null;
    PropertyDescriptor? ICustomTypeDescriptor.GetDefaultProperty() => null;
    object? ICustomTypeDescriptor.GetEditor(Type editorBaseType) => null;
    EventDescriptorCollection ICustomTypeDescriptor.GetEvents() => EventDescriptorCollection.Empty;
    EventDescriptorCollection ICustomTypeDescriptor.GetEvents(Attribute[]? attributes) => EventDescriptorCollection.Empty;
    PropertyDescriptorCollection ICustomTypeDescriptor.GetProperties() => RelevantProperties();
    PropertyDescriptorCollection ICustomTypeDescriptor.GetProperties(Attribute[]? attributes) => RelevantProperties();
    object ICustomTypeDescriptor.GetPropertyOwner(PropertyDescriptor? pd) => this;
    private PropertyDescriptorCollection RelevantProperties() { var all = TypeDescriptor.GetProperties(typeof(ElementProperties)).Cast<PropertyDescriptor>(); bool control = e.Kind is "button" or "dropdown"; return new(all.Where(p => p.Name == "Texture" ? e.Kind == "image" || control : new[] { "Text", "FontSize", "Font", "Bind" }.Contains(p.Name) ? e.Kind == "text" || control : p.Name is "HoverColor" or "PressedColor" or "TextColor" ? control : p.Name == "OptionCount" ? e.Kind == "dropdown" : true).ToArray()); }
    public static List<string> Split(string value) => value.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries).Distinct(StringComparer.Ordinal).ToList();
}

