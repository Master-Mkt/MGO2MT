using System.Drawing.Imaging;
using System.Text.Json;
namespace MgoUi;

internal sealed partial class MainForm : Form
{
    private LayoutDocument document = Samples.Create(); private readonly History history = new(); private readonly NativeRenderer renderer = new();
    private readonly Canvas canvas = new() { Dock = DockStyle.Fill }; private readonly ListBox layers = new() { Dock = DockStyle.Fill, BorderStyle = BorderStyle.None, IntegralHeight = false }; private readonly PropertyGrid properties = new() { Dock = DockStyle.Fill, ToolbarVisible = false, HelpVisible = true };
    private readonly ComboBox screens = new() { DropDownStyle = ComboBoxStyle.DropDownList, Width = 128 }; private readonly ComboBox states = new() { Width = 126, Text = "idle" }; private readonly TextBox flags = new() { Width = 150 }; private readonly ToolStripStatusLabel status = new() { Spring = true, TextAlign = ContentAlignment.MiddleLeft }; private readonly ToolStripStatusLabel dimensions = new() { Text = "1280 × 720 · 16:9" };
    private bool closingVerification;
    private string assetRoot = AppContext.BaseDirectory; private string? projectFile; private string savedSnapshot = ""; private bool refreshing; private string bindings = "{}"; private readonly string[] arguments;
    private readonly ToolStripButton undo = new("元に戻す"), redo = new("やり直す");
    private static readonly Color Bg = Color.FromArgb(27, 32, 39), PanelBg = Color.FromArgb(35, 42, 50), Fg = Color.FromArgb(220, 231, 236), Accent = Color.FromArgb(62, 201, 174);
    public MainForm(string[] args)
    {
        arguments = args; Text = "MGO2MT Multi UI Designer"; MinimumSize = new(1100, 740); Size = new(1560, 970); StartPosition = FormStartPosition.CenterScreen; BackColor = Bg; ForeColor = Fg; Font = new("Yu Gothic UI", 9.5f); KeyPreview = true;
        var menu = new MenuStrip { BackColor = Bg, ForeColor = Fg }; var file = new ToolStripMenuItem("ファイル"); file.DropDownItems.Add(Item("新規", (_, _) => New(), Keys.Control | Keys.N)); file.DropDownItems.Add(Item("開く…", (_, _) => Open(), Keys.Control | Keys.O)); file.DropDownItems.Add(Item("LA2を取り込む…", (_, _) => ImportLa2())); file.DropDownItems.Add(Item("保存", (_, _) => Save(false), Keys.Control | Keys.S)); file.DropDownItems.Add(Item("名前を付けて保存…", (_, _) => Save(true), Keys.Control | Keys.Shift | Keys.S)); file.DropDownItems.Add(new ToolStripSeparator()); file.DropDownItems.Add(Item("ゲーム用JSONを書き出す…", (_, _) => Export(), Keys.Control | Keys.E)); file.DropDownItems.Add(Item("プレビュー画像を保存…", (_, _) => SavePreview())); file.DropDownItems.Add(Item("閉じる", (_, _) => Close())); menu.Items.Add(file);
        var edit = new ToolStripMenuItem("編集"); edit.DropDownItems.Add(Item("元に戻す", (_, _) => Undo(), Keys.Control | Keys.Z)); edit.DropDownItems.Add(Item("やり直す", (_, _) => Redo(), Keys.Control | Keys.Y)); edit.DropDownItems.Add(Item("複製", (_, _) => Duplicate(), Keys.Control | Keys.D)); edit.DropDownItems.Add(Item("削除", (_, _) => Delete())); menu.Items.Add(edit); menu.Items.Add(Item("使い方", (_, _) => MessageBox.Show("左の部品を中央へドラッグします。\n要素を選択して移動、四隅・辺でサイズ変更。\n矢印キー: 1 px / Shift + 矢印: 8 px。\n左の階層では上の要素が手前です。\n右のプロパティで9アンカー・状態・フラグを編集。\n『画像を取り込む』はPNG/JPEG/BMP/GIF/TIFF/ICO/DDSを元形式のままコピーします。\nDDSが必要な場合だけ『DDS変換』を選びます。\n保存は編集用 .mgoui、書き出しはゲーム用 JSON と素材です。\nプレビュー内容はゲーム共通DLLが描きます。\n音・動作で「音の終了→イベント」を設定し、動作を試すで確認できます。", "使い方")));
        var toolbar = new ToolStrip { BackColor = PanelBg, ForeColor = Fg, GripStyle = ToolStripGripStyle.Hidden, Padding = new(12, 6, 12, 6), AutoSize = false, Height = 48 }; toolbar.Items.Add(Button("新規", (_, _) => New())); toolbar.Items.Add(Button("開く", (_, _) => Open())); toolbar.Items.Add(Button("保存", (_, _) => Save(false))); toolbar.Items.Add(new ToolStripSeparator()); undo.Click += (_, _) => Undo(); redo.Click += (_, _) => Redo(); toolbar.Items.Add(undo); toolbar.Items.Add(redo); toolbar.Items.Add(new ToolStripSeparator()); toolbar.Items.Add(Button("LA2取込", (_, _) => ImportLa2())); toolbar.Items.Add(Button("素材フォルダー", (_, _) => ChooseRoot())); toolbar.Items.Add(Button("画像を取り込む", (_, _) => ImportImage())); toolbar.Items.Add(Button("DDS変換", (_, _) => ImportImage(convertToDds: true))); toolbar.Items.Add(new ToolStripSeparator()); toolbar.Items.Add(Button("JSONを書き出す", (_, _) => Export()));
        var context = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 50, Padding = new(12, 8, 8, 4), BackColor = Bg, WrapContents = false }; context.Controls.Add(Label("画面")); screens.ForeColor=Color.Black; screens.BackColor=Color.White; screens.DrawMode=DrawMode.OwnerDrawFixed; screens.DrawItem+=(_, e)=>{e.DrawBackground();var text=e.Index>=0?screens.Items[e.Index]?.ToString()??"":screens.Text;TextRenderer.DrawText(e.Graphics,text,e.Font,e.Bounds,(e.State&DrawItemState.Selected)!=0?SystemColors.HighlightText:Color.Black,TextFormatFlags.Left|TextFormatFlags.VerticalCenter|TextFormatFlags.NoPrefix);e.DrawFocusRectangle();}; screens.Items.AddRange(LayoutRules.Screens); screens.SelectedIndex = 0; context.Controls.Add(screens); context.Controls.Add(Label("状態")); states.Items.AddRange(new object[] { "idle", "loading", "disconnected", "agreement", "login", "settings", "characters", "creation", "groups", "list", "rooms", "detail", "waiting", "loadout", "active", "dead", "reloading", "summary" }); context.Controls.Add(states); context.Controls.Add(Label("ON フラグ")); context.Controls.Add(flags); var vars = new Button { Text = "変数…", Width = 76, Height = 28, FlatStyle = FlatStyle.Flat }; vars.Click += (_, _) => EditBindings(); context.Controls.Add(vars); var grid = new CheckBox { Text = "グリッド", Checked = true, AutoSize = true, Margin = new(14, 4, 8, 0) }; grid.CheckedChanged += (_, _) => { canvas.Grid = grid.Checked; canvas.Invalidate(); }; context.Controls.Add(grid); var snap = new CheckBox { Text = "8 px 吸着", Checked = true, AutoSize = true, Margin = new(6, 4, 0, 0) }; snap.CheckedChanged += (_, _) => canvas.Snap = snap.Checked; context.Controls.Add(snap);
        var body = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1, Padding = new(12, 4, 12, 10) }; body.ColumnStyles.Add(new(SizeType.Absolute, 214)); body.ColumnStyles.Add(new(SizeType.Percent, 100)); body.ColumnStyles.Add(new(SizeType.Absolute, 292));
        var left = new Panel { Dock = DockStyle.Fill, BackColor = PanelBg, Margin = new(0, 0, 10, 0) }; var tools = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 218, Padding = new(10, 8, 10, 8), FlowDirection = FlowDirection.TopDown, WrapContents = false }; tools.Controls.Add(new Label { Text = "部品を追加", AutoSize = true, ForeColor = Accent }); foreach (var (kind, title) in new[] { ("panel", "▭  パネル"), ("text", "T  テキスト"), ("image", "▧  画像"), ("button", "▣  ボタン"), ("dropdown", "▾  プルダウン") }) { var b = new Button { Text = title, Tag = kind, Width = 182, Height = 30, FlatStyle = FlatStyle.Flat, TextAlign = ContentAlignment.MiddleLeft }; b.MouseDown += (_, e) => { if (e.Button == MouseButtons.Left) b.DoDragDrop(kind, DragDropEffects.Copy); }; b.Click += (_, _) => Add(kind, new(160, 180)); tools.Controls.Add(b); }
        var layerTitle = new Label { Text = "階層  ·  上ほど手前", Dock = DockStyle.Top, Height = 32, Padding = new(10, 8, 0, 0), ForeColor = Accent }; var layerArea = new Panel { Dock = DockStyle.Fill, Padding = new(8) }; layers.BackColor = PanelBg; layers.ForeColor = Fg; layers.Font = new("Yu Gothic UI", 9); layerArea.Controls.Add(layers); var order = new FlowLayoutPanel { Dock = DockStyle.Bottom, Height = 80, Padding = new(4), WrapContents = true }; foreach (var (title, action) in new (string, Action)[] { ("手前へ", () => Reorder(1)), ("奥へ", () => Reorder(-1)), ("複製", Duplicate), ("削除", Delete) }) { var b = new Button { Text = title, Width = 90, Height = 29, FlatStyle = FlatStyle.Flat }; b.Click += (_, _) => action(); order.Controls.Add(b); }
        left.Controls.Add(layerArea); left.Controls.Add(layerTitle); left.Controls.Add(tools); left.Controls.Add(order);
        var center = new Panel { Dock = DockStyle.Fill, Margin = new(0, 0, 10, 0) }; canvas.Renderer = renderer; center.Controls.Add(canvas); var caption = new Label { Dock = DockStyle.Bottom, Text = "ゲーム共通描画  ·  ドラッグで配置 / 選択枠でサイズ変更", Height = 30, TextAlign = ContentAlignment.MiddleLeft, ForeColor = Color.FromArgb(137, 157, 170) }; center.Controls.Add(caption);
        var right = new Panel { Dock = DockStyle.Fill, BackColor = PanelBg, Margin = Padding.Empty }; properties.ViewBackColor = PanelBg; properties.ViewForeColor = Fg; properties.HelpBackColor = Bg; properties.HelpForeColor = Fg; properties.CategoryForeColor = Accent; properties.LineColor = Color.FromArgb(48, 59, 69); properties.CommandsBackColor = PanelBg; right.Controls.Add(properties); right.Controls.Add(new Label { Text = "プロパティ", Dock = DockStyle.Top, Height = 36, Padding = new(10, 8, 0, 0), ForeColor = Accent }); body.Controls.Add(left, 0, 0); body.Controls.Add(center, 1, 0); body.Controls.Add(right, 2, 0);
        var bottom = new StatusStrip { BackColor = Bg, ForeColor = Fg }; bottom.Items.Add(status); bottom.Items.Add(dimensions); Controls.Add(body); Controls.Add(context); Controls.Add(toolbar); Controls.Add(menu); Controls.Add(bottom); toolbar.Dock = DockStyle.Top; menu.Dock = DockStyle.Top; MainMenuStrip = menu;
        screens.SelectedIndexChanged += (_, _) => { canvas.Selected = null; states.Text = screens.Text switch { "title" => "idle", "menu" => "login", "lobby" => "list", "briefing" => "waiting", "hud" => "active", _ => "summary" }; RefreshAll(); }; states.TextChanged += (_, _) => RefreshCanvas(); flags.TextChanged += (_, _) => RefreshCanvas(); layers.SelectedIndexChanged += (_, _) => { if (refreshing) return; canvas.Selected = (layers.SelectedItem as LayerItem)?.Element; properties.SelectedObject = canvas.Selected is null ? null : new ElementProperties(canvas.Selected, document); canvas.Invalidate(); }; canvas.SelectionChanged += () => RefreshSelection(); canvas.EditCompleted += Commit; canvas.ToolDropped += (kind, point) => Add(kind, point); canvas.Diagnostic += s => { if (s.Length > 0) status.Text = s.Replace('\n', ' '); else status.Text = "共有描画で表示中  |  素材: " + assetRoot; }; properties.PropertyValueChanged += (_, _) => { try { Commit(); } catch (Exception ex) { document = history.Current; canvas.Selected = null; RefreshAll(); MessageBox.Show(ex.Message, "入力を確認してください"); } };
        FormClosing += (_, e) => { if (!ConfirmDiscard()) e.Cancel = true; }; FormClosed += (_, _) => renderer.Dispose(); history.Reset(document); savedSnapshot = CurrentSnapshot; RefreshAll(); InitializeAudio(); Shown += (_, _) => AfterShown();
    }
    protected override bool ProcessCmdKey(ref Message msg, Keys keyData) { if (keyData == Keys.Delete && !canvas.PreviewMode && (canvas.ContainsFocus || layers.ContainsFocus)) { Delete(); return true; } return base.ProcessCmdKey(ref msg, keyData); }
    private string CurrentSnapshot => Serialization.Encode(document) + "|" + assetRoot;
    private static ToolStripMenuItem Item(string text, EventHandler action, Keys shortcut = Keys.None) { var i = new ToolStripMenuItem(text) { ShortcutKeys = shortcut }; i.Click += action; return i; }
    private static ToolStripButton Button(string text, EventHandler action) { var b = new ToolStripButton(text); b.Click += action; return b; }
    private static Label Label(string text) => new() { Text = text, AutoSize = true, Margin = new(8, 6, 6, 0) };
    private void RefreshCanvas() { canvas.Document = document; canvas.AssetRoot = assetRoot; canvas.Screen = screens.Text; canvas.State = states.Text; canvas.Flags = ElementProperties.Split(flags.Text).ToHashSet(StringComparer.Ordinal); canvas.Bindings = bindings; canvas.RefreshPreview(); }
    private void RefreshSelection() { refreshing = true; try { layers.SelectedIndex = -1; for (int i = 0; i < layers.Items.Count; i++) if (((LayerItem)layers.Items[i]).Element == canvas.Selected) { layers.SelectedIndex = i; break; } properties.SelectedObject = canvas.Selected is null ? null : new ElementProperties(canvas.Selected, document); } finally { refreshing = false; } canvas.Invalidate(); }
    private void RefreshAll() { refreshing = true; try { var id = canvas.Selected?.Id; layers.Items.Clear(); foreach (var e in document.Elements.Where(e => e.Screen == screens.Text).OrderByDescending(e => e.Z).ThenByDescending(e => document.Elements.IndexOf(e))) layers.Items.Add(new LayerItem(e)); canvas.Selected = document.Elements.Find(e => e.Id == id && e.Screen == screens.Text); undo.Enabled = history.CanUndo; redo.Enabled = history.CanRedo; Text = "MGO2MT Multi UI Designer — " + (projectFile is null ? "新しいレイアウト" : Path.GetFileName(projectFile)) + (CurrentSnapshot != savedSnapshot ? " *" : ""); } finally { refreshing = false; } RefreshSelection(); RefreshCanvas(); }
    private void Commit() { renderer.Cancel(reset: true); LayoutRules.Validate(document); history.Commit(document); RefreshAll(); }
    private void Add(string kind, PointF point, bool commit = true)
    {
        var prefix = kind + "_"; int n = 1; while (document.Elements.Any(e => e.Id == prefix + n)) n++;
        bool control = kind is "button" or "dropdown";
        var e = new Element { Id = prefix + n, Kind = kind, Text = kind == "text" ? "テキスト" : kind == "button" ? "ボタン" : "", Screen = screens.Text, X = point.X, Y = point.Y, Z = document.Elements.Select(x => x.Z).DefaultIfEmpty(0).Max() + 1, Color = kind == "panel" || control ? [30,50,63,240] : [240,247,250,255], Height = kind == "text" || control ? 56 : 100 };
        if (control) { e.HoverColor = [49,92,105,255]; e.PressedColor = [28,135,121,255]; e.TextColor = [240,248,249,255]; e.ShadowColor = [0,0,0,120]; e.ShadowX = 4; e.ShadowY = 4; }
        if (kind == "dropdown") { e.Options = [new() { Value = "first", Label = "項目 1" },new() { Value = "second", Label = "項目 2" }]; e.Selected = "first"; }
        document.Elements.Add(e); canvas.Selected = e; if (commit) Commit();
    }
    private void Delete() { if (canvas.Selected is null) return; var id=canvas.Selected.Id; document.Elements.Remove(canvas.Selected); foreach(var item in document.Elements) { item.OnClick.RemoveAll(a=>a.Type=="setVisible"&&a.Target==id); item.OnChange.RemoveAll(a=>a.Type=="setVisible"&&a.Target==id); } canvas.Selected = null; Commit(); }
    private void Duplicate() { if (canvas.Selected is null) return; var clone = Serialization.Clone(document).Elements.Single(e => e.Id == canvas.Selected.Id); var originalId=clone.Id; var baseId = clone.Id + "_copy"; clone.Id = baseId; for (int i = 2; document.Elements.Any(e => e.Id == clone.Id); i++) clone.Id = baseId + i; foreach(var action in clone.OnClick.Concat(clone.OnChange)) if(action.Type=="setVisible"&&action.Target==originalId)action.Target=clone.Id; clone.X += 16; clone.Y += 16; clone.Z = document.Elements.Select(e => e.Z).DefaultIfEmpty(0).Max() + 1; document.Elements.Add(clone); canvas.Selected = clone; Commit(); }
    private void Reorder(int delta) { if (canvas.Selected is null) return; var ordered = document.Elements.Where(e => e.Screen == screens.Text).OrderBy(e => e.Z).ThenBy(document.Elements.IndexOf).ToList(); int i = ordered.IndexOf(canvas.Selected), j = Math.Clamp(i + delta, 0, ordered.Count - 1); (ordered[i], ordered[j]) = (ordered[j], ordered[i]); for (int k = 0; k < ordered.Count; k++) ordered[k].Z = k; Commit(); }
    private void Undo() { if (!history.CanUndo) return; renderer.Cancel(reset: true); document = history.Undo(); RefreshAll(); }
    private void Redo() { if (!history.CanRedo) return; renderer.Cancel(reset: true); document = history.Redo(); RefreshAll(); }
    private bool ConfirmDiscard() { if(closingVerification)return true; if (CurrentSnapshot == savedSnapshot) return true; var r = MessageBox.Show("変更を保存しますか？", "未保存の変更", MessageBoxButtons.YesNoCancel, MessageBoxIcon.Question); if (r == DialogResult.Cancel) return false; if (r == DialogResult.Yes) return Save(false); return true; }
    private void New() { if (!ConfirmDiscard()) return; renderer.Cancel(reset: true); document = Samples.Create(); projectFile = null; canvas.Selected = null; history.Reset(document); savedSnapshot = CurrentSnapshot; RefreshAll(); }
    private void Open() { if (!ConfirmDiscard()) return; using var dialog = new OpenFileDialog { Filter = "UI プロジェクト / JSON|*.mgoui;*.json|すべて|*.*" }; if (dialog.ShowDialog() == DialogResult.OK) Run(() => LoadFile(dialog.FileName)); }
    private void LoadFile(string file) { renderer.Cancel(reset: true); var p = Serialization.ReadProject(file); document = p.Layout; assetRoot = p.AssetRoot; projectFile = Path.GetFullPath(file); canvas.Selected = null; history.Reset(document); savedSnapshot = CurrentSnapshot; RefreshAll(); }
    private bool Save(bool saveAs) { if (projectFile is null || saveAs || Path.GetExtension(projectFile).Equals(".json", StringComparison.OrdinalIgnoreCase)) { using var dialog = new SaveFileDialog { Filter = "UI 編集プロジェクト|*.mgoui", FileName = projectFile is null ? "layout.mgoui" : Path.GetFileNameWithoutExtension(projectFile) + ".mgoui" }; if (dialog.ShowDialog() != DialogResult.OK) return false; projectFile = dialog.FileName; } try { Serialization.WriteProject(projectFile, new() { AssetRoot = assetRoot, Layout = document }); savedSnapshot = CurrentSnapshot; RefreshAll(); return true; } catch (Exception ex) { MessageBox.Show(ex.Message, "保存できません"); return false; } }
    private void ChooseRoot() { using var d = new FolderBrowserDialog { Description = "ゲームのdata、または使用する画像を含む素材フォルダー", SelectedPath = assetRoot }; if (d.ShowDialog() == DialogResult.OK) { assetRoot = d.SelectedPath; RefreshAll(); } }
    private void ImportImage(bool convertToDds = false)
    {
        using var dialog = new OpenFileDialog { Title = convertToDds ? "DDSへ変換して取り込む" : "元の形式のまま画像を取り込む", Filter = "対応画像|*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff;*.ico;*.dds|PNG|*.png|JPEG|*.jpg;*.jpeg|BMP|*.bmp|GIF|*.gif|TIFF|*.tif;*.tiff|ICO|*.ico|DDS|*.dds" };
        if (dialog.ShowDialog() != DialogResult.OK) return;
        Run(() =>
        {
            if (projectFile is null)
            {
                var useLocal = assetRoot == AppContext.BaseDirectory && document.Elements.All(e => e.Texture.Length == 0);
                if (!Save(true)) return;
                if (useLocal) assetRoot = Path.Combine(Path.GetDirectoryName(projectFile!)!, Path.GetFileNameWithoutExtension(projectFile) + ".assets");
            }
            ImportImageFile(dialog.FileName, convertToDds);
        });
    }
    private string ImportImageFile(string input, bool convertToDds = false)
    {
        // Decode through the game renderer first so a corrupt file never becomes
        // a project reference. The ordinary import keeps every input byte.
        renderer.DecodeImage(input, out var width, out var height);
        string relative;
        if (convertToDds)
        {
            var directory = Path.Combine(assetRoot, "designer_textures");
            Directory.CreateDirectory(directory);
            var name = Path.GetFileNameWithoutExtension(input);
            var path = Path.Combine(directory, name + ".dds");
            for (int i = 2; File.Exists(path); i++) path = Path.Combine(directory, name + "_" + i + ".dds");
            renderer.ImportDds(input, path);
            relative = "designer_textures/" + Path.GetFileName(path);
        }
        else relative = ImageImport.CopyOriginal(input, assetRoot);
        if (canvas.Selected?.Kind is not ("image" or "button" or "dropdown"))
        {
            Add("image", new(200, 200), commit: false);
            var scale = Math.Min(1f, Math.Min(320f / width, 180f / height));
            canvas.Selected!.Width = width * scale;
            canvas.Selected.Height = height * scale;
        }
        canvas.Selected!.Texture = relative;
        Commit();
        status.Text = convertToDds ? "DDSへ変換して取り込みました。" : "元の画像をそのまま取り込みました（形式・内容を保持）。";
        return relative;
    }
    private void Export() { using var d = new SaveFileDialog { Filter = "ゲーム用 UI JSON|*.json", FileName = "ui_layout.json" }; if (d.ShowDialog() != DialogResult.OK) return; Run(() => { if (!renderer.Load(document, assetRoot, strict: true)) throw new InvalidDataException(renderer.Error); var count = RuntimeExport.Export(d.FileName, document, assetRoot); status.Text = $"書き出しました: {d.FileName}  ·  素材 {count} 個"; }); }
    private void SavePreview() { using var d = new SaveFileDialog { Filter = "PNG画像|*.png", FileName = "ui_preview.png" }; if (d.ShowDialog() != DialogResult.OK) return; Run(() => CapturePreview(d.FileName)); }
    private void CapturePreview(string file) { if (!renderer.Load(document, assetRoot, strict: true)) throw new InvalidDataException(renderer.Error); var rgba = renderer.Render(1280, 720, screens.Text, states.Text, canvas.Flags, bindings) ?? throw new InvalidDataException(renderer.Error); for (int i = 0; i < rgba.Length; i += 4) (rgba[i], rgba[i + 2]) = (rgba[i + 2], rgba[i]); using var bmp = new Bitmap(1280, 720, PixelFormat.Format32bppArgb); var data = bmp.LockBits(new(0, 0, 1280, 720), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb); try { System.Runtime.InteropServices.Marshal.Copy(rgba, 0, data.Scan0, rgba.Length); } finally { bmp.UnlockBits(data); } Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(file))!); bmp.Save(file, ImageFormat.Png); }
    private void EditBindings() { using var f = new Form { Text = "プレビュー変数", Size = new(460, 420), StartPosition = FormStartPosition.CenterParent, BackColor = Bg, ForeColor = Fg }; var info = new Label { Text = "一行につき 変数名=表示する値 を入力\n例  player_name=プレイヤー", Dock = DockStyle.Top, Height = 54, Padding = new(10) }; var box = new TextBox { Multiline = true, Dock = DockStyle.Fill, ScrollBars = ScrollBars.Vertical, Font = Font }; var map = JsonSerializer.Deserialize<Dictionary<string, string>>(bindings)!; box.Text = string.Join(Environment.NewLine, map.Select(kv => kv.Key + "=" + kv.Value)); var ok = new Button { Text = "適用", Dock = DockStyle.Bottom, Height = 36, DialogResult = DialogResult.OK }; f.Controls.Add(box); f.Controls.Add(info); f.Controls.Add(ok); f.AcceptButton = ok; if (f.ShowDialog(this) != DialogResult.OK) return; Run(() => { var result = new Dictionary<string, string>(); foreach (var line in box.Lines.Where(l => l.Length > 0)) { int i = line.IndexOf('='); if (i <= 0) throw new InvalidDataException("変数名=値 の形式で入力してください。"); result[line[..i].Trim()] = line[(i + 1)..]; } bindings = JsonSerializer.Serialize(result); RefreshCanvas(); }); }
    private static void Run(Action action) { try { action(); } catch (Exception ex) { MessageBox.Show(ex.Message, "操作を完了できません", MessageBoxButtons.OK, MessageBoxIcon.Warning); } }
    private void AfterShown() { if(Array.IndexOf(arguments,"--verify-la2") is var la2Index && la2Index>=0 && la2Index+1<arguments.Length) { _=VerifyLa2(); return; } try { var index = Array.IndexOf(arguments, "--open"); if (index >= 0 && index + 1 < arguments.Length) LoadFile(arguments[index + 1]); index = Array.IndexOf(arguments, "--asset-root"); if (index >= 0 && index + 1 < arguments.Length) { assetRoot = Path.GetFullPath(arguments[index + 1]); RefreshCanvas(); } index = Array.IndexOf(arguments, "--capture"); if (index >= 0 && index + 1 < arguments.Length) { var folder = Path.GetFullPath(arguments[index + 1]); Directory.CreateDirectory(folder); var timer = new System.Windows.Forms.Timer { Interval = 1200 }; timer.Tick += (_, _) => { timer.Stop(); try { if (arguments.Contains("--verify")) ExerciseEditor(folder); canvas.Selected = document.Elements.Find(e => e.Id == "title_start") ?? document.Elements.FirstOrDefault(); RefreshAll(); Refresh(); using var screenshot = new Bitmap(Width, Height); DrawToBitmap(screenshot, new Rectangle(0, 0, Width, Height)); screenshot.Save(Path.Combine(folder, "designer.png"), ImageFormat.Png); CapturePreview(Path.Combine(folder, "runtime-preview.png")); File.WriteAllText(Path.Combine(folder, "capture.json"), JsonSerializer.Serialize(new { sharedRenderer = renderer.Available, error = renderer.Error, screen = screens.Text, elements = document.Elements.Count, runtimeDirectory = System.Runtime.InteropServices.RuntimeEnvironment.GetRuntimeDirectory() })); Environment.ExitCode = 0; } catch (Exception ex) { File.WriteAllText(Path.Combine(folder, "capture-error.txt"), ex.ToString()); Environment.ExitCode = 1; } savedSnapshot = CurrentSnapshot; Close(); }; timer.Start(); } } catch (Exception ex) { MessageBox.Show(ex.Message, "起動できません"); } }
    private void ExerciseEditor(string folder)
    {
        int checks = 0; void Check(bool b, string label) { checks++; if (!b) throw new InvalidOperationException("Editor test: " + label); }
        document = Samples.Create(); history.Reset(document); screens.SelectedItem = "title"; canvas.Selected = document.Elements.Single(e => e.Id == "title_start"); RefreshAll(); canvas.Snap = false; var before = LayoutRules.Bounds(canvas.Selected!); canvas.ExerciseDrag(new(before.X + 20, before.Y + 20), new(before.X + 60, before.Y + 44)); var moved = LayoutRules.Bounds(canvas.Selected!); Check(moved.X > before.X + 35 && moved.Y > before.Y + 20, "pointer drag"); Check(history.CanUndo, "gesture commit"); Undo(); Check(LayoutRules.Bounds(canvas.Selected!) == before, "drag undo"); Redo(); Check(LayoutRules.Bounds(canvas.Selected!) == moved, "drag redo"); Undo(); var b = LayoutRules.Bounds(canvas.Selected!); canvas.ExerciseDrag(new(b.X + b.Width, b.Y + b.Height), new(b.X + b.Width + 40, b.Y + b.Height + 24)); Check(canvas.Selected!.Width > b.Width + 35 && canvas.Selected.Height > b.Height + 20, "resize handle"); Undo(); var count = document.Elements.Count; Duplicate(); Check(document.Elements.Count == count + 1, "duplicate"); Delete(); Check(document.Elements.Count == count, "delete"); Undo(); Check(document.Elements.Count == count + 1, "undo delete"); Redo(); Check(document.Elements.Count == count, "redo delete"); canvas.Selected = document.Elements.Single(e => e.Id == "title_start"); var oldZ = canvas.Selected.Z; Reorder(1); Check(canvas.Selected.Z > oldZ, "layer up"); Undo();
        var input = Path.Combine(folder, "test-image.png"); using (var bmp = new Bitmap(64, 40)) { using var g = Graphics.FromImage(bmp); g.Clear(Color.Transparent); using var brush = new SolidBrush(Color.FromArgb(173, 55, 200, 160)); g.FillRectangle(brush, 8, 8, 48, 24); bmp.Save(input, ImageFormat.Png); }
        var dds = Path.Combine(folder, "test-image.dds"); renderer.ImportDds(input, dds); var a = renderer.DecodeImage(input, out var w, out var h); var c = renderer.DecodeImage(dds, out var w2, out var h2); Check(w == w2 && h == h2 && a.SequenceEqual(c), "PNG to DDS RGBA exact");
        assetRoot = folder; Add("image", new(100, 220)); canvas.Selected!.Texture = "test-image.dds"; canvas.Selected.Width = 192; canvas.Selected.Height = 120; Commit(); Check(renderer.Load(document, assetRoot, strict: true), "complete native validation"); var export = Path.Combine(folder, "export", "layout.json"); Check(RuntimeExport.Export(export, document, assetRoot) == 1, "texture runtime export"); var roundtrip = Serialization.ReadProject(export); Check(renderer.Load(roundtrip.Layout, roundtrip.AssetRoot, strict: true), "export native reload"); var formatResults = new List<object>();
        var fixtureArg = Array.IndexOf(arguments, "--image-fixtures");
        if (fixtureArg >= 0 && fixtureArg + 1 < arguments.Length)
        {
            var fixtureRoot = Path.GetFullPath(arguments[fixtureArg + 1]);
            using (var sourceBitmap = new Bitmap(input))
            {
                sourceBitmap.Save(Path.Combine(folder, "test-image.jpg"), ImageFormat.Jpeg);
                sourceBitmap.Save(Path.Combine(folder, "test-image.bmp"), ImageFormat.Bmp);
            }
            File.Copy(Path.Combine(folder, "test-image.jpg"), Path.Combine(folder, "test-image.jpeg"), true);
            File.Copy(Path.Combine(fixtureRoot, "pages.tiff"), Path.Combine(folder, "test-pages.tif"), true);
            var sources = new[] { Path.Combine(fixtureRoot, "alpha.png"), Path.Combine(fixtureRoot, "animated.gif"), Path.Combine(fixtureRoot, "pages.tiff"), Path.Combine(fixtureRoot, "sizes.ico"), Path.Combine(folder, "test-image.jpg"), Path.Combine(folder, "test-image.jpeg"), Path.Combine(folder, "test-image.bmp"), Path.Combine(folder, "test-pages.tif"), dds };
            for (int i = 0; i < sources.Length; i++)
            {
                var sourceBytes = File.ReadAllBytes(sources[i]);
                var sourcePixels = renderer.DecodeImage(sources[i], out var sourceWidth, out var sourceHeight);
                canvas.Selected = null;
                var previousCount = document.Elements.Count;
                var relative = ImportImageFile(sources[i]);
                var importedPath = LayoutRules.AssetPath(assetRoot, relative);
                Check(File.ReadAllBytes(importedPath).SequenceEqual(sourceBytes) && File.ReadAllBytes(sources[i]).SequenceEqual(sourceBytes), "lossless original import " + Path.GetExtension(sources[i]));
                Check(Path.GetExtension(relative) == Path.GetExtension(sources[i]).ToLowerInvariant(), "original format retained");
                var importedPixels = renderer.DecodeImage(importedPath, out var importedWidth, out var importedHeight);
                Check(sourceWidth == importedWidth && sourceHeight == importedHeight && sourcePixels.SequenceEqual(importedPixels), "same native decoded pixels");
                if (i < 4) Check(sourceWidth == 16 && sourceHeight == 16, "WIC first frame selected");
                if (i < 4) Check(sourcePixels.Where((_, offset) => offset % 4 == 3).Any(alpha => alpha == 0), "transparent fixture retained");
                Check(document.Elements.Count == previousCount + 1, "new image imported");
                Undo(); Check(document.Elements.Count == previousCount, "single undo removes import");
                Redo(); Check(document.Elements.Count == previousCount + 1, "redo restores original reference");
                formatResults.Add(new { extension = Path.GetExtension(sources[i]), relative, width = sourceWidth, height = sourceHeight, inputSha256 = Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(sourceBytes)).ToLowerInvariant(), copiedBytesEqual = true, nativePixelsEqual = true });
            }
            var formatsExport = Path.Combine(folder, "formats-export", "layout.json");
            RuntimeExport.Export(formatsExport, document, assetRoot);
            var exported = Serialization.ReadProject(formatsExport);
            var exportedExtensions = exported.Layout.Elements.Where(e => e.Kind == "image").Select(e => Path.GetExtension(e.Texture)).ToHashSet();
            Check(ImageImport.Extensions.All(exportedExtensions.Contains), "JSON export preserves all nine extensions");
            Check(renderer.Load(exported.Layout, exported.AssetRoot, strict: true), "all formats native export reload");
            Check(renderer.Render(1280, 720, "title", "idle", [], "{}") is not null, "all formats native rendered");
            File.WriteAllText(Path.Combine(folder, "image-formats.json"), JsonSerializer.Serialize(formatResults, new JsonSerializerOptions { WriteIndented = true }));
        }
        File.WriteAllText(Path.Combine(folder, "editor-tests.json"), JsonSerializer.Serialize(new { status = "pass", checks, actualWinFormsInputEvents = true, nativeImagePixelEquality = true, originalFormatImports = formatResults.Count })); canvas.Snap = true; savedSnapshot = CurrentSnapshot; RefreshAll();
    }
    private sealed record LayerItem(Element Element) { public override string ToString() => $"{(Element.Visible ? "●" : "○")} {Element.Id}  [{Element.Kind}]"; }
}









