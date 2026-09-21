using System.Security.Cryptography;
namespace MgoUi;

public static class ImageImport
{
    public static readonly string[] Extensions = [".png", ".jpg", ".jpeg", ".bmp", ".gif", ".tif", ".tiff", ".ico", ".dds"];

    // A lossless file import: validation/decode is handled by the shared renderer
    // before this operation. No image encoder is called on this default path.
    public static string CopyOriginal(string input, string assetRoot)
    {
        var extension = Path.GetExtension(input).ToLowerInvariant();
        if (!Extensions.Contains(extension)) throw new InvalidDataException("対応しない画像形式です: " + extension);
        var info = new FileInfo(input);
        if (!info.Exists || info.Length <= 0 || info.Length > 64 * 1024 * 1024) throw new InvalidDataException("画像がない、空、または64 MiBを超えています。");
        var bytes = File.ReadAllBytes(input);
        var directory = Path.Combine(assetRoot, "designer_textures");
        Directory.CreateDirectory(directory);
        var stem = Path.GetFileNameWithoutExtension(input);
        var name = stem + extension;
        var path = Path.Combine(directory, name);
        for (int index = 2; File.Exists(path); index++)
        {
            if (SHA256.HashData(File.ReadAllBytes(path)).SequenceEqual(SHA256.HashData(bytes))) return "designer_textures/" + name;
            name = stem + "_" + index + extension;
            path = Path.Combine(directory, name);
        }
        using (var stream = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.None)) stream.Write(bytes);
        return "designer_textures/" + name;
    }
}
