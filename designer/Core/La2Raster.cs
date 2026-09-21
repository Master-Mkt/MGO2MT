using System.Buffers.Binary;
using System.IO.Compression;
using System.Text;

namespace MgoUi;

internal sealed record La2Quad(PointF[] Points, PointF[] Uv, int[][] Colors, La2Texture? Texture);
internal sealed record La2Bitmap(int X, int Y, int Width, int Height, byte[] Rgba);

internal static class La2Raster
{
    public static La2Bitmap Bake(IReadOnlyList<La2Quad> quads)
    {
        if (quads.Count == 0) throw new InvalidDataException("描画する LA2 頂点がありません。");
        var points = quads.SelectMany(q => q.Points).ToArray();
        if (points.Any(p => !float.IsFinite(p.X) || !float.IsFinite(p.Y) || Math.Abs(p.X) > 12288 || Math.Abs(p.Y) > 12288)) throw new InvalidDataException("LA2 の変換後座標が範囲外です。");
        int x = (int)MathF.Floor(points.Min(p => p.X)), y = (int)MathF.Floor(points.Min(p => p.Y));
        int w = (int)MathF.Ceiling(points.Max(p => p.X)) - x, h = (int)MathF.Ceiling(points.Max(p => p.Y)) - y;
        if (w is < 1 or > 4096 || h is < 1 or > 4096) throw new InvalidDataException("LA2 部品の画像化サイズは1〜4096です。");
        var rgba = new byte[checked(w * h * 4)];
        Span<float> color = stackalloc float[4];
        foreach (var quad in quads)
        {
            int left = Math.Max(0, (int)MathF.Floor(quad.Points.Min(p => p.X)) - x);
            int right = Math.Min(w, (int)MathF.Ceiling(quad.Points.Max(p => p.X)) - x);
            int top = Math.Max(0, (int)MathF.Floor(quad.Points.Min(p => p.Y)) - y);
            int bottom = Math.Min(h, (int)MathF.Ceiling(quad.Points.Max(p => p.Y)) - y);
            for (int py = top; py < bottom; ++py)
                for (int px = left; px < right; ++px)
                {
                    var point = new PointF(x + px + .5f, y + py + .5f);
                    int a = 0, b = 1, c = 2;
                    if (!Weights(point, quad.Points[a], quad.Points[b], quad.Points[c], out float wa, out float wb, out float wc))
                    {
                        b = 2; c = 3;
                        if (!Weights(point, quad.Points[a], quad.Points[b], quad.Points[c], out wa, out wb, out wc)) continue;
                    }
                    float u = quad.Uv[a].X * wa + quad.Uv[b].X * wb + quad.Uv[c].X * wc;
                    float v = quad.Uv[a].Y * wa + quad.Uv[b].Y * wb + quad.Uv[c].Y * wc;
                    for (int k = 0; k < 4; ++k)
                    {
                        float tint = (quad.Colors[a][k] * wa + quad.Colors[b][k] * wb + quad.Colors[c][k] * wc) / 255f;
                        color[k] = tint * (quad.Texture is null ? 255f : Sample(quad.Texture, u, v, k));
                    }
                    int dest = (py * w + px) * 4; float alpha = color[3] / 255f, oldAlpha = rgba[dest + 3] / 255f;
                    float combined = alpha + oldAlpha * (1 - alpha);
                    for (int k = 0; k < 3; ++k)
                        rgba[dest + k] = Byte(combined <= 0 ? 0 : (color[k] * alpha + rgba[dest + k] * oldAlpha * (1 - alpha)) / combined);
                    rgba[dest + 3] = Byte(combined * 255);
                }
        }
        return new(x, y, w, h, rgba);
    }

    private static byte Byte(float f) => (byte)Math.Clamp((int)MathF.Round(f), 0, 255);
    private static bool Weights(PointF p, PointF a, PointF b, PointF c, out float wa, out float wb, out float wc)
    {
        float determinant = (b.Y - c.Y) * (a.X - c.X) + (c.X - b.X) * (a.Y - c.Y);
        if (Math.Abs(determinant) < .000001f) { wa = wb = wc = 0; return false; }
        wa = ((b.Y - c.Y) * (p.X - c.X) + (c.X - b.X) * (p.Y - c.Y)) / determinant;
        wb = ((c.Y - a.Y) * (p.X - c.X) + (a.X - c.X) * (p.Y - c.Y)) / determinant;
        wc = 1 - wa - wb;
        return wa >= -.00001f && wb >= -.00001f && wc >= -.00001f;
    }
    private static float Sample(La2Texture texture, float u, float v, int channel)
    {
        // Original UV pixel centres and the native UI's linear clamp sampler.
        float sx = Math.Clamp(u - .5f, 0, texture.Width - 1), sy = Math.Clamp(v - .5f, 0, texture.Height - 1);
        int x0 = (int)sx, y0 = (int)sy, x1 = Math.Min(x0 + 1, texture.Width - 1), y1 = Math.Min(y0 + 1, texture.Height - 1);
        float fx = sx - x0, fy = sy - y0;
        float top = texture.Rgba[(y0 * texture.Width + x0) * 4 + channel] * (1 - fx) + texture.Rgba[(y0 * texture.Width + x1) * 4 + channel] * fx;
        float bottom = texture.Rgba[(y1 * texture.Width + x0) * 4 + channel] * (1 - fx) + texture.Rgba[(y1 * texture.Width + x1) * 4 + channel] * fx;
        return top * (1 - fy) + bottom * fy;
    }

    public static byte[] Png(La2Bitmap bitmap)
    {
        using var output = new MemoryStream(); output.Write(new byte[] { 137, 80, 78, 71, 13, 10, 26, 10 });
        byte[] header = new byte[13]; BinaryPrimitives.WriteInt32BigEndian(header.AsSpan(0, 4), bitmap.Width); BinaryPrimitives.WriteInt32BigEndian(header.AsSpan(4, 4), bitmap.Height); header[8] = 8; header[9] = 6;
        Chunk(output, "IHDR", header);
        using var pixels = new MemoryStream();
        using (var compressor = new ZLibStream(pixels, CompressionLevel.Optimal, true))
            for (int y = 0; y < bitmap.Height; ++y) { compressor.WriteByte(0); compressor.Write(bitmap.Rgba, y * bitmap.Width * 4, bitmap.Width * 4); }
        Chunk(output, "IDAT", pixels.ToArray()); Chunk(output, "IEND", []); return output.ToArray();
    }
    private static void Chunk(Stream output, string name, byte[] data)
    {
        Span<byte> number = stackalloc byte[4]; BinaryPrimitives.WriteInt32BigEndian(number, data.Length); output.Write(number);
        byte[] type = Encoding.ASCII.GetBytes(name); output.Write(type); output.Write(data);
        uint crc = 0xffffffff;
        foreach (byte v in type.Concat(data)) { crc ^= v; for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) != 0 ? 0xedb88320u : 0); }
        BinaryPrimitives.WriteUInt32BigEndian(number, ~crc); output.Write(number);
    }
}
