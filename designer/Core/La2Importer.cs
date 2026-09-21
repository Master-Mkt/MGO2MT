using System.Security.Cryptography;
using System.Text.Json;

namespace MgoUi;

public sealed record La2EventInfo(uint Hash, int TrackCount, long DurationTicks);
public sealed record La2TextureReference(uint Texture, uint Archive);
public sealed record La2Inspection(int NodeCount, IReadOnlyList<La2EventInfo> Events, IReadOnlyList<La2TextureReference> TextureReferences);
public sealed record La2ImportResult(LayoutDocument Document, string AssetRoot, string ReportPath,
    IReadOnlyList<string> Warnings, int SourceNodes, int ResolvedTextures);

public sealed class MissingLa2ResourcesException(IReadOnlyList<string> missing) : IOException(
    "LA2 の参照画像が見つかりません。元の TXN・QAR・DLD・DLZ があるフォルダーを指定してください。\n" + string.Join("\n", missing.Take(12)))
{
    public IReadOnlyList<string> MissingTextures { get; } = missing;
}

/// <summary>Imports the reviewed static LA2 pose into editable native UI parts.
/// Source events are retained as evidence; this does not execute GCX or install
/// source animation commands into the designer's native action interpreter.</summary>
public static class La2Importer
{
    public static La2Inspection Inspect(string input)
    {
        var layout = Read(input);
        return new(layout.Nodes.Count, layout.Events.Select(EventInfo).ToArray(), layout.Textures.Select(t => new La2TextureReference(t.Key, t.Archive)).ToArray());
    }

    public static La2ImportResult Import(string input, string assetRoot, string? searchRoot = null,
        uint? eventHash = null, string screen = "title", bool initialState = false)
    {
        if (!LayoutRules.Screens.Contains(screen)) throw new InvalidDataException("取り込み先の画面名が不正です。");
        input = Path.GetFullPath(input); assetRoot = Path.GetFullPath(assetRoot);
        var layout = Read(input); var warnings = new List<string>();
        var states = new La2State[layout.Nodes.Count + 1]; states[0] = new();
        long appliedBytes = 0;
        void Apply(La2State state, La2Binary.Property property)
        {
            appliedBytes += property.Size;
            if (appliedBytes > 32L * 1024 * 1024) throw new InvalidDataException("LA2 の属性適用量が上限を超えています。");
            state.Apply(layout, property, warnings);
        }
        foreach (var node in layout.Nodes)
        {
            var state = new La2State(); Apply(state, layout.Properties[node.Property]); states[node.Id] = state;
        }
        uint? selected = eventHash;
        if (initialState && eventHash is not null) throw new InvalidDataException("初期状態とイベント終点は同時に指定できません。");
        if (selected is null && !initialState)
        {
            if (layout.Events.Any(e => e.Hash == 0x642397)) selected = 0x642397;
            else if (layout.Events.Any(e => e.Hash == 0xf7c4bc)) selected = 0xf7c4bc;
        }
        if (selected is not null)
        {
            var pose = layout.Events.FirstOrDefault(e => e.Hash == selected.Value) ?? throw new InvalidDataException("指定した LA2 イベントがありません。");
            foreach (var track in pose.Tracks)
            {
                if (track.Node == 0) { warnings.Add($"イベント 0x{pose.Hash:X6}: 対象が実行時に決まるトラック0は適用せず記録しました。"); continue; }
                foreach (var command in track.Commands)
                {
                    var r = new La2Binary.Reader(command.Data);
                    if (command.Opcode is 7 or 8)
                    {
                        int pointer = r.Offset(command.Opcode == 7 ? 0 : 4);
                        if (!layout.Properties.TryGetValue(pointer, out var property)) throw new InvalidDataException("LA2 イベントの属性参照が不正です。");
                        Apply(states[track.Node], property);
                    }
                    else if (command.Opcode is not (0 or 3)) warnings.Add($"イベント 0x{pose.Hash:X6}: 命令 {command.Opcode} は静止状態に適用できません。");
                }
            }
        }

        var resources = new La2Resources(input, searchRoot, warnings);
        var textures = new La2Texture?[layout.Textures.Count]; var missing = new List<string>();
        var used = states.Where(s => s.Texture != 0).Select(s => s.Texture - 1).Distinct().ToArray();
        foreach (int index in used)
        {
            if (index < 0 || index >= textures.Length) throw new InvalidDataException("LA2 テクスチャ番号が範囲外です。");
            var reference = layout.Textures[index]; textures[index] = resources.Resolve(reference.Key, reference.Archive);
            if (textures[index] is null) missing.Add($"texture 0x{reference.Key:X6} / archive 0x{reference.Archive:X6}");
        }
        if (missing.Count > 0) throw new MissingLa2ResourcesException(missing);

        string digest = Convert.ToHexString(SHA256.HashData(layout.Bytes)).ToLowerInvariant();
        // Include the chosen pose in the namespace; importing another endpoint
        // cannot replace the images used by a previously saved project.
        string folder = $"la2_import/{digest[..16]}_{(selected is null ? "base" : selected.Value.ToString("x6"))}";
        var document = new LayoutDocument(); var pending = new Dictionary<string, byte[]>(StringComparer.Ordinal);
        var transforms = new Affine[states.Length]; transforms[0] = new(1, 0, 0, 0, 1, 0);
        var inheritedColors = new int[states.Length][]; inheritedColors[0] = [255, 255, 255, 128];
        var visibility = new bool[states.Length]; visibility[0] = true;
        long pixels = 0; var nodeReport = new List<object>(); int additive = 0;
        foreach (var node in layout.Nodes)
        {
            var state = states[node.Id]; var parent = transforms[node.Parent];
            bool shown = visibility[node.Parent] && state.Visible; visibility[node.Id] = shown;
            int[][] colors = state.Colors.Select(c => c.Select((v, k) => Math.Min(255, (int)Math.Round(v * inheritedColors[node.Parent][k] / (k == 3 ? 128.0 : 255.0)))).ToArray()).ToArray();
            inheritedColors[node.Id] = colors[0]; transforms[node.Id] = parent;
            nodeReport.Add(new { node.Id, node.Name, node.Type, node.Parent, node.Layer, node.Flags, node.Extra, state, effectiveVisible = shown });
            if (node.Type == 0)
            {
                transforms[node.Id] = parent.Group(state.Position[0], state.Scale / 256f, state.Angle * 2 * MathF.PI / 4096); continue;
            }
            if (node.Type is not (2 or 3 or 4 or 6 or 10))
            {
                warnings.Add($"ノード {node.Id}: 種類 {node.Type} は未対応のため元データを記録しました。"); continue;
            }
            var texture = state.Texture == 0 ? null : textures[state.Texture - 1];
            var finalColors = colors.Select(c => new[] { c[0], c[1], c[2], Math.Min(255, (int)Math.Round(c[3] * 255.0 / 128)) }).ToArray();
            int[][] quadColors = finalColors.Length == 1 ? Enumerable.Repeat(finalColors[0], 4).ToArray() : finalColors;
            if (quadColors.Length != 4) throw new InvalidDataException("LA2 頂点色の数が不正です。");
            var quads = new List<La2Quad>();
            if (node.Type == 10)
            {
                if (string.IsNullOrEmpty(state.Text)) continue;
                if (texture is null || state.Font is null) { warnings.Add($"ノード {node.Id}: 文字画像・フォント設定がなく、文字を記録のみとしました。"); continue; }
                if (!TextQuads(layout, state, texture, parent, quadColors, quads))
                {
                    warnings.Add($"ノード {node.Id}: 未対応の文字コード、未収録字形、または整列方法のため文字を記録のみとしました。"); continue;
                }
            }
            else
            {
                PointF[] positions;
                if (node.Type is 4 or 6)
                {
                    if (state.Position.Count != 4) throw new InvalidDataException("LA2 四角形の頂点数が不正です。");
                    positions = state.Position.ToArray();
                }
                else
                {
                    var p = state.Position[0]; var size = state.Size;
                    positions = [p, new(p.X + size.X, p.Y), new(p.X + size.X, p.Y + size.Y), new(p.X, p.Y + size.Y)];
                }
                var points = positions.Select(parent.World).ToArray();
                if (points.Max(p => p.X) - points.Min(p => p.X) < .0001f || points.Max(p => p.Y) - points.Min(p => p.Y) < .0001f) continue;
                PointF[] uv = [new(0, 0), new(1, 0), new(1, 1), new(0, 1)];
                if (texture is not null)
                {
                    if (state.Uv is null) { warnings.Add($"ノード {node.Id}: 暗黙UVの解釈が未確定のため元データを記録しました。"); continue; }
                    if (state.Uv < 0 || state.Uv >= layout.Uv.Count) throw new InvalidDataException("LA2 UV番号が範囲外です。");
                    var row = layout.Uv[state.Uv.Value]; var r = new La2Binary.Reader(row.Data);
                    if (row.Count is not (2 or 4) || row.Data.Length != row.Count * 4) throw new InvalidDataException("LA2 UV頂点数が不正です。");
                    var coords = Enumerable.Range(0, row.Count).Select(i => new PointF(r.U16(i * 4) + texture.X, r.U16(i * 4 + 2) + texture.Y)).ToArray();
                    uv = row.Count == 4 ? coords : [coords[0], new(coords[1].X, coords[0].Y), coords[1], new(coords[0].X, coords[1].Y)];
                }
                quads.Add(new(points, uv, quadColors, texture));
            }
            if (quads.Count == 0) continue;
            if (state.Blend == 0x2001) ++additive;
            var element = new Element { Id = $"la2_{node.Id}_{node.Name:x6}", Screen = screen, Visible = shown, Z = node.Id };
            var single = quads[0];
            if (quads.Count == 1 && texture is null && AxisRectangle(single.Points) && single.Colors.Skip(1).All(c => c.SequenceEqual(single.Colors[0])))
            {
                element.Kind = "panel"; element.X = single.Points.Min(p => p.X); element.Y = single.Points.Min(p => p.Y);
                element.Width = single.Points.Max(p => p.X) - element.X; element.Height = single.Points.Max(p => p.Y) - element.Y; element.Color = single.Colors[0];
            }
            else
            {
                var bitmap = La2Raster.Bake(quads); byte[] png = La2Raster.Png(bitmap);
                string name = $"{folder}/images/{Convert.ToHexString(SHA256.HashData(png)).ToLowerInvariant()}.png";
                if (!pending.ContainsKey(name))
                {
                    pixels += bitmap.Rgba.LongLength;
                    if (pixels > 128L * 1024 * 1024 || pending.Count >= 512) throw new InvalidDataException("画像化した素材が512枚または128 MiBを超えます。");
                    pending.Add(name, png);
                }
                element.Kind = "image"; element.X = bitmap.X; element.Y = bitmap.Y; element.Width = bitmap.Width; element.Height = bitmap.Height; element.Texture = name;
            }
            document.Elements.Add(element);
        }
        if (additive > 0) warnings.Add($"加算合成を指定した {additive} 部品は通常のアルファ合成で近似しています。元の合成指定はレポートに保持しました。");
        if (!document.Elements.Any(e => e.Visible && e.Color[3] > 0)) warnings.Add("選択した静止状態には表示部品がありません。イベントを選び直すか、非表示部品を表示してください。");
        warnings.Add(selected is null ? "基本属性の静止状態を取り込みました。" : $"イベント 0x{selected:X6} の終点を取り込みました。");
        warnings.Add("元のイベント命令・階層・文字列はレポートに保存しました。GCX、動的変数、アニメーションの再生は自動移植していません。");
        _ = Serialization.Encode(document); // Validate size and all native layout constraints before writing anything.
        string sourcePath = folder + "/source.la2", reportPath = folder + "/import-report.json";
        pending.Add(sourcePath, layout.Bytes);
        var report = new
        {
            format = "MGO2MT.LA2_IMPORT.1", source = Path.GetFileName(input), sourceSha256 = digest,
            selectedEvent = selected, pose = selected is null ? "base" : "event-end", layoutVersion = "0x10009",
            sourceNodes = layout.Nodes.Count, outputElements = document.Elements.Count, sourceCoordinateWidth = 1280, sourceCoordinateHeight = 720,
            assumptions = new[] { "position /16", "group scale /256", "angle /4096 turn", "RGB /255, source alpha /128", "source node order retained", "static ASCII glyph branch" },
            warnings = warnings.Distinct().ToArray(), nodes = nodeReport, properties = layout.Properties.Values, events = layout.Events,
            eventSummary = layout.Events.Select(EventInfo), textureReferences = layout.Textures,
            resolvedTextures = used.Select(i => new { reference = layout.Textures[i], textures[i]!.Source, textures[i]!.Width, textures[i]!.Height, textures[i]!.X, textures[i]!.Y }),
            fonts = layout.Fonts, colors = layout.Colors, uv = layout.Uv
        };
        byte[] reportBytes = JsonSerializer.SerializeToUtf8Bytes(report, Serialization.Options);
        reportPath = folder + "/import-report-" + Convert.ToHexString(SHA256.HashData(reportBytes))[..12].ToLowerInvariant() + ".json";
        pending.Add(reportPath, reportBytes);
        foreach (var item in pending)
        {
            string target = LayoutRules.AssetPath(assetRoot, item.Key);
            if (File.Exists(target) && !File.ReadAllBytes(target).AsSpan().SequenceEqual(item.Value))
                throw new IOException("同じ LA2 の取り込み素材が既に変更されています。別の素材フォルダーを指定してください: " + target);
        }
        foreach (var item in pending)
        {
            string target = LayoutRules.AssetPath(assetRoot, item.Key); Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            File.WriteAllBytes(target, item.Value);
        }
        return new(document, assetRoot, LayoutRules.AssetPath(assetRoot, reportPath), warnings.Distinct().ToArray(), layout.Nodes.Count, used.Length);
    }

    private static La2Binary Read(string input)
    {
        var info = new FileInfo(input);
        if (!info.Exists || info.Length > 16 * 1024 * 1024) throw new InvalidDataException("LA2 ファイルがないか16 MiBを超えています。");
        return new(File.ReadAllBytes(input));
    }
    private static La2EventInfo EventInfo(La2Binary.Event e)
    {
        long duration = 0;
        foreach (var track in e.Tracks)
        {
            long ticks = 0;
            foreach (var command in track.Commands) if (command.Opcode == 8) ticks += new La2Binary.Reader(command.Data).U32(0);
            duration = Math.Max(duration, ticks);
        }
        return new(e.Hash, e.Tracks.Count, duration);
    }
    private readonly record struct Affine(float A, float B, float X, float C, float D, float Y)
    {
        private static float Snap(float value) => Math.Abs(value - MathF.Round(value)) < .0001f ? MathF.Round(value) : value;
        public PointF World(PointF p) => new(Snap(A * p.X / 16 + B * p.Y / 16 + X), Snap(C * p.X / 16 + D * p.Y / 16 + Y));
        public Affine Group(PointF at, float scale, float angle)
        {
            var p = World(at); float co = MathF.Cos(angle) * scale, si = MathF.Sin(angle) * scale;
            return new(A * co + B * si, -A * si + B * co, p.X, C * co + D * si, -C * si + D * co, p.Y);
        }
    }
    private static bool AxisRectangle(PointF[] p) => Math.Abs(p[0].Y - p[1].Y) < .0001f && Math.Abs(p[1].X - p[2].X) < .0001f && Math.Abs(p[2].Y - p[3].Y) < .0001f && Math.Abs(p[3].X - p[0].X) < .0001f;
    private static bool TextQuads(La2Binary layout, La2State state, La2Texture texture, Affine transform, int[][] colors, List<La2Quad> result)
    {
        var r = new La2Binary.Reader(state.Font!); int spacing = r.I16(0), alignment = r.U16(2), font = r.U16(4);
        if (alignment > 2 || font < 1 || font > layout.Fonts.Count || state.Text!.Any(c => c < 32 || c > 126)) return false;
        var byCharacter = layout.Fonts[font - 1].Glyphs.Where(g => g.Character?.Length == 1).GroupBy(g => g.Character![0]).ToDictionary(g => g.Key, g => g.First());
        if (state.Text!.Any(c => !byCharacter.ContainsKey(c))) return false;
        var glyphs = state.Text!.Select(c => byCharacter[c]).ToArray();
        int width = glyphs.Sum(g => g.Width) + spacing * Math.Max(0, glyphs.Length - 1);
        int offset = (int)state.Size.X - width, cursor = alignment == 1 ? offset : alignment == 2 ? offset / 2 : 0;
        const float norm = 1.5259022e-5f;
        static float Uv(int value, int extent, int origin) => MathF.Floor(((extent * (value * norm)) + origin) * 16 + .5f) / 16;
        foreach (var g in glyphs)
        {
            float x = state.Position[0].X + cursor, y = state.Position[0].Y;
            if (g.Width > 0 && g.Height > 0)
            {
                PointF[] points = [new(x, y), new(x + g.Width, y), new(x + g.Width, y + g.Height), new(x, y + g.Height)];
                float u0 = Uv(g.U0, texture.TextureWidth, texture.X), u1 = Uv(g.U1, texture.TextureWidth, texture.X), v0 = Uv(g.V0, texture.TextureHeight, texture.Y), v1 = Uv(g.V1, texture.TextureHeight, texture.Y);
                result.Add(new(points.Select(transform.World).ToArray(), [new(u0, v0), new(u1, v0), new(u1, v1), new(u0, v1)], colors, texture));
            }
            cursor += g.Width + spacing;
        }
        return true;
    }
}
