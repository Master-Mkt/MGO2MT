using System.Text.Json.Nodes;
namespace Mgo2Mt.WpnEditor;
internal sealed class CurveDialog:Form
{
    private readonly DataGridView grid=new(){Dock=DockStyle.Fill,AllowUserToAddRows=true,AllowUserToDeleteRows=true,RowHeadersWidth=30,AutoSizeColumnsMode=DataGridViewAutoSizeColumnsMode.Fill};
    public JsonArray Result{get;private set;}=[];
    public CurveDialog(string title,JsonArray values,double maximum)
    {
        Text=title;Font=new("Yu Gothic UI",10);Size=new(520,540);StartPosition=FormStartPosition.CenterParent;MinimizeBox=false;MaximizeBox=false;
        grid.Columns.Add("age","経過（0=開始 / 1=終了）");grid.Columns.Add("value","値");foreach(var p in values.OfType<JsonArray>())grid.Rows.Add(Fields.Number(p[0]!),Fields.Number(p[1]!));
        var hint=new Label{Text=$"2～16点。経過は昇順、先頭0・末尾1。値は0～{maximum}。\n間の値は直線で補間します。",Dock=DockStyle.Top,Height=58,Padding=new(8)};
        var bar=new FlowLayoutPanel{Dock=DockStyle.Bottom,Height=44,FlowDirection=FlowDirection.RightToLeft};var ok=new Button{Text="反映",Width=90,Height=30};var cancel=new Button{Text="キャンセル",Width=110,Height=30,DialogResult=DialogResult.Cancel};bar.Controls.Add(ok);bar.Controls.Add(cancel);Controls.Add(grid);Controls.Add(hint);Controls.Add(bar);CancelButton=cancel;
        ok.Click+=(_,_)=>{try{grid.EndEdit();var a=new JsonArray();double last=-1;foreach(DataGridViewRow row in grid.Rows){if(row.IsNewRow)continue;if(!double.TryParse(Convert.ToString(row.Cells[0].Value),out var t)||!double.TryParse(Convert.ToString(row.Cells[1].Value),out var v)||!double.IsFinite(t)||!double.IsFinite(v)||t<=last||t>1||t<0||v<0||v>maximum)throw new InvalidDataException("経過と値の範囲・順序を確認してください。");a.Add(new JsonArray(t,v));last=t;}if(a.Count is <2 or >16||a[0]![0]!.GetValue<double>()!=0||last!=1)throw new InvalidDataException("2～16点、先頭0・末尾1で指定してください。");Result=a;DialogResult=DialogResult.OK;}catch(Exception ex){MessageBox.Show(this,ex.Message,"曲線を反映できません");}};
    }
}
