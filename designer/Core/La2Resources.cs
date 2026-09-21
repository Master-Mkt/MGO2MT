using System.Buffers.Binary;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text;

namespace MgoUi;

// Full source atlas and its TXN rectangle. UVs in the LA2 remain independent.
internal sealed record La2Texture(int Width, int Height, int X, int Y,
    int TextureWidth, int TextureHeight, byte[] Rgba, string Source)
{
    public float UvScaleX { get; init; } = 1;
    public float UvScaleY { get; init; } = 1;
    public float UvOffsetX { get; init; }
    public float UvOffsetY { get; init; }
}

// Reads already extracted local resource packages. Never runs GCX, downloads
// assets, guesses a texture by its name, or changes the original files.
internal sealed class La2Resources
{
    private const int MaxSource = 128 * 1024 * 1024;
    private const int MaxPool = 512 * 1024 * 1024;
    private readonly List<string> warnings;
    private readonly HashSet<string> warningSet = [];
    private readonly List<Texture> textures = [];
    private readonly List<Part> parts = [];
    private readonly List<Remap> remaps = [];
    private readonly Dictionary<uint, La2Texture?> resolved = [];
    private readonly Dictionary<string, byte[]> pixels = [];
    private long retainedBytes, pixelBytes;
    private sealed record Texture(uint Key, uint Archive, int ImageIndex, int Width, int Height,
        int X, int Y, int TextureWidth, int TextureHeight, int Codec, int Flags,
        int DataOffset, byte[] Data, string Source, int Layer, float UvScaleX, float UvScaleY, float UvOffsetX, float UvOffsetY);
    private sealed record Part(uint Archive, uint Index, int Priority, int Offset,
        int Size, byte[] Data, string Source, string Family, int Layer);
    private sealed record Remap(uint Archive, int Priority, Dictionary<uint, uint> Entries,
        string Source, string Family, int Layer);

    public La2Resources(string inputLa2, string? searchRoot, List<string> warnings)
    {
        this.warnings = warnings;
        var inputFolder = Path.GetDirectoryName(Path.GetFullPath(inputLa2))!;
        var roots = new List<(string Path, int Depth)> { (inputFolder, 2) };
        if (Path.GetFileName(inputFolder).Equals("layouts", StringComparison.OrdinalIgnoreCase)
            || Path.GetFileName(inputFolder).Equals("cache_files", StringComparison.OrdinalIgnoreCase))
            roots.Add((Path.GetDirectoryName(inputFolder)!, 5));
        if (!string.IsNullOrWhiteSpace(searchRoot)) roots.Add((Path.GetFullPath(searchRoot), 5));
        var files = Discover(roots);
        foreach (var path in files)
        {
            try
            {
                var info = new FileInfo(path);
                Require(info.Length > 0 && info.Length <= MaxSource, "資源ファイルは1 byte〜128 MiBが必要です。");
                var data = File.ReadAllBytes(path);
                switch (Path.GetExtension(path).ToLowerInvariant())
                {
                    case ".txn": ParseTxn(data, path, Layer(path)); break;
                    case ".qar": ParseQar(data, path); break;
                    case ".dld": ParseDld(data, path); break;
                    case ".dlz": ParseDld(InflateDlz(data), path); break;
                    case ".dci": ParseDci(data, path); break;
                }
            }
            catch (Exception error) when (error is InvalidDataException or IOException or UnauthorizedAccessException or OverflowException or ArgumentException)
            {
                Warn("資源を読み込めません: " + path + " — " + error.Message);
            }
        }
    }

    public La2Texture? Resolve(uint textureKey, uint archiveHint)
    {
        if (resolved.TryGetValue(textureKey, out var cached)) return cached;
        // PPU1106E8 searches the other loaded TXN tables by the exact texture
        // key after an archive miss. Its historical archive hint is not an ID
        // that can safely be replaced with another similarly named texture.
        var references = textures.Where(t => t.Key == textureKey)
            .OrderByDescending(t => t.Layer).ThenByDescending(t => t.Archive == archiveHint)
            .ThenBy(t => t.Source, StringComparer.OrdinalIgnoreCase).ToList();
        foreach (var layer in references.GroupBy(t => t.Layer))
        {
            var candidates = new List<(Texture Texture, byte[] Payload, string Source)>();
            foreach (var texture in layer)
            {
                try
                {
                    var size = checked(((texture.Width + 3) / 4) * ((texture.Height + 3) / 4)
                        * (texture.Codec == 9 ? 8 : 16));
                    if (texture.Flags >> 4 != 15)
                    {
                        candidates.Add((texture, Slice(texture.Data, texture.DataOffset, size).ToArray(), texture.Source + " (inline)"));
                        continue;
                    }
                    var matches = new List<Part>();
                    foreach (var part in parts.Where(p => p.Archive == texture.Archive && p.Layer <= texture.Layer))
                    {
                        var index = part.Index;
                        if (texture.Layer > 0)
                        {
                            var maps = remaps.Where(m => m.Layer == texture.Layer && m.Family == part.Family
                                && m.Archive == part.Archive && m.Priority == part.Priority).ToList();
                            if (maps.Count > 0)
                            {
                                var mapped = maps.Select(m => m.Entries.TryGetValue(part.Index, out var value) ? value : 65535u).Distinct().ToArray();
                                Require(mapped.Length == 1, "異なるDCIの対応表が競合しています。");
                                index = mapped[0];
                            }
                        }
                        if (index == texture.ImageIndex && part.Size >= size) matches.Add(part);
                    }
                    if (matches.Count == 0) continue;
                    var bestLayer = matches.Max(p => p.Layer);
                    var bestPriority = matches.Where(p => p.Layer == bestLayer).Min(p => p.Priority);
                    matches = matches.Where(p => p.Layer == bestLayer && p.Priority == bestPriority).ToList();
                    var payload = Slice(matches[0].Data, matches[0].Offset, size).ToArray();
                    Require(matches.All(p => Slice(p.Data, p.Offset, size).SequenceEqual(payload)), "同優先度のDLD画像が競合しています。");
                    candidates.Add((texture, payload, texture.Source + " → " + matches[0].Source
                        + $" [archive={texture.Archive:x6}, image={texture.ImageIndex}, offset={matches[0].Offset}]") );
                }
                catch (Exception error) when (error is InvalidDataException or IOException or OverflowException or ArgumentException)
                {
                    Warn($"画像{textureKey:x6}の対応を拒否しました: " + error.Message);
                }
            }
            if (candidates.Count == 0) continue;
            var first = candidates[0];
            if (candidates.Any(c => c.Texture.Width != first.Texture.Width || c.Texture.Height != first.Texture.Height
                || c.Texture.Codec != first.Texture.Codec || c.Texture.X != first.Texture.X || c.Texture.Y != first.Texture.Y
                || c.Texture.TextureWidth != first.Texture.TextureWidth || c.Texture.TextureHeight != first.Texture.TextureHeight
                || !c.Payload.AsSpan().SequenceEqual(first.Payload)))
            {
                Warn($"画像{textureKey:x6}は選択フォルダー内の複数のTXNで競合しています。対応するステージの資源だけを指定してください。");
                return resolved[textureKey] = null;
            }
            var t = first.Texture;
            var identity = $"{t.Width}:{t.Height}:{t.Codec}:" + Convert.ToHexString(SHA256.HashData(first.Payload));
            if (!pixels.TryGetValue(identity, out var rgba))
            {
                long extent = (long)t.Width * t.Height * 4;
                if (pixelBytes + extent > MaxSource)
                {
                    Warn("LA2画像の展開合計が128 MiBを超えます。");
                    return resolved[textureKey] = null;
                }
                rgba = DecodeBlocks(first.Payload, t.Width, t.Height, t.Codec);
                pixels.Add(identity, rgba); pixelBytes += extent;
            }
            if (t.Archive != archiveHint)
                Warn($"画像{textureKey:x6}: LA2の旧アーカイブ{archiveHint:x6}から、完全一致する画像キーでTXN {t.Archive:x6}を解決しました。");
            return resolved[textureKey] = new(t.Width, t.Height, t.X, t.Y, t.TextureWidth, t.TextureHeight, rgba, first.Source)
                { UvScaleX = t.UvScaleX, UvScaleY = t.UvScaleY, UvOffsetX = t.UvOffsetX, UvOffsetY = t.UvOffsetY };
        }
        return resolved[textureKey] = null;
    }

    private void ParseTxn(byte[] data, string source, int layer)
    {
        Slice(data, 0, 32); int imageCount = Int(data, 8), imageAt = Int(data, 12), count = Int(data, 16), at = Int(data, 20);
        Require(imageCount <= 4096 && count <= 65536 && textures.Count + count <= 131072, "TXN件数の上限です。");
        Slice(data, imageAt, checked(imageCount * 16)); Slice(data, at, checked(count * 48));
        var pending = new List<Texture>();
        for (int n = 0; n < count; ++n)
        {
            int row = at + n * 48, reference = Int(data, row + 20);
            Require(reference >= imageAt && (reference - imageAt) % 16 == 0 && (reference - imageAt) / 16 < imageCount, "TXN画像参照範囲が不正です。");
            int width = U16(data, reference), height = U16(data, reference + 2), codec = U16(data, reference + 4);
            if (codec != 9 && codec != 11) { Warn(source + $": codec{codec}の画像は未対応です。"); continue; }
            Require(width is > 0 and <= 8192 && height is > 0 and <= 8192 && (long)width * height * 4 <= MaxSource, "TXN画像の寸法上限です。");
            int x = U16(data, row + 16), y = U16(data, row + 18), tw = U16(data, row + 12), th = U16(data, row + 14);
            Require(tw > 0 && th > 0 && x + tw <= width && y + th <= height, "TXNアトラスの矩形が不正です。");
            float us = Float(data, row + 28), vs = Float(data, row + 32), uo = Float(data, row + 36), vo = Float(data, row + 40);
            Require(Math.Abs(us) <= 16 && Math.Abs(vs) <= 16 && Math.Abs(uo) <= 16 && Math.Abs(vo) <= 16, "TXN UV値の範囲が不正です。");
            pending.Add(new(U32(data, row + 4), U32(data, row + 8), (reference - imageAt) / 16,
                width, height, x, y, tw, th, codec, U16(data, reference + 6), Int(data, reference + 8), data, source, layer, us, vs, uo, vo));
        }
        Retain(data.Length); textures.AddRange(pending);
    }

    private void ParseQar(byte[] data, string source)
    {
        int table = Int(data, data.Length - 4), count = U16(data, table), nameAt = checked(table + 4 + count * 8), at = 0;
        Require(count <= 10000 && nameAt <= data.Length - 4, "QARテーブル範囲が不正です。");
        var members = new List<(string Name, int Offset, int Size)>();
        for (int n = 0; n < count; ++n)
        {
            int size = Int(data, table + 8 + n * 8), end = nameAt;
            while (end < data.Length - 4 && end - nameAt < 256 && data[end] != 0) ++end;
            Require(end < data.Length - 4 && end - nameAt is > 0 and < 256 && data[end] == 0, "QARの名前が不正です。");
            var name = Encoding.ASCII.GetString(data, nameAt, end - nameAt);
            Require(name.All(c => c is >= ' ' and <= '~' && c != '/' && c != '\\' && c != ':'), "QARの名前に未対応文字があります。");
            Require((long)at + size <= table, "QARデータがテーブルと重複しています。");
            if (name.EndsWith(".txn", StringComparison.OrdinalIgnoreCase)) members.Add((name, at, size));
            nameAt = end + 1; at = checked((at + size + 127) & ~127);
        }
        Require(at == table && Slice(data, nameAt, data.Length - 4 - nameAt).IndexOfAnyExcept((byte)0) < 0, "QAR終端が不正です。");
        foreach (var member in members)
        {
            try { ParseTxn(Slice(data, member.Offset, member.Size).ToArray(), source + "::" + member.Name, Layer(source)); }
            catch (Exception error) when (error is InvalidDataException or IOException or OverflowException or ArgumentException) { Warn(source + "::" + member.Name + " — " + error.Message); }
        }
    }

    private void ParseDld(byte[] data, string source)
    {
        int at = 0; var pending = new List<Part>();
        while (at < data.Length)
        {
            if (Slice(data, at, Math.Min(32, data.Length - at)).IndexOfAnyExcept((byte)0) < 0
                && Slice(data, at, data.Length - at).IndexOfAnyExcept((byte)0) < 0) break;
            Slice(data, at, 32); int size = Int(data, at + 16), parent = Int(data, at + 12);
            Slice(data, at + 32, size);
            Require(parts.Count + pending.Count < 131072, "DLD画像件数の上限です。");
            if (parent == 0) pending.Add(new(U32(data, at + 8), U32(data, at + 24), data[at + 1], at + 32, size, data, source, Family(source), Layer(source)));
            at = checked((at + 32 + size + 15) & ~15); Require(at <= data.Length, "DLD終端のアラインメントがありません。");
        }
        Retain(data.Length); parts.AddRange(pending);
    }

    private void ParseDci(byte[] data, string source)
    {
        Slice(data, 0, 16); int count = Int(data, 8);
        Require(U32(data, 0) != 0 && U32(data, 4) == 4096 && U32(data, 12) == 0 && count <= 4096, "DCIヘッダーが不正です。");
        Slice(data, 16, checked(count * 12)); var pending = new List<Remap>();
        for (int n = 0; n < count; ++n)
        {
            uint archive = U32(data, 16 + n * 12), flags = U32(data, 24 + n * 12); int offset = Int(data, 20 + n * 12), length = (int)(flags & 65535), priority = (int)(flags >> 16);
            if (length == 0) continue;
            Require(priority is 0 or 3 && offset >= 16 + count * 12, "DCI優先度または対応表の範囲が不正です。");
            Require(!pending.Any(p => p.Archive == archive && p.Priority == priority), "DCIに重複するアーカイブがあります。");
            Slice(data, offset, checked(length * 4)); var map = new Dictionary<uint, uint>();
            for (int i = 0; i < length; ++i) Require(map.TryAdd((uint)U16(data, offset + i * 4), (uint)U16(data, offset + i * 4 + 2)), "DCIに重複する旧番号があります。");
            pending.Add(new(archive, priority, map, source, Family(source), Layer(source)));
        }
        remaps.AddRange(pending);
    }

    private static byte[] InflateDlz(byte[] data)
    {
        using var output = new MemoryStream();
        for (int start = 0; start < data.Length; start += 0x20000)
        {
            Require(Slice(data, start, 4).SequenceEqual("segs"u8), "未復号または未対応のDLZです（segsがありません）。");
            int flag = U16(data, start + 4), count = U16(data, start + 6), total = Int(data, start + 8);
            int compressed = checked((int)BinaryPrimitives.ReadUInt32LittleEndian(Slice(data, start + 12, 4)));
            Require(flag == 4 && count <= 4096 && total <= 0x1000000 && output.Length + total <= MaxSource, "DLZ展開サイズの上限です。");
            Require(compressed >= 16 + count * 8 && compressed <= Math.Min(0x20000, data.Length - start), "DLZ圧縮範囲が不正です。");
            long before = output.Length;
            for (int n = 0; n < count; ++n)
            {
                int size = U16(data, start + 16 + n * 8), expected = U16(data, start + 18 + n * 8), offset = Int(data, start + 20 + n * 8) - 1;
                if (size == 0) size = 65536; if (expected == 0) expected = 65536;
                Require(offset >= 16 + count * 8 && (long)offset + size <= compressed && output.Length + expected <= MaxSource, "DLZチャンクの範囲が不正です。");
                using var input = new MemoryStream(data, start + offset, size, false);
                using var inflate = new DeflateStream(input, CompressionMode.Decompress);
                var chunk = new byte[expected]; inflate.ReadExactly(chunk);
                Require(inflate.ReadByte() == -1, "DLZの展開結果が宣言サイズと一致しません。");
                output.Write(chunk);
            }
            Require(output.Length - before == total, "DLZの合計展開サイズが一致しません。");
        }
        return output.ToArray();
    }

    // Standard BC1/BC3; block clipping also handles source sizes below4 pixels.
    private static byte[] DecodeBlocks(byte[] data, int width, int height, int codec)
    {
        var rgba = new byte[checked(width * height * 4)]; int at = 0;
        Span<byte> colors = stackalloc byte[16]; Span<byte> alpha = stackalloc byte[8];
        for (int by = 0; by < height; by += 4) for (int bx = 0; bx < width; bx += 4)
        {
            ulong alphaBits = 0;
            if (codec == 11)
            {
                alpha[0] = data[at]; alpha[1] = data[at + 1];
                if (alpha[0] > alpha[1]) for (int n = 2; n < 8; ++n) alpha[n] = (byte)(((8 - n) * alpha[0] + (n - 1) * alpha[1]) / 7);
                else { for (int n = 2; n < 6; ++n) alpha[n] = (byte)(((6 - n) * alpha[0] + (n - 1) * alpha[1]) / 5); alpha[6] = 0; alpha[7] = 255; }
                for (int n = 0; n < 6; ++n) alphaBits |= (ulong)data[at + 2 + n] << (8 * n);
                at += 8;
            }
            ushort c0 = BinaryPrimitives.ReadUInt16LittleEndian(Slice(data, at, 2)), c1 = BinaryPrimitives.ReadUInt16LittleEndian(Slice(data, at + 2, 2));
            for (int n = 0; n < 2; ++n)
            {
                int value = n == 0 ? c0 : c1, r = value >> 11, g = (value >> 5) & 63, b = value & 31;
                colors[n * 4] = (byte)((r << 3) | (r >> 2)); colors[n * 4 + 1] = (byte)((g << 2) | (g >> 4)); colors[n * 4 + 2] = (byte)((b << 3) | (b >> 2)); colors[n * 4 + 3] = 255;
            }
            if (c0 > c1 || codec == 11)
            {
                for (int k = 0; k < 3; ++k) { colors[8 + k] = (byte)((2 * colors[k] + colors[4 + k]) / 3); colors[12 + k] = (byte)((colors[k] + 2 * colors[4 + k]) / 3); }
                colors[11] = colors[15] = 255;
            }
            else { for (int k = 0; k < 3; ++k) { colors[8 + k] = (byte)((colors[k] + colors[4 + k]) / 2); colors[12 + k] = 0; } colors[11] = 255; colors[15] = 0; }
            uint bits = BinaryPrimitives.ReadUInt32LittleEndian(Slice(data, at + 4, 4)); at += 8;
            for (int p = 0; p < 16; ++p)
            {
                int x = bx + p % 4, y = by + p / 4; if (x >= width || y >= height) continue;
                int source = (int)((bits >> (2 * p)) & 3) * 4, target = (y * width + x) * 4;
                colors.Slice(source, 4).CopyTo(rgba.AsSpan(target, 4));
                if (codec == 11) rgba[target + 3] = alpha[(int)((alphaBits >> (3 * p)) & 7)];
            }
        }
        return rgba;
    }

    private List<string> Discover(List<(string Path, int Depth)> roots)
    {
        var result = new List<string>(); var seen = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        var seenFiles = new HashSet<string>(StringComparer.OrdinalIgnoreCase); int inspected = 0;
        void Visit(string folder, int depth)
        {
            if (seen.Count >= 2048 || inspected >= 16384) { Warn("資源の探索上限です。対象LA2と同じステージのフォルダーを指定してください。"); return; }
            if (seen.TryGetValue(folder, out int priorDepth) && priorDepth >= depth) return;
            seen[folder] = depth;
            try
            {
                if (!Directory.Exists(folder) || (File.GetAttributes(folder) & FileAttributes.ReparsePoint) != 0) return;
                foreach (var path in Directory.EnumerateFileSystemEntries(folder).OrderBy(p => p, StringComparer.OrdinalIgnoreCase))
                {
                    if (++inspected > 16384 || result.Count >= 2048) { Warn("資源の探索上限に達しました。"); break; }
                    var attributes = File.GetAttributes(path); if ((attributes & FileAttributes.ReparsePoint) != 0) continue;
                    if ((attributes & FileAttributes.Directory) != 0)
                    {
                        if (depth > 0 && Path.GetFileName(path) is not ("bin" or "obj" or ".git" or ".codex")) Visit(path, depth - 1);
                    }
                    else if (new[] { ".txn", ".qar", ".dld", ".dlz", ".dci" }.Contains(Path.GetExtension(path).ToLowerInvariant()) && seenFiles.Add(path)) result.Add(path);
                }
            }
            catch (Exception error) when (error is InvalidDataException or IOException or UnauthorizedAccessException) { Warn("資源フォルダーを読めません: " + folder + " — " + error.Message); }
        }
        foreach (var root in roots) Visit(root.Path, root.Depth);
        return result;
    }

    private static int Layer(string source)
    {
        var path = source.Replace('\\', '/').ToLowerInvariant();
        return path.Contains("/layer1/") || path.Contains("/patch_source/") || path.Contains("/dl/p/") ? 1 : 0;
    }
    private static string Family(string source)
    {
        string name = Path.GetFileName(source).ToLowerInvariant();
        foreach (var family in new[] { "resident_nodld", "resident_d", "resident", "cache_nodld", "cache_d", "cache" })
            if (name.StartsWith(family + ".", StringComparison.Ordinal) || name.StartsWith(family + "_", StringComparison.Ordinal)) return family;
        return name.Split('.')[0];
    }
    private void Retain(int bytes) { Require(retainedBytes + bytes <= MaxPool, "資源データ合計が512 MiBを超えます。"); retainedBytes += bytes; }
    private void Warn(string text) { if (warningSet.Add(text) && warnings.Count < 512) warnings.Add(text); }
    private static void Require(bool condition, string error) { if (!condition) throw new InvalidDataException(error); }
    private static ReadOnlySpan<byte> Slice(byte[] data, int offset, int size)
    {
        Require(offset >= 0 && size >= 0 && (long)offset + size <= data.Length, "バイナリデータの参照範囲が不正です。");
        return data.AsSpan(offset, size);
    }
    private static uint U32(byte[] data, int offset) => BinaryPrimitives.ReadUInt32BigEndian(Slice(data, offset, 4));
    private static int Int(byte[] data, int offset) => checked((int)U32(data, offset));
    private static int U16(byte[] data, int offset) => BinaryPrimitives.ReadUInt16BigEndian(Slice(data, offset, 2));
    private static float Float(byte[] data, int offset)
    {
        float value = BitConverter.Int32BitsToSingle(unchecked((int)U32(data, offset)));
        Require(float.IsFinite(value), "TXNの非有限UV値です。"); return value;
    }
}
