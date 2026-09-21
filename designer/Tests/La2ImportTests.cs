using System.Buffers.Binary;
using System.IO.Compression;
using MgoUi;

internal static class La2ImportTests
{
    public static void Run(Action<bool, string> check)
    {
        string root = Path.Combine(Path.GetTempPath(), "mgo-la2-tests-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        try
        {
            string input = Path.Combine(root, "source.la2"), assets = Path.Combine(root, "assets");
            File.WriteAllBytes(input, Fixture(false));
            var inspection = La2Importer.Inspect(input);
            check(inspection.NodeCount == 2 && inspection.Events.Count == 1 && inspection.Events[0].Hash == 0x642397, "LA2 original big endian header/nodes/events");
            var result = La2Importer.Import(input, assets);
            var part = result.Document.Elements.Single();
            check(part.Kind == "panel" && part.X == 20 && part.Y == 30 && part.Width == 64 && part.Height == 32, "LA2 hierarchy /16 and default event endpoint become editable panel");
            check(part.Color.SequenceEqual(new[] { 255, 0, 0, 128 }), "LA2 inherited alpha /128 becomes native /255");
            check(File.Exists(result.ReportPath) && File.ReadAllText(result.ReportPath).Contains("selectedEvent"), "LA2 source commands and selected pose report retained");
            check(Directory.GetFiles(assets, "source.la2", SearchOption.AllDirectories).Single() is string original && File.ReadAllBytes(original).SequenceEqual(File.ReadAllBytes(input)), "LA2 exact original bytes retained");
            check(Serialization.Encode(result.Document) == Serialization.Encode(Serialization.Clone(result.Document)), "LA2 imported document roundtrips strict native schema");
            string exported = Path.Combine(root, "exported", "layout.json");
            check(RuntimeExport.Export(exported, result.Document, assets) == 0, "LA2 panel portable export");
            var initial = La2Importer.Import(input, assets, initialState: true).Document.Elements.Single();
            check(initial.X == 10 && initial.Y == 20, "LA2 explicit initial-state selection bypasses automatic endpoint");

            File.WriteAllBytes(input, Fixture(true)); string missingAssets = Path.Combine(root, "missing-assets");
            bool missing = false;
            try { La2Importer.Import(input, missingAssets); }
            catch (MissingLa2ResourcesException e) { missing = e.MissingTextures.Count == 1 && e.MissingTextures[0].Contains("123456"); }
            check(missing && !Directory.Exists(missingAssets), "LA2 missing dependencies reported before any asset write");
            File.WriteAllBytes(Path.Combine(root, "atlas.txn"), InlineTxn());
            var image = La2Importer.Import(input, Path.Combine(root, "images"));
            var imagePart = image.Document.Elements.Single();
            check(image.ResolvedTextures == 1 && imagePart.Kind == "image", "LA2 sibling inline TXN discovered without Python or manual image assignment");
            byte[] png = File.ReadAllBytes(LayoutRules.AssetPath(image.AssetRoot, imagePart.Texture));
            var decoded = DecodePng(png);
            check(decoded.Width == 64 && decoded.Height == 32 && decoded.Pixels[0] == 255 && decoded.Pixels[1] == 0 && decoded.Pixels[2] == 0 && decoded.Pixels[3] == 128, "LA2 BC1 UV image bake PNG color and inherited alpha");
            check(RuntimeExport.Export(Path.Combine(root, "image-export", "layout.json"), image.Document, image.AssetRoot) == 1, "LA2 baked original image exported with layout");
            var second = La2Importer.Import(input, image.AssetRoot);
            check(second.Document.Elements[0].Texture == imagePart.Texture, "LA2 reimport reuses deterministic content image");
            File.WriteAllBytes(Path.Combine(root, "atlas.txn"), [0, 1, 2]); missing = false;
            try { La2Importer.Import(input, Path.Combine(root, "corrupt-assets")); }
            catch (MissingLa2ResourcesException) { missing = true; }
            check(missing && !Directory.Exists(Path.Combine(root, "corrupt-assets")), "LA2 malformed dependency cannot become substituted image");
            File.WriteAllBytes(Path.Combine(root, "atlas.txn"), InlineTxn());

            string rotated = Path.Combine(root, "rotated.la2"); File.WriteAllBytes(rotated, Fixture(false, 1024));
            var rotatedResult = La2Importer.Import(rotated, Path.Combine(root, "rotated-assets"));
            var rotatedPart = rotatedResult.Document.Elements.Single();
            check(Math.Abs(rotatedPart.X + 32) < .01 && Math.Abs(rotatedPart.Y - 30) < .01 && Math.Abs(rotatedPart.Width - 32) < .01 && Math.Abs(rotatedPart.Height - 64) < .01, "LA2 parent quarter-turn preserves transformed bounds");
            byte[] bad = Fixture(false); bad[3] = 10; File.WriteAllBytes(Path.Combine(root, "version.la2"), bad);
            Reject(() => La2Importer.Inspect(Path.Combine(root, "version.la2")), check, "LA2 unknown version rejected");
            bad = Fixture(false); BinaryPrimitives.WriteUInt16BigEndian(bad.AsSpan(52 + 44 + 10, 2), 1); File.WriteAllBytes(Path.Combine(root, "parent.la2"), bad);
            Reject(() => La2Importer.Inspect(Path.Combine(root, "parent.la2")), check, "LA2 cyclic/forward parent rejected");
            File.WriteAllBytes(Path.Combine(root, "truncated.la2"), Fixture(false)[..80]);
            Reject(() => La2Importer.Inspect(Path.Combine(root, "truncated.la2")), check, "LA2 truncated offsets rejected");
            Reject(() => La2Importer.Import(input, assets, eventHash: 0xabcdef), check, "LA2 missing selected event rejected");
            string repeated = Path.Combine(root, "repeated.la2"); File.WriteAllBytes(repeated, RepeatedEvents());
            Reject(() => La2Importer.Inspect(repeated), check, "LA2 shared event offsets cannot amplify a small file into unbounded records");
        }
        finally { Directory.Delete(root, true); }
    }

    private static void Reject(Action action, Action<bool, string> check, string label)
    {
        bool rejected = false; try { action(); } catch (InvalidDataException) { rejected = true; } check(rejected, label);
    }
    private static byte[] Fixture(bool image, ushort angle = 0)
    {
        using var stream = new MemoryStream(); stream.Write(new byte[52]); int[] section = new int[13];
        void U16(int value) { Span<byte> b = stackalloc byte[2]; BinaryPrimitives.WriteUInt16BigEndian(b, (ushort)value); stream.Write(b); }
        void U32(uint value) { Span<byte> b = stackalloc byte[4]; BinaryPrimitives.WriteUInt32BigEndian(b, value); stream.Write(b); }
        byte[] Payload(params int[] words) { using var s = new MemoryStream(); Span<byte> b = stackalloc byte[2]; foreach (int w in words) { BinaryPrimitives.WriteUInt16BigEndian(b, (ushort)w); s.Write(b); } return s.ToArray(); }
        void Property(params (int Code, byte[] Data)[] commands)
        {
            U16(commands.Length + 1); U16(8 + commands.Sum(c => 4 + c.Data.Length));
            foreach (var command in commands) { U16(command.Code); U16(command.Data.Length + 4); stream.Write(command.Data); } U16(0); U16(4);
        }
        section[1] = (int)stream.Position; U16(2); stream.Write(new byte[42]);
        foreach (int id in new[] { 1, 2 }) { U32((uint)(100 + id)); U32((uint)(id == 1 ? 0 : image ? 3 : 2)); U16(id); U16(id - 1); U16(0); U16(0); U16(0); U16(0); }
        section[2] = (int)stream.Position; U32(3);
        Property((3, Payload(0, 1, 1, 0, 160, 320)), (6, Payload(0, 1, 0, 0xf000)), (11, Payload(angle, 0)));
        var child = new List<(int, byte[])> { (3, Payload(0, 1, 1, 0, 0, 0)), (5, Payload(0, 1, 1024, 512)), (6, Payload(0, 1, 0, 0xf001)) };
        if (image) { child.Add((7, Payload(0, 1))); child.Add((8, Payload(0, 0))); }
        Property(child.ToArray()); uint endpoint = (uint)stream.Position; Property((3, Payload(0, 1, 1, 0, 160, 160)));
        section[3] = (int)stream.Position; U32(0);
        section[4] = (int)stream.Position; U32(1); U32(0x642397); U32((uint)stream.Position + 4); U16(1); U16(0); U16(28); U16(0); U16(2); U16(0); U32(20); U16(7); U16(8); U32(endpoint); U16(0); U16(4);
        section[5] = (int)stream.Position; U32(0); section[6] = (int)stream.Position; U32(0);
        section[7] = (int)stream.Position; U32(image ? 1u : 0u); if (image) { U32(0x123456); U32(0x654321); }
        section[8] = (int)stream.Position; U32(2); U16(1); U16(8); stream.Write(new byte[] { 255, 255, 255, 64 }); U16(1); U16(8); stream.Write(new byte[] { 255, 0, 0, 128 });
        section[9] = (int)stream.Position; U32(1); U16(2); U16(12); U16(0); U16(0); U16(4); U16(4);
        section[10] = (int)stream.Position; U32(0); section[11] = (int)stream.Position; U32(0); section[12] = (int)stream.Position; U32(0);
        byte[] result = stream.ToArray(); BinaryPrimitives.WriteUInt32BigEndian(result.AsSpan(0, 4), 0x10009);
        for (int i = 1; i <= 12; ++i) BinaryPrimitives.WriteInt32BigEndian(result.AsSpan(i * 4, 4), section[i]); return result;
    }
    private static byte[] InlineTxn()
    {
        byte[] b = new byte[104];
        void U32(int at, uint value) => BinaryPrimitives.WriteUInt32BigEndian(b.AsSpan(at, 4), value);
        void U16(int at, int value) => BinaryPrimitives.WriteUInt16BigEndian(b.AsSpan(at, 2), (ushort)value);
        U32(8, 1); U32(12, 32); U32(16, 1); U32(20, 48);
        U16(32, 4); U16(34, 4); U16(36, 9); U32(40, 96);
        U32(52, 0x123456); U32(56, 0x654321); U16(60, 4); U16(62, 4); U32(68, 32); U32(76, 0x3f800000); U32(80, 0x3f800000);
        b[96] = 0; b[97] = 0xf8; // BC1 red RGB565 endpoint, every selector zero.
        return b;
    }
    private static byte[] RepeatedEvents()
    {
        byte[] original = Fixture(false); int table = original.Length, target = table + 4 + 10000 * 8;
        byte[] result = new byte[target + 140]; original.CopyTo(result, 0);
        void U32(int at, uint value) => BinaryPrimitives.WriteUInt32BigEndian(result.AsSpan(at, 4), value);
        void U16(int at, int value) => BinaryPrimitives.WriteUInt16BigEndian(result.AsSpan(at, 2), (ushort)value);
        U32(16, (uint)table); U32(table, 10000);
        for (int i = 0; i < 10000; ++i) { U32(table + 4 + i * 8, (uint)(i + 1)); U32(table + 8 + i * 8, (uint)target); }
        U16(target, 1); U16(target + 4, 140); U16(target + 8, 2); U32(target + 12, 132);
        for (int i = 0; i < 31; ++i) { U16(target + 16 + i * 4, i == 30 ? 0 : 3); U16(target + 18 + i * 4, 4); }
        return result;
    }
    private static (int Width, int Height, byte[] Pixels) DecodePng(byte[] png)
    {
        int width = BinaryPrimitives.ReadInt32BigEndian(png.AsSpan(16, 4)), height = BinaryPrimitives.ReadInt32BigEndian(png.AsSpan(20, 4));
        using var compressed = new MemoryStream(); int p = 8;
        while (p < png.Length) { int n = BinaryPrimitives.ReadInt32BigEndian(png.AsSpan(p, 4)); if (System.Text.Encoding.ASCII.GetString(png, p + 4, 4) == "IDAT") compressed.Write(png, p + 8, n); p += n + 12; }
        compressed.Position = 0; using var z = new ZLibStream(compressed, CompressionMode.Decompress); using var raw = new MemoryStream(); z.CopyTo(raw); var bytes = raw.ToArray();
        byte[] pixels = new byte[width * height * 4]; for (int y = 0; y < height; ++y) { if (bytes[y * (width * 4 + 1)] != 0) throw new Exception("unexpected PNG filter"); bytes.AsSpan(y * (width * 4 + 1) + 1, width * 4).CopyTo(pixels.AsSpan(y * width * 4)); }
        return (width, height, pixels);
    }
}
