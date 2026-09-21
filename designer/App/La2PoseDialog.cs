namespace MgoUi;

internal sealed class La2PoseDialog : Form
{
    private readonly ComboBox poses=new(){DropDownStyle=ComboBoxStyle.DropDownList,Width=570};
    public uint? EventHash => (poses.SelectedItem as Choice)?.Hash;
    public bool InitialState => (poses.SelectedItem as Choice)?.Initial??false;
    public La2PoseDialog(La2Inspection inspection)
    {
        Text="取り込む表示状態";Size=new(650,270);MinimumSize=Size;MaximumSize=Size;StartPosition=FormStartPosition.CenterParent;Font=new("Yu Gothic UI",10);MaximizeBox=false;MinimizeBox=false;
        var body=new FlowLayoutPanel{Dock=DockStyle.Fill,FlowDirection=FlowDirection.TopDown,WrapContents=false,Padding=new(20)};
        body.Controls.Add(new Label{Text=$"原UI: {inspection.NodeCount}部品・{inspection.Events.Count}個の表示変化",AutoSize=true,Margin=new(0,0,0,15)});
        poses.Items.Add(new Choice("推奨の表示状態",null,false));poses.Items.Add(new Choice("表示変化を適用する前の初期状態",null,true));
        foreach(var item in inspection.Events)poses.Items.Add(new Choice($"表示状態 {poses.Items.Count-1}  ·  {item.Hash:x6}  ({item.TrackCount}部品の変化)",item.Hash,false));
        poses.SelectedIndex=0;body.Controls.Add(poses);body.Controls.Add(new Label{Text="選んだ表示変化の終点を、静止した部品として取り込みます。\nアニメーションの時間再生は行いません。元の情報はレポートに保存します。",AutoSize=true,Margin=new(0,15,0,0)});
        var bottom=new FlowLayoutPanel{Dock=DockStyle.Bottom,Height=50,FlowDirection=FlowDirection.RightToLeft,Padding=new(10)};var cancel=new Button{Text="キャンセル",Width=100,DialogResult=DialogResult.Cancel};var ok=new Button{Text="この状態で取込",Width=140,DialogResult=DialogResult.OK};bottom.Controls.Add(cancel);bottom.Controls.Add(ok);AcceptButton=ok;CancelButton=cancel;Controls.Add(body);Controls.Add(bottom);
    }
    private sealed record Choice(string Label,uint? Hash,bool Initial){public override string ToString()=>Label;}
}
