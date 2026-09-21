#include "alert_media.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wrl/client.h>
#include <bcrypt.h>
#include <array>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace mgo2mt;
using Microsoft::WRL::ComPtr;
namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void hr(HRESULT result){if(FAILED(result)){std::cerr<<"MF fixture HRESULT="<<std::hex<<uint32_t(result)<<std::dec<<'\n';throw std::runtime_error("MF fixture failed");}}
struct Runtime {Runtime(){hr(CoInitializeEx(nullptr,COINIT_MULTITHREADED));hr(MFStartup(MF_VERSION));}~Runtime(){MFShutdown();CoUninitialize();}};
void generate(const std::filesystem::path& path,unsigned frames=4,unsigned width=64,unsigned height=48){
 ComPtr<IMFSinkWriter> writer;ComPtr<IMFAttributes> attrs;hr(MFCreateAttributes(&attrs,1));hr(attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS,FALSE));hr(MFCreateSinkWriterFromURL(path.c_str(),nullptr,attrs.Get(),&writer));
 ComPtr<IMFMediaType> output;hr(MFCreateMediaType(&output));hr(output->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video));hr(output->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_H264));hr(output->SetUINT32(MF_MT_AVG_BITRATE,400000));hr(output->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive));hr(MFSetAttributeSize(output.Get(),MF_MT_FRAME_SIZE,width,height));hr(MFSetAttributeRatio(output.Get(),MF_MT_FRAME_RATE,10,1));hr(MFSetAttributeRatio(output.Get(),MF_MT_PIXEL_ASPECT_RATIO,1,1));DWORD stream=0;hr(writer->AddStream(output.Get(),&stream));
 ComPtr<IMFMediaType> input;hr(MFCreateMediaType(&input));hr(input->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video));hr(input->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_NV12));hr(input->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive));hr(MFSetAttributeSize(input.Get(),MF_MT_FRAME_SIZE,width,height));hr(MFSetAttributeRatio(input.Get(),MF_MT_FRAME_RATE,10,1));hr(MFSetAttributeRatio(input.Get(),MF_MT_PIXEL_ASPECT_RATIO,1,1));hr(writer->SetInputMediaType(stream,input.Get(),nullptr));hr(writer->BeginWriting());
 for(unsigned index=0;index<frames;++index){ComPtr<IMFSample> sample;ComPtr<IMFMediaBuffer> buffer;hr(MFCreateSample(&sample));hr(MFCreateMemoryBuffer(width*height*3/2,&buffer));BYTE* p=nullptr;hr(buffer->Lock(&p,nullptr,nullptr));
  // NV12 is top-down, avoiding RGB sink-input DIB orientation ambiguity.
  constexpr unsigned char colors[4][3]{{82,90,240},{41,240,110},{145,54,34},{210,16,146}};
  for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)p[size_t(y)*width+x]=colors[(y>=height/2?2:0)+(x>=width/2?1:0)][0];
  for(unsigned y=0;y<height/2;++y)for(unsigned x=0;x<width;x+=2){const auto& color=colors[(y>=height/4?2:0)+(x>=width/2?1:0)];const auto at=size_t(width)*height+size_t(y)*width+x;p[at]=color[1];p[at+1]=color[2];}
  hr(buffer->Unlock());hr(buffer->SetCurrentLength(width*height*3/2));hr(sample->AddBuffer(buffer.Get()));hr(sample->SetSampleTime(LONGLONG(index)*1000000));hr(sample->SetSampleDuration(1000000));hr(writer->WriteSample(stream,sample.Get()));
 }hr(writer->Finalize());
}
std::vector<uint8_t> read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);check(bool(f),"fixture file opens");return {std::istreambuf_iterator<char>(f),{}};}
void write(const std::filesystem::path& p,std::span<const uint8_t> bytes){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));check(bool(f),"fixture file writes");}
std::string hash(std::span<const uint8_t> bytes){BCRYPT_ALG_HANDLE algorithm=nullptr;check(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"test hash provider");std::array<uint8_t,32> digest{};const auto status=BCryptHash(algorithm,nullptr,0,const_cast<PUCHAR>(bytes.data()),ULONG(bytes.size()),digest.data(),ULONG(digest.size()));BCryptCloseAlgorithmProvider(algorithm,0);check(status>=0,"test hash");std::string result;for(auto v:digest){result+="0123456789abcdef"[v>>4];result+="0123456789abcdef"[v&15];}return result;}
notices::Media descriptor(const std::filesystem::path& p){auto bytes=read(p);return {p.filename().string(),hash(bytes),uint32_t(bytes.size())};}
bool junction(const std::filesystem::path& link,const std::filesystem::path& target){
 const auto existing=GetFileAttributesW(link.c_str());if(existing!=INVALID_FILE_ATTRIBUTES&&(existing&FILE_ATTRIBUTE_REPARSE_POINT))return true;
 if(!CreateDirectoryW(link.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)return false;
 auto h=CreateFileW(link.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,nullptr);if(h==INVALID_HANDLE_VALUE)return false;
 const auto print=std::filesystem::absolute(target).wstring(),sub=L"\\??\\"+print;std::vector<uint8_t> bytes(16+(sub.size()+print.size()+2)*sizeof(wchar_t));
 auto u=[&](size_t at,uint32_t value,unsigned n){for(unsigned i=0;i<n;++i)bytes[at+i]=uint8_t(value>>(i*8));};u(0,IO_REPARSE_TAG_MOUNT_POINT,4);u(4,uint32_t(bytes.size()-8),2);u(8,0,2);u(10,uint32_t(sub.size()*2),2);u(12,uint32_t((sub.size()+1)*2),2);u(14,uint32_t(print.size()*2),2);
 std::memcpy(bytes.data()+16,sub.c_str(),(sub.size()+1)*2);std::memcpy(bytes.data()+16+(sub.size()+1)*2,print.c_str(),(print.size()+1)*2);DWORD used=0;const bool ok=DeviceIoControl(h,FSCTL_SET_REPARSE_POINT,bytes.data(),DWORD(bytes.size()),nullptr,0,&used,nullptr)!=FALSE;CloseHandle(h);return ok;
}
std::vector<uint8_t> wave(){std::vector<uint8_t> b;auto text=[&](const char* s){for(unsigned i=0;i<4;++i)b.push_back(uint8_t(s[i]));};auto u=[&](uint32_t x,unsigned n){for(unsigned i=0;i<n;++i)b.push_back(uint8_t(x>>(8*i)));};text("RIFF");u(36+1920,4);text("WAVE");text("fmt ");u(16,4);u(1,2);u(2,2);u(48000,4);u(192000,4);u(4,2);u(16,2);text("data");u(1920,4);b.resize(44+1920);return b;}
template<class F>void rejected(F action,const char* why){bool bad=false;try{action();}catch(const std::exception&){bad=true;}check(bad,why);}
void wait_done(alert_media::Player& player){const auto end=GetTickCount64()+10000;while(!player.status().finished&&GetTickCount64()<end)Sleep(10);auto s=player.status();if(!s.error.empty())std::cout<<"media_status="<<s.error<<'\n';check(s.finished,"bounded fixture worker finishes");}
void wait_frame(alert_media::Player& player){const auto end=GetTickCount64()+5000;while(!player.frame()&&!player.status().finished&&GetTickCount64()<end)Sleep(5);check(bool(player.frame()),"paced movie yields a frame");}
void save(const std::filesystem::path& path,std::span<const uint32_t> pixels){BITMAPFILEHEADER h{};BITMAPINFOHEADER info{};info.biSize=40;info.biWidth=1280;info.biHeight=-720;info.biPlanes=1;info.biBitCount=32;h.bfType=0x4d42;h.bfOffBits=sizeof(h)+sizeof(info);h.bfSize=h.bfOffBits+1280*720*4;std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(&h),sizeof(h));f.write(reinterpret_cast<const char*>(&info),sizeof(info));f.write(reinterpret_cast<const char*>(pixels.data()),1280*720*4);check(bool(f),"decoded video BMP");}
void run(const std::filesystem::path& dir){
 Runtime runtime;generate(dir/L"test.mp4");generate(dir/L"paced.mp4",20);generate(dir/L"oversize.mp4",1,656,368);auto pcm=wave();write(dir/L"good.wav",pcm);std::array<uint8_t,44> corrupt{};write(dir/L"bad.wav",corrupt);write(dir/L"bad.mp4",corrupt);
 auto media=descriptor(dir/L"test.mp4");check(alert_media::verified_bytes(dir,media,true)==read(dir/L"test.mp4"),"real MP4 bytes SHA-verified before decoder");
 auto bad=media;bad.size++;rejected([&]{alert_media::verified_bytes(dir,bad,true);},"size mismatch refused");bad=media;bad.sha256[0]=bad.sha256[0]=='0'?'1':'0';rejected([&]{alert_media::verified_bytes(dir,bad,true);},"SHA mismatch refused");bad=media;bad.path="../test.mp4";rejected([&]{alert_media::verified_bytes(dir,bad,true);},"path escape refused");bad=media;bad.path="missing.mp4";rejected([&]{alert_media::verified_bytes(dir,bad,true);},"missing asset refused");
 auto outside=dir/L"outside";std::filesystem::create_directory(outside);write(outside/L"test.mp4",read(dir/L"test.mp4"));auto link=dir/L"linked";
 check(CreateSymbolicLinkW(link.c_str(),outside.c_str(),SYMBOLIC_LINK_FLAG_DIRECTORY|SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)||junction(link,outside),"own-fixture directory reparse created without modifying external paths");rejected([&]{alert_media::verified_bytes(link,media,true);},"directory reparse refused");std::cout<<"reparse_test=passed\n";
 notices::Alert a;a.id=1;a.version=1;a.publishedAt=1000;a.expiresAt=100000;a.text="native fixture";a.video=media;a.audio=descriptor(dir/L"good.wav");alert_media::Player player(dir);
 player.select(7,a,2000,100,false);wait_done(player);auto status=player.status();check(status.error.empty()&&status.decodedFrames==4&&status.audioValidated&&!status.audioStarted,"actual H264 decode and muted PCM validation; no audio device opened");auto frame=player.frame();check(frame&&frame->width==240&&frame->height==180&&frame->pixels.size()==43200&&frame->index==4,"decoded frame scales and aspect fits native movie panel");
 const auto topLeft=frame->pixels[10*frame->width+10],bottomRight=frame->pixels[170*frame->width+230];std::cout<<"top_left_rgb="<<std::hex<<topLeft<<" bottom_right_rgb="<<bottomRight<<std::dec<<'\n';check(((topLeft>>16)&255)>150&&(topLeft&255)<70&&((bottomRight>>16)&255)>150&&((bottomRight>>8)&255)>150&&(bottomRight&255)<70,"decoded RGB quadrant orientation and color survive MF conversion");
 std::vector<uint32_t> canvas(1280*720,0xff252525);check(player.paint(canvas,1280,720),"movie paints into320x180 rectangle");check(canvas[154*1280+920]==0xff101010&&canvas[154*1280+919]==0xff252525,"movie bars and outside bounds");save(dir/L"decoded.bmp",canvas);
 player.select(7,a,2001,15100,false);check(player.status().finished&&player.status().decodedFrames==4,"15-second poll never starts a second worker");
 player.select(7,a,2002,30100,false);check(!player.frame()&&player.status().cancelled,"presentation age expires and removes frame");player.select(7,a,2003,30101,false);check(!player.frame(),"expired same revision cannot republish");
 const auto first=a;
 a.version++;a.audio=descriptor(dir/L"bad.wav");player.select(7,a,2000,40000,false);wait_done(player);check(!player.status().audioError.empty()&&player.status().videoError.empty()&&player.status().decodedFrames==4&&player.frame(),"invalid audio does not prevent valid video");
 player.select(7,first,2001,40001,false);check(!player.frame()&&player.status().cancelled,"A to B to A cannot replay an already presented revision");
 a.version++;a.audio=descriptor(dir/L"good.wav");a.video=descriptor(dir/L"bad.mp4");player.select(7,a,2000,50000,false);wait_done(player);check(player.status().audioValidated&&!player.status().audioStarted&&!player.status().videoError.empty()&&!player.frame(),"invalid video does not prevent independent muted audio validation");
 a.version++;a.video=descriptor(dir/L"oversize.mp4");player.select(7,a,2000,51000,false);wait_done(player);check(!player.status().videoError.empty()&&!player.frame(),"native video dimensions over640x360 rejected");
 a.version++;a.video=descriptor(dir/L"paced.mp4");player.select(7,a,2000,60000,false);wait_frame(player);player.select(7,std::nullopt,2000,60001,false);check(!player.frame()&&player.status().cancelled,"cancel clears currently playing movie");Sleep(120);check(!player.frame()&&!player.status().audioStarted,"cancel cannot publish a later frame or start output audio");player.select(7,a,2001,60150,false);check(!player.frame(),"same revision reappearing after cancel does not replay");
 a.version++;a.expiresAt=2150;player.select(7,a,2000,61000,false);Sleep(220);check(!player.frame(),"worker and frame accessor enforce real deadline without another select");wait_done(player);check(player.status().cancelled,"real expiry reaches cancellation state");
 a.version++;a.expiresAt=100000;player.select(7,a,2000,62000,false);wait_frame(player);player.select(7,a,2001,61999,false);check(!player.frame()&&player.status().cancelled,"monotonic rollback cancels without playback restart");
 a.version++;const auto start=GetTickCount64();do{player.select(8,a,2000,63000,false);if(player.frame())break;Sleep(5);}while(GetTickCount64()-start<5000);wait_frame(player);
 const auto clearAt=GetTickCount64();player.clear();check(GetTickCount64()-clearAt<250,"clear does not join a paced decoder");check(!player.frame()&&player.status().cancelled&&!player.status().audioStarted,"disconnect clears publication immediately");
 a.version++;const auto restartAt=GetTickCount64();do{player.select(9,a,2000,64000,false);if(player.frame())break;Sleep(5);}while(GetTickCount64()-restartAt<5000);wait_frame(player);check(!player.status().cancelled,"new scope starts only after cancelled worker retires");player.clear();wait_done(player);check(!player.frame()&&!player.status().running&&!player.status().audioStarted,"retired worker cannot republish cleared state");
 std::cout<<"fixture_video_bytes="<<media.size<<" fixture_video_sha256="<<media.sha256<<" audio_output_opened=false\n";
}
}
int main(int argc,char** argv){try{std::filesystem::path dir;if(argc==2)dir=argv[1];else dir=std::filesystem::temp_directory_path()/ (L"mgo2mt-alert-media-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));std::filesystem::create_directories(dir);run(dir);std::cout<<"alert media PASS: real MF H264 decode, immutable SHA paths, independent attachments, expiry/cancel/replay, muted PCM and real pixels\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
