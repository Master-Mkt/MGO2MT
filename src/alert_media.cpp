#include "alert_media.h"
#include "pcm_wave.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfobjects.h>
#include <shlwapi.h>
#include <propvarutil.h>
#include <xaudio2.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <set>
#include <thread>
namespace mgo2mt::alert_media {
using Microsoft::WRL::ComPtr;
namespace {
void need(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void hr(HRESULT value){need(SUCCEEDED(value),"Announcement media codec unavailable or invalid");}
std::string sha(std::span<const uint8_t> b){
 BCRYPT_ALG_HANDLE a=nullptr;BCRYPT_HASH_HANDLE h=nullptr;std::array<uint8_t,32>d{};
 try{need(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"Announcement hash provider");need(BCryptCreateHash(a,&h,nullptr,0,nullptr,0,0)>=0,"Announcement hash");need(BCryptHashData(h,const_cast<PUCHAR>(b.data()),ULONG(b.size()),0)>=0,"Announcement hash data");need(BCryptFinishHash(h,d.data(),ULONG(d.size()),0)>=0,"Announcement hash finish");}
 catch(...){if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);throw;}BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);
 std::string text;for(auto v:d){text+="0123456789abcdef"[v>>4];text+="0123456789abcdef"[v&15];}return text;
}
struct Com {bool init=false,mf=false;Com(){hr(CoInitializeEx(nullptr,COINIT_MULTITHREADED));init=true;}void media(){hr(MFStartup(MF_VERSION,MFSTARTUP_LITE));mf=true;}~Com(){if(mf)MFShutdown();if(init)CoUninitialize();}};
struct Audio {
 ComPtr<IXAudio2> engine;IXAudio2MasteringVoice* master=nullptr;IXAudio2SourceVoice* source=nullptr;
 ~Audio(){if(source){source->Stop();source->DestroyVoice();}if(master)master->DestroyVoice();}
 void prepare(const std::vector<uint8_t>& bytes,const PcmWave& w){
  hr(XAudio2Create(&engine,0,XAUDIO2_DEFAULT_PROCESSOR));hr(engine->CreateMasteringVoice(&master));
  WAVEFORMATEX f{WAVE_FORMAT_PCM,WORD(w.channels),w.rate,w.rate*w.channels*2,WORD(w.channels*2),16,0};
  hr(engine->CreateSourceVoice(&source,&f));hr(source->SetVolume(.2f));
  XAUDIO2_BUFFER b{};b.AudioBytes=w.dataSize;b.pAudioData=bytes.data()+w.dataAt;b.Flags=XAUDIO2_END_OF_STREAM;
  hr(source->SubmitSourceBuffer(&b));
 }
 void start(){hr(source->Start());}
 void stop(){if(source)source->Stop();}
 bool playing()const{if(!source)return false;XAUDIO2_VOICE_STATE s{};source->GetState(&s,XAUDIO2_VOICE_NOSAMPLESPLAYED);return s.BuffersQueued!=0;}
};
}
std::vector<uint8_t> verified_bytes(const std::filesystem::path& directory,const notices::Media&m,bool video){
 need(notices::valid_media(m,video),"Invalid announcement asset descriptor");
 auto root=std::filesystem::absolute(directory).lexically_normal();
 for(auto p=root;!p.empty();p=p.parent_path()){const auto a=GetFileAttributesW(p.c_str());need(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY)&&!(a&FILE_ATTRIBUTE_REPARSE_POINT),"Announcement asset directory unavailable/reparse");if(p==p.root_path())break;}
 const auto path=root/m.path;HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
 need(h!=INVALID_HANDLE_VALUE,"Announcement asset missing");struct Close{HANDLE h;~Close(){CloseHandle(h);}} close{h};
 BY_HANDLE_FILE_INFORMATION info{};need(GetFileInformationByHandle(h,&info)&&!(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))&&!info.nFileSizeHigh&&info.nFileSizeLow==m.size,"Announcement asset size/reparse mismatch");
 const auto length=GetFinalPathNameByHandleW(h,nullptr,0,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);need(length&&length<32768,"Announcement asset final path");
 std::wstring actual(length,L'\0');const auto used=GetFinalPathNameByHandleW(h,actual.data(),length,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);need(used&&used<length,"Announcement asset final path");actual.resize(used);
 auto expected=path.wstring();if(!expected.starts_with(L"\\\\?\\"))expected=L"\\\\?\\"+expected;
 need(_wcsicmp(actual.c_str(),expected.c_str())==0,"Announcement asset escaped checked directory");
 std::vector<uint8_t> bytes(m.size);DWORD count=0;need(ReadFile(h,bytes.data(),m.size,&count,nullptr)&&count==m.size,"Announcement asset read");need(sha(bytes)==m.sha256,"Announcement asset SHA mismatch");return bytes;
}
struct Player::Impl {
 std::filesystem::path directory;std::mutex mutex;std::condition_variable wake;std::atomic_bool stop{false};
 std::thread worker;std::shared_ptr<const Frame> frame;Status status;uint64_t scope=0,began=0,deadline=0,lifetime=30000;std::string key;
 std::set<std::string> played; // presentation-thread only; saturate rather than replay evicted revisions
 Audio* activeAudio=nullptr; // guarded by mutex; unregisters before destruction
 explicit Impl(std::filesystem::path p):directory(std::move(p)){}
 void cancel(){stop=true;{std::lock_guard lock(mutex);if(activeAudio)activeAudio->stop();frame.reset();status.cancelled=true;}wake.notify_all();}
 void halt(){cancel();if(worker.joinable())worker.join();std::lock_guard lock(mutex);frame.reset();status={};}
 bool live(){if(stop)return false;if(GetTickCount64()>=deadline){cancel();return false;}return true;}
 bool wait(uint64_t start,uint64_t elapsed){std::unique_lock lock(mutex);while(!stop){auto now=GetTickCount64();if(now>=deadline){lock.unlock();cancel();return false;}if(now-start>=elapsed)return true;wake.wait_for(lock,std::chrono::milliseconds((std::min)({elapsed-(now-start),deadline-now,uint64_t(20)})));}return false;}
 void failure(bool audio,const std::exception& e){std::lock_guard lock(mutex);if(stop)return;auto& field=audio?status.audioError:status.videoError;field=e.what();status.error=status.audioError;if(!status.videoError.empty()){if(!status.error.empty())status.error+="; ";status.error+=status.videoError;}}
 void run(notices::Alert alert,bool sound){
  try{
   Com com;std::vector<uint8_t> wave,video;Audio audio;
   struct VoiceGuard{Impl* owner;~VoiceGuard(){std::lock_guard lock(owner->mutex);if(owner->activeAudio)owner->activeAudio->stop();owner->activeAudio=nullptr;}} voiceGuard{this};
   const auto started=GetTickCount64();
   if(alert.audio&&live())try{
    wave=verified_bytes(directory,*alert.audio,false);
    if(live()){
     auto w=read_pcm_wave(wave);need(w.channels<=2&&w.dataSize/(w.channels*2)<=uint64_t(w.rate)*30,"Announcement WAV maximum30seconds stereo");
     {std::lock_guard lock(mutex);if(!stop)status.audioValidated=true;}
     if(sound&&live()){
      audio.prepare(wave,w);
      std::lock_guard lock(mutex);if(!stop&&GetTickCount64()<deadline){activeAudio=&audio;audio.start();status.audioStarted=true;}
     }
    }
   }catch(const std::exception& e){audio.stop();failure(true,e);}
   // Failure of either independent attachment must not suppress the other.
   if(alert.video&&live())try{
    video=verified_bytes(directory,*alert.video,true);
    if(!video.empty()&&live()){
    com.media();ComPtr<IStream> stream;stream.Attach(SHCreateMemStream(video.data(),UINT(video.size())));need(bool(stream),"Announcement memory stream");
    ComPtr<IMFByteStream> bytes;hr(MFCreateMFByteStreamOnStream(stream.Get(),&bytes));
    ComPtr<IMFAttributes> attrs;hr(MFCreateAttributes(&attrs,2));hr(attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,TRUE));
    ComPtr<IMFSourceReader> reader;hr(MFCreateSourceReaderFromByteStream(bytes.Get(),attrs.Get(),&reader));
    hr(reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS,FALSE));hr(reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM,TRUE));
    ComPtr<IMFMediaType> native;hr(reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,&native));UINT32 width=0,height=0;hr(MFGetAttributeSize(native.Get(),MF_MT_FRAME_SIZE,&width,&height));
    need(width&&height&&width<=640&&height<=360,"Announcement video maximum640x360");
    PROPVARIANT duration;PropVariantInit(&duration);hr(reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE,MF_PD_DURATION,&duration));auto length=duration.vt==VT_UI8?duration.uhVal.QuadPart:0;PropVariantClear(&duration);need(length&&length<=300000000,"Announcement video maximum30seconds");
    ComPtr<IMFMediaType> type;hr(MFCreateMediaType(&type));hr(type->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video));hr(type->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_RGB32));hr(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,nullptr,type.Get()));
    ComPtr<IMFMediaType> output;hr(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,&output));hr(MFGetAttributeSize(output.Get(),MF_MT_FRAME_SIZE,&width,&height));need(width&&height&&width<=640&&height<=360,"Announcement decoded extent");
    LONG stride=LONG(MFGetAttributeUINT32(output.Get(),MF_MT_DEFAULT_STRIDE,0));if(!stride)hr(MFGetStrideForBitmapInfoHeader(MFVideoFormat_RGB32.Data1,width,&stride));
    uint64_t index=0;LONGLONG first=-1,previous=-1;
    for(unsigned packets=0;packets<1801&&live();++packets){
     DWORD flags=0;LONGLONG timestamp=0;ComPtr<IMFSample> sample;hr(reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,nullptr,&flags,&timestamp,&sample));if(!live())break;
     need(!(flags&(MF_SOURCE_READERF_ERROR|MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)),"Announcement media type changed");
     if(!sample){if(flags&MF_SOURCE_READERF_ENDOFSTREAM)break;continue;}
     if(first<0)first=timestamp;need(timestamp>=first&&timestamp>=previous&&timestamp-first<=300000000,"Announcement frame timestamp");previous=timestamp;
     if(!wait(started,uint64_t(timestamp-first)/10000))break;
     ComPtr<IMFMediaBuffer> buffer;hr(sample->ConvertToContiguousBuffer(&buffer));BYTE* ptr=nullptr;DWORD size=0;hr(buffer->Lock(&ptr,nullptr,&size));
     struct Unlock{IMFMediaBuffer* buffer;~Unlock(){buffer->Unlock();}}unlock{buffer.Get()};
     const auto pitch=uint64_t(stride<0?-int64_t(stride):stride);need(pitch>=width*4&&pitch*height<=size,"Announcement RGB stride");
     auto image=std::make_shared<Frame>();const float scale=(std::min)(320.f/width,180.f/height);image->width=(std::max)(1u,unsigned(width*scale));image->height=(std::max)(1u,unsigned(height*scale));image->index=++index;image->pixels.resize(size_t(image->width)*image->height);
     for(unsigned y=0;y<image->height;++y){auto sy=uint64_t(y)*height/image->height;const auto row=stride<0?height-1-sy:sy;for(unsigned x=0;x<image->width;++x){auto sx=uint64_t(x)*width/image->width;uint32_t pixel=0;std::memcpy(&pixel,ptr+row*pitch+sx*4,4);image->pixels[size_t(y)*image->width+x]=pixel|0xff000000;}}
     {std::lock_guard lock(mutex);if(!stop&&GetTickCount64()<deadline){frame=image;status.decodedFrames=index;}}
     if(flags&MF_SOURCE_READERF_ENDOFSTREAM)break;
    }

    }
   }catch(const std::exception& e){failure(false,e);}
   while(live()&&audio.playing())if(!wait(started,GetTickCount64()-started+20))break;
  }catch(const std::exception&e){std::lock_guard lock(mutex);if(!stop)status.error=e.what();}
  std::lock_guard lock(mutex);status.running=false;status.finished=true;
 }
};
Player::Player(std::filesystem::path p):impl_(std::make_unique<Impl>(std::move(p))){}
Player::~Player(){impl_->halt();}
void Player::clear(){
 impl_->cancel();impl_->scope=impl_->began=0;impl_->key.clear();impl_->played.clear();
 std::lock_guard lock(impl_->mutex);const bool busy=impl_->worker.joinable()&&!impl_->status.finished;
 impl_->status={};impl_->status.running=busy;impl_->status.finished=!busy;impl_->status.cancelled=true;
}
void Player::select(uint64_t scope,const std::optional<notices::Alert>& alert,uint64_t unixNow,uint64_t now,bool sound){
 if(!scope){clear();return;}
 if(impl_->scope&&impl_->scope!=scope)clear();
 if(!alert||!alert->id||!alert->version||alert->publishedAt>unixNow||(alert->expiresAt&&alert->expiresAt<=unixNow)||(!alert->audio&&!alert->video)){if(!impl_->key.empty())impl_->cancel();return;}
 auto key=std::to_string(alert->id)+":"+std::to_string(alert->version);
 if(impl_->scope==scope&&impl_->key==key){if(now<impl_->began||now-impl_->began>=impl_->lifetime)impl_->cancel();return;}
 if(impl_->played.contains(key)){impl_->cancel();return;}
 if(impl_->played.size()>=128){impl_->cancel();std::lock_guard lock(impl_->mutex);impl_->status.error="Announcement presentation history full for this connection";return;}
 // Never join a live synchronous decoder on a presentation update. The caller
 // repeats select next frame; no new job or revision is consumed while retiring.
 bool busy=false;{std::lock_guard lock(impl_->mutex);busy=impl_->worker.joinable()&&!impl_->status.finished;}
 if(busy){impl_->cancel();return;}
 impl_->halt();impl_->scope=scope;impl_->key=key;impl_->began=now;impl_->lifetime=alert->expiresAt?(std::min)(uint64_t(30000),alert->expiresAt-unixNow):30000;impl_->deadline=GetTickCount64()+impl_->lifetime;impl_->stop=false;
 impl_->played.insert(key);
 {std::lock_guard lock(impl_->mutex);impl_->status.running=true;}
 impl_->worker=std::thread([p=impl_.get(),a=*alert,sound]{p->run(a,sound);});
}
std::shared_ptr<const Frame> Player::frame()const{std::lock_guard lock(impl_->mutex);return impl_->stop||GetTickCount64()>=impl_->deadline?nullptr:impl_->frame;}
Status Player::status()const{std::lock_guard lock(impl_->mutex);return impl_->status;}
bool Player::paint(std::span<uint32_t> pixels,int width,int height,int left,int top)const{
 auto f=frame();if(!f||width!=1280||height!=720||pixels.size()!=size_t(width)*height||left<0||top<0||left>width-320||top>height-180)return false;
 for(int y=0;y<180;++y)for(int x=0;x<320;++x)pixels[size_t(top+y)*width+left+x]=0xff101010;
 const int x0=left+(320-int(f->width))/2,y0=top+(180-int(f->height))/2;
 for(unsigned y=0;y<f->height;++y)std::copy_n(f->pixels.data()+size_t(y)*f->width,f->width,pixels.data()+size_t(y0+y)*width+x0);
 return true;
}
}
