#include "multi_ui_audio.h"
#include <windows.h>
#include <mfapi.h>
#include <mfplay.h>
#include <mferror.h>
#include <wrl/client.h>
#include <atomic>
#include <cmath>
#include <deque>
#include <mutex>
#include <cstdio>
#include <cwctype>
namespace mgo2mt::multi_ui {
namespace {
using Microsoft::WRL::ComPtr;
struct Event {MFP_EVENT_TYPE kind{};HRESULT result=S_OK;ComPtr<IMFPMediaItem> item;};
struct Inbox {std::mutex mutex;bool active=true;std::deque<Event> events;std::atomic<HRESULT> failure{S_OK};};
class Callback final:public IMFPMediaPlayerCallback {
 std::atomic<ULONG> refs_{1};std::shared_ptr<Inbox> inbox_;
public:
 explicit Callback(std::shared_ptr<Inbox> i):inbox_(std::move(i)){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void**out)override{if(!out)return E_POINTER;*out=nullptr;if(id!=__uuidof(IUnknown)&&id!=__uuidof(IMFPMediaPlayerCallback))return E_NOINTERFACE;*out=static_cast<IMFPMediaPlayerCallback*>(this);AddRef();return S_OK;}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}
 ULONG STDMETHODCALLTYPE Release()override{auto n=--refs_;if(!n)delete this;return n;}
 void STDMETHODCALLTYPE OnMediaPlayerEvent(MFP_EVENT_HEADER*h)override{if(!h)return;if(SUCCEEDED(h->hrEvent)&&h->eEventType!=MFP_EVENT_TYPE_MEDIAITEM_CREATED&&h->eEventType!=MFP_EVENT_TYPE_MEDIAITEM_SET&&h->eEventType!=MFP_EVENT_TYPE_PLAY&&h->eEventType!=MFP_EVENT_TYPE_PLAYBACK_ENDED&&h->eEventType!=MFP_EVENT_TYPE_ERROR)return;try{Event e{h->eEventType,h->hrEvent};if(e.kind==MFP_EVENT_TYPE_MEDIAITEM_CREATED&&SUCCEEDED(e.result))e.item=MFP_GET_MEDIAITEM_CREATED_EVENT(h)->pMediaItem;std::lock_guard lock(inbox_->mutex);if(!inbox_->active)return;if(inbox_->events.size()>=64){inbox_->failure=E_UNEXPECTED;return;}inbox_->events.push_back(std::move(e));}catch(...){inbox_->failure=E_OUTOFMEMORY;}}
};
std::string describe(const char*action,HRESULT result){char code[24]{};std::snprintf(code,sizeof(code)," (0x%08lX)",static_cast<unsigned long>(result));return std::string(action)+code;}
class Player final:public AudioBackend {
 ComPtr<IMFPMediaPlayer> player_;ComPtr<IMFPMediaPlayerCallback> callback_;std::shared_ptr<Inbox> inbox_;
 DWORD thread_=GetCurrentThreadId();bool com_=false,mf_=false,muted_=false,started_=false;uint64_t token_=0,opened_=0;float volume_=1;
 bool initialize(std::string&error){if(GetCurrentThreadId()!=thread_){error="UI audio called outside its UI thread";return false;}if(mf_)return true;auto hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(SUCCEEDED(hr))com_=true;else if(hr!=RPC_E_CHANGED_MODE){error=describe("UI audio COM",hr);return false;}hr=MFStartup(MF_VERSION,MFSTARTUP_FULL);if(FAILED(hr)){error=describe("UI audio Media Foundation",hr);if(com_){CoUninitialize();com_=false;}return false;}mf_=true;return true;}
public:
 ~Player()override{stop();if(mf_)MFShutdown();if(com_)CoUninitialize();}
 void stop()override{std::deque<Event> retired;if(inbox_){std::lock_guard lock(inbox_->mutex);inbox_->active=false;retired.swap(inbox_->events);}retired.clear();if(player_){player_->Shutdown();player_.Reset();}callback_.Reset();inbox_.reset();token_=0;started_=false;}
 void mute(bool m)override{muted_=m;if(player_)player_->SetMute(m?TRUE:FALSE);}
 bool start(const std::filesystem::path&p,float volume,uint64_t token,std::string&error)override{stop();error.clear();std::error_code ec;if(!token||!std::isfinite(volume)||volume<0||volume>1||!p.is_absolute()||!std::filesystem::is_regular_file(p,ec)||ec){error="UI audio needs a bounded local file and volume";return false;}auto ext=p.extension().wstring();for(auto&c:ext)c=std::towlower(c);if((ext!=L".wav"&&ext!=L".mp3")||std::filesystem::file_size(p,ec)>64*1024*1024||ec){error="UI audio supports WAV/MP3 up to 64 MiB";return false;}if(!initialize(error))return false;
  inbox_=std::make_shared<Inbox>();callback_.Attach(new Callback(inbox_));auto hr=MFPCreateMediaPlayer(nullptr,FALSE,MFP_OPTION_FREE_THREADED_CALLBACK,callback_.Get(),nullptr,&player_);if(SUCCEEDED(hr))hr=player_->SetMute(muted_?TRUE:FALSE);if(SUCCEEDED(hr))hr=player_->SetVolume(volume);if(SUCCEEDED(hr))hr=player_->CreateMediaItemFromURL(p.c_str(),FALSE,0,nullptr);if(FAILED(hr)){error=describe("UI audio open",hr);stop();return false;}volume_=volume;token_=token;opened_=GetTickCount64();return true;
 }
 std::vector<AudioNotification> poll()override{std::vector<AudioNotification> out;if(!inbox_)return out;auto fail=[&](const char*label,HRESULT hr){out.push_back({token_,AudioResult::failed,describe(label,hr)});stop();};if(FAILED(inbox_->failure)){fail("UI audio callback",inbox_->failure);return out;}std::deque<Event> events;{std::lock_guard lock(inbox_->mutex);events.swap(inbox_->events);}for(auto&e:events){if(!player_)break;if(FAILED(e.result)||e.kind==MFP_EVENT_TYPE_ERROR){fail("UI audio playback",FAILED(e.result)?e.result:E_FAIL);break;}switch(e.kind){
   case MFP_EVENT_TYPE_MEDIAITEM_CREATED:{BOOL audio=FALSE,selected=FALSE,video=FALSE;HRESULT hr=e.item?e.item->HasAudio(&audio,&selected):E_POINTER;if(SUCCEEDED(hr)&&(!audio||!selected))hr=MF_E_INVALIDMEDIATYPE;if(SUCCEEDED(hr))hr=e.item->HasVideo(&video,nullptr);if(SUCCEEDED(hr)&&video)hr=MF_E_INVALIDMEDIATYPE;if(SUCCEEDED(hr))hr=player_->SetMediaItem(e.item.Get());if(FAILED(hr))fail("UI audio source",hr);break;}
   case MFP_EVENT_TYPE_MEDIAITEM_SET:{PROPVARIANT duration{};auto hr=player_->GetDuration(MFP_POSITIONTYPE_100NS,&duration);if(SUCCEEDED(hr)&&(duration.vt!=VT_UI8||!duration.uhVal.QuadPart||duration.uhVal.QuadPart>6000000000ull))hr=MF_E_OUT_OF_RANGE;PropVariantClear(&duration);if(SUCCEEDED(hr))hr=player_->Play();if(FAILED(hr))fail("UI audio start/duration",hr);break;}
   case MFP_EVENT_TYPE_PLAY:started_=true;out.push_back({token_,AudioResult::started,{}});break;
   case MFP_EVENT_TYPE_PLAYBACK_ENDED:if(started_)out.push_back({token_,AudioResult::completed,{}});else out.push_back({token_,AudioResult::failed,"UI audio ended before playback started"});stop();break;
   default:break;
  }}if(player_&&!started_&&GetTickCount64()-opened_>30000)fail("UI audio load timeout",HRESULT_FROM_WIN32(ERROR_TIMEOUT));return out;
 }
};
}
std::unique_ptr<AudioBackend> make_audio_backend(){return std::make_unique<Player>();}
}
