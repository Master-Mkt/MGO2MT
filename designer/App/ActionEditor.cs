using System.Text.Json;
namespace MgoUi;

internal sealed class ActionEditor : Form
{
    private readonly List<UiAction> actions;
    private readonly ListBox list = new() { Dock = DockStyle.Fill, IntegralHeight = false };
    private readonly TextBox sound = new() { ReadOnly = true, Width = 310 };
    private readonly TextBox name = new() { Width = 370 }, value = new() { Width = 370 };
    private readonly NumericUpDown milliseconds = new() { Minimum = 0, Maximum = 60000, Increment = 50, Width = 130 };
    private readonly NumericUpDown volume = new() { Minimum = 0, Maximum = 100, Value = 100, Width = 80 };
    private readonly CheckBox wait = new() { Text = "音が最後まで鳴り終わってから次へ", Checked = true, AutoSize = true };
    private readonly ComboBox target = new() { Width = 370, DropDownStyle = ComboBoxStyle.DropDownList };
    private readonly CheckBox visible = new() { Text = "部品を表示する（OFFで非表示）", Checked = true, AutoSize = true };
    private readonly FlowLayoutPanel fields = new() { Dock = DockStyle.Fill, FlowDirection = FlowDirection.TopDown, WrapContents = false, Padding = new(14) };
    private readonly LayoutDocument document; private readonly Element element; private readonly string root; private bool loading;
    public List<UiAction> Result => actions;
    public ActionEditor(LayoutDocument d, Element e, string assetRoot)
    {
        document = d; element = e; root = assetRoot;
        actions = JsonSerializer.Deserialize<List<UiAction>>(JsonSerializer.Serialize(e.Kind == "dropdown" ? e.OnChange : e.OnClick, Serialization.Options), Serialization.Options)!;
        Text = (e.Kind == "dropdown" ? "選択を変えたときの音・動作 — " : "押したときの音・動作 — ") + e.Id; Size = new(950, 540); MinimumSize = new(900, 500); StartPosition = FormStartPosition.CenterParent; Font = new("Yu Gothic UI", 10);
        var body = new TableLayoutPanel { Dock = DockStyle.Fill, Size = new(900,350), ColumnCount=2, RowCount=1, Margin=new(0), Padding=new(0) };
        body.ColumnStyles.Add(new(SizeType.Absolute,430));body.ColumnStyles.Add(new(SizeType.Percent,100));body.RowStyles.Add(new(SizeType.Percent,100));
        var stepList=new Panel{Dock=DockStyle.Fill,Margin=new(0)};var stepFields=new Panel{Dock=DockStyle.Fill,Margin=new(0)};body.Controls.Add(stepList,0,0);body.Controls.Add(stepFields,1,0);stepList.Controls.Add(list);stepFields.Controls.Add(fields);
        var add = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 74, Padding = new(8), WrapContents = true };
        void Button(string title, Action run) { var b = new Button { Text = title, AutoSize = true, Height = 30 }; b.Click += (_, _) => run(); add.Controls.Add(b); }
        Button("＋ 音を再生", () => ChooseSound(true)); Button("＋ 待つ", () => Add(new() { Type = "delay", Ms = 500 })); Button("＋ イベント開始", () => Add(new() { Type = "emitEvent", Name = "after_sound" })); Button("＋ 表示する値", () => Add(new() { Type = "setVariable", Name = "message", Value = "準備完了" })); Button("＋ 部品の表示", () => Add(new() { Type = "setVisible", Target = e.Id, Visible = true }));
        var order = new FlowLayoutPanel { Dock = DockStyle.Bottom, Height = 52, Padding = new(8) };
        foreach (var (title, command) in new (string, Action)[] { ("上へ", () => MoveStep(-1)), ("下へ", () => MoveStep(1)), ("削除", Remove) }) { var b = new Button { Text = title, Width = 95 }; b.Click += (_, _) => command(); order.Controls.Add(b); }
        stepList.Controls.Add(order); var info = new Label { Text = "上から順番に実行します。音声の失敗・画面変更・停止で残りを中止します。\nイベント名はゲーム側で対応する処理へ接続できます。表示変更はこのUI内だけです。", Dock = DockStyle.Top, Height = 62, Padding = new(12) };
        var bottom = new FlowLayoutPanel { Dock = DockStyle.Bottom, Height = 48, FlowDirection = FlowDirection.RightToLeft, Padding = new(8) }; var ok = new Button { Text = "適用", Width = 100 }; ok.Click += (_, _) => { try { ApplyFields(); var clone = Serialization.Clone(document); var targetElement = clone.Elements.Single(x => x.Id == element.Id); if (element.Kind == "dropdown") targetElement.OnChange = actions; else targetElement.OnClick = actions; LayoutRules.Validate(clone); DialogResult = DialogResult.OK; Close(); } catch (Exception ex) { MessageBox.Show(ex.Message, "動作を確認してください"); } }; var cancel = new Button { Text = "キャンセル", Width = 110, DialogResult = DialogResult.Cancel }; bottom.Controls.Add(cancel); bottom.Controls.Add(ok); CancelButton = cancel;
        Controls.Add(body); Controls.Add(add); Controls.Add(info); Controls.Add(bottom); list.SelectedIndexChanged += (_, _) => ShowFields();
        foreach (var control in new Control[] { name, value }) control.TextChanged += (_, _) => ApplyFields(); volume.ValueChanged += (_, _) => ApplyFields(); milliseconds.ValueChanged += (_, _) => ApplyFields(); wait.CheckedChanged += (_, _) => ApplyFields(); target.SelectedIndexChanged += (_, _) => ApplyFields(); visible.CheckedChanged += (_, _) => ApplyFields();
        target.Items.AddRange(d.Elements.Select(x => x.Id).ToArray()); RefreshRows(); if (actions.Count > 0) list.SelectedIndex = 0;
    }
    private UiAction? Selected => list.SelectedIndex >= 0 && list.SelectedIndex < actions.Count ? actions[list.SelectedIndex] : null;
    private void RefreshRows() { var i = list.SelectedIndex; loading = true; list.Items.Clear(); foreach (var a in actions) list.Items.Add(AudioRules.Describe(a)); if (actions.Count > 0) list.SelectedIndex = Math.Clamp(i, 0, actions.Count - 1); loading = false; ShowFields(); }
    private void Add(UiAction a) { if (actions.Count >= 32) { MessageBox.Show("1つのボタンの動作は32個までです。"); return; } actions.Add(a); RefreshRows(); list.SelectedIndex = actions.Count - 1; }
    private void Remove() { if (Selected is null) return; actions.RemoveAt(list.SelectedIndex); RefreshRows(); }
    private void MoveStep(int delta) { int i = list.SelectedIndex, j = i + delta; if (i < 0 || j < 0 || j >= actions.Count) return; (actions[i], actions[j]) = (actions[j], actions[i]); RefreshRows(); list.SelectedIndex = j; }
    private void ChooseSound(bool add) { using var dialog = new OpenFileDialog { Title = "再生する音を選ぶ", Filter = "音声 (WAV / MP3)|*.wav;*.mp3|WAV|*.wav|MP3|*.mp3" }; if (dialog.ShowDialog(this) != DialogResult.OK) return; try { var relative = AudioImport.CopyOriginal(dialog.FileName, root); if (add) Add(new() { Type = "playSound", Sound = relative, Volume = 1, WaitForEnd = true }); else if (Selected is { } a) { a.Sound = relative; RefreshRows(); } } catch (Exception ex) { MessageBox.Show(ex.Message, "音を取り込めません"); } }
    private void ShowFields()
    {
        if (loading) return; loading = true; foreach(var control in new Control[]{sound,name,value,volume,milliseconds,wait,target,visible})control.Parent?.Controls.Remove(control); foreach(var old in fields.Controls.Cast<Control>().ToArray())old.Dispose(); fields.Controls.Clear(); var a = Selected;
        void Label(string title) => fields.Controls.Add(new Label { Text = title, AutoSize = true, Margin = new(0, 8, 0, 5) });
        if (a is null) Label("左上のボタンで動作を追加してください。");
        else if (a.Type == "playSound") { Label("再生する音（元のファイルをそのまま取り込みます）"); sound.Text = a.Sound; var row = new FlowLayoutPanel { AutoSize = true }; var choose = new Button { Text = "選ぶ…", Width = 70 }; choose.Click += (_, _) => ChooseSound(false); row.Controls.Add(sound); row.Controls.Add(choose); fields.Controls.Add(row); Label("音量 (%)"); volume.Value = (decimal)Math.Clamp((a.Volume ?? 1) * 100, 0, 100); fields.Controls.Add(volume); wait.Checked = a.WaitForEnd ?? true; fields.Controls.Add(wait); Label("WAV / MP3、64 MiB・10分以内。\n終了は音声プレーヤーの通知で判定します。"); }
        else if (a.Type == "delay") { Label("待ち時間 (ms)  —  1000 ms = 1秒"); milliseconds.Value = a.Ms ?? 0; fields.Controls.Add(milliseconds); Label("この時間が経過してから、次の動作を開始します。"); }
        else if (a.Type == "setVisible") { Label("表示を切り替える部品"); target.SelectedItem = a.Target; fields.Controls.Add(target); visible.Checked = a.Visible ?? true; fields.Controls.Add(visible); }
        else { Label(a.Type == "emitEvent" ? "開始するイベント名" : "表示に使う変数名"); name.Text = a.Name; fields.Controls.Add(name); if (a.Type == "setVariable") { Label("表示する値"); value.Text = a.Value; fields.Controls.Add(value); } else Label("例: title.start（タイトルから開始）\nmenu.confirm / menu.back（決定 / 戻る）\n独自イベント例: after_sound\nプレビューの実行履歴で開始を確認できます。"); }
        loading = false;
    }
    private void ApplyFields() { if (loading || Selected is not { } a) return; if (a.Type == "playSound") { a.Volume = (float)volume.Value / 100; a.WaitForEnd = wait.Checked; } else if (a.Type == "delay") a.Ms = (int)milliseconds.Value; else if (a.Type == "setVisible") { a.Target = target.SelectedItem?.ToString(); a.Visible = visible.Checked; } else { a.Name = name.Text; if (a.Type == "setVariable") a.Value = value.Text; } var i = list.SelectedIndex; loading = true; list.Items[i] = AudioRules.Describe(a); loading = false; }
}



