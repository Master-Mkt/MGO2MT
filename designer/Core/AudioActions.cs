using System.Security.Cryptography;
namespace MgoUi;

public static class AudioRules
{
    public static readonly string[] Extensions = [".wav", ".mp3"];
    public static void Validate(UiAction a)
    {
        static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
        static bool Name(string? name) => !string.IsNullOrWhiteSpace(name) && System.Text.Encoding.UTF8.GetByteCount(name) <= 128 && !name.Any(char.IsControl);
        Require(a.Type is "playSound" or "emitEvent" or "setVariable" or "setVisible" or "delay", "不明な動作です。");
        if (a.Type == "delay") { Require(a.Ms is >= 0 and <= 60000 && a.Sound is null && a.Volume is null && a.WaitForEnd is null && a.Name is null && a.Value is null && a.Target is null && a.Visible is null, "待ち時間は0〜60000 msです。他の動作の設定は追加できません。"); return; }
        Require(a.Ms is null, "待ち時間は専用の待つ動作に指定してください。");
        if (a.Type == "playSound")
        {
            Require(!string.IsNullOrWhiteSpace(a.Sound) && Extensions.Contains(Path.GetExtension(a.Sound).ToLowerInvariant()), "音声は WAV または MP3 を指定してください。");
            LayoutRules.ValidateRelative(a.Sound!);
            Require(a.Volume is null || (float.IsFinite(a.Volume.Value) && a.Volume is >= 0 and <= 1), "音量は0〜100%です。");
            Require(a.Name is null && a.Value is null && a.Target is null && a.Visible is null, "音声の動作に他の種類の設定があります。");
        }
        else
        {
            Require(a.Sound is null && a.Volume is null && a.WaitForEnd is null, "音声以外の動作に音声の設定があります。");
            if (a.Type == "setVisible") Require(Name(a.Target) && a.Name is null && a.Value is null, "表示を変える部品を指定してください。");
            else
            {
                Require(Name(a.Name) && a.Target is null && a.Visible is null, "イベントまたは変数の名前を指定してください。");
                if (a.Type == "setVariable") Require(a.Value is not null && System.Text.Encoding.UTF8.GetByteCount(a.Value) <= 4096, "変数の値は4096バイト以内です。");
                else Require(a.Value is null, "イベント通知に変数の値は指定できません。");
            }
        }
    }
    public static string Describe(UiAction a) => a.Type switch
    {
        "playSound" => $"音を再生: {Path.GetFileName(a.Sound)}  ({(a.WaitForEnd ?? true ? "最後まで待つ" : "再生開始後に進む")})",
        "emitEvent" => "イベントを開始: " + a.Name,
        "setVariable" => $"表示する値: {a.Name} = {a.Value}",
        "setVisible" => $"部品を{(a.Visible ?? true ? "表示" : "非表示")}: {a.Target}",
        "delay" => $"待つ: {a.Ms} ms",
        _ => "不明な動作"
    };
}
public static class AudioImport
{
    public static string CopyOriginal(string input, string assetRoot)
    {
        var ext = Path.GetExtension(input).ToLowerInvariant();
        if (!AudioRules.Extensions.Contains(ext)) throw new InvalidDataException("対応する音声は WAV / MP3 です。");
        var info = new FileInfo(input);
        if (!info.Exists || info.Length <= 0 || info.Length > 64 * 1024 * 1024) throw new InvalidDataException("音声がない、空、または64 MiBを超えています。");
        var bytes = File.ReadAllBytes(input);
        var hash = Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();
        var relative = "designer_audio/" + hash + ext;
        var path = LayoutRules.AssetPath(assetRoot, relative);
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        if (!File.Exists(path)) { using var f = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.None); f.Write(bytes); }
        else if (!File.ReadAllBytes(path).SequenceEqual(bytes)) throw new InvalidDataException("既存の音声ファイルと内容が一致しません。");
        return relative;
    }
}
