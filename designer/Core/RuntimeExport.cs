using System.Security.Cryptography;
namespace MgoUi;

public static class RuntimeExport
{
    public static int Export(string file, LayoutDocument document, string assetRoot)
    {
        var clone = Serialization.Clone(document); if (clone.Elements.Any(e => e.Kind == "image" && e.Texture.Length == 0)) throw new InvalidDataException("画像にテクスチャを指定してください。"); var output = Path.GetDirectoryName(Path.GetFullPath(file))!; var assets = Path.GetFileNameWithoutExtension(file) + ".assets";
        // Validate all inputs before producing any runtime JSON. No incomplete manifest is published.
        var copies = new Dictionary<string, (string source, string target)>();
        foreach (var e in clone.Elements.Where(x => x.Texture.Length > 0))
        {
            var source = LayoutRules.AssetPath(assetRoot, e.Texture); var info = new FileInfo(source); if (!info.Exists) throw new FileNotFoundException("素材がありません: " + e.Texture, source); if (info.Length > 64 * 1024 * 1024) throw new InvalidDataException("素材が64 MiBを超えています。"); var ext = Path.GetExtension(source).ToLowerInvariant(); if (!ImageImport.Extensions.Contains(ext)) throw new InvalidDataException("対応しない素材形式です: " + ext);
            var hash = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(source))).ToLowerInvariant(); var relative = assets + "/" + hash + ext; copies.TryAdd(relative, (source, Path.Combine(output, relative))); e.Texture = relative;
        }
        foreach (var action in clone.Elements.SelectMany(e => e.OnClick.Concat(e.OnChange)).Where(a => a.Type == "playSound"))
        {
            var source = LayoutRules.AssetPath(assetRoot, action.Sound!); var info = new FileInfo(source);
            if (!info.Exists) throw new FileNotFoundException("音声がありません: " + action.Sound, source);
            if (info.Length <= 0 || info.Length > 64 * 1024 * 1024) throw new InvalidDataException("音声が空、または64 MiBを超えています。");
            var ext = Path.GetExtension(source).ToLowerInvariant(); if (!AudioRules.Extensions.Contains(ext)) throw new InvalidDataException("音声は WAV / MP3 を指定してください。");
            var hash = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(source))).ToLowerInvariant(); var relative = assets + "/" + hash + ext;
            copies.TryAdd(relative, (source, Path.Combine(output, relative))); action.Sound = relative;
        }
        var finalJson=Serialization.Encode(clone);
        foreach (var (_, pair) in copies) { Directory.CreateDirectory(Path.GetDirectoryName(pair.target)!); if (!Path.GetFullPath(pair.source).Equals(Path.GetFullPath(pair.target), StringComparison.OrdinalIgnoreCase)) { var tmp = pair.target + "." + Guid.NewGuid().ToString("N") + ".tmp"; try { File.Copy(pair.source, tmp); File.Move(tmp, pair.target, true); } finally { if (File.Exists(tmp)) File.Delete(tmp); } } }
        Serialization.AtomicWrite(file, finalJson); return copies.Count;
    }
}
