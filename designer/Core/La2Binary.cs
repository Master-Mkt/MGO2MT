using System.Buffers.Binary;
using System.Text;

namespace MgoUi;

// The reviewed LA2 0x10009 structure; offsets and source commands are retained
// in the import report rather than discarded when a command is unsupported.
internal sealed class La2Binary
{
    internal sealed record Command(int Offset, int Opcode, byte[] Data);
    internal sealed record Property(int Offset, int Size, List<Command> Commands);
    internal sealed record Node(uint Name, uint Flags, int Id, int Parent, int Next, int Aux, int Layer, byte[] Extra)
    {
        public int Type => (int)(Flags & 15);
        public int Property { get; set; }
    }
    internal sealed record Pool(int Count, byte[] Data);
    internal sealed record Glyph(int Width, int Height, int U0, int U1, int V0, int V1, string? Character, byte[] CharacterBytes);
    internal sealed record Font(uint Key, List<Glyph> Glyphs);
    internal sealed record Texture(uint Key, uint Archive);
    internal sealed record Track(int Node, int Aux, List<Command> Commands);
    internal sealed record Event(uint Hash, int Flags, List<Track> Tracks);

    public byte[] Bytes { get; }
    public List<Node> Nodes { get; } = [];
    public Dictionary<int, Property> Properties { get; } = [];
    public List<Pool> Colors { get; }
    public List<Pool> Uv { get; }
    public List<Font> Fonts { get; } = [];
    public List<Texture> Textures { get; } = [];
    public List<Event> Events { get; } = [];
    public int[] Sections { get; }

    internal sealed class Reader(byte[] data)
    {
        private long copiedBytes;
        private int records;
        public byte[] Data => data;
        public void Record()
        {
            if (++records > 250000) throw new InvalidDataException("LA2 の構造レコード数が上限を超えています。");
        }
        public void Check(int at, int count)
        {
            if (at < 0 || count < 0 || at > data.Length - count) throw new InvalidDataException("LA2 の範囲外参照です。");
        }
        public ushort U16(int at) { Check(at, 2); return BinaryPrimitives.ReadUInt16BigEndian(data.AsSpan(at, 2)); }
        public short I16(int at) => unchecked((short)U16(at));
        public uint U32(int at) { Check(at, 4); return BinaryPrimitives.ReadUInt32BigEndian(data.AsSpan(at, 4)); }
        public int Offset(int at) { var v = U32(at); if (v > int.MaxValue) throw new InvalidDataException("LA2 オフセットが大きすぎます。"); return (int)v; }
        public byte[] Slice(int at, int count)
        {
            Check(at, count); copiedBytes += count;
            if (copiedBytes > 32L * 1024 * 1024) throw new InvalidDataException("LA2 の参照展開量が上限を超えています。");
            return data.AsSpan(at, count).ToArray();
        }
        public int Count(int at, int limit = 10000) { int n = Offset(at); if (n > limit) throw new InvalidDataException("LA2 の要素数が上限を超えています。"); return n; }
    }

    public La2Binary(byte[] bytes)
    {
        if (bytes.Length > 16 * 1024 * 1024) throw new InvalidDataException("LA2 は16 MiB以内です。");
        Bytes = bytes;
        var r = new Reader(bytes);
        if (r.U32(0) != 0x10009) throw new InvalidDataException("対応する LA2 バージョンは 0x10009 です。");
        Sections = Enumerable.Range(1, 12).Select(i => r.Offset(i * 4)).ToArray();
        foreach (int at in Sections) if (at != 0) r.Check(at, 4);
        int p = Sections[0], nodes = r.U16(p); r.Check(p, 44); p += 44;
        if (nodes > 10000) throw new InvalidDataException("LA2 ノードが多すぎます。");
        for (int i = 0; i < nodes; ++i)
        {
            int id = r.U16(p + 8), parent = r.U16(p + 10), next = r.U16(p + 12), extra = r.U16(p + 16);
            if (id != i + 1 || parent >= id || next > nodes) throw new InvalidDataException("LA2 の親子関係が不正です。");
            Nodes.Add(new(r.U32(p), r.U32(p + 4), id, parent, next, r.U16(p + 14), r.U16(p + 18), r.Slice(p + 20, extra * 4)));
            p += 20 + extra * 4;
        }
        if (p != Sections[1]) throw new InvalidDataException("LA2 ノード領域の末尾が一致しません。");
        int count = r.Count(p, 100000); p += 4;
        if (count < nodes) throw new InvalidDataException("LA2 ノード属性が不足しています。");
        for (int i = 0; i < count; ++i)
        {
            int n = r.U16(p), size = r.U16(p + 2);
            if (size < 4) throw new InvalidDataException("LA2 属性サイズが不正です。");
            r.Check(p, size);
            var commands = Commands(r, p + 4, p + size);
            if (commands.Count != n) throw new InvalidDataException("LA2 属性の命令数が一致しません。");
            Properties.Add(p, new(p, size, commands));
            if (i < nodes) Nodes[i].Property = p;
            p += size;
        }
        if (p != Sections[2]) throw new InvalidDataException("LA2 属性領域の末尾が一致しません。");
        Colors = ReadPool(r, Sections[7]); Uv = ReadPool(r, Sections[8]);
        p = Sections[10]; count = r.Count(p); p += 4;
        for (int i = 0; i < count; ++i)
        {
            uint key = r.U32(p); int n = r.U16(p + 4), size = r.U16(p + 6);
            if (size != 8 + n * 16) throw new InvalidDataException("LA2 フォント領域が不正です。");
            r.Check(p, size); var glyphs = new List<Glyph>();
            for (int j = 0; j < n; ++j)
            {
                int q = p + 8 + j * 16; byte[] raw = r.Slice(q + 12, 4);
                int end = Array.IndexOf(raw, (byte)0); if (end < 0) end = raw.Length;
                string? character = raw.Take(end).All(x => x < 128) ? Encoding.ASCII.GetString(raw, 0, end) : null;
                glyphs.Add(new(r.U16(q), r.U16(q + 2), r.U16(q + 4), r.U16(q + 6), r.U16(q + 8), r.U16(q + 10), character, raw));
            }
            Fonts.Add(new(key, glyphs)); p += size;
        }
        if (p != Sections[11]) throw new InvalidDataException("LA2 フォント領域の末尾が一致しません。");
        p = Sections[6]; count = r.Count(p); r.Check(p + 4, count * 8);
        for (int i = 0; i < count; ++i) Textures.Add(new(r.U32(p + 4 + i * 8), r.U32(p + 8 + i * 8)));
        p = Sections[3]; count = r.Count(p); r.Check(p + 4, count * 8);
        var eventHashes = new HashSet<uint>();
        for (int i = 0; i < count; ++i)
        {
            uint hash = r.U32(p + 4 + i * 8); int at = r.Offset(p + 8 + i * 8);
            if (!eventHashes.Add(hash)) throw new InvalidDataException("LA2 イベントIDが重複しています。");
            int n = r.U16(at), size = r.U16(at + 4), q = at + 8;
            if (size < 8) throw new InvalidDataException("LA2 イベントサイズが不正です。");
            r.Check(at, size); var tracks = new List<Track>();
            for (int j = 0; j < n; ++j)
            {
                int node = r.U16(q), extent = r.Offset(q + 4);
                if (node > nodes || extent < 12 || q > at + size - extent) throw new InvalidDataException("LA2 イベントトラックが不正です。");
                tracks.Add(new(node, r.U16(q + 2), Commands(r, q + 8, q + extent))); q += extent;
            }
            if (q != at + size) throw new InvalidDataException("LA2 イベント領域の末尾が一致しません。");
            Events.Add(new(hash, r.U16(at + 2), tracks));
        }
    }

    private static List<Command> Commands(Reader r, int p, int end)
    {
        var result = new List<Command>();
        while (p < end)
        {
            r.Record();
            int op = r.U16(p), size = r.U16(p + 2);
            if (size < 4 || p > end - size) throw new InvalidDataException("LA2 命令サイズが不正です。");
            result.Add(new(p, op, r.Slice(p + 4, size - 4))); p += size;
            if (op == 0) break;
        }
        if (p != end) throw new InvalidDataException("LA2 命令領域の末尾が一致しません。");
        return result;
    }
    private static List<Pool> ReadPool(Reader r, int p)
    {
        int n = r.Count(p); p += 4; var result = new List<Pool>();
        for (int i = 0; i < n; ++i)
        {
            int count = r.U16(p), size = r.U16(p + 2);
            if (size < 4) throw new InvalidDataException("LA2 プール領域が不正です。");
            result.Add(new(count, r.Slice(p + 4, size - 4))); p += size;
        }
        return result;
    }
}

internal sealed class La2State
{
    public List<PointF> Position { get; set; } = [new(0, 0)];
    public PointF Size { get; set; }
    public List<int[]> Colors { get; set; } = [[255, 255, 255, 128]];
    public bool Visible { get; set; } = true;
    public int Scale { get; set; } = 256;
    public int Angle { get; set; }
    public int Texture { get; set; }
    public int? Uv { get; set; }
    public uint Blend { get; set; }
    public string? Text { get; set; }
    public byte[]? Font { get; set; }

    public void Apply(La2Binary layout, La2Binary.Property property, List<string> warnings)
    {
        foreach (var command in property.Commands)
        {
            var r = new La2Binary.Reader(command.Data);
            switch (command.Opcode)
            {
                case 0: break;
                case 1: Visible = true; break;
                case 2: Visible = false; break;
                case 3:
                {
                    int n = r.Count(0), p = 4; var points = new List<PointF>(); bool supported = true;
                    for (int j = 0; j < n; ++j)
                    {
                        int count = r.U16(p), mode = r.U16(p + 2); p += 4;
                        if (mode != 0) supported = false;
                        r.Check(p, count * 4);
                        for (int k = 0; k < count; ++k) { points.Add(new(r.I16(p), r.I16(p + 2))); p += 4; }
                    }
                    if (supported && points.Count > 0) Position = points;
                    else warnings.Add($"属性 0x{command.Offset:X}: 未対応の座標モードを保持しました。");
                    break;
                }
                case 5:
                    if (r.U32(0) == 1) Size = new(r.I16(4), r.I16(6));
                    else warnings.Add($"属性 0x{command.Offset:X}: 未対応のサイズ配列を保持しました。");
                    break;
                case 6:
                {
                    int mode = r.U16(4), index = r.U16(6);
                    if (r.U32(0) != 1 || mode != 0 || index >> 12 != 15) { warnings.Add($"属性 0x{command.Offset:X}: 色の部分マスクは未対応です。"); break; }
                    int pool = index & 4095;
                    if (pool >= layout.Colors.Count) throw new InvalidDataException("LA2 色プール参照が不正です。");
                    var color = layout.Colors[pool];
                    if (color.Count is not (1 or 4) || color.Data.Length != color.Count * 4) throw new InvalidDataException("LA2 色の頂点数が不正です。");
                    Colors = Enumerable.Range(0, color.Count).Select(i => color.Data.AsSpan(i * 4, 4).ToArray().Select(x => (int)x).ToArray()).ToList();
                    break;
                }
                case 7: Texture = r.Offset(0); break;
                case 8: Uv = r.Offset(0); break;
                case 9: Blend = r.U32(0); break;
                case 10: Scale = r.I16(2); break;
                case 11: Angle = r.I16(0); break;
                case 13:
                    Text = Encoding.UTF8.GetString(r.Slice(4, r.Count(0, 65536))).TrimEnd('\0'); break;
                case 24: r.Check(0, 8); Font = command.Data; break;
                default: warnings.Add($"属性 0x{command.Offset:X}: 命令 {command.Opcode} は未対応です（元データを記録）。"); break;
            }
        }
    }
}
