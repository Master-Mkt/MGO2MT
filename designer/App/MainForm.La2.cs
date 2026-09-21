namespace MgoUi;

internal sealed partial class MainForm
{
    private async Task VerifyLa2()
    {
        var input=arguments[Array.IndexOf(arguments,"--verify-la2")+1]; var outputIndex=Array.IndexOf(arguments,"--la2-output");
        var folder=Path.GetFullPath(outputIndex>=0 && outputIndex+1<arguments.Length ? arguments[outputIndex+1] : "la2-verification"); Directory.CreateDirectory(folder);
        var rootIndex=Array.IndexOf(arguments,"--la2-search-root"); string? searchRoot=rootIndex>=0 && rootIndex+1<arguments.Length ? arguments[rootIndex+1] : null;
        int checks=0; void Check(bool value,string message){checks++;if(!value)throw new InvalidDataException(message);}
        try
        {
            renderer.Cancel(reset:true); var result=await Task.Run(()=>La2Importer.Import(input,Path.Combine(folder,"assets"),searchRoot));
            document=result.Document; assetRoot=result.AssetRoot; projectFile=Path.Combine(folder,"imported.mgoui"); canvas.Selected=document.Elements.FirstOrDefault(); screens.SelectedItem="title"; states.Text="idle";
            Check(document.Elements.Count>0,"LA2 produced editable elements"); Check(renderer.Load(document,assetRoot,true),renderer.Error);
            var before=renderer.Render(1280,720,"title","idle",[],"{}"); Check(before is not null,"native import render");
            Serialization.WriteProject(projectFile,new(){AssetRoot=assetRoot,Layout=document}); Check(Serialization.Encode(Serialization.ReadProject(projectFile).Layout)==Serialization.Encode(document),"editable project roundtrip");
            var runtimeFile=Path.Combine(folder,"runtime","layout.json"); var copied=RuntimeExport.Export(runtimeFile,document,assetRoot); var exported=Serialization.ReadProject(runtimeFile); Check(renderer.Load(exported.Layout,exported.AssetRoot,true),"native export reload");
            var after=renderer.Render(1280,720,"title","idle",[],"{}"); Check(before!.SequenceEqual(after??[]),"exported layout renders identical pixels");
            Check(renderer.Load(document,assetRoot,true),"restore authoring layout"); history.Reset(document); savedSnapshot=CurrentSnapshot; RefreshAll(); status.Text=$"LA2取込: 原本 {result.SourceNodes} 部品 → 編集可能 {document.Elements.Count} 部品・画像 {result.ResolvedTextures} 枚";
            Refresh(); CapturePreview(Path.Combine(folder,"la2-runtime.png")); using(var bmp=new Bitmap(Width,Height)){DrawToBitmap(bmp,new(0,0,Width,Height));bmp.Save(Path.Combine(folder,"designer-la2.png"),System.Drawing.Imaging.ImageFormat.Png);}
            File.WriteAllText(Path.Combine(folder,"la2-designer-tests.json"),System.Text.Json.JsonSerializer.Serialize(new{status="pass",checks,result.SourceNodes,elements=document.Elements.Count,result.ResolvedTextures,exportedImages=copied,result.Warnings,result.ReportPath,nativePixelRoundtrip=true,runtimeDirectory=System.Runtime.InteropServices.RuntimeEnvironment.GetRuntimeDirectory(),sourceInput=Path.GetFullPath(input)},new System.Text.Json.JsonSerializerOptions{WriteIndented=true})); Environment.ExitCode=0;
        }
        catch(Exception ex){File.WriteAllText(Path.Combine(folder,"la2-error.txt"),ex.ToString());Environment.ExitCode=1;}
        finally{closingVerification=true;Close();}
    }
    private async void ImportLa2()
    {
        if (!ConfirmDiscard()) return;
        using var input=new OpenFileDialog { Title="取り込む原UIを選択",Filter="原UIレイアウト (.la2)|*.la2|すべてのファイル|*.*" };
        if(input.ShowDialog(this)!=DialogResult.OK)return;
        La2Inspection inspection;try{inspection=La2Importer.Inspect(input.FileName);}catch(Exception ex){MessageBox.Show(this,ex.Message,"LA2を読み込めません");return;}
        using var pose=new La2PoseDialog(inspection);if(pose.ShowDialog(this)!=DialogResult.OK)return;var eventHash=pose.EventHash;var initialState=pose.InitialState;
        using var output=new SaveFileDialog { Title="編集用プロジェクトの保存先",Filter="UI編集プロジェクト|*.mgoui",FileName=Path.GetFileNameWithoutExtension(input.FileName)+".mgoui" };
        if(output.ShowDialog(this)!=DialogResult.OK)return;
        string? searchRoot=null; var destination=Path.Combine(Path.GetDirectoryName(output.FileName)!,Path.GetFileNameWithoutExtension(output.FileName)+".assets");
        renderer.Cancel(reset:true);
        while(true)
        {
            try
            {
                UseWaitCursor=true; Enabled=false; var importScreen=screens.Text; var result=await Task.Run(()=>La2Importer.Import(input.FileName,destination,searchRoot,eventHash:eventHash,screen:importScreen,initialState:initialState)); Enabled=true; UseWaitCursor=false;
                if(!renderer.Load(result.Document,result.AssetRoot,true))throw new InvalidDataException(renderer.Error);
                Serialization.WriteProject(output.FileName,new() { AssetRoot=result.AssetRoot,Layout=result.Document });
                document=result.Document; assetRoot=result.AssetRoot; projectFile=Path.GetFullPath(output.FileName); canvas.Selected=null; history.Reset(document); savedSnapshot=CurrentSnapshot; RefreshAll();
                status.Text=$"LA2取込: {result.SourceNodes}部品、画像{result.ResolvedTextures}枚。編集用プロジェクトを保存しました。";
                var warnings=result.Warnings.Count==0 ? "" : "\n\n注意:\n"+string.Join("\n",result.Warnings.Take(8))+(result.Warnings.Count>8?"\n…詳しくは取込レポートを確認してください。":"");
                MessageBox.Show(this,$"原UIを編集可能な部品として取り込みました。\n元のLA2と素材は変更しません。\n\n変形・色・画像を選んだ表示状態へ反映します。\nアニメーションは静止した状態として取り込み、詳細をレポートへ保存します。\n\n取込レポート: {result.ReportPath}"+warnings,"LA2の取込が完了しました",MessageBoxButtons.OK,MessageBoxIcon.Information); return;
            }
            catch(MissingLa2ResourcesException ex)
            {
                Enabled=true; UseWaitCursor=false;
                MessageBox.Show(this,"必要な素材が見つかりません。次の画面で、展開済みの原リソースを含むフォルダーを選んでください。\n\n"+string.Join(", ",ex.MissingTextures.Take(12)),"素材フォルダーを指定してください",MessageBoxButtons.OK,MessageBoxIcon.Information);
                using var folder=new FolderBrowserDialog { Description="原UIで使う画像・TXN・DLDなどの展開先を選択",SelectedPath=searchRoot??Path.GetDirectoryName(input.FileName)! };
                if(folder.ShowDialog(this)!=DialogResult.OK)return; searchRoot=folder.SelectedPath;
            }
            catch(Exception ex) { Enabled=true; UseWaitCursor=false; renderer.Cancel(reset:true); RefreshCanvas(); MessageBox.Show(this,ex.Message,"LA2を取り込めません",MessageBoxButtons.OK,MessageBoxIcon.Warning); return; }
        }
    }
}



