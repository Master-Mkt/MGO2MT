using System.Diagnostics;
using System.Text.Json.Nodes;
namespace Mgo2Mt.WpnEditor;
internal sealed partial class MainForm:Form
{
    private readonly string[] args;
    private Document? doc;
    private int weaponId=25,layerIndex;
    private bool refreshing,playing;
    private readonly ListBox weapons=new(){Dock=DockStyle.Fill,IntegralHeight=false};
    private readonly TextBox search=new(){Dock=DockStyle.Top,PlaceholderText="武器名 / ID で探す"};
    private readonly Label status=new(){Dock=DockStyle.Bottom,Height=35,Padding=new(12,8,4,4),AutoEllipsis=true,BackColor=Color.FromArgb(225,234,239)};
    private readonly Label selection=new(){Dock=DockStyle.Top,Height=65,Padding=new(12),Font=new("Yu Gothic UI",14,FontStyle.Bold)};
    private readonly Preview preview=new();
    private readonly PropertyGrid basic=Grid(),accuracy=Grid(),visual=Grid(),particle=Grid(),light=Grid(),sound=Grid(),cue=Grid(),reticle=Grid();
    private readonly ComboBox channel=Combo(),layers=Combo(),lightEvent=Combo(),soundEvent=Combo();
    private readonly CheckBox shared=new(){Text="全武器の共通設定を編集",AutoSize=true};
    private readonly TrackBar timeline=new(){Dock=DockStyle.Fill,Minimum=0,Maximum=3000,TickFrequency=250};
    private readonly NumericUpDown duration=new(){Minimum=100,Maximum=80000,Value=3000,Increment=100,Width=90};
    private readonly NumericUpDown zoom=new(){Minimum=.01m,Maximum=10,Value=.6m,Increment=.1m,DecimalPlaces=2,Width=72};
    private readonly CheckBox loop=new(){Text="繰り返す",AutoSize=true,Checked=true};
    private readonly CheckBox mirror=new(){Text="HOSTにもゲーム数値を保存",AutoSize=true};
    private readonly TextBox hostFolder=new(){ReadOnly=true,Width=360};
    private readonly AudioPlayer audio=new();
    private readonly System.Windows.Forms.Timer timer=new(){Interval=16};
    private readonly Stopwatch clock=new();
    private int playStart;
    private bool audioPending;
    private int audioDelay;
    private string? audioFile;
    private double audioGain=1;
    private readonly Button play=new(){Text="▶ 再生",Width=90,Height=30};
    private static PropertyGrid Grid()=>new(){Dock=DockStyle.Fill,ToolbarVisible=false,HelpVisible=true,PropertySort=PropertySort.NoSort,Font=new("Yu Gothic UI",10)};
    private static ComboBox Combo(){var c=new EditorCombo{DropDownStyle=ComboBoxStyle.DropDownList,Width=190,DrawMode=DrawMode.OwnerDrawFixed};c.DrawItem+=(_,e)=>{e.DrawBackground();if(e.Index>=0)TextRenderer.DrawText(e.Graphics,c.Items[e.Index]?.ToString(),c.Font,e.Bounds,e.ForeColor,TextFormatFlags.Left|TextFormatFlags.VerticalCenter);e.DrawFocusRectangle();};return c;}
    private sealed record Choice(string Key,string Label){public override string ToString()=>Label;}
    private sealed record WeaponRow(int Id,string Name){public override string ToString()=>$"{Id:000}   {Name}";}
    public MainForm(string[] arguments)
    {
        args=arguments;Text="MGO2MT WPN Effect Editor";Font=new("Yu Gothic UI",10);ClientSize=new(1460,910);MinimumSize=new(1120,720);StartPosition=FormStartPosition.CenterScreen;KeyPreview=true;
        var menu=new MenuStrip();var file=new ToolStripMenuItem("ファイル");file.DropDownItems.Add("設定を開く…",null,(_,_)=>OpenDialog());file.DropDownItems.Add("保存",null,(_,_)=>Save());file.DropDownItems.Add("終了",null,(_,_)=>Close());var edit=new ToolStripMenuItem("編集");edit.DropDownItems.Add("元に戻す  Ctrl+Z",null,(_,_)=>Undo());edit.DropDownItems.Add("やり直す  Ctrl+Y",null,(_,_)=>Redo());menu.Items.Add(file);menu.Items.Add(edit);MainMenuStrip=menu;
        var bar=new FlowLayoutPanel{Dock=DockStyle.Top,Height=48,Padding=new(8),WrapContents=false};Add(bar,"開く…",OpenDialog,90);Add(bar,"保存",()=>Save(),90);Add(bar,"元に戻す",Undo,105);Add(bar,"やり直す",Redo,105);bar.Controls.Add(mirror);bar.Controls.Add(hostFolder);Add(bar,"HOSTのdata…",ChooseHost,130);
        var left=new Panel{Dock=DockStyle.Left,Width=230,Padding=new(8)};left.Controls.Add(weapons);left.Controls.Add(search);
        var tabs=new TabControl{Dock=DockStyle.Right,Width=475,Multiline=true,Padding=new(3,4)};Page(tabs,"基本",basic);var ac=Page(tabs,"命中精度",accuracy);var acbar=new FlowLayoutPanel{Dock=DockStyle.Top,Height=40};Add(acbar,"原設定に戻す",()=>Change(()=>doc!.Weapon(weaponId)["parameters"]?["tuning"]?.AsObject().Remove("accuracy")),140);ac.Controls.Add(acbar);Page(tabs,"モデル",visual);BuildIconPage(tabs);
        var fx=Page(tabs,"粒子",particle);var fxbar=new FlowLayoutPanel{Dock=DockStyle.Top,Height=190,Padding=new(5)};channel.Items.AddRange(new object[]{new Choice("muzzle","発射の閃光"),new Choice("smoke","銃口の煙"),new Choice("casing","薬莢"),new Choice("explosion","爆発"),new Choice("smokeCloud","煙の雲"),new Choice("humanBlood","人間の出血"),new Choice("gekkoBlood","月光の出血")});channel.SelectedIndex=0;fxbar.Controls.Add(shared);fxbar.SetFlowBreak(shared,true);fxbar.Controls.Add(channel);fxbar.Controls.Add(layers);Add(fxbar,"層を追加",AddLayer,95);Add(fxbar,"層を削除",DeleteLayer,95);Add(fxbar,"元の設定へ",ResetChannel,110);Add(fxbar,"画像を取り込む…",ImportTexture,145);Add(fxbar,"原画像一覧…",OriginalTexture,135);Add(fxbar,"大きさの曲線",()=>EditCurve("sizeCurve",16),125);Add(fxbar,"透明度の曲線",()=>EditCurve("alphaCurve",1),125);fx.Controls.Add(fxbar);
        var lp=Page(tabs,"光",light);var lb=new FlowLayoutPanel{Dock=DockStyle.Top,Height=78,Padding=new(5)};lightEvent.Items.AddRange(new object[]{new Choice("shot","発射時"),new Choice("explosion","爆発時")});lightEvent.SelectedIndex=0;lb.Controls.Add(lightEvent);Add(lb,"明るさの曲線",()=>EditCurve("intensityCurve",1),140);Add(lb,"光を設定",()=>Change(()=>EnsureEvent("lights",LightEvent())),110);Add(lb,"元の設定へ",()=>ResetEvent("lights",LightEvent()),110);lp.Controls.Add(lb);
        var sp=Page(tabs,"音",sound);var sb=new FlowLayoutPanel{Dock=DockStyle.Top,Height=118,Padding=new(5)};soundEvent.Items.AddRange(new object[]{new Choice("shot","発射"),new Choice("reload","リロード"),new Choice("click","弾切れ"),new Choice("casing","薬莢"),new Choice("explosion","爆発")});soundEvent.SelectedIndex=0;sb.Controls.Add(soundEvent);Add(sb,"WAVを取り込む…",ImportAudio,160);Add(sb,"試聴",Audition,85);Add(sb,"音を停止",()=>audio.Stop(),95);Add(sb,"元の設定へ",()=>ResetEvent("sounds",SoundEvent()),120);var note=new Label{Text="16bit PCM WAV・30秒以内。音量倍率も試聴に反映。",AutoSize=true};sb.Controls.Add(note);sp.Controls.Add(sb);var cp=Page(tabs,"原音ID",cue);cp.Controls.Add(new Label{Text="原音IDはdata/sfxの登録音を参照します。\n「音」タブの設定がある場合はそちらを優先。",Dock=DockStyle.Top,Height=60,Padding=new(7)});
        Add(sb,"原音を設定",()=>Change(()=>EnsureEvent("sounds",SoundEvent())),110);sb.Height=155;
        var rp=Page(tabs,"照準",reticle);var rb=new FlowLayoutPanel{Dock=DockStyle.Top,Height=90,Padding=new(5)};Add(rb,"照準を設定",()=>Change(()=>EnsureReticle()),125);Add(rb,"元の設定へ",()=>Change(()=>EffectRow()?.Remove("reticle")),115);rb.Controls.Add(new Label{Text="照準の外観を指定します。開閉する幅は「命中精度」の\n数値に応じてゲーム側が計算します。",AutoSize=true});rp.Controls.Add(rb);Add(fxbar,"原素材テンプレート",OriginalPreset,160);
        var center=new Panel{Dock=DockStyle.Fill,Padding=new(4)};var playback=new Panel{Dock=DockStyle.Bottom,Height=100};var controls=new FlowLayoutPanel{Dock=DockStyle.Top,Height=44,Padding=new(6)};controls.Controls.Add(play);Add(controls,"停止",Stop,80);controls.Controls.Add(loop);controls.Controls.Add(new Label{Text="範囲 ms",AutoSize=true,Margin=new(9,7,0,0)});controls.Controls.Add(duration);controls.Controls.Add(new Label{Text="表示倍率",AutoSize=true,Margin=new(9,7,0,0)});controls.Controls.Add(zoom);playback.Controls.Add(timeline);playback.Controls.Add(controls);center.Controls.Add(preview);center.Controls.Add(playback);center.Controls.Add(selection);
        Controls.Add(center);Controls.Add(tabs);Controls.Add(left);Controls.Add(bar);Controls.Add(menu);Controls.Add(status);status.Text="gameplay.jsonを開くと編集できます。原素材はツールに同梱されません。";
        weapons.SelectedIndexChanged+=(_,_)=>{if(refreshing||weapons.SelectedItem is not WeaponRow w)return;Stop();weaponId=w.Id;layerIndex=0;RefreshFields();};search.TextChanged+=(_,_)=>RefreshList();shared.CheckedChanged+=(_,_)=>{Stop();RefreshFields();};channel.SelectedIndexChanged+=(_,_)=>{Stop();layerIndex=0;RefreshFields();};layers.SelectedIndexChanged+=(_,_)=>{if(refreshing)return;layerIndex=Math.Max(0,layers.SelectedIndex);RefreshFields();};lightEvent.SelectedIndexChanged+=(_,_)=>RefreshFields();soundEvent.SelectedIndexChanged+=(_,_)=>RefreshFields();
        timeline.Scroll+=(_,_)=>{Stop();preview.TimeMs=timeline.Value;preview.Invalidate();};duration.ValueChanged+=(_,_)=>{timeline.Maximum=(int)duration.Value;timeline.TickFrequency=Math.Max(1,timeline.Maximum/12);};zoom.ValueChanged+=(_,_)=>{preview.ViewScale=(double)zoom.Value;preview.Invalidate();};play.Click+=(_,_)=>Play();timer.Tick+=(_,_)=>Tick();timer.Start();Deactivate+=(_,_)=>{if(!verifying)Stop();};FormClosed+=(_,_)=>{timer.Dispose();audio.Dispose();};FormClosing+=(_,e)=>{Stop();if(!verifying&&!ConfirmDirty())e.Cancel=true;};KeyDown+=(_,e)=>{if(e.Control&&e.KeyCode==Keys.S){e.SuppressKeyPress=true;Save();}else if(e.Control&&e.KeyCode==Keys.O){e.SuppressKeyPress=true;OpenDialog();}else if(e.Control&&e.KeyCode==Keys.Z){e.SuppressKeyPress=true;Undo();}else if(e.Control&&e.KeyCode==Keys.Y){e.SuppressKeyPress=true;Redo();}};
        Shown+=async(_,_)=>{int v=Array.IndexOf(args,"--verify");if(v>=0&&v+1<args.Length){await Verify(args[v+1]);return;}var p=args.FirstOrDefault(s=>s.EndsWith(".json",StringComparison.OrdinalIgnoreCase));if(p is not null)Guard(()=>LoadFile(p));};
    }
    private static TabPage Page(TabControl tabs,string text,Control content){var p=new TabPage(text);p.Controls.Add(content);tabs.TabPages.Add(p);return p;}
    private static void Add(Control parent,string text,Action action,int width){var b=new Button{Text=text,Width=width,Height=30,Margin=new(3)};b.Click+=(_,_)=>action();parent.Controls.Add(b);}
    private void Guard(Action action){try{action();}catch(Exception ex){status.Text="操作できません: "+ex.Message;if(!verifying)MessageBox.Show(this,ex.Message,"操作できません",MessageBoxButtons.OK,MessageBoxIcon.Warning);else throw;}}
    private bool ConfirmDirty(){if(doc?.Dirty!=true)return true;return MessageBox.Show(this,"編集内容を保存しますか？","未保存の変更",MessageBoxButtons.YesNoCancel) switch{DialogResult.Yes=>Save(),DialogResult.No=>true,_=>false};}
    private void OpenDialog(){if(!ConfirmDirty())return;using var dialog=new OpenFileDialog{Title="ゲームのdata/gameplay.jsonを選択",Filter="武器設定|gameplay.json|JSON|*.json"};if(dialog.ShowDialog(this)==DialogResult.OK)Guard(()=>LoadFile(dialog.FileName));}
    private void LoadFile(string path){Stop();var next=Document.Open(path);next.ValidateCurrent(Native.Validate);doc=next;preview.Document=doc;weaponId=doc.Weapons[0]!["id"]!.GetValue<int>();layerIndex=0;preview.ReloadImages();RefreshList();RefreshFields();status.Text=(next.MigratedLegacyFormat?"旧形式から読み込みました。保存するとMGO2MT形式へ移行します: ":"読み込みました: ")+path;}
    private void RefreshList(){if(doc is null)return;refreshing=true;weapons.Items.Clear();foreach(var row in doc.Weapons.OfType<JsonObject>()){var w=new WeaponRow(row["id"]!.GetValue<int>(),row["name"]!.GetValue<string>());if(search.Text.Length==0||w.ToString().Contains(search.Text,StringComparison.OrdinalIgnoreCase)){weapons.Items.Add(w);if(w.Id==weaponId)weapons.SelectedIndex=weapons.Items.Count-1;}}refreshing=false;}
    private string Channel()=>((Choice)channel.SelectedItem!).Key;
    private string LightEvent()=>((Choice)lightEvent.SelectedItem!).Key;
    private string SoundEvent()=>((Choice)soundEvent.SelectedItem!).Key;
    private JsonObject? EffectRow(bool create=false)
    {
        if(doc is null)return null;if(shared.Checked){if(create)doc.Effects["defaults"]??=new JsonObject();return doc.Effects["defaults"] as JsonObject;}
        if(create)doc.Effects["weapons"]??=new JsonArray();var rows=doc.Effects["weapons"] as JsonArray;var found=rows?.OfType<JsonObject>().FirstOrDefault(w=>w["id"]?.GetValue<int>()==weaponId);if(found is null&&create){found=new JsonObject{["id"]=weaponId};rows!.Add(found);}return found;
    }
    private JsonNode? Resolved(string category,string name)=>EffectRow()?[category]?[name]??(!shared.Checked?doc?.Effects["defaults"]?[category]?[name]:null);
    private JsonArray? LayerArray()=>Resolved("particles",Channel()) as JsonArray;
    private JsonArray EnsureLayers(){var row=EffectRow(true)!;row["particles"]??=new JsonObject();var p=row["particles"]!.AsObject();if(p[Channel()] is null)p[Channel()]=Resolved("particles",Channel())?.DeepClone()??new JsonArray();doc!.EnableEffects();return p[Channel()]!.AsArray();}
    private JsonObject EnsureEvent(string category,string name){var source=Resolved(category,name);var row=EffectRow(true)!;row[category]??=new JsonObject();row[category]![name]??=source?.DeepClone()??new JsonObject{["enabled"]=category!="sounds"};doc!.EnableEffects();return row[category]![name]!.AsObject();}
    private JsonObject? ReticleValue()=>EffectRow()?["reticle"] as JsonObject??(!shared.Checked?doc?.Effects["defaults"]?["reticle"] as JsonObject:null);
    private JsonObject EnsureReticle(){var source=ReticleValue();var row=EffectRow(true)!;row["reticle"]??=source?.DeepClone()??new JsonObject();doc!.EnableEffects();return row["reticle"]!.AsObject();}
    private void RefreshFields()
    {
        if(doc is null)return;refreshing=true;var w=doc.Weapon(weaponId);var p=w["parameters"]!.AsObject();selection.Text=$"{w["name"]!.GetValue<string>()}  /  ID {weaponId}";basic.SelectedObject=Adapter(()=>doc!.Weapon(weaponId)["parameters"]!.AsObject(),Fields.Mechanics,"mechanics");accuracy.SelectedObject=Adapter(()=>doc!.Weapon(weaponId)["parameters"]!.AsObject(),Fields.Accuracy,"mechanics");visual.SelectedObject=Adapter(()=>doc!.Weapon(weaponId)["visual"]!.AsObject(),Fields.Visual,"visual");cue.SelectedObject=Adapter(()=>doc!.Weapon(weaponId)["parameters"]!.AsObject(),Fields.Cues,"mechanics");
        layers.Items.Clear();var a=LayerArray();for(int i=0;i<(a?.Count??0);i++)layers.Items.Add($"層 {i+1}"+(a![i]?["enabled"]?.GetValue<bool>()==false?"（無効）":""));layerIndex=Math.Clamp(layerIndex,0,Math.Max(0,layers.Items.Count-1));if(layers.Items.Count>0)layers.SelectedIndex=layerIndex;
        particle.SelectedObject=a is {Count:>0}?Adapter(()=>LayerArray()![layerIndex]!.AsObject(),Fields.Particle,"particle"):null;
        light.SelectedObject=Resolved("lights",LightEvent()) is JsonObject?Adapter(()=>Resolved("lights",LightEvent())!.AsObject(),Fields.Light,"light"):null;
        sound.SelectedObject=Resolved("sounds",SoundEvent()) is JsonObject?Adapter(()=>Resolved("sounds",SoundEvent())!.AsObject(),Fields.Sound,"sound"):null;
        reticle.SelectedObject=ReticleValue() is {}?Adapter(()=>ReticleValue()!,Fields.Reticle,"reticle"):null;
        preview.WeaponId=shared.Checked?0:weaponId;preview.Channel=Channel();preview.Invalidate();Text="MGO2MT WPN Effect Editor"+(doc.Dirty?"  • 未保存":"");RefreshIcon();refreshing=false;
    }
    private PropertyAdapter Adapter(Func<JsonObject> get,Field[] fields,string group)=>new(get,fields,(f,v)=>Guard(()=>Change(()=>{
        JsonObject target=group switch{"particle"=>EnsureLayers()[layerIndex]!.AsObject(),"light"=>EnsureEvent("lights",LightEvent()),"sound"=>EnsureEvent("sounds",SoundEvent()),"reticle"=>EnsureReticle(),_=>get()};
        if(f.Key.StartsWith("velocity.",StringComparison.Ordinal))target["velocity"]??=new JsonArray(0,0,0);
        if(f.Key.StartsWith("stretch.",StringComparison.Ordinal))target["stretch"]??=new JsonArray(1,1);
        if(f.Key.StartsWith("color.",StringComparison.Ordinal))target["color"]??=group=="light"?new JsonArray(1,.64,.25):group=="reticle"?new JsonArray(1,.65,.17,1):new JsonArray(1,1,1,1);
        Fields.Set(target,f,v);
        if(f.Key=="intervalMs"&&Convert.ToInt32(v)>0)target["fireIntervalTicks"]=null;
        if(f.Key=="fireIntervalTicks"&&Convert.ToInt32(v)>0)target["intervalMs"]=0;
        if(f.Key=="reloadMs"&&Convert.ToInt32(v)>0){target["nativePrimaryMastery"]=false;target["reloadMotion"]=null;target["reloadRefillMs"]=Math.Min(target["reloadRefillMs"]?.GetValue<int>()??0,Convert.ToInt32(v));}
        if(f.Key=="range"&&target["ballistics"] is JsonObject ball){var value=Convert.ToDouble(v);ball["range"]=value;foreach(var key in new[]{"decayStart","decayEnd"})ball[key]=Math.Min(ball[key]?.GetValue<double>()??0,value);}
        if(group=="sound"&&f.Key=="cue"&&Convert.ToInt32(v)>0){target.Remove("path");target["enabled"]=true;}if(group=="sound"&&f.Key=="path"&&Convert.ToString(v) is {Length:>0}){target.Remove("cue");target["enabled"]=true;}
        if(group=="particle"&&f.Key=="texture"&&Convert.ToString(v) is {Length:>0})target["enabled"]=true;
    })));
    private void Change(Action action){if(doc is null)return;Stop();doc.Change(action);preview.ClearImages();RefreshFields();status.Text="変更しました。保存するとゲーム用設定へ反映されます。";}
    private void Undo(){if(doc is null)return;Stop();doc.Undo();RefreshFields();preview.ReloadImages();status.Text="元に戻しました。";}
    private void Redo(){if(doc is null)return;Stop();doc.Redo();RefreshFields();preview.ReloadImages();status.Text="やり直しました。";}
    private void AddLayer(){if(doc is null)return;Guard(()=>Change(()=>{var a=EnsureLayers();if(a.Count>=8)throw new InvalidDataException("1種類につき8層までです。");a.Add(new JsonObject{["enabled"]=false});layerIndex=a.Count-1;}));}
    private void DeleteLayer(){if(doc is null||LayerArray() is not {Count:>0})return;Change(()=>EnsureLayers().RemoveAt(layerIndex));}
    private void ResetChannel(){if(doc is null)return;Change(()=>EffectRow()?["particles"]?.AsObject().Remove(Channel()));}
    private void ResetEvent(string category,string name){if(doc is null)return;Change(()=>EffectRow()?[category]?.AsObject().Remove(name));}
    private void ImportTexture(){if(doc is null)return;using var d=new OpenFileDialog{Filter="画像|*.png;*.dds;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff;*.ico"};if(d.ShowDialog(this)!=DialogResult.OK)return;Guard(()=>{using var checkedImage=Native.Image(d.FileName);string relative=doc.Import(d.FileName,false);Change(()=>{var a=EnsureLayers();if(a.Count==0){a.Add(new JsonObject());layerIndex=0;}var row=a[layerIndex]!.AsObject();row["texture"]=relative;row["enabled"]=true;});preview.ReloadImages();});}
    private void OriginalTexture(){if(doc is null)return;using var d=new TextureDialog(preview.OriginalImages);if(d.ShowDialog(this)!=DialogResult.OK)return;Change(()=>{var a=EnsureLayers();if(a.Count==0){a.Add(new JsonObject());layerIndex=0;}a[layerIndex]!["texture"]=d.Selected;a[layerIndex]!["enabled"]=true;});preview.ReloadImages();}
    private void OriginalPreset()=>Guard(()=>{if(doc is null)return;uint key=Channel() switch{"muzzle"=>0x090aec,"smoke"=>0xca92b7,"explosion"=>0x510d60,"smokeCloud"=>0xcaa2b5,"humanBlood"=>0x668997,"gekkoBlood"=>0x056a8a,_=>0};if(key==0||!preview.OriginalImages.ContainsKey(key))throw new InvalidDataException("この種類の原画像がdataにありません。画像を取り込むか、原画像一覧から選んでください。");Change(()=>{var a=EnsureLayers();a.Clear();bool smoke=Channel() is "smoke" or "smokeCloud";int columns=Channel()=="smoke"?8:Channel()=="humanBlood"?4:Channel()=="gekkoBlood"?1:2,rows=Channel() is "smoke" or "humanBlood"?4:Channel()=="gekkoBlood"?1:2;a.Add(new JsonObject{["texture"]=key,["radius"]=smoke?180:90,["lifetimeMs"]=smoke?2400:Channel()=="muzzle"?80:600,["additive"]=Channel() is "muzzle" or "explosion",["atlas"]=new JsonObject{["columns"]=columns,["rows"]=rows,["frames"]=columns*rows,["fps"]=Channel()=="smoke"?20:Channel()=="humanBlood"?60:6.66}});layerIndex=0;});});
    private void ImportAudio(){if(doc is null)return;using var d=new OpenFileDialog{Filter="音声（WAV）|*.wav"};if(d.ShowDialog(this)!=DialogResult.OK)return;Guard(()=>{Native.Wave(d.FileName);var relative=doc.Import(d.FileName,true);Change(()=>{var row=EnsureEvent("sounds",SoundEvent());row.Remove("cue");row["path"]=relative;row["enabled"]=true;});});}
    private void EditCurve(string field,double maximum){if(doc is null)return;bool isLight=field=="intensityCurve";var row=isLight?Resolved("lights",LightEvent()) as JsonObject:LayerArray() is {Count:>0} a?a[layerIndex] as JsonObject:null;if(row is null)return;var value=row[field] as JsonArray??new JsonArray(new JsonArray(0,1),new JsonArray(1,field=="sizeCurve"?1:0));using var dialog=new CurveDialog(field=="sizeCurve"?"大きさの曲線":isLight?"明るさの曲線":"透明度の曲線",value,maximum);if(dialog.ShowDialog(this)==DialogResult.OK)Change(()=>{var target=isLight?EnsureEvent("lights",LightEvent()):EnsureLayers()[layerIndex]!.AsObject();target[field]=dialog.Result.DeepClone();});}
    private string? ResolveSound(string eventName,out int delay,out double gain)
    {
        delay=0;gain=1;if(doc is null)return null;var row=Resolved("sounds",eventName) as JsonObject;if(row?["enabled"]?.GetValue<bool>()==false)return null;delay=row?["delayMs"]?.GetValue<int>()??0;gain=row?["gain"]?.GetValue<double>()??1;if(row?["path"]?.GetValue<string>() is {Length:>0} path)return Paths.Resolve(doc.Root,path);
        int id=row?["cue"]?.GetValue<int>()??(eventName=="shot"?doc.Weapon(weaponId)["parameters"]?["shotCue"]?.GetValue<int>()??0:0);if(id==0)return null;
        var relative=doc.Gameplay["resources"]?["audioManifest"]?.GetValue<string>();if(relative is null)return null;var manifest=Paths.Resolve(doc.Root,relative);if(!File.Exists(manifest))return null;
        foreach(var line in File.ReadLines(manifest).Skip(1)){var parts=line.Split(' ',StringSplitOptions.RemoveEmptyEntries);if(parts.Length>=2&&int.TryParse(parts[0],out var cueId)&&cueId==id)return Paths.Resolve(Path.GetDirectoryName(manifest)!,parts[1]);}return null;
    }
    private void Audition()=>Guard(()=>{Stop();audioFile=ResolveSound(SoundEvent(),out audioDelay,out audioGain);if(audioFile is null)throw new InvalidDataException("試聴するWAVを選ぶか、登録済みの原音IDを指定してください。");audioPending=true;playStart=0;clock.Restart();status.Text="音を試聴しています（指定の開始遅延を反映）。";});
    private void Play(){if(doc is null)return;if(playing){Stop();return;}Guard(()=>{playStart=timeline.Value>=timeline.Maximum?0:timeline.Value;clock.Restart();playing=true;play.Text="❚❚ 一時停止";audioFile=ResolveSound(Channel()=="explosion"?"explosion":"shot",out audioDelay,out audioGain);audioPending=playStart==0&&audioFile is not null;});}
    private void Tick(){if(audioPending&&clock.ElapsedMilliseconds>=audioDelay){audioPending=false;Guard(()=>audio.Play(audioFile!,audioGain));}if(!playing)return;int t=playStart+(int)clock.ElapsedMilliseconds;if(t>=timeline.Maximum){if(loop.Checked){clock.Restart();playStart=0;t=0;audioPending=audioFile is not null;}else{Stop();t=timeline.Maximum;}}timeline.Value=t;preview.TimeMs=t;preview.Invalidate();}
    private void Stop(){playing=false;audioPending=false;clock.Stop();audio.Stop();play.Text="▶ 再生";}
    private void ChooseHost(){using var d=new FolderBrowserDialog{Description="HOSTのdataフォルダーを指定"};if(d.ShowDialog(this)==DialogResult.OK){hostFolder.Text=d.SelectedPath;mirror.Checked=true;}}
    private void ValidateSave(string gameplay,string? effects)
    {
        Native.Validate(gameplay,effects);ValidateIcons();if(effects is null||doc is null)return;
        var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);var rows=doc.Effects["weapons"]?.AsArray().OfType<JsonObject>().ToList()??[];if(doc.Effects["defaults"] is JsonObject common)rows.Add(common);
        foreach(var row in rows)
        {
            if(row["particles"] is JsonObject particles)foreach(var channelPair in particles)foreach(var emitter in channelPair.Value!.AsArray().OfType<JsonObject>())
            {
                if(emitter["enabled"]?.GetValue<bool>()==false||emitter["texture"] is not JsonValue value)continue;
                if(value.TryGetValue<string>(out var relative)){if(seen.Add(relative)){using var image=Native.Image(Paths.Resolve(doc.Root,relative));}}
                else{uint key=(uint)Fields.Number(value);if(!preview.OriginalImages.ContainsKey(key))throw new InvalidDataException($"原画像 {key:X6} がdataのGWFXに見つかりません。");}
            }
            if(row["sounds"] is JsonObject sounds)foreach(var soundPair in sounds)if(soundPair.Value is JsonObject setting&&setting["enabled"]?.GetValue<bool>()!=false&&setting["path"]?.GetValue<string>() is {Length:>0} relative&&seen.Add(relative))Native.Wave(Paths.Resolve(doc.Root,relative));
        }
    }
    private bool Save(){if(doc is null)return false;Stop();try{doc.Save(ValidateSave,mirror.Checked?hostFolder.Text:null);RefreshFields();status.Text="保存しました。元の設定は .bak に保持。ゲーム/HOSTを再起動して反映してください。";return true;}catch(Exception ex){status.Text="保存していません: "+ex.Message;if(!verifying)MessageBox.Show(this,ex.Message,"保存できません",MessageBoxButtons.OK,MessageBoxIcon.Warning);return false;}}
}
