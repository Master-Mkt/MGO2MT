namespace MgoUi;

internal sealed class DropdownEditor : Form
{
    private readonly DataGridView grid = new() { Dock = DockStyle.Fill, AllowUserToAddRows = true, AllowUserToDeleteRows = true, RowHeadersWidth = 34, AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill };
    private readonly ComboBox selected = new() { Width = 330, DropDownStyle = ComboBoxStyle.DropDownList };
    public List<SelectOption> Options { get; private set; } = [];
    public string Selected { get; private set; } = "";
    public DropdownEditor(LayoutDocument document, Element element)
    {
        Text = "プルダウンの選択項目 — " + element.Id; Size = new(680, 460); MinimumSize = new(600, 400); Font = new("Yu Gothic UI", 10); StartPosition = FormStartPosition.CenterParent;
        grid.Columns.Add("label", "画面に表示する名前"); grid.Columns.Add("value", "選択した値");
        foreach (var option in element.Options) grid.Rows.Add(option.Label, option.Value);
        var top = new Label { Text = "行を追加して項目を設定します。選択した値は、変数やイベントに渡します。\n値を重複させず、1〜64項目を設定してください。", Height = 60, Dock = DockStyle.Top, Padding = new(12,8,12,0) };
        var bottom = new FlowLayoutPanel { Dock = DockStyle.Bottom, Height = 82, Padding = new(8), WrapContents = true };
        bottom.Controls.Add(new Label { Text = "最初の選択", AutoSize = true, Margin = new(3,7,8,0) }); bottom.Controls.Add(selected);
        var ok = new Button { Text = "適用", Width = 90 }; var cancel = new Button { Text = "キャンセル", Width = 100, DialogResult = DialogResult.Cancel };
        bottom.Controls.Add(ok); bottom.Controls.Add(cancel); CancelButton = cancel;
        void RefreshChoices() { var before = selected.SelectedItem as Choice; var beforeIndex=selected.SelectedIndex; selected.Items.Clear(); foreach (DataGridViewRow row in grid.Rows) if (!row.IsNewRow) { var value = row.Cells[1].Value?.ToString() ?? ""; var label = row.Cells[0].Value?.ToString() ?? ""; selected.Items.Add(new Choice(value, label)); } var desired = before?.Value ?? element.Selected; selected.SelectedIndex = -1; for (int i=0;i<selected.Items.Count;i++) if ((selected.Items[i] as Choice)?.Value == desired) selected.SelectedIndex=i; if (selected.SelectedIndex<0 && selected.Items.Count>0) selected.SelectedIndex=Math.Clamp(beforeIndex,0,selected.Items.Count-1); }
        grid.CellEndEdit += (_, _) => RefreshChoices(); grid.RowsRemoved += (_, _) => RefreshChoices();
        ok.Click += (_, _) => { try { grid.EndEdit(); Options = grid.Rows.Cast<DataGridViewRow>().Where(r=>!r.IsNewRow).Select(r=>new SelectOption { Label=r.Cells[0].Value?.ToString() ?? "", Value=r.Cells[1].Value?.ToString() ?? "" }).ToList(); Selected = (selected.SelectedItem as Choice)?.Value ?? ""; var clone=Serialization.Clone(document); var e=clone.Elements.Single(e=>e.Id==element.Id); e.Options=Options; e.Selected=Selected; LayoutRules.Validate(clone); DialogResult=DialogResult.OK; Close(); } catch(Exception ex) { MessageBox.Show(ex.Message,"選択項目を確認してください"); } };
        Controls.Add(grid); Controls.Add(top); Controls.Add(bottom); RefreshChoices();
    }
    private sealed record Choice(string Value,string Label) { public override string ToString()=>Label+"  ["+Value+"]"; }
}


