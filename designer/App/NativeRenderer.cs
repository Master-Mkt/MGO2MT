using System.Runtime.InteropServices;
using System.Text;
namespace MgoUi;

internal sealed class NativeRenderer : IDisposable
{
    private nint handle;
    public bool Available => handle != 0;
    public string Error { get; private set; } = "";
    private string lastJson = "", lastRoot = "";
    public NativeRenderer() { try { handle = Create(); if (handle == 0) Error = "共有描画エンジンを開始できません。"; } catch (Exception e) when (e is DllNotFoundException or BadImageFormatException or EntryPointNotFoundException) { Error = "MGO2MTMultiUI.dll が見つからないか、このPCで使用できません。\n" + e.Message; } }
    public void Dispose() { if (handle != 0) { Destroy(handle); handle = 0; } GC.SuppressFinalize(this); }
    public bool Load(LayoutDocument doc, string root, bool strict = false) { if (!Available) return false; var pending = doc.Elements.Count(e => e.Kind == "image" && e.Texture.Length == 0); if (strict && pending > 0) { Error = "テクスチャ未指定の画像があります。画像を取り込んでから書き出してください。"; return false; } if (pending > 0) { doc = Serialization.Clone(doc); doc.Elements.RemoveAll(e => e.Kind == "image" && e.Texture.Length == 0); } var json = Serialization.Encode(doc); if (json == lastJson && root == lastRoot) return true; var raw = Encoding.UTF8.GetBytes(json); if (LoadRaw(handle, raw, (nuint)raw.Length, root) != 1) { Error = GetError(); return false; } lastJson = json; lastRoot = root; Error = ""; return true; }
    public byte[]? Render(int width, int height, string screen, string state, HashSet<string> flags, string bindings) { if (!Available || width <= 0 || height <= 0) return null; var rgba = new byte[checked(width * height * 4)]; if (RenderRaw(handle, width, height, screen, state, System.Text.Json.JsonSerializer.Serialize(flags), bindings, 0xff221c18, rgba, (nuint)(width * 4)) != 1) { Error = GetError(); return null; } Error = ""; return rgba; }
    public void ImportDds(string input, string output) { if (!Available) throw new InvalidOperationException(Error); if (ExportDds(input, output) != 1) throw new InvalidDataException(GetGlobalError()); }
    public byte[] DecodeImage(string file, out uint width, out uint height) { width = 0; height = 0; if (ImageRaw(file, null, ref width, ref height, 0) != 2 || width == 0 || height == 0 || width * height > 16 * 1024 * 1024) throw new InvalidDataException(GetGlobalError()); var bytes = new byte[checked((int)(width * height * 4))]; if (ImageRaw(file, bytes, ref width, ref height, (nuint)bytes.Length) != 1) throw new InvalidDataException(GetGlobalError()); return bytes; }
    public void Tick(string screen, string state, HashSet<string> flags, string bindings, bool focused, bool soundEnabled = true) { if (Available && TickRaw(handle, screen, state, System.Text.Json.JsonSerializer.Serialize(flags), bindings, focused ? 1 : 0, soundEnabled ? 1 : 0) != 1) Error = GetError(); }
    public bool Click(PointF point) => Available && ClickRaw(handle, point.X, point.Y) == 1;
    public bool Pointer(PointF point, bool down) => Available && PointerRaw(handle, point.X, point.Y, down ? 1 : 0) == 1;
    public bool Key(Keys key) => Available && KeyRaw(handle, (uint)key) == 1;
    public bool Trigger(string id) => Available && TriggerRaw(handle, id) == 1;
    public bool Busy => Available && BusyRaw(handle) == 1;
    public void Cancel(bool reset = false) { if (Available) CancelRaw(handle); if (reset) lastJson = ""; }
    public List<ActionNotice> Events() { if (!Available) return []; var n = EventsRaw(handle, null, 0); if (n < 3 || n > 1024 * 1024) return []; var data = new byte[(int)n]; if (EventsRaw(handle, data, n) != n) return []; return System.Text.Json.JsonSerializer.Deserialize<List<ActionNotice>>(Encoding.UTF8.GetString(data).TrimEnd('\0'), Serialization.Options) ?? []; }
    [DllImport("MGO2MTMultiUI.dll", EntryPoint = "mui_tick", CallingConvention = CallingConvention.Cdecl)] private static extern int TickRaw(nint h, [MarshalAs(UnmanagedType.LPUTF8Str)] string screen, [MarshalAs(UnmanagedType.LPUTF8Str)] string state, [MarshalAs(UnmanagedType.LPUTF8Str)] string flags, [MarshalAs(UnmanagedType.LPUTF8Str)] string bindings, int focused, int sound);
    [DllImport("MGO2MTMultiUI.dll", EntryPoint = "mui_click", CallingConvention = CallingConvention.Cdecl)] private static extern int ClickRaw(nint h, float x, float y);
    [DllImport("MGO2MTMultiUI.dll", EntryPoint = "mui_pointer", CallingConvention = CallingConvention.Cdecl)] private static extern int PointerRaw(nint h, float x, float y, int down);
    [DllImport("MGO2MTMultiUI.dll", EntryPoint = "mui_key", CallingConvention = CallingConvention.Cdecl)] private static extern int KeyRaw(nint h, uint key);
    [DllImport("MGO2MTMultiUI.dll", EntryPoint = "mui_trigger", CallingConvention = CallingConvention.Cdecl)] private static extern int TriggerRaw(nint h, [MarshalAs(UnmanagedType.LPUTF8Str)] string id);
    [DllImport("MGO2MTMultiUI.dll", EntryPoint = "mui_busy", CallingConvention = CallingConvention.Cdecl)] private static extern int BusyRaw(nint h);
    [DllImport("MGO2MTMultiUI.dll", EntryPoint = "mui_cancel_actions", CallingConvention = CallingConvention.Cdecl)] private static extern void CancelRaw(nint h);
    [DllImport("MGO2MTMultiUI.dll", EntryPoint = "mui_action_events", CallingConvention = CallingConvention.Cdecl)] private static extern nuint EventsRaw(nint h, [Out] byte[]? json, nuint capacity);
    private static string GetGlobalError() { var n = ErrorRaw(0, null, 0); if (n == 0 || n > 65536) return "画像を変換できません。"; var b = new byte[(int)n]; ErrorRaw(0, b, n); return Encoding.UTF8.GetString(b).TrimEnd('\0'); }
    [DllImport("MGO2MTMultiUI.dll", EntryPoint = "mui_image_rgba", CallingConvention = CallingConvention.Cdecl)] private static extern int ImageRaw([MarshalAs(UnmanagedType.LPUTF8Str)] string file, [Out] byte[]? rgba, ref uint width, ref uint height, nuint capacity);
    private string GetError() { var n = ErrorRaw(handle, null, 0); if (n == 0 || n > 65536) return "共有描画エンジンでエラーが発生しました。"; var b = new byte[(int)n]; ErrorRaw(handle, b, n); return Encoding.UTF8.GetString(b).TrimEnd('\0'); }
    [DllImport("MGO2MTMultiUI.dll", EntryPoint = "mui_create", CallingConvention = CallingConvention.Cdecl)] private static extern nint Create();
    [DllImport("MGO2MTMultiUI.dll", EntryPoint = "mui_destroy", CallingConvention = CallingConvention.Cdecl)] private static extern void Destroy(nint h);
    [DllImport("MGO2MTMultiUI.dll", EntryPoint = "mui_load", CallingConvention = CallingConvention.Cdecl)] private static extern int LoadRaw(nint h, byte[] json, nuint len, [MarshalAs(UnmanagedType.LPUTF8Str)] string root);
    [DllImport("MGO2MTMultiUI.dll", EntryPoint = "mui_render", CallingConvention = CallingConvention.Cdecl)] private static extern int RenderRaw(nint h, int w, int height, [MarshalAs(UnmanagedType.LPUTF8Str)] string screen, [MarshalAs(UnmanagedType.LPUTF8Str)] string state, [MarshalAs(UnmanagedType.LPUTF8Str)] string flags, [MarshalAs(UnmanagedType.LPUTF8Str)] string bindings, uint bg, [Out] byte[] rgba, nuint stride);
    [DllImport("MGO2MTMultiUI.dll", EntryPoint = "mui_error", CallingConvention = CallingConvention.Cdecl)] private static extern nuint ErrorRaw(nint h, [Out] byte[]? text, nuint capacity);
    [DllImport("MGO2MTMultiUI.dll", EntryPoint = "mui_export_dds", CallingConvention = CallingConvention.Cdecl)] private static extern int ExportDds([MarshalAs(UnmanagedType.LPUTF8Str)] string input, [MarshalAs(UnmanagedType.LPUTF8Str)] string output);
}
internal sealed class ActionNotice { public string Kind { get; set; } = ""; public string Element { get; set; } = ""; public string Name { get; set; } = ""; public string Value { get; set; } = ""; public string Message { get; set; } = ""; }
