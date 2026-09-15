#include "title_movie.h"
#include <mfapi.h>
#include <mfplay.h>
#include <mferror.h>
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <mutex>
#include <utility>

// Local title movie playback (2026-09-15).
// API contracts checked against Windows SDK 10.0.26100.0/mfplay.h and:
// https://learn.microsoft.com/en-us/windows/win32/medfound/getting-started-with-mfplay
// https://learn.microsoft.com/en-us/windows/win32/medfound/how-to-play-a-sequence-of-files
// https://learn.microsoft.com/en-us/windows/win32/medfound/mfplay-tutorial--video-playback
// MFPlay positions video via its child HWND + UpdateVideo, not SetVideoPosition.
namespace mgo2win::title_movie {
using Microsoft::WRL::ComPtr;
namespace {
struct Event { MFP_EVENT_TYPE kind{}; HRESULT result=S_OK; ComPtr<IMFPMediaItem> item; };
struct Inbox {
 std::mutex mutex;
 bool active=true;
 std::deque<Event> events;
 std::atomic<HRESULT> failure{S_OK};
};
// The callback never references Player, Impl or an HWND. A retired callback
// owns its retired inbox until MFPlay releases it, so late events cannot access
// a destroyed window or be consumed by a later open(). No COM reference cycle.
class Callback final:public IMFPMediaPlayerCallback {
 std::atomic<ULONG> refs_{1};
 const std::shared_ptr<Inbox> inbox_;
public:
 explicit Callback(std::shared_ptr<Inbox> inbox):inbox_(std::move(inbox)){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override {
  if(!out)return E_POINTER;
  *out=nullptr;
  if(iid!=__uuidof(IUnknown)&&iid!=__uuidof(IMFPMediaPlayerCallback))return E_NOINTERFACE;
  *out=static_cast<IMFPMediaPlayerCallback*>(this);AddRef();return S_OK;
 }
 ULONG STDMETHODCALLTYPE AddRef() override {return ++refs_;}
 ULONG STDMETHODCALLTYPE Release() override {const auto n=--refs_;if(!n)delete this;return n;}
 void STDMETHODCALLTYPE OnMediaPlayerEvent(MFP_EVENT_HEADER* header) override {
  if(!header)return;
  const auto kind=header->eEventType;
  if(SUCCEEDED(header->hrEvent)&&kind!=MFP_EVENT_TYPE_MEDIAITEM_CREATED&&
     kind!=MFP_EVENT_TYPE_MEDIAITEM_SET&&kind!=MFP_EVENT_TYPE_PLAY&&
     kind!=MFP_EVENT_TYPE_PAUSE&&kind!=MFP_EVENT_TYPE_PLAYBACK_ENDED&&
     kind!=MFP_EVENT_TYPE_ERROR)return;
  try {
   Event event;event.kind=kind;event.result=header->hrEvent;
   if(kind==MFP_EVENT_TYPE_MEDIAITEM_CREATED&&SUCCEEDED(event.result))
    event.item=MFP_GET_MEDIAITEM_CREATED_EVENT(header)->pMediaItem;
   std::lock_guard lock(inbox_->mutex);
   if(!inbox_->active)return;
   if(inbox_->events.size()>=64){inbox_->failure.store(E_UNEXPECTED);return;}
   inbox_->events.push_back(std::move(event));
  }catch(...){inbox_->failure.store(E_OUTOFMEMORY);}
 }
};
std::string describe(const char* action,HRESULT result) {
 char code[24]{};std::snprintf(code,sizeof(code)," (0x%08lX)",static_cast<unsigned long>(result));
 return std::string(action)+code;
}
}

struct Player::Impl {
 HWND parent=nullptr,child=nullptr;
 DWORD thread=GetCurrentThreadId();
 bool comOwned=false,mfStarted=false;
 ComPtr<IMFPMediaPlayer> player;
 ComPtr<IMFPMediaPlayerCallback> callback;
 std::shared_ptr<Inbox> inbox;
 Status state=Status::idle;
 std::string message;
 bool wantPlay=false,manualPause=false,actualPlaying=false,hasStarted=false,shown=false;
 bool muted=false; // User sound preference persists across closeSource/open.
 enum class Pending {none,play,pause};
 Pending pending=Pending::none;
 ULONGLONG loadingAt=0,commandAt=0;
 explicit Impl(HWND window):parent(window){}
 ~Impl(){
  closeSource();
  if(child){SetWindowLongPtrW(child,GWLP_USERDATA,0);DestroyWindow(child);child=nullptr;}
  if(mfStarted)MFShutdown();
  if(comOwned)CoUninitialize();
 }
 static LRESULT CALLBACK windowProc(HWND window,UINT msg,WPARAM w,LPARAM l) {
  auto* self=reinterpret_cast<Impl*>(GetWindowLongPtrW(window,GWLP_USERDATA));
  if(msg==WM_NCCREATE){
   self=static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
   SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
  }
  switch(msg){
   case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
   case WM_NCHITTEST:return HTTRANSPARENT;
   case WM_ERASEBKGND:return 1;
   case WM_PAINT:{
    PAINTSTRUCT ps{};auto dc=BeginPaint(window,&ps);
    if(!self||!self->player||FAILED(self->player->UpdateVideo()))
     FillRect(dc,&ps.rcPaint,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    EndPaint(window,&ps);return 0;
   }
   // Route resize refresh through WM_PAINT so UpdateVideo always follows
   // BeginPaint, including the first frame after asynchronous source setup.
   case WM_SIZE:InvalidateRect(window,nullptr,FALSE);return 0;
   case WM_NCDESTROY:SetWindowLongPtrW(window,GWLP_USERDATA,0);break;
  }
  return DefWindowProcW(window,msg,w,l);
 }
 bool initialize(){
  if(GetCurrentThreadId()!=thread){message="Title movie called outside its UI thread";state=Status::failed;return false;}
  if(!IsWindow(parent)){message="Title movie parent window unavailable";state=Status::failed;return false;}
  if(!mfStarted){
   const auto co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
   if(SUCCEEDED(co))comOwned=true;
   else if(co!=RPC_E_CHANGED_MODE){fail("Title movie COM initialization",co);return false;}
   // RPC_E_CHANGED_MODE means this UI thread already owns a different COM
   // apartment. Do not balance another owner's initialization at destruction.
   const auto mf=MFStartup(MF_VERSION,MFSTARTUP_FULL);
   if(FAILED(mf)){
    if(comOwned){CoUninitialize();comOwned=false;}
    fail("Title movie Media Foundation initialization",mf);return false;
   }
   mfStarted=true;
  }
  if(!child){
   static const ATOM atom=[](){
    WNDCLASSEXW cls{};cls.cbSize=sizeof(cls);cls.hInstance=GetModuleHandleW(nullptr);
    cls.lpfnWndProc=&Impl::windowProc;cls.lpszClassName=L"MGO2WIN.TitleMovie.Video.v1";
    cls.hbrBackground=static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    return RegisterClassExW(&cls);
   }();
   if(!atom){fail("Title movie video window registration",HRESULT_FROM_WIN32(GetLastError()));return false;}
   child=CreateWindowExW(WS_EX_NOACTIVATE,L"MGO2WIN.TitleMovie.Video.v1",L"",
    WS_CHILD|WS_DISABLED|WS_CLIPSIBLINGS,0,0,16,9,parent,nullptr,GetModuleHandleW(nullptr),this);
   if(!child){fail("Title movie video window creation",HRESULT_FROM_WIN32(GetLastError()));return false;}
  }
  return true;
 }
 void closeSource(){
  if(child)ShowWindow(child,SW_HIDE);
  shown=false;wantPlay=false;manualPause=false;actualPlaying=false;hasStarted=false;pending=Pending::none;
  std::deque<Event> retired;
  if(inbox){std::lock_guard lock(inbox->mutex);inbox->active=false;retired.swap(inbox->events);}
  // No inbox lock held across source/COM release or Shutdown. Any callback
  // already in flight still owns a safe inbox and discards its result.
  retired.clear();
  if(player){player->Shutdown();player.Reset();}
  callback.Reset();inbox.reset();
 }
 void fail(const char* action,HRESULT result){closeSource();message=describe(action,result);state=Status::failed;}
 bool foreground() const {
  const auto root=GetAncestor(parent,GA_ROOT);
  return root&&GetForegroundWindow()==root&&!IsIconic(root);
 }
 void drive(){
  if(!player||pending!=Pending::none||(state!=Status::ready&&state!=Status::playing))return;
  const bool run=wantPlay&&!manualPause&&foreground();
  if(run==actualPlaying)return;
  const auto operation=run?Pending::play:Pending::pause;
  const auto result=run?player->Play():player->Pause();
  if(FAILED(result)){fail(run?"Title movie play":"Title movie pause",result);return;}
  pending=operation;commandAt=GetTickCount64();
 }
 void open(const std::filesystem::path& path){
  closeSource();state=Status::idle;message.clear();
  std::error_code ec;
  // MFP accepts URLs too; this adapter intentionally accepts only an existing
  // absolute local file. No shell launch, credentials or remote URL fallback.
  if(!path.is_absolute()||!std::filesystem::is_regular_file(path,ec)||ec){
   fail("Title movie local file unavailable",HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));return;
  }
  if(!initialize())return;
  inbox=std::make_shared<Inbox>();callback.Attach(new Callback(inbox));
  auto result=MFPCreateMediaPlayer(nullptr,FALSE,MFP_OPTION_FREE_THREADED_CALLBACK,callback.Get(),child,&player);
  if(FAILED(result)){fail("Title movie player creation",result);return;}
  result=player->SetMute(muted?TRUE:FALSE);
  if(FAILED(result)){fail("Title movie mute configuration",result);return;}
  state=Status::loading;loadingAt=GetTickCount64();
  result=player->CreateMediaItemFromURL(path.c_str(),FALSE,0,nullptr);
  if(FAILED(result))fail("Title movie asynchronous file open",result);
 }
 void pump(){
  if(!inbox)return;
  const auto callbackFailure=inbox->failure.load();
  if(FAILED(callbackFailure)){fail("Title movie callback queue",callbackFailure);return;}
  std::deque<Event> events;
  {std::lock_guard lock(inbox->mutex);events.swap(inbox->events);}
  for(auto& event:events){
   if(!player)break;
   if(FAILED(event.result)||event.kind==MFP_EVENT_TYPE_ERROR){
    fail("Title movie media event",FAILED(event.result)?event.result:E_FAIL);break;
   }
   switch(event.kind){
    case MFP_EVENT_TYPE_MEDIAITEM_CREATED:{
     if(!event.item){fail("Title movie source item missing",E_POINTER);break;}
     BOOL video=FALSE,selected=FALSE;
     auto result=event.item->HasVideo(&video,&selected);
     if(SUCCEEDED(result)&&(!video||!selected))result=MF_E_INVALIDMEDIATYPE;
     if(SUCCEEDED(result))result=player->SetMediaItem(event.item.Get());
     if(FAILED(result))fail("Title movie video source selection",result);
     break;
    }
    case MFP_EVENT_TYPE_MEDIAITEM_SET:{
     SIZE native{},aspect{};auto result=player->GetNativeVideoSize(&native,&aspect);
     if(FAILED(result)||native.cx<=0||native.cy<=0){fail("Title movie video size",FAILED(result)?result:MF_E_INVALIDMEDIATYPE);break;}
     // User requested horizontal stretch of the 4:3 source to fill 16:9.
     result=player->SetAspectRatioMode(MFVideoARMode_None);
     if(SUCCEEDED(result))result=player->SetBorderColor(RGB(0,0,0));
     if(FAILED(result)){fail("Title movie video configuration",result);break;}
     state=Status::ready;InvalidateRect(child,nullptr,FALSE);break;
    }
    case MFP_EVENT_TYPE_PLAY:actualPlaying=true;hasStarted=true;state=Status::playing;pending=Pending::none;break;
    case MFP_EVENT_TYPE_PAUSE:actualPlaying=false;state=hasStarted?Status::playing:Status::ready;pending=Pending::none;break;
    case MFP_EVENT_TYPE_PLAYBACK_ENDED:closeSource();state=Status::ended;break;
    default:break;
   }
  }
  if(state==Status::loading&&GetTickCount64()-loadingAt>30000){fail("Title movie load timeout",HRESULT_FROM_WIN32(ERROR_TIMEOUT));return;}
  if(pending!=Pending::none&&GetTickCount64()-commandAt>10000){fail("Title movie control timeout",HRESULT_FROM_WIN32(ERROR_TIMEOUT));return;}
  drive();
 }
};
Player::Player(HWND parent):impl_(std::make_unique<Impl>(parent)){}
Player::~Player()=default;
void Player::open(const std::filesystem::path& path){impl_->open(path);}
void Player::play(){impl_->wantPlay=true;impl_->drive();}
void Player::stop(){impl_->closeSource();impl_->state=Status::idle;impl_->message.clear();}
void Player::pause(bool paused){impl_->manualPause=paused;impl_->drive();}
void Player::mute(bool muted){
 impl_->muted=muted;
 if(impl_->player){
  const auto result=impl_->player->SetMute(muted?TRUE:FALSE);
  if(FAILED(result))impl_->fail("Title movie mute configuration",result);
 }
}
void Player::place(int x,int y,int width,int height){
 if(!impl_->child)return;
 if(width<=0||height<=0){visible(false);return;}
 const int w=static_cast<int>((std::min)(int64_t(width),int64_t(height)*16/9));
 const int h=static_cast<int>((std::min)(int64_t(height),int64_t(width)*9/16));
 SetWindowPos(impl_->child,HWND_TOP,x+(width-w)/2,y+(height-h)/2,(std::max)(1,w),(std::max)(1,h),SWP_NOACTIVATE);
 InvalidateRect(impl_->child,nullptr,FALSE);
}
void Player::visible(bool show){
 impl_->shown=show;
 if(impl_->child)ShowWindow(impl_->child,show?SW_SHOWNOACTIVATE:SW_HIDE);
}
void Player::pump(){impl_->pump();}
Status Player::status()const{return impl_->state;}
std::string Player::error()const{return impl_->message;}
}
