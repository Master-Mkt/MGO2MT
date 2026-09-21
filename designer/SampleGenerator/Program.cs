using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text.Json;
using MgoUi;

if (args.Length != 3) throw new ArgumentException("GenerateSamples <samples-folder> <MGO2MTMultiUI.dll> <shared-schema.json>");
var folder = Path.GetFullPath(args[0]);
Directory.CreateDirectory(Path.Combine(folder, "textures"));
Directory.CreateDirectory(Path.Combine(folder, "schemas"));
var png = Path.Combine(folder, "textures", "sample-swatch.png");
var dds = Path.Combine(folder, "textures", "sample-swatch.dds");
var jpeg = Path.Combine(folder, "textures", "sample-swatch.jpg");
// Original artwork for this editor example: three colored bars with differing alpha.
using (var bitmap = new Bitmap(96, 60, PixelFormat.Format32bppArgb))
{
    using var graphics = Graphics.FromImage(bitmap);
    graphics.CompositingMode = System.Drawing.Drawing2D.CompositingMode.SourceCopy;
    graphics.Clear(Color.Transparent);
    foreach (var row in new[] { (8, Color.FromArgb(255, 61, 210, 177)), (26, Color.FromArgb(190, 106, 183, 221)), (44, Color.FromArgb(110, 237, 220, 133)) })
    {
        using var brush = new SolidBrush(row.Item2);
        graphics.FillRectangle(brush, 8, row.Item1, 80, 10);
    }
    bitmap.Save(png, ImageFormat.Png);
}
using (var bitmap = new Bitmap(96, 60, PixelFormat.Format24bppRgb))
{
    using var graphics = Graphics.FromImage(bitmap);
    graphics.Clear(Color.FromArgb(24, 28, 34));
    using var source = new Bitmap(png);
    graphics.DrawImageUnscaled(source, 0, 0);
    bitmap.Save(jpeg, ImageFormat.Jpeg);
}
var library = NativeLibrary.Load(Path.GetFullPath(args[1]));
try
{
    var convert = Marshal.GetDelegateForFunctionPointer<ExportDds>(NativeLibrary.GetExport(library, "mui_export_dds"));
    if (convert(png, dds) != 1) throw new InvalidOperationException("Shared DDS export failed.");
}
finally { NativeLibrary.Free(library); }
var document = Samples.Create();
document.Elements.Add(new Element { Id = "title_sample_image", Kind = "image", Screen = "title", Anchor = "top-center", X = 0, Y = 182, Width = 240, Height = 150, Z = 4, Texture = "textures/sample-swatch.png" });
document.Elements.Add(new Element { Id = "title_sample_label", Kind = "text", Screen = "title", Anchor = "top-center", X = 0, Y = 338, Width = 330, Height = 26, Z = 5, Text = "透過 PNG（元形式のまま）", FontSize = 21, Color = [154, 185, 199, 255] });
document.Elements.Add(new Element { Id = "title_sample_jpeg", Kind = "image", Screen = "title", X = 855, Y = 192, Width = 192, Height = 120, Z = 4, Texture = "textures/sample-swatch.jpg" });
document.Elements.Add(new Element { Id = "title_jpeg_label", Kind = "text", Screen = "title", X = 827, Y = 323, Width = 270, Height = 26, Z = 5, Text = "JPEG（元形式のまま）", FontSize = 21, Color = [154, 185, 199, 255] });
Serialization.AtomicWrite(Path.Combine(folder, "layout.example.json"), Serialization.Encode(document));
Serialization.WriteProject(Path.Combine(folder, "example.mgoui"), new ProjectDocument { AssetRoot = ".", Layout = document });
var loaded = Serialization.ReadProject(Path.Combine(folder, "example.mgoui"));
if (Serialization.Encode(loaded.Layout) != Serialization.Encode(document) || loaded.AssetRoot != folder) throw new InvalidDataException("Portable project roundtrip failed.");
var schemaOutput = Path.Combine(folder, "schemas", "multi_ui_layout.schema.json");
File.Copy(args[2], schemaOutput, true);
var records = new[] { png, jpeg, dds, Path.Combine(folder, "example.mgoui"), Path.Combine(folder, "layout.example.json"), schemaOutput }.Select(file => new { path = Path.GetRelativePath(folder, file).Replace('\\', '/'), size = new FileInfo(file).Length, sha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(file))).ToLowerInvariant() });
File.WriteAllText(Path.Combine(folder, "sample-provenance.json"), JsonSerializer.Serialize(new { format = "MGO2MT.UI_SAMPLE.1", originalGameAssets = false, imageSource = "Programmatically drawn original three-bar RGBA swatch; no external art", ddsWriter = "MGO2MTMultiUI.dll mui_export_dds", projectWriter = "MGO2MTMultiUIDesigner.Core Serialization.WriteProject", runtimeWriter = "MGO2MTMultiUIDesigner.Core Serialization.Encode", portableAssetRoot = ".", elements = document.Elements.Count, files = records }, new JsonSerializerOptions { WriteIndented = true }));
Console.WriteLine($"PASS: portable project + image layout + original DDS + shared schema ({document.Elements.Count} elements)");

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
delegate int ExportDds([MarshalAs(UnmanagedType.LPUTF8Str)] string input, [MarshalAs(UnmanagedType.LPUTF8Str)] string output);
