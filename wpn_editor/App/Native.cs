using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;
namespace Mgo2Mt.WpnEditor;
[StructLayout(LayoutKind.Sequential)]internal struct Particle
{
    public float X,Y,Z,Radius,Rotation,StretchX,StretchY,R,G,B,A,U0,V0,U1,V1;
    public uint Texture,Additive;
}
internal static class Native
{
    private const string Dll="MGO2MTWpnEffects.dll";
    [DllImport(Dll,CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)]private static extern int mw_validate(string g,string? e,[Out]byte[] error,int capacity);
    [DllImport(Dll,CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)]private static extern int mw_image(string file,uint key,[Out]byte[]? rgba,ref uint w,ref uint h,int capacity);
    [DllImport(Dll,CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)]private static extern int mw_texture_keys(string file,[Out]uint[]? keys,int capacity);
    [DllImport(Dll,CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)]private static extern int mw_wave(string file,[Out]byte[] error,int capacity);
    [DllImport(Dll,CallingConvention=CallingConvention.Cdecl)]private static extern int mw_last_error([Out]byte[] error,int capacity);
    [DllImport(Dll,CallingConvention=CallingConvention.Cdecl)]private static extern int mw_sample(byte[] json,int length,ushort weapon,[MarshalAs(UnmanagedType.LPUTF8Str)]string channel,ulong ageMs,ulong seed,[Out]Particle[]? samples,int capacity);
    [DllImport(Dll,CallingConvention=CallingConvention.Cdecl)]private static extern int mw_texture_path(uint key,[Out]byte[] path,int capacity);
    private static string Message(byte[] data)=>Encoding.UTF8.GetString(data).TrimEnd('\0');
    public static string Error(){var b=new byte[4096];mw_last_error(b,b.Length);return Message(b);}
    public static void Validate(string g,string? e){var b=new byte[8192];if(mw_validate(g,e,b,b.Length)!=1)throw new InvalidDataException(Message(b));}
    public static void Wave(string file){var b=new byte[4096];if(mw_wave(file,b,b.Length)!=1)throw new InvalidDataException(Message(b));}
    public static string TexturePath(uint key){var b=new byte[1024];return mw_texture_path(key,b,b.Length)>0?Message(b):"";}
    public static uint[] Keys(string file){int n=mw_texture_keys(file,null,0);if(n<0||n>4096)throw new InvalidDataException(Error());var a=new uint[n];if(mw_texture_keys(file,a,n)!=n)throw new InvalidDataException(Error());return a;}
    public static Particle[] Sample(string json,int id,string channel,int age){var b=Encoding.UTF8.GetBytes(json);var a=new Particle[128];int n=mw_sample(b,b.Length,(ushort)id,channel,(ulong)Math.Max(0,age),314159,a,a.Length);if(n<0)throw new InvalidDataException(Error());return a[..n];}
    public static Bitmap Image(string path,uint key=0)
    {
        uint w=0,h=0;if(mw_image(path,key,null,ref w,ref h,0)!=2||w==0||h==0||w*h>16777216)throw new InvalidDataException(Error());
        var b=new byte[checked((int)(w*h*4))];if(mw_image(path,key,b,ref w,ref h,b.Length)!=1)throw new InvalidDataException(Error());
        for(int i=0;i<b.Length;i+=4)(b[i],b[i+2])=(b[i+2],b[i]);var bitmap=new Bitmap((int)w,(int)h,PixelFormat.Format32bppArgb);var locked=bitmap.LockBits(new(0,0,(int)w,(int)h),ImageLockMode.WriteOnly,PixelFormat.Format32bppArgb);try{for(int y=0;y<h;y++)Marshal.Copy(b,y*(int)w*4,locked.Scan0+y*locked.Stride,(int)w*4);}finally{bitmap.UnlockBits(locked);}return bitmap;
    }
}
internal sealed class AudioPlayer:IDisposable
{
    [StructLayout(LayoutKind.Sequential,Pack=2)]private struct WaveFormat{public ushort Tag,Channels;public uint Rate,BytesPerSecond;public ushort Alignment,Bits,Extra;}
    [StructLayout(LayoutKind.Sequential)]private struct Header{public nint Data;public uint Length,Recorded;public nuint User;public uint Flags,Loops;public nint Next;public nuint Reserved;}
    [DllImport("winmm.dll")]private static extern uint waveOutOpen(out nint device,uint id,ref WaveFormat format,nint callback,nint instance,uint flags);
    [DllImport("winmm.dll")]private static extern uint waveOutPrepareHeader(nint device,nint header,uint size);
    [DllImport("winmm.dll")]private static extern uint waveOutWrite(nint device,nint header,uint size);
    [DllImport("winmm.dll")]private static extern uint waveOutReset(nint device);
    [DllImport("winmm.dll")]private static extern uint waveOutUnprepareHeader(nint device,nint header,uint size);
    [DllImport("winmm.dll")]private static extern uint waveOutClose(nint device);
    private nint device,header;private GCHandle pinned;
    private static void Check(uint result){if(result!=0)throw new IOException("音声出力を開始できません（Windows音声エラー "+result+"）。");}
    public void Play(string file,double gain)
    {
        Stop();Native.Wave(file);var bytes=File.ReadAllBytes(file);var format=new WaveFormat();byte[]? pcm=null;
        for(int at=12;at+8<=bytes.Length;){int n=BitConverter.ToInt32(bytes,at+4);if(n<0||n>bytes.Length-at-8)throw new InvalidDataException("WAVが破損しています。");var tag=Encoding.ASCII.GetString(bytes,at,4);if(tag=="fmt "&&n>=16){format.Tag=BitConverter.ToUInt16(bytes,at+8);format.Channels=BitConverter.ToUInt16(bytes,at+10);format.Rate=BitConverter.ToUInt32(bytes,at+12);format.BytesPerSecond=BitConverter.ToUInt32(bytes,at+16);format.Alignment=BitConverter.ToUInt16(bytes,at+20);format.Bits=BitConverter.ToUInt16(bytes,at+22);}if(tag=="data")pcm=bytes[(at+8)..(at+8+n)];at=checked(at+8+n+(n&1));}
        if(pcm is null||format.Tag!=1||format.Bits!=16)throw new InvalidDataException("16bit PCM WAVを使用してください。");
        for(int i=0;i+1<pcm.Length;i+=2){short sample=BitConverter.ToInt16(pcm,i);short value=(short)Math.Clamp(sample*Math.Clamp(gain,0,4),short.MinValue,short.MaxValue);pcm[i]=(byte)value;pcm[i+1]=(byte)(value>>8);}
        try{Check(waveOutOpen(out device,uint.MaxValue,ref format,0,0,0));pinned=GCHandle.Alloc(pcm,GCHandleType.Pinned);header=Marshal.AllocHGlobal(Marshal.SizeOf<Header>());Marshal.StructureToPtr(new Header{Data=pinned.AddrOfPinnedObject(),Length=(uint)pcm.Length},header,false);Check(waveOutPrepareHeader(device,header,(uint)Marshal.SizeOf<Header>()));Check(waveOutWrite(device,header,(uint)Marshal.SizeOf<Header>()));}catch{Stop();throw;}
    }
    public void Stop(){if(device!=0){waveOutReset(device);if(header!=0)waveOutUnprepareHeader(device,header,(uint)Marshal.SizeOf<Header>());waveOutClose(device);device=0;}if(header!=0){Marshal.FreeHGlobal(header);header=0;}if(pinned.IsAllocated)pinned.Free();}
    public void Dispose(){Stop();GC.SuppressFinalize(this);}
}
