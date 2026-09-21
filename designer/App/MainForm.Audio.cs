using System.Diagnostics;
using System.Text.Json;
namespace MgoUi;

internal sealed partial class MainForm
{
    private readonly CheckBox audioPreview = new() { Text = "動作を試す（クリックで再生）", AutoSize = true, Margin = new(10, 6, 10, 0) };
    private readonly ListBox actionTrace = new() { Dock = DockStyle.Fill, IntegralHeight = false, Font = new("Yu Gothic UI", 9) };
    private readonly System.Windows.Forms.Timer audioTimer = new() { Interval = 40 };
    private readonly List<ActionNotice> actionNotices = [];
    private bool audioVerification;
    private void InitializeAudio()
    {
        var panel = new Panel { Dock = DockStyle.Bottom, Height = 138, Padding = new(12, 4, 12, 4), BackColor = PanelBg };
        var bar = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 37, WrapContents = false };
        var options = new Button { Text = "選択項目…", Width = 100, Height = 29 }; options.Click += (_, _) => EditOptions();
        var edit = new Button { Text = "音・動作を設定…", Width = 150, Height = 29 };
        var play = new Button { Text = "選択した部品を試す", Width = 170, Height = 29 };
        var stop = new Button { Text = "停止・表示を戻す", Width = 150, Height = 29 };
        edit.Click += (_, _) => EditActions(); play.Click += (_, _) => StartSelectedActions(); stop.Click += (_, _) => { renderer.Cancel(reset: true); CollectActions(); canvas.RefreshPreview(); };
        bar.Controls.Add(edit); bar.Controls.Add(options); bar.Controls.Add(audioPreview); bar.Controls.Add(play); bar.Controls.Add(stop);
        panel.Controls.Add(actionTrace); panel.Controls.Add(bar); Controls.Add(panel); Controls.SetChildIndex(panel, 1);
        actionTrace.BackColor = Bg; actionTrace.ForeColor = Fg;
        audioPreview.CheckedChanged += (_, _) => { canvas.PreviewMode = audioPreview.Checked; if (!audioPreview.Checked) { renderer.Cancel(reset: true); CollectActions(); canvas.RefreshPreview(); } };
        canvas.ActionPointer += (point, down) => { if (!audioPreview.Checked) return; renderer.Tick(canvas.Screen, canvas.State, canvas.Flags, bindings, true); renderer.Pointer(point, down); CollectActions(); canvas.RefreshPreview(); };
        canvas.ActionKey += key => { if (!audioPreview.Checked) return false; renderer.Tick(canvas.Screen,canvas.State,canvas.Flags,bindings,true); var handled=renderer.Key(key); CollectActions(); canvas.RefreshPreview(); return handled; };
        audioTimer.Tick += (_, _) => { if (!renderer.Available) return; try { if (audioPreview.Checked) { renderer.Tick(canvas.Screen, canvas.State, canvas.Flags, bindings, audioVerification || ContainsFocus, !audioVerification); if (CollectActions()) canvas.RefreshPreview(); } } catch (Exception ex) { audioTimer.Stop(); status.Text = "音声機能を利用できません: " + ex.Message; } };
        audioTimer.Start(); Deactivate += (_, _) => { if (!audioVerification) { renderer.Cancel(); CollectActions(); } };
        FormClosing += (_, _) => renderer.Cancel(); FormClosed += (_, _) => audioTimer.Dispose();
        Shown += async (_, _) => { var i = Array.IndexOf(arguments, "--verify-audio"); if (i >= 0 && i + 1 < arguments.Length) await VerifyAudio(Path.GetFullPath(arguments[i + 1])); };
    }
    private bool CollectActions()
    {
        var events = renderer.Events(); foreach (var e in events)
        {
            actionNotices.Add(e); if (actionNotices.Count > 512) actionNotices.RemoveAt(0);
            var label = e.Kind switch { "started" => "開始", "soundRequested" => "音を準備", "delay" => "待機", "selection" => "選択変更", "soundStarted" => "再生開始", "soundCompleted" => "音の再生終了", "event" => "イベント開始", "variable" => "表示の値", "visibility" => "表示切替", "completed" => "動作完了", "cancelled" => "中止", "failed" => "失敗", _ => e.Kind };
            actionTrace.Items.Add($"{DateTime.Now:HH:mm:ss.fff}  {label}  {e.Element}  {e.Name} {e.Value} {e.Message}"); if (actionTrace.Items.Count > 128) actionTrace.Items.RemoveAt(0); actionTrace.TopIndex = actionTrace.Items.Count - 1;
        }
        return events.Count > 0;
    }
    private void EditActions()
    {
        if (canvas.Selected is null) { MessageBox.Show("音を鳴らしたい部品を選択してください。パネル・画像・テキストのどれでもボタンにできます。", "音・動作"); return; }
        renderer.Cancel(); CollectActions();
        if (projectFile is null && assetRoot == AppContext.BaseDirectory) { if (!Save(true)) return; assetRoot = Path.Combine(Path.GetDirectoryName(projectFile!)!, Path.GetFileNameWithoutExtension(projectFile) + ".assets"); Directory.CreateDirectory(assetRoot); }
        using var editor = new ActionEditor(document, canvas.Selected, assetRoot); if (editor.ShowDialog(this) != DialogResult.OK) return;
        if (canvas.Selected.Kind == "dropdown") canvas.Selected.OnChange = editor.Result; else canvas.Selected.OnClick = editor.Result; Commit(); status.Text = "音・動作を設定しました。「動作を試す」で部品をクリックしてください。";
    }
    private void EditOptions()
    {
        if (canvas.Selected?.Kind != "dropdown") { MessageBox.Show("プルダウン部品を選択してください。", "選択項目"); return; }
        renderer.Cancel(); using var editor = new DropdownEditor(document, canvas.Selected);
        if (editor.ShowDialog(this) != DialogResult.OK) return;
        canvas.Selected.Options = editor.Options; canvas.Selected.Selected = editor.Selected; Commit();
    }
    private void StartSelectedActions()
    {
        if (canvas.Selected is null || (canvas.Selected.Kind == "dropdown" ? canvas.Selected.OnChange.Count : canvas.Selected.OnClick.Count) == 0) { MessageBox.Show("部品を選んで「音・動作を設定」から動作を追加してください。", "動作を試す"); return; }
        if (!renderer.Load(document, assetRoot, strict: true)) { MessageBox.Show(renderer.Error, "動作を開始できません"); return; }
        audioPreview.Checked = true; renderer.Tick(canvas.Screen, canvas.State, canvas.Flags, bindings, true); renderer.Trigger(canvas.Selected.Id); CollectActions(); canvas.RefreshPreview(); canvas.Focus(); if(canvas.Selected.Kind=="dropdown")status.Text="項目を選択すると、変更時の音・動作を試せます。";
    }
    private static void Tone(string file)
    {
        const int rate = 44100, samples = 22050; using var writer = new BinaryWriter(File.Create(file));
        writer.Write("RIFF"u8); writer.Write(36 + samples * 2); writer.Write("WAVEfmt "u8); writer.Write(16); writer.Write((short)1); writer.Write((short)1); writer.Write(rate); writer.Write(rate * 2); writer.Write((short)2); writer.Write((short)16); writer.Write("data"u8); writer.Write(samples * 2);
        for (int i = 0; i < samples; i++) writer.Write((short)(Math.Sin(i * Math.Tau * 660 / rate) * 2000 * Math.Min(1, Math.Min(i / 500.0, (samples - i) / 500.0))));
    }
    private async Task VerifyAudio(string folder)
    {
        Directory.CreateDirectory(folder); var checks = 0; void Check(bool b, string label) { checks++; if (!b) throw new InvalidOperationException(label); }
        async Task Until(Func<bool> done) { var watch = Stopwatch.StartNew(); while (!done() && watch.ElapsedMilliseconds < 15000) { await Task.Delay(20); CollectActions(); } Check(done(), "audio operation timed out"); }
        try
        {
            audioVerification = true; assetRoot = folder; Tone(Path.Combine(folder, "confirmation.wav"));
            var mp3Index=Array.IndexOf(arguments,"--audio-mp3"); var mp3=mp3Index>=0 && mp3Index+1<arguments.Length ? arguments[mp3Index+1] : Path.Combine(AppContext.BaseDirectory,"samples","audio","confirmation.mp3");
            Check(File.Exists(mp3),"authored MP3 fixture required"); File.Copy(mp3,Path.Combine(folder,"confirmation.mp3"),true);
            document = new() { Elements = [
                new() { Id="backdrop", Kind="panel", X=60,Y=130,Width=1160,Height=480,Color=[20,40,50,255] },
                new() { Id="heading", Kind="text",Text="BUTTON  /  SOUND  /  EVENT",X=110,Y=170,Width=1020,Height=50,FontSize=36,Color=[112,220,205,255],Z=1 },
                new() { Id="sound_button",Kind="button",Text="音を鳴らしてから開始",X=110,Y=260,Width=570,Height=78,FontSize=32,Color=[39,91,106,255],HoverColor=[49,124,140,255],PressedColor=[37,170,148,255],TextColor=[242,250,250,255],ShadowColor=[0,0,0,140],ShadowX=5,ShadowY=6,DelayMs=250,Z=2,
                    OnClick=[new() { Type="playSound",Sound="confirmation.wav",WaitForEnd=true },new() { Type="delay",Ms=120 },new() { Type="setVariable",Name="message",Value="再生終了を確認 → イベント開始" },new() { Type="setVisible",Target="after",Visible=true },new() { Type="emitEvent",Name="title.start" }] },
                new() { Id="weather",Kind="dropdown",X=730,Y=260,Width=400,Height=78,FontSize=30,Color=[43,63,82,255],HoverColor=[62,88,109,255],PressedColor=[45,128,145,255],TextColor=[240,245,255,255],ShadowColor=[0,0,0,120],ShadowX=5,ShadowY=6,DelayMs=150,Z=3,Bind="weather_choice",Selected="clear",Options=[new(){Value="clear",Label="晴れ"},new(){Value="rain",Label="雨"},new(){Value="snow",Label="雪"}],OnChange=[new(){Type="playSound",Sound="confirmation.mp3",WaitForEnd=true},new(){Type="emitEvent",Name="weather_changed"}] },
                new() { Id="after",Kind="text",Text="待機中",Bind="message",X=110,Y=395,Width=1020,Height=65,FontSize=32,Visible=false,Color=[130,240,204,255],Z=1 },
                new() { Id="hint",Kind="text",Text="Delay 250 ms → WAV終了 → 120 ms → title.start\nプルダウン変更 → MP3終了 → 選択値をイベントへ",X=110,Y=490,Width=1020,Height=85,FontSize=24,Color=[173,195,207,255],Z=1 }
            ] };
            var button=document.Elements.Single(e=>e.Id=="sound_button");
            history.Reset(document); screens.SelectedItem="title"; states.Text="idle"; canvas.Selected=button; savedSnapshot=CurrentSnapshot; RefreshAll();
            Check(renderer.Load(document,assetRoot,true),renderer.Error); audioPreview.Checked=true; renderer.Tick("title","idle",[],"{}",true,false); actionNotices.Clear(); renderer.Events();
            var began=Stopwatch.StartNew(); canvas.ExerciseDrag(new(300,290),new(300,290)); CollectActions(); Check(renderer.Busy,"actual WinForms button release accepted"); Check(!actionNotices.Any(e=>e.Kind=="soundRequested"||e.Kind=="event"),"element delay runs before sound/function");
            await Until(()=>actionNotices.Any(e=>e.Kind=="event")); Check(began.ElapsedMilliseconds>=800,"element delay, sound and action delay honored"); Check(actionNotices.FindIndex(e=>e.Kind=="soundCompleted")<actionNotices.FindIndex(e=>e.Kind=="event"),"event waits for actual playback-end callback"); Check(!renderer.Busy,"sequence completes");
            var complete=actionNotices.ToArray(); CapturePreview(Path.Combine(folder,"after-sound.png"));
            using(var bmp=new Bitmap(Width,Height)){ Refresh(); DrawToBitmap(bmp,new(0,0,Width,Height)); bmp.Save(Path.Combine(folder,"designer-audio.png"),System.Drawing.Imaging.ImageFormat.Png); }
            using(var editor=new ActionEditor(document,button,assetRoot)){ editor.Show(this); editor.Refresh(); using var bmp=new Bitmap(editor.Width,editor.Height); editor.DrawToBitmap(bmp,new(0,0,editor.Width,editor.Height)); bmp.Save(Path.Combine(folder,"action-editor.png"),System.Drawing.Imaging.ImageFormat.Png); editor.Close(); }
            actionNotices.Clear(); canvas.ExerciseDrag(new(880,290),new(880,290)); CollectActions(); CapturePreview(Path.Combine(folder,"dropdown-open.png"));
            canvas.ExerciseDrag(new(880,455),new(880,455)); CollectActions(); await Until(()=>actionNotices.Any(e=>e.Kind=="event"&&e.Name=="weather_changed"));
            Check(actionNotices.Any(e=>e.Kind=="event"&&e.Value=="rain"),"dropdown selection value reaches event after MP3 playback"); Check(actionNotices.FindIndex(e=>e.Kind=="soundCompleted")<actionNotices.FindIndex(e=>e.Kind=="event"),"MP3 end precedes selection event"); var selection=actionNotices.ToArray();
            actionNotices.Clear(); renderer.Trigger("sound_button"); await Until(()=>actionNotices.Any(e=>e.Kind=="soundStarted")); renderer.Cancel(); CollectActions(); await Task.Delay(700); renderer.Tick("title","idle",[],"{}",true,false); CollectActions(); Check(!actionNotices.Any(e=>e.Kind=="event")&&!renderer.Busy,"cancel cannot run later event");
            File.WriteAllBytes(Path.Combine(folder,"broken.wav"),[1,2,3,4]); button.OnClick[0].Sound="broken.wav"; Check(renderer.Load(document,assetRoot,true),"corrupt media path loads; decoder owns failure"); actionNotices.Clear(); renderer.Tick("title","idle",[],"{}",true,false); renderer.Trigger("sound_button"); await Until(()=>actionNotices.Any(e=>e.Kind=="failed")); Check(!actionNotices.Any(e=>e.Kind=="event"),"failed decoder cannot start event");
            button.OnClick[0].Sound="confirmation.wav"; var imported=AudioImport.CopyOriginal(Path.Combine(folder,"confirmation.wav"),folder); Check(File.ReadAllBytes(LayoutRules.AssetPath(folder,imported)).SequenceEqual(File.ReadAllBytes(Path.Combine(folder,"confirmation.wav"))),"audio import preserves bytes"); button.OnClick[0].Sound=imported;
            var output=Path.Combine(folder,"export","layout.json"); Check(RuntimeExport.Export(output,document,folder)==2,"WAV and MP3 onClick/onChange export"); var exported=Serialization.ReadProject(output); Check(renderer.Load(exported.Layout,exported.AssetRoot,true),"exported audio controls JSON native reload");
            Serialization.WriteProject(Path.Combine(folder,"audio-example.mgoui"),new(){AssetRoot=folder,Layout=document}); File.WriteAllText(Path.Combine(folder,"audio-tests.json"),JsonSerializer.Serialize(new{status="pass",checks,actualMediaFoundationPlaybackEnd=true,actualWinFormsMouseEvents=true,runtimeDirectory=System.Runtime.InteropServices.RuntimeEnvironment.GetRuntimeDirectory(),playbackMuted=true,verifiedFormats=new[]{"wav","mp3"},successfulSequence=complete,selectionSequence=selection},new JsonSerializerOptions{WriteIndented=true})); Environment.ExitCode=0;
        }
        catch(Exception ex){ File.WriteAllText(Path.Combine(folder,"audio-error.txt"),ex.ToString()); Environment.ExitCode=1; }
        finally { renderer.Cancel(); audioVerification=false; closingVerification=true; Close(); }
    }
}






