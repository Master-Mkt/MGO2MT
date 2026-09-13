#include "build_version.h"
#include "combat_spawn_profile.h"
#include "water_effects_overlay.h"
#include "bullet_decals_overlay.h"
#include "material_effects_overlay.h"
#include "local_playtest.h"
#include "combat_audio.h"
#include "footstep_presentation.h"
#include "combat_light_effects.h"
#include "remote_avatar.h"
#include "sop_visuals.h"
#include "sop_presentation.h"
#include "special_action_motion.h"
#include "enemy_tag_target.h"
#include "player_lock.h"
#include "enemy_name_tag.h"
#include "menu_audio.h"
#include "selection_model.h"
#include "music_menu.h"
#include "invitation_presenter.h"
#include "invitation_input_guard.h"
// Partial title preview with optional native GCX subset; full game boot is pending.
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <syncstream>
#include "agreement_screen.h"
#include "login_screen.h"
#include "character_renderer.h"
#include "stage_assets.h"
#include "stage_music.h"
#include "round_music.h"
#include "stage_debug.h"
#include "stage_navigation.h"
#include "player_control.h"
#include "player_menu.h"
#include "preset_radio_audio.h"
#include "player_motion.h"
#include "player_ragdoll.h"
#include <future>
#include "character_catalog.h"
#include <stdexcept>
#include <thread>
#include <vector>
#include <memory>
#include "title_animation.h"
#include "title_gcx.h"
#include "audio_control.h"
#include "audio_fade.h"
using Microsoft::WRL::ComPtr;
int run_audio_probe(int,wchar_t**,const std::atomic_bool*,const mgo2win::AudioControl*);
static void check(HRESULT hr) { if(FAILED(hr)) throw std::runtime_error("D3D HRESULT " + std::to_string(static_cast<unsigned long>(hr))); }
static std::vector<char> file(const std::filesystem::path& path) {
 std::ifstream f(path,std::ios::binary|std::ios::ate); if(!f) throw std::runtime_error("Cannot open asset");
 auto n=f.tellg();if(n<0||n>128*1024*1024)throw std::runtime_error("Asset size limit");
 std::vector<char>b(static_cast<size_t>(n));f.seekg(0);f.read(b.data(),n);if(!f)throw std::runtime_error("Asset read");return b;
}
static uint32_t u32(const std::vector<char>& b,size_t p){if(p+4>b.size())throw std::runtime_error("Truncated asset");uint32_t v;std::memcpy(&v,b.data()+p,4);return v;}
using mgo2win::Vertex;
using mgo2win::Quad;
static_assert(sizeof(Quad)==140);
struct Window {
 static inline bool localTest=false;
 std::wstring instanceLabel;
 void title(const wchar_t* value){SetWindowTextW(handle,mgo2win::versioned_title(instanceLabel.empty()?std::wstring(value):instanceLabel+L" | "+value).c_str());}
 static inline uint32_t pressed=0;
 static inline bool stageAudition=false;
 static inline bool inspection=false;
 static inline bool gameplayMode=false;
 static inline bool testRagdoll=false,testBody=false;
 static inline mgo2win::PlayerMenu* playerMenu=nullptr;
 static inline mgo2win::MusicMenu* musicMenu=nullptr;
 static inline mgo2win::invitation_ui::Presenter* invitation=nullptr;
 static inline bool invitationChar=false;
 static inline mgo2win::invitation_ui::InputGuard invitationGuard;
 static inline std::vector<unsigned> uiCues;
 static void cue(unsigned c){if(uiCues.size()<32)uiCues.push_back(c);}
 static inline mgo2win::stage::DebugControls debug;
 static inline std::optional<mgo2win::host::LoadRequest> resetRequest;
 static inline unsigned agreementInput=0;
 static inline bool agreementActive=false;
 static inline mgo2win::LoginScreen* login=nullptr;
 static inline mgo2win::ControllerInput* input=nullptr;
 static inline float viewX=0,viewY=0,viewW=1,viewH=1;
 HWND handle=nullptr;
 ~Window(){login=nullptr;input=nullptr;playerMenu=nullptr;musicMenu=nullptr;invitation=nullptr;invitationGuard.reset();uiCues.clear();if(handle)DestroyWindow(handle);}
 static LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
  if(localTest&&GetForegroundWindow()!=hwnd&&!(msg==WM_KEYDOWN&&(lp&(1LL<<25)))&&((msg>=WM_KEYFIRST&&msg<=WM_KEYLAST)||(msg>=WM_MOUSEFIRST&&msg<=WM_MOUSELAST)))return 0;
  if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(r.right&&r.bottom){float x=float(short(LOWORD(lp)))/r.right,y=float(short(HIWORD(lp)))/r.bottom;if(x<viewX||y<viewY||x>=viewX+viewW||y>=viewY+viewH)return 0;lp=MAKELPARAM(int((x-viewX)/viewW*r.right),int((y-viewY)/viewH*r.bottom));}}
  const bool wasInvitationModal=invitation&&invitation->overlay().visible();
  if(invitation)invitation->update(GetTickCount64());
  const bool invitationModal=invitation&&invitation->overlay().visible();
  if((msg==WM_LBUTTONDOWN||msg==WM_LBUTTONUP)&&invitationGuard.left_button(msg==WM_LBUTTONDOWN,invitationModal))return 0;
  // A timeout, disconnect or replacement can close the dialog between paint
  // and input. Do not deliver that old dialog's decision to the room beneath it.
  if(((wasInvitationModal&&!invitationModal)||invitationGuard.stale(invitationModal))&&
     ((msg>=WM_KEYFIRST&&msg<=WM_KEYLAST)||(msg>=WM_MOUSEFIRST&&msg<=WM_MOUSELAST)||msg==WM_IME_STARTCOMPOSITION||msg==WM_IME_COMPOSITION||msg==WM_IME_ENDCOMPOSITION)){
   if(msg==WM_KEYDOWN)invitationChar=true;return 0;
  }
  if(msg==WM_CHAR&&invitationChar)return 0;
  if(msg==WM_KEYDOWN)invitationChar=false;
  if(msg==WM_KILLFOCUS&&invitation)invitation->overlay().close();
  // Only the explicit START/options menu shortcut opens an invitation dialog.
  // Keep text entry and controller/key binding capture out of this route.
  if(invitation&&playerMenu&&playerMenu->settings_menu()&&!playerMenu->capturing()&&msg==WM_KEYDOWN&&!(lp&(1LL<<30))&&
     (wp==VK_F6||(input&&!input->config.device&&wp==input->config.keyboard[7]))&&invitation->overlay().open(GetTickCount64())){
   playerMenu->close();invitationChar=true;cue(mgo2win::menu_audio::Confirm);return 0;
  }
  if(invitation&&invitation->overlay().visible()){
   auto& overlay=invitation->overlay();
   auto confirm=[&]{if(auto response=overlay.confirm(GetTickCount64())){const auto queued=invitation->respond(*response,GetTickCount64());cue(queued?mgo2win::menu_audio::Confirm:mgo2win::menu_audio::Cancel);}};
   if(msg==WM_KEYDOWN){invitationChar=true;if(lp&(1LL<<30))return 0;
    if(input&&!(lp&(1LL<<25))&&wp!=VK_ESCAPE&&wp!=VK_RETURN&&!(wp>=VK_LEFT&&wp<=VK_DOWN)){if(wp==input->config.keyboard[12])wp=VK_ESCAPE;else if(auto key=input->keyboard_menu(unsigned(wp)))wp=key;}
    if(wp==VK_ESCAPE){overlay.close();cue(mgo2win::menu_audio::Cancel);}
    else if(wp==VK_RETURN)confirm();
    else if(wp==VK_LEFT||wp==VK_UP||wp==VK_RIGHT||wp==VK_DOWN){overlay.move(wp==VK_LEFT||wp==VK_UP?-1:1);cue(mgo2win::menu_audio::Cursor);}return 0;
   }
   if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(r.right&&r.bottom)if(auto accept=overlay.hit_test(int(short(LOWORD(lp)))*1280/r.right,int(short(HIWORD(lp)))*720/r.bottom)){overlay.choose(*accept);confirm();}return 0;}
   if(msg==WM_KEYUP||msg==WM_CHAR||msg==WM_LBUTTONDOWN||msg==WM_MOUSEWHEEL||msg==WM_IME_STARTCOMPOSITION||msg==WM_IME_COMPOSITION||msg==WM_IME_ENDCOMPOSITION)return 0;
  }
  if(playerMenu&&playerMenu->visible()&&!playerMenu->capturing()&&!playerMenu->text_entry()&&input&&!input->config.device&&msg==WM_KEYDOWN&&!(lp&(1LL<<30))&&wp==input->config.keyboard[12]){playerMenu->close(true);return 0;}
  if((!login||(musicMenu&&musicMenu->visible())||(playerMenu&&playerMenu->visible()&&!playerMenu->capturing()&&!playerMenu->text_entry()))&&input&&msg==WM_KEYDOWN&&!(lp&(1LL<<25))&&wp!=VK_ESCAPE&&wp!=VK_RETURN&&!(wp>=VK_LEFT&&wp<=VK_DOWN)){auto mapped=input->keyboard_menu(unsigned(wp));if(mapped)wp=mapped;}
  if(msg==WM_KILLFOCUS){debug.focus_lost();testRagdoll=testBody=false;}
  if(msg==WM_KEYDOWN&&wp==VK_F5&&!(lp&(1LL<<30))&&login&&playerMenu&&!debug.confirmReset&&!login->personal_overlay_visible()&&(!musicMenu||!musicMenu->visible())&&!playerMenu->visible())if(auto session=login->chat_session();session&&session->state().joined){playerMenu->chat_session(session);playerMenu->open(mgo2win::player::Menu::chat);return 0;}
  if(musicMenu&&musicMenu->visible()&&musicMenu->message(hwnd,msg,wp,lp))return 0;
  if(playerMenu&&playerMenu->visible()&&playerMenu->message(hwnd,msg,wp,lp))return 0;
  if(playerMenu&&playerMenu->text_entry()&&(msg==WM_KEYDOWN||msg==WM_KEYUP||msg==WM_CHAR||msg==WM_IME_STARTCOMPOSITION||msg==WM_IME_COMPOSITION||msg==WM_IME_ENDCOMPOSITION))return DefWindowProcW(hwnd,msg,wp,lp);
  auto currentStage=login?login->stage_request():std::nullopt;
  if((debug.confirmReset||debug.reset)&&resetRequest!=currentStage)debug.cancel_reset();
  if(msg==WM_KEYDOWN){bool wasOpen=debug.confirmReset,wasYes=debug.resetYes,wasEnabled=debug.enabled;int oldStep=debug.musicStep;bool oldToggle=debug.toggleMusic;
   if(debug.key(unsigned(wp),bool(lp&(1LL<<30)),bool(currentStage))){
    if(!wasOpen&&debug.confirmReset){resetRequest=currentStage;cue(mgo2win::menu_audio::Confirm);}
    else if(wasOpen&&!debug.confirmReset)cue(debug.reset?mgo2win::menu_audio::Confirm:mgo2win::menu_audio::Cancel);
    else if(wasYes!=debug.resetYes||oldStep!=debug.musicStep)cue(mgo2win::menu_audio::Cursor);
    else if(wasEnabled!=debug.enabled)cue(debug.enabled?mgo2win::menu_audio::Confirm:mgo2win::menu_audio::Cancel);
    else if(oldToggle!=debug.toggleMusic)cue(mgo2win::menu_audio::Confirm);
    return 0;
   }}
  if(debug.confirmReset){
   if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(r.right&&r.bottom){debug.click(int(short(LOWORD(lp)))*1280/r.right,int(short(HIWORD(lp)))*720/r.bottom);if(!debug.confirmReset)cue(debug.reset?mgo2win::menu_audio::Confirm:mgo2win::menu_audio::Cancel);}return 0;}
   if(msg==WM_LBUTTONDOWN||msg==WM_MOUSEWHEEL||msg==WM_CHAR||msg==WM_KEYUP)return 0;
  }
  if(debug.enabled&&currentStage&&msg==WM_KEYDOWN&&wp==VK_F10){if(!(lp&(1LL<<30)))inspection=!inspection;return 0;}
  if(debug.enabled&&inspection&&currentStage&&msg==WM_KEYDOWN&&wp==VK_F11){if(!(lp&(1LL<<30))){if(GetKeyState(VK_SHIFT)&0x8000)testBody=true;else testRagdoll=true;}return 0;}
  if(debug.enabled&&inspection&&currentStage&&input&&!input->config.device){
   if(msg==WM_KEYDOWN||msg==WM_KEYUP)for(unsigned a=4;a<24;++a)if(wp==input->config.keyboard[a])return 0;
   if(msg==WM_CHAR)return 0;
  }
  if(login&&msg==WM_KEYDOWN&&wp==VK_F6&&!(lp&(1LL<<30))&&login->stage_request()){stageAudition=true;return 0;}
  if(login&&login->message(hwnd,msg,wp,lp))return 0;
  if(!login&&agreementActive&&msg==WM_KEYDOWN&&wp==VK_ESCAPE){if(!(lp&(1LL<<30)))agreementInput|=mgo2win::AgreementScreen::cancel;return 0;}
  if(msg==WM_CLOSE || (msg==WM_KEYDOWN&&wp==VK_ESCAPE)){PostQuitMessage(0);return 0;}
  if(msg==WM_MOUSEWHEEL){agreementInput |= static_cast<short>(HIWORD(wp))>0?8:16;return 0;}
  if(msg==WM_KEYDOWN&&!(lp&(1LL<<30))){switch(wp){case VK_LEFT:agreementInput|=1;break;case VK_RIGHT:agreementInput|=2;break;case VK_RETURN:agreementInput|=4;break;case VK_UP:agreementInput|=8;break;case VK_DOWN:agreementInput|=16;break;case VK_PRIOR:agreementInput|=32;break;case VK_NEXT:agreementInput|=64;break;case VK_HOME:agreementInput|=128;break;case VK_END:agreementInput|=256;break;case 'R':agreementInput|=512;break;}}
  if(msg==WM_KEYDOWN&&wp==VK_RETURN&&!(lp&(1LL<<30))){pressed|=8;return 0;}
  return DefWindowProcW(hwnd,msg,wp,lp);
 }
 void create(){WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"MGO2WIN_AssetPreview";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
  if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("Register window");
  RECT rect{0,0,1280,720};AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
  handle=CreateWindowW(wc.lpszClassName,(mgo2win::versioned_title(L"MGO2WIN - Partial title asset preview (not game boot)")+instanceLabel).c_str(),WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,wc.hInstance,nullptr);
  if(!handle)throw std::runtime_error("Create window");ShowWindow(handle,SW_SHOW);
 }
};
struct AudioThread {
 std::atomic_bool stop{false}; std::atomic_int result{-1}; std::thread thread;
 mgo2win::AudioControl control;
 ~AudioThread(){stop=true;if(thread.joinable())thread.join();}
 void start(const std::filesystem::path& wav,const std::wstring& seconds,unsigned cue=0,const char* stream=nullptr){control.cue=cue;control.stream=stream?stream:(wav.stem()==L"lobby"||wav.stem()==L"bgm_mgo_lobby01")?"lobby_bgm":"other";thread=std::thread([this,wav,seconds]{
  std::vector<std::wstring> args{L"audio",wav.wstring(),seconds};std::vector<wchar_t*> pointers;for(auto& a:args)pointers.push_back(a.data());result=run_audio_probe(static_cast<int>(args.size()),pointers.data(),&stop,&control);
 });}
};
// An orange inspection capsule; no original object or network identity is implied.
static mgo2win::CharacterModel physics_capsule(const mgo2win::physics::RigidBody& body){
 using namespace mgo2win;using namespace mgo2win::physics;CharacterModel model;
 model.bounds={-400,-600,-400,400,600,400};model.textures.push_back({4,4,9,{0x20,0xfc,0x20,0xfc,0,0,0,0}});
 constexpr unsigned rings=18,sides=20;constexpr float pi=3.14159265359f;
 for(unsigned y=0;y<=rings;++y){float latitude=-pi*.5f+pi*float(y)/rings;for(unsigned x=0;x<=sides;++x){float angle=2*pi*float(x)/sides;
  Vec3 normal={std::cos(latitude)*std::cos(angle),std::sin(latitude),std::cos(latitude)*std::sin(angle)};
  Vec3 p=mul(normal,body.radius);p[1]+=(y<rings/2?-1.f:1.f)*body.halfLength;p=rotate(body.rotation,p);normal=rotate(body.rotation,normal);
  ModelVertex vertex{};vertex.x=p[0];vertex.y=p[1];vertex.z=p[2];vertex.nx=normal[0];vertex.ny=normal[1];vertex.nz=normal[2];model.vertices.push_back(vertex);
 }}
 for(unsigned y=0;y<rings;++y)for(unsigned x=0;x<sides;++x){unsigned a=y*(sides+1)+x,b=a+sides+1;for(unsigned i:{a,b,a+1,a+1,b,b+1})model.indices.push_back(i);}
 model.parts.push_back({0,unsigned(model.indices.size()),0,0});return model;
}
static void capture(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* source,const std::filesystem::path& output){
 D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
 ComPtr<ID3D11Texture2D> staging;check(device->CreateTexture2D(&desc,nullptr,&staging));context->CopyResource(staging.Get(),source);
 D3D11_MAPPED_SUBRESOURCE map{};check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map));
 std::vector<unsigned char> pixels(desc.Width*desc.Height*4);
 for(UINT y=0;y<desc.Height;y++)for(UINT x=0;x<desc.Width;x++){
  auto p=static_cast<const unsigned char*>(map.pData)+y*map.RowPitch+4*x;auto q=pixels.data()+(y*desc.Width+x)*4;q[0]=p[2];q[1]=p[1];q[2]=p[0];q[3]=255;
 }
 context->Unmap(staging.Get(),0);BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+static_cast<DWORD>(pixels.size());
 BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=desc.Width;info.biHeight=-static_cast<LONG>(desc.Height);info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
 std::ofstream f(output,std::ios::binary);f.write(reinterpret_cast<char*>(&header),sizeof(header));f.write(reinterpret_cast<char*>(&info),sizeof(info));f.write(reinterpret_cast<char*>(pixels.data()),pixels.size());if(!f)throw std::runtime_error("Capture write");
}
int run_title_preview(int argc,wchar_t**argv){try{
 if(argc<4)throw std::runtime_error("Usage: mgo2win_title_preview scene seconds capture.bmp [--audio] [--scripted-input] [--gcx file --entry procedure --wav file]");
 double seconds=std::stod(argv[2]);if(!(seconds>0&&seconds<=600))throw std::runtime_error("Duration limit");
 mgo2win::LocalPlaytest playtest;
 bool sound=false,scripted=false,scriptedNo=false,scriptedLogin=false,scriptedPorts=false,scriptedStun=false,scriptedControls=false,scriptedGraphics=false,scriptedCharacters=false,scriptedSlots=false,scriptedSelection=false,scriptedAppearance=false,scriptedCreation=false,safeGraphics=false,saveCaptures=true;std::filesystem::path gcxPath,wavPath,sePath,loadingPath,menuCancelPath,menuConfirmPath,menuMovePath,agreementBackgroundPath,lobbyMusicPath,motionPath,loginPath,networkKeys,modelPath,catalogPath,voiceDirectory;std::wstring policyUrl;uint32_t entry=18;
 for(int i=4;i<argc;++i){std::wstring arg=argv[i];if(arg==L"--audio")sound=true;else if(arg==L"--scripted-input")scripted=true;else if(arg==L"--scripted-no"){scripted=true;scriptedNo=true;}
  else if(arg==L"--scripted-login"){scripted=true;scriptedLogin=true;}
  else if(arg==L"--scripted-ports"){scripted=true;scriptedPorts=true;}
  else if(arg==L"--scripted-stun"){scripted=true;scriptedStun=true;}
  else if(arg==L"--scripted-slots"){scripted=true;scriptedSlots=true;}
  else if(arg==L"--scripted-creation"){scripted=true;scriptedCreation=true;}
  else if(arg==L"--scripted-selection"){scripted=true;scriptedSelection=true;}
  else if(arg==L"--scripted-appearance"){scripted=true;scriptedAppearance=true;}
  else if(arg==L"--scripted-characters"){scripted=true;scriptedCharacters=true;}
  else if(arg==L"--character-catalog"&&i+1<argc)catalogPath=argv[++i];
  else if(arg==L"--voice-directory"&&i+1<argc)voiceDirectory=argv[++i];
  else if(arg==L"--character-model"&&i+1<argc)modelPath=argv[++i];
  else if(arg==L"--network-keys"&&i+1<argc)networkKeys=argv[++i];
  else if(arg==L"--scripted-controls"){scripted=true;scriptedControls=true;}
  else if(arg==L"--scripted-graphics"){scripted=true;scriptedGraphics=true;}
  else if(auto test=mgo2win::LocalPlaytest::parse(arg);test.enabled){if(playtest.enabled)throw std::runtime_error("Duplicate local playtest role");playtest=test;}
  else if(arg==L"--safe-graphics")safeGraphics=true;
  else if(arg==L"--login-background"&&i+1<argc)loginPath=argv[++i];
  else if(arg==L"--no-capture")saveCaptures=false;
  else if(arg==L"--se"&&i+1<argc)sePath=argv[++i];
  else if(arg==L"--policy-url"&&i+1<argc)policyUrl=argv[++i];
  else if(arg==L"--menu-cancel"&&i+1<argc)menuCancelPath=argv[++i];
  else if(arg==L"--menu-confirm"&&i+1<argc)menuConfirmPath=argv[++i];
  else if(arg==L"--menu-move"&&i+1<argc)menuMovePath=argv[++i];
  else if(arg==L"--agreement-motion"&&i+1<argc)motionPath=argv[++i];
  else if(arg==L"--agreement-background"&&i+1<argc)agreementBackgroundPath=argv[++i];
  else if(arg==L"--lobby-music"&&i+1<argc)lobbyMusicPath=argv[++i];
  else if(arg==L"--loading"&&i+1<argc)loadingPath=argv[++i];
  else if((arg==L"--gcx"||arg==L"--entry"||arg==L"--wav")&&i+1<argc){++i;if(arg==L"--gcx")gcxPath=argv[i];else if(arg==L"--wav")wavPath=argv[i];else {auto n=std::stoul(argv[i]);if(!n||n>32767)throw std::runtime_error("Entry range");entry=static_cast<uint32_t>(n);}}
  else throw std::runtime_error("Unknown/missing option");}
 if(playtest.enabled&&scripted)throw std::runtime_error("Local playtest cannot use scripted input");
 auto path=std::filesystem::absolute(argv[1]);auto bytes=file(path);
 std::unique_ptr<mgo2win::TitleGcx> gcx;
 if(!gcxPath.empty()){gcx=std::make_unique<mgo2win::TitleGcx>(file(gcxPath),std::cout);gcx->start(entry);}
 std::unique_ptr<mgo2win::TitleAnimation> animation;uint32_t textureCount=0,count=0;std::vector<Quad>quads;
 if(bytes.size()>=4&&!std::memcmp(bytes.data(),"M2AN",4)){animation=std::make_unique<mgo2win::TitleAnimation>(bytes,gcx?gcx->timeout():18000);textureCount=animation->texture_count();quads=animation->geometry();count=static_cast<uint32_t>(quads.size());}
 else{if(scripted)throw std::runtime_error("Scripted input requires animation");if(bytes.size()<16||std::memcmp(bytes.data(),"M2PV",4)||u32(bytes,4)!=1||!u32(bytes,12)||u32(bytes,12)>64)throw std::runtime_error("Preview header");textureCount=u32(bytes,12);count=u32(bytes,8);if(count>10000||bytes.size()!=16+size_t(count)*sizeof(Quad))throw std::runtime_error("Preview extent");quads.resize(count);std::memcpy(quads.data(),bytes.data()+16,count*sizeof(Quad));}
 std::unique_ptr<mgo2win::AgreementScreen> agreement;
 if(!policyUrl.empty()){if(loadingPath.empty()||menuCancelPath.empty()||menuConfirmPath.empty()||menuMovePath.empty())throw std::runtime_error("Agreement needs loading and menu sounds");agreement=std::make_unique<mgo2win::AgreementScreen>(policyUrl);}
 std::vector<Quad> agreementBackground;unsigned backgroundTextures=0;
 if(!agreementBackgroundPath.empty()){
  auto b=file(agreementBackgroundPath);if(b.size()<16||std::memcmp(b.data(),"M2PV",4)||u32(b,4)!=1||!u32(b,12)||u32(b,12)>64||u32(b,8)>=10000||b.size()!=16+size_t(u32(b,8))*sizeof(Quad))throw std::runtime_error("Agreement background extent");
  backgroundTextures=u32(b,12);agreementBackground.resize(u32(b,8));std::memcpy(agreementBackground.data(),b.data()+16,agreementBackground.size()*sizeof(Quad));
  for(const auto&q:agreementBackground){if(q.atlas< -1||q.atlas>=static_cast<int>(backgroundTextures)||q.blend<0||q.blend>1)throw std::runtime_error("Background draw range");for(const auto&v:q.vertices){float f[8];std::memcpy(f,&v,sizeof(v));for(float x:f)if(!std::isfinite(x))throw std::runtime_error("Background vertex");}}
 }
 std::vector<Quad> loginBackground;unsigned loginTextures=0;
 if(!loginPath.empty()){
  auto b=file(loginPath);if(b.size()<16||std::memcmp(b.data(),"M2PV",4)||u32(b,4)!=1||!u32(b,12)||u32(b,12)>64||u32(b,8)>=10000||b.size()!=16+size_t(u32(b,8))*sizeof(Quad))throw std::runtime_error("Agreement background extent");
  loginTextures=u32(b,12);loginBackground.resize(u32(b,8));std::memcpy(loginBackground.data(),b.data()+16,loginBackground.size()*sizeof(Quad));
  for(const auto&q:loginBackground){if(q.atlas< -1||q.atlas>=static_cast<int>(loginTextures)||q.blend<0||q.blend>1)throw std::runtime_error("Background draw range");for(const auto&v:q.vertices){float f[8];std::memcpy(f,&v,sizeof(v));for(float x:f)if(!std::isfinite(x))throw std::runtime_error("Background vertex");}}
 }
 std::unique_ptr<mgo2win::TitleAnimation> motionBack,motionFront;
 if(!motionPath.empty()){auto b=file(motionPath);motionBack=std::make_unique<mgo2win::TitleAnimation>(b);motionFront=std::make_unique<mgo2win::TitleAnimation>(b,18000,true);if(motionBack->texture_count()!=8)throw std::runtime_error("Background motion textures");}
 std::vector<Vertex>vertices;
 if(gcx&&!animation)throw std::runtime_error("GCX requires animated title asset");
 std::unique_ptr<mgo2win::TitleAnimation> loading;
 if(!loadingPath.empty()){
  if(!gcx)throw std::runtime_error("Loading requires GCX");
  loading=std::make_unique<mgo2win::TitleAnimation>(file(loadingPath));
  if(loading->texture_count()!=2)throw std::runtime_error("Loading textures");
 }
 if(animation)animation->set_callbacks([&](uint32_t result){std::osyncstream(std::cout)<<"{\"host_callback\":\"selected\",\"result\":"<<result<<",\"tick\":"<<animation->ticks()<<"}"<<std::endl;if(gcx)gcx->callback(false,result);},[&](uint32_t result){std::osyncstream(std::cout)<<"{\"host_callback\":\"completed\",\"result\":"<<result<<",\"tick\":"<<animation->ticks()<<"}"<<std::endl;if(gcx)gcx->callback(true,result);});
 for(const auto&q:quads){if(q.atlas< -1||q.atlas>=static_cast<int>(textureCount)||q.blend<0||q.blend>1)throw std::runtime_error("Preview draw range");for(const auto&v:q.vertices){float values[8];std::memcpy(values,&v,sizeof(v));for(float value:values)if(!std::isfinite(value))throw std::runtime_error("Nonfinite vertex");}for(int i:{0,1,2,0,2,3})vertices.push_back(q.vertices[i]);}
 if(vertices.empty())throw std::runtime_error("Empty scene");
 std::filesystem::path inputPath;if(scripted)inputPath=std::filesystem::path(argv[3]).parent_path()/L"input.cfg";else{wchar_t local[32768];DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);if(!n||n>=32768)throw std::runtime_error("Input settings directory unavailable");inputPath=playtest.profile(std::filesystem::path(local)/L"MGO2WIN")/L"input.cfg";}
 mgo2win::LocalPlaytestSession playtestSession;playtestSession.prepare(playtest,inputPath.parent_path());
 auto controllerInput=std::make_shared<mgo2win::ControllerInput>(inputPath);
 Window window;window.instanceLabel=playtest.label();Window::localTest=playtest.enabled;window.create();Window::input=controllerInput.get();if(animation)window.title(L"MGO2WIN - Title actor preview | Enter: START | Esc: close | No GCX boot");ComPtr<ID3D11Device>device;ComPtr<ID3D11DeviceContext>context;ComPtr<IDXGISwapChain>swap;
 if(gcx)window.title(L"MGO2WIN - Title | Enter: START | Esc: close");
 DXGI_SWAP_CHAIN_DESC sd{};sd.BufferDesc.Width=1280;sd.BufferDesc.Height=720;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=2;sd.OutputWindow=window.handle;sd.Windowed=TRUE;sd.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;sd.Flags=DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
 D3D_FEATURE_LEVEL level{};HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&sd,&swap,&device,&level,&context);const bool warp=FAILED(hr);
 if(warp)check(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&sd,&swap,&device,&level,&context));
 struct FullscreenExit{IDXGISwapChain* s;~FullscreenExit(){s->SetFullscreenState(FALSE,nullptr);}} fullscreenExit{swap.Get()};
 ComPtr<IDXGIFactory> factory;if(SUCCEEDED(swap->GetParent(IID_PPV_ARGS(&factory))))factory->MakeWindowAssociation(window.handle,DXGI_MWA_NO_ALT_ENTER);
 ComPtr<ID3D11Texture2D>back;check(swap->GetBuffer(0,IID_PPV_ARGS(&back)));ComPtr<ID3D11RenderTargetView>rt;check(device->CreateRenderTargetView(back.Get(),nullptr,&rt));
 const char*shader=R"(struct V{float2 p:POSITION;float2 uv:TEXCOORD;float4 c:COLOR;};struct P{float4 p:SV_POSITION;float2 uv:TEXCOORD;float4 c:COLOR;};P vs(V v){P o;o.p=float4(v.p.x/640-1,1-v.p.y/360,0,1);o.uv=v.uv;o.c=v.c;return o;}Texture2D tex:register(t0);SamplerState smp:register(s0);float4 ps(P p):SV_TARGET{return tex.Sample(smp,p.uv)*p.c;})";
 ComPtr<ID3DBlob>vsb,psb,errors;check(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,"vs","vs_4_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&vsb,&errors));check(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,"ps","ps_4_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&psb,&errors));
 ComPtr<ID3D11VertexShader>vs;ComPtr<ID3D11PixelShader>ps;check(device->CreateVertexShader(vsb->GetBufferPointer(),vsb->GetBufferSize(),nullptr,&vs));check(device->CreatePixelShader(psb->GetBufferPointer(),psb->GetBufferSize(),nullptr,&ps));
 D3D11_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0}};
 ComPtr<ID3D11InputLayout>input;check(device->CreateInputLayout(elements,3,vsb->GetBufferPointer(),vsb->GetBufferSize(),&input));
 D3D11_BUFFER_DESC bd{};bd.ByteWidth=10000*6*sizeof(Vertex);bd.Usage=D3D11_USAGE_DYNAMIC;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;ComPtr<ID3D11Buffer>vb;check(device->CreateBuffer(&bd,nullptr,&vb));
 std::unique_ptr<mgo2win::CharacterCatalog> characterCatalog; mgo2win::PreparedCharacter prepared;
 std::optional<mgo2win::CharacterModel> selectionComposite,selectionBox;bool previewBox=false;std::vector<mgo2win::ModelVertex> selectionBlendFrom;auto previousSelectionKind=mgo2win::SelectionPresentation::Kind::idle;float selectionLift=0,selectionLiftFrom=0;ULONGLONG selectionTransitionBegan=0;double selectionBlendDuration=0;
 const auto selectionBoxPath=networkKeys.parent_path()/L"stage"/L"objects"/L"cbox_a_sk.gwm";
 if(std::filesystem::is_regular_file(selectionBoxPath))try{selectionBox.emplace(file(selectionBoxPath));}catch(...){std::osyncstream(std::cout)<<"{\"selection_box_load_failed\":true}\n";}
 std::optional<std::array<uint8_t,28>> preparedAppearance;uint32_t preparedId=0;ULONGLONG modelBegan=0;unsigned appearanceChanges=0;
 if(!catalogPath.empty()){characterCatalog=std::make_unique<mgo2win::CharacterCatalog>(file(catalogPath));std::osyncstream(std::cout)<<"{\"character_catalog_loaded\":true,\"meshes\":"<<characterCatalog->mesh_count()<<",\"motion_clip\":"<<characterCatalog->clip()<<"}"<<std::endl;}
 mgo2win::stage::Assets stageAssets(networkKeys.empty()?std::filesystem::path{}:networkKeys.parent_path()/"stage");
 std::unique_ptr<mgo2win::CharacterRenderer> stageRenderer;std::shared_ptr<const mgo2win::CharacterModel> stageModel;unsigned stageFrames=0;
 mgo2win::stage::Navigation navigation;std::shared_ptr<const mgo2win::stage::Collision> navigationWorld,navigationBase;
 std::optional<mgo2win::host::LoadRequest> navigationRequest;uint64_t navigationGeneration=0;
 std::shared_ptr<const mgo2win::stage::Collision> movementSource,movementWorld; mgo2win::stage::WaterEffects waterEffects;
 std::shared_ptr<const mgo2win::stage::Water> navigationWater;
 mgo2win::combat::decals::Pool bulletMarks({1024,120000,10000,32.f,1.f});
 mgo2win::combat::material_effects::Pool materialParticles;
 std::optional<mgo2win::WorldView> stageCamera;auto navigationTick=GetTickCount64();bool navigationArmed=false;
 mgo2win::player::Control player;bool movementRunning=false;double motionSeconds=0;bool yFirstPerson=false;
 auto lastMotion=mgo2win::player::Motion::idle;bool motionMissing=false;
 std::unique_ptr<mgo2win::PlayerMotionBank> playerMotions;
 auto playerMotionPath=catalogPath.parent_path()/L"player.gwmot";
 if(std::filesystem::is_regular_file(playerMotionPath)){try{playerMotions=std::make_unique<mgo2win::PlayerMotionBank>(file(playerMotionPath));}catch(...){std::osyncstream(std::cout)<<"{\"player_motion_load_failed\":true}\n";}}
 std::unique_ptr<mgo2win::player::SpecialMotionBank> specialMotions;
 const auto specialMotionPath=catalogPath.parent_path()/L"special_male.gwmot";
 if(std::filesystem::is_regular_file(specialMotionPath))try{specialMotions=std::make_unique<mgo2win::player::SpecialMotionBank>(file(specialMotionPath));}catch(...){std::osyncstream(std::cout)<<"{\"special_motion_load_failed\":true}\n";}
 auto specialPose=[&](unsigned gender,mgo2win::combat::SpecialPhase phase,double seconds)->std::optional<mgo2win::MotionPose>{
  // Female and alternate special-action archives are not recovered here.
  if(!specialMotions||gender!=0||phase==mgo2win::combat::SpecialPhase::none)return {};
  return specialMotions->sample(mgo2win::player::SpecialPhase(unsigned(phase)-1),seconds);
 };
 std::array<std::unique_ptr<mgo2win::PlayerMotionBank>,2> selectionMotions;
 for(unsigned gender=0;gender<2;++gender){auto path=catalogPath.parent_path()/(L"selection"+std::to_wstring(gender)+L".gwmot");if(std::filesystem::is_regular_file(path))try{selectionMotions[gender]=std::make_unique<mgo2win::PlayerMotionBank>(file(path));}catch(...){std::osyncstream(std::cout)<<"{\"selection_motion_load_failed\":true}\n";}}
 const auto selectionSoundPath=networkKeys.parent_path()/L"audio"/L"salute.gwa";
 std::unique_ptr<AudioThread> selectionAudio;
 const auto sopSoundPath=networkKeys.parent_path()/L"audio"/L"sop_native.wav";
 std::unique_ptr<AudioThread> sopAudio;
 mgo2win::PreparedCharacter avatar;std::optional<std::array<uint8_t,28>> avatarAppearance;
 std::unique_ptr<mgo2win::CharacterRenderer> avatarRenderer;
 mgo2win::remote::Scene remoteScene;
 mgo2win::sop::Presentation sopPresentation;mgo2win::sop::SpecialInput specialInput;mgo2win::sop::PhaseClock specialClock;
 std::unique_ptr<mgo2win::sop::Renderer> sopRenderer;
 try{sopRenderer=std::make_unique<mgo2win::sop::Renderer>(device.Get());}catch(...){std::osyncstream(std::cout)<<"{\"sop_renderer_load_failed\":true}\n";}
 double specialSeconds=0;
 struct RemoteModel {std::array<uint8_t,28> appearance{};mgo2win::PreparedCharacter body;std::unique_ptr<mgo2win::CharacterRenderer> renderer;bool visible=false;};
 std::array<std::optional<RemoteModel>,24> remoteModels;
 std::vector<mgo2win::remote::Avatar> remoteAvatars;
 mgo2win::combat::LightEffects combatLights;
 std::vector<mgo2win::DynamicPointLight> combatPointLights;
 // AUTO AIM remains an opt-in action.
 // Current AK102 geometry in the native camera/torso adapter. Original weapon
 // actor flags and C05C38 state are absent here: explicitly neutral. Surveyor
 // remains unequipped (level 0) until a trusted round loadout is available;
 // never infer it from room flags or local HUD. Original aim-frame parity is pending.
 uint8_t lockSurveyorLevel=0;
 mgo2win::player_lock::Lock playerLock({*mgo2win::original_lock::ak102_parameters(25,0,0.f,0),.65f});
 mgo2win::hud::EnemyNameTagRenderer enemyTagRenderer;
 std::vector<uint32_t> enemyTagSurface(1280*720);
 std::unique_ptr<mgo2win::clan::Bitmap> enemyTagBitmap;
 uint64_t enemyTagClanSerial=~uint64_t(0);
 mgo2win::player::Ragdoll ragdoll;std::optional<mgo2win::physics::RigidBody> testBody;
 std::unique_ptr<mgo2win::CharacterRenderer> bodyRenderer;std::wstring physicsError;
 std::unique_ptr<mgo2win::CharacterRenderer> characterRenderer;unsigned modelFrames=0,emptyModelFrames=0;bool lastModelVisible=false;
 if(!modelPath.empty()){mgo2win::CharacterModel model(file(modelPath));characterRenderer=std::make_unique<mgo2win::CharacterRenderer>(device.Get(),model);std::osyncstream(std::cout)<<"{\"character_model_loaded\":true,\"vertices\":"<<model.vertices.size()<<",\"triangles\":"<<model.indices.size()/3<<",\"textures\":"<<model.textures.size()<<"}"<<std::endl;}
 auto titleTextures=textureCount;if(loading)textureCount+=loading->texture_count();auto backgroundOffset=textureCount;textureCount+=backgroundTextures;auto motionOffset=textureCount;if(motionBack)textureCount+=motionBack->texture_count();auto loginOffset=textureCount;textureCount+=loginTextures;
 for(auto&q:agreementBackground)if(q.atlas>=0)q.atlas+=backgroundOffset;
 for(auto&q:loginBackground)if(q.atlas>=0)q.atlas+=loginOffset;
 std::vector<ComPtr<ID3D11ShaderResourceView>>textures(textureCount+1);
 for(UINT i=0;i<=textureCount;i++){
  D3D11_TEXTURE2D_DESC td{};td.MipLevels=1;td.ArraySize=1;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{};uint32_t white=0xffffffff;std::vector<char>dds;
  if(i==textureCount){td.Width=1;td.Height=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;data.pSysMem=&white;data.SysMemPitch=4;}
  else{dds=file(i>=loginOffset?loginPath.parent_path()/L"images"/(std::to_wstring(i-loginOffset)+L".dds"):i>=motionOffset?motionPath.parent_path()/L"images"/(std::to_wstring(i-motionOffset)+L".dds"):i>=backgroundOffset?agreementBackgroundPath.parent_path()/L"images"/(std::to_wstring(i-backgroundOffset)+L".dds"):loading&&i>=titleTextures?loadingPath.parent_path()/L"images"/(std::to_wstring(i-titleTextures)+L".dds"):path.parent_path()/L"images"/(std::to_wstring(i)+L".dds"));if(dds.size()<128||std::memcmp(dds.data(),"DDS ",4)||u32(dds,4)!=124)throw std::runtime_error("DDS header");td.Height=u32(dds,12);td.Width=u32(dds,16);auto code=u32(dds,84);if(code!=0x31545844&&code!=0x35545844)throw std::runtime_error("DDS codec");td.Format=code==0x31545844?DXGI_FORMAT_BC1_UNORM:DXGI_FORMAT_BC3_UNORM;auto block=code==0x31545844?8u:16u;if(!td.Width||!td.Height||td.Width>8192||td.Height>8192||dds.size()!=128+size_t((td.Width+3)/4)*((td.Height+3)/4)*block)throw std::runtime_error("DDS extent");data.pSysMem=dds.data()+128;data.SysMemPitch=((td.Width+3)/4)*block;}
  ComPtr<ID3D11Texture2D>texture;check(device->CreateTexture2D(&td,&data,&texture));check(device->CreateShaderResourceView(texture.Get(),nullptr,&textures[i]));
 }
 D3D11_SAMPLER_DESC samp{};samp.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;samp.AddressU=samp.AddressV=samp.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;samp.MaxLOD=D3D11_FLOAT32_MAX;ComPtr<ID3D11SamplerState>sampler;check(device->CreateSamplerState(&samp,&sampler));
 ComPtr<ID3D11BlendState>blend[2];for(int i=0;i<2;i++){D3D11_BLEND_DESC b{};auto&r=b.RenderTarget[0];r.BlendEnable=TRUE;r.SrcBlend=D3D11_BLEND_SRC_ALPHA;r.DestBlend=i?D3D11_BLEND_ONE:D3D11_BLEND_INV_SRC_ALPHA;r.BlendOp=D3D11_BLEND_OP_ADD;r.SrcBlendAlpha=D3D11_BLEND_ONE;r.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;r.BlendOpAlpha=D3D11_BLEND_OP_ADD;r.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;check(device->CreateBlendState(&b,&blend[i]));}
 D3D11_RASTERIZER_DESC rs{};rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_NONE;rs.DepthClipEnable=TRUE;ComPtr<ID3D11RasterizerState>raster;check(device->CreateRasterizerState(&rs,&raster));
 context->IASetInputLayout(input.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);UINT stride=sizeof(Vertex),offset=0;ID3D11Buffer*vptr=vb.Get();context->IASetVertexBuffers(0,1,&vptr,&stride,&offset);context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);ID3D11SamplerState*sp=sampler.Get();context->PSSetSamplers(0,1,&sp);context->RSSetState(raster.Get());
 D3D11_VIEWPORT viewport{0,0,1280,720,0,1};context->RSSetViewports(1,&viewport);ID3D11RenderTargetView*rp=rt.Get();context->OMSetRenderTargets(1,&rp,nullptr);
 auto graphics=std::make_shared<mgo2win::GraphicsSettings>(inputPath.parent_path()/L"graphics.cfg");
 ComPtr<IDXGIOutput> output;std::vector<DXGI_MODE_DESC> displayModes;
 if(SUCCEEDED(swap->GetContainingOutput(&output))){UINT n=0;if(SUCCEEDED(output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM,0,&n,nullptr))&&n<=4096){displayModes.resize(n);if(SUCCEEDED(output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM,0,&n,displayModes.data()))){displayModes.resize(n);for(auto m:displayModes){mgo2win::GraphicsConfig test;test.width=m.Width;test.height=m.Height;test.refresh_num=m.RefreshRate.Numerator;test.refresh_den=m.RefreshRate.Denominator;if(mgo2win::valid_graphics(test)&&m.ScanlineOrdering!=DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST&&m.ScanlineOrdering!=DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST)graphics->modes.push_back({m.Width,m.Height,m.RefreshRate.Numerator,m.RefreshRate.Denominator});}}}}
 std::osyncstream(std::cout)<<"{\"graphics_display_modes\":"<<graphics->modes.size()<<"}"<<std::endl;
 graphics->apply=[&](const mgo2win::GraphicsConfig& cfg){
  if(playtest.enabled&&cfg.fullscreen)return false;
  DXGI_MODE_DESC target{};target.Width=cfg.width;target.Height=cfg.height;target.Format=DXGI_FORMAT_R8G8B8A8_UNORM;target.RefreshRate={cfg.refresh_num,cfg.refresh_den};
  if(cfg.fullscreen){if(!output||warp)return false;DXGI_MODE_DESC found{};if(FAILED(output->FindClosestMatchingMode(&target,&found,device.Get()))||found.Width!=cfg.width||found.Height!=cfg.height)return false;if(cfg.refresh_num&&uint64_t(found.RefreshRate.Numerator)*cfg.refresh_den!=uint64_t(cfg.refresh_num)*found.RefreshRate.Denominator)return false;target=found;}
  context->OMSetRenderTargets(0,nullptr,nullptr);rt.Reset();back.Reset();context->Flush();
  HRESULT changed=swap->SetFullscreenState(cfg.fullscreen?TRUE:FALSE,cfg.fullscreen?output.Get():nullptr);
  if(changed!=S_OK)return false;
  if(cfg.fullscreen){if(swap->ResizeTarget(&target)!=S_OK)return false;}
  else{RECT bounds{0,0,LONG(cfg.width),LONG(cfg.height)};AdjustWindowRect(&bounds,WS_OVERLAPPEDWINDOW,FALSE);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(window.handle,MONITOR_DEFAULTTONEAREST),&mi);int w=bounds.right-bounds.left,h=bounds.bottom-bounds.top;double scale=std::min({1.0,double(mi.rcWork.right-mi.rcWork.left)/w,double(mi.rcWork.bottom-mi.rcWork.top)/h});w=int(w*scale);h=int(h*scale);SetWindowPos(window.handle,nullptr,mi.rcWork.left+(mi.rcWork.right-mi.rcWork.left-w)/2,mi.rcWork.top+(mi.rcWork.bottom-mi.rcWork.top-h)/2,w,h,SWP_NOZORDER|SWP_NOACTIVATE);}
  if(FAILED(swap->ResizeBuffers(2,cfg.width,cfg.height,DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH)))return false;
  if(FAILED(swap->GetBuffer(0,IID_PPV_ARGS(&back)))||FAILED(device->CreateRenderTargetView(back.Get(),nullptr,&rt)))return false;
  float scale=std::min(float(cfg.width)/1280,float(cfg.height)/720);viewport={ (cfg.width-1280*scale)/2,(cfg.height-720*scale)/2,1280*scale,720*scale,0,1};context->RSSetViewports(1,&viewport);rp=rt.Get();context->OMSetRenderTargets(1,&rp,nullptr);
  Window::viewX=viewport.TopLeftX/cfg.width;Window::viewY=viewport.TopLeftY/cfg.height;Window::viewW=viewport.Width/cfg.width;Window::viewH=viewport.Height/cfg.height;
  BOOL actualFull=FALSE;if(FAILED(swap->GetFullscreenState(&actualFull,nullptr))||bool(actualFull)!=bool(cfg.fullscreen))return false;
  D3D11_TEXTURE2D_DESC actualBuffer{};back->GetDesc(&actualBuffer);
  std::osyncstream(std::cout)<<"{\"graphics_renderer\":true,\"fullscreen\":"<<(actualFull?"true":"false")<<",\"buffer_width\":"<<actualBuffer.Width<<",\"buffer_height\":"<<actualBuffer.Height<<",\"target_refresh_num\":"<<(cfg.fullscreen?target.RefreshRate.Numerator:0)<<",\"target_refresh_den\":"<<(cfg.fullscreen?target.RefreshRate.Denominator:1)<<"}"<<std::endl;
  return true;
 };
 if(playtest.enabled)graphics->draft.fullscreen=0;
 if(safeGraphics)graphics->draft=graphics->active;
 if(!scripted&&!safeGraphics&&graphics->draft!=graphics->active){if(graphics->apply(graphics->draft)){graphics->active=graphics->draft;std::osyncstream(std::cout)<<"{\"graphics_restored\":true}"<<std::endl;}else{check(graphics->apply(graphics->active)?S_OK:E_FAIL);graphics->draft=graphics->active;}}
 AudioThread audio;if(sound&&(!gcx||gcx->bgm_requested()))audio.start(wavPath.empty()?path.parent_path().parent_path()/L"audio/bgm_mgo_title01.wav":wavPath,argv[2],23);
 AudioThread effect,lobbyAudio;bool seStarted=false,lobbyMusicStarted=false;
 std::unique_ptr<AudioThread> stageAudio;unsigned stageAudioIndex=0;
 std::unique_ptr<AudioThread> stageMusic;
 std::optional<mgo2win::stage::RoundMusicPhase> loggedMusicPhase;
 mgo2win::stage::MusicLibrary musicLibrary;mgo2win::stage::MusicSelection musicSelection;
 auto musicRoot=networkKeys.empty()?std::filesystem::path{}:networkKeys.parent_path()/"bgm";
 auto musicScan=std::async(std::launch::async,[musicRoot]{return musicRoot.empty()?mgo2win::stage::MusicLibrary{}:mgo2win::stage::MusicLibrary::scan(musicRoot);});
 bool musicReady=false,musicPlaying=false,debugTitle=false;size_t musicIndex=0;mgo2win::stage::MusicPlayback musicPlayback;std::wstring musicError;
 if(gcx&&sound&&!sePath.empty())gcx->set_se_handler([&](uint32_t cue){
  if(seStarted)throw std::runtime_error("Duplicate START sound");seStarted=true;
  std::osyncstream(std::cout)<<"{\"start_sound\":true,\"cue\":"<<cue<<",\"tick\":"<<animation->ticks()<<"}"<<std::endl;
  effect.start(sePath,L"600",18999);
 });
 std::vector<std::unique_ptr<AudioThread>> menuAudio;unsigned menuFailures=0;
 std::unique_ptr<AudioThread> voiceAudio;
 mgo2win::combat::Effects combatEffects;combatEffects.load(networkKeys.parent_path()/"sfx");
 std::vector<std::unique_ptr<AudioThread>> combatAudio;
 mgo2win::combat::footsteps::Timeline selfFootsteps({.5});
 std::vector<mgo2win::combat::footsteps::Timeline> remoteFootsteps(24,mgo2win::combat::footsteps::Timeline({.5}));
 auto playCombatSound=[&](const mgo2win::combat::Sound& s){
  if(!sound)return;std::erase_if(combatAudio,[](const auto&a){return a->result.load()!=-1;});if(combatAudio.size()>=24)return;
  auto voice=std::make_unique<AudioThread>();voice->control.gain=s.gain;voice->start(s.file,L"30",s.cue,"combat_se");combatAudio.push_back(std::move(voice));
 };
 std::unique_ptr<AudioThread> radioAudio;std::shared_ptr<mgo2win::radio::Session> radioAudioSession;uint64_t radioAudioGeneration=0;
 uint64_t combatEpoch=0,combatRevision=0,combatSendAt=0;uint32_t combatSequence=0,combatLife=0;bool combatReloadPending=false,combatFirePending=false,combatInputWasActive=false;
 std::optional<mgo2win::combat::Player> hostPlayer;

 auto menuSound=[&](unsigned cue){if(!sound||mgo2win::menu_audio::asset(cue).empty())return;for(auto it=menuAudio.begin();it!=menuAudio.end();){if((*it)->result.load()!=-1){if((*it)->result.load())++menuFailures;it=menuAudio.erase(it);}else ++it;}
  if(menuAudio.size()>=8)menuAudio.erase(menuAudio.begin());
  const auto& cuePath=cue==mgo2win::menu_audio::Confirm?menuConfirmPath:cue==mgo2win::menu_audio::Cancel?menuCancelPath:menuMovePath;
  if(cuePath.empty())return;auto a=std::make_unique<AudioThread>();a->start(cuePath,L"10",cue);menuAudio.push_back(std::move(a));
  std::osyncstream(std::cout)<<"{\"menu_sound_requested\":"<<cue<<"}"<<std::endl;
 };
 ComPtr<ID3D11Texture2D> agreementTexture;ComPtr<ID3D11ShaderResourceView> agreementView;
 if(agreement){D3D11_TEXTURE2D_DESC td{};td.Width=1280;td.Height=720;td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;check(device->CreateTexture2D(&td,nullptr,&agreementTexture));check(device->CreateShaderResourceView(agreementTexture.Get(),nullptr,&agreementView));}
 bool agreementStarted=false,agreementVisible=false,agreementCaptured=false,agreementScrolledCaptured=false,agreementChoiceCaptured=false;
 std::unique_ptr<mgo2win::LoginScreen> login;unsigned loginFrames=0,loginVisits=0,returnFrames=0;
 mgo2win::PlayerMenu playerMenu(inputPath,controllerInput,graphics);Window::playerMenu=&playerMenu;
 mgo2win::MusicMenu musicMenu;Window::musicMenu=&musicMenu;
 mgo2win::invitation_ui::Presenter invitation;Window::invitation=&invitation;
 std::optional<mgo2win::host::LoadRequest> musicMenuRequest;bool wasMusicDeployed=false;
 bool portTitle=false,charTitle=false;ULONGLONG slotBegan=0;unsigned slotStep=0,slotCaptured=0;bool modelRotated=false;
 struct LoginReset{~LoginReset(){Window::login=nullptr;}} loginReset;
 unsigned agreementFrames=0;ULONGLONG closeAt=0;
 mgo2win::AudioFade fade;
 if(gcx&&sound)gcx->set_fade_handler([&](int duration){fade.stop(duration);audio.control.gain=fade.gain();std::osyncstream(std::cout)<<"{\"bgm_fade\":true,\"tick\":"<<animation->ticks()<<",\"argument\":"<<duration<<",\"remaining_frames\":"<<fade.remaining()<<",\"gain\":"<<fade.gain()<<"}"<<std::endl;});
 auto began=std::chrono::steady_clock::now();auto deadline=GetTickCount64()+static_cast<ULONGLONG>(seconds*1000);bool running=true,loadingVisible=false,loadingCaptured=false,loadingReady=false;unsigned frames=0,occluded=0,lastState=0,captureIndex=0,loadingFrames=0;const uint32_t captureTicks[]={5,300,625,800,1000,1200};
 while(running&&GetTickCount64()<deadline){MSG msg{};while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)running=false;TranslateMessage(&msg);DispatchMessage(&msg);}if(!running)break;bool invitationFrameModal=false;
  graphics->tick(GetTickCount64(),GetForegroundWindow()==window.handle);
  const bool foreground=GetForegroundWindow()==window.handle;
  playtest.enforce_input(controllerInput->config);
  const bool inputActive=playtest.gameplay_active(foreground,scripted,controllerInput->config.device);
  const bool invitationModalBeforePoll=invitation.overlay().visible();
  invitation.session(login?login->invitation_session():nullptr);invitation.update(GetTickCount64());
  const bool invitationClosedBeforePoll=invitationModalBeforePoll&&!invitation.overlay().visible();
  auto pad=playtest.enabled&&!playtest.pad?mgo2win::PadSample{}:controllerInput->poll(playtest.pad_active(foreground,scripted),playtest.enabled&&playtest.pad?playtest.slot:playerMenu.slot(login?login->input_slot():controllerInput->config.slot));
  const bool radioDpad=playerMenu.radio_visible();if(inputActive&&radioDpad&&controllerInput->config.device&&pad.connected&&pad.armed){const auto directions=pad.held&15;if((pad.pressed&directions)==directions)playerMenu.radio_digital_mask(directions);}
  bool consumed=(invitationClosedBeforePoll||Window::invitationGuard.stale(invitation.overlay().visible()))?true:invitation.overlay().visible()?false:musicMenu.visible()?false:playerMenu.visible()?playerMenu.sample(pad):(login&&login->controller_sample(pad));
  if(inputActive&&!consumed){auto actions=controllerInput->actions(pad);for(unsigned a=0;a<mgo2win::input_actions;++a)if(actions[a]){
   if(invitation.overlay().visible()){auto k=a==12?VK_ESCAPE:mgo2win::menu_key(a);if(k){Window::proc(window.handle,WM_KEYDOWN,k,1LL<<25);break;}continue;}
   if(a==7&&playerMenu.settings_menu()&&!playerMenu.capturing()&&invitation.overlay().available()){Window::proc(window.handle,WM_KEYDOWN,VK_F6,1LL<<25);break;}
   if(radioDpad&&a<4)continue;
   if(a==13&&playerMenu.chat_menu()){playerMenu.select_chat_radio();continue;}
   if(a==13&&!Window::debug.confirmReset&&login&&!login->personal_overlay_visible()&&login->chat_session()&&login->chat_session()->state().joined&&!playerMenu.visible()&&!musicMenu.visible()){playerMenu.chat_session(login->chat_session());playerMenu.select_chat_radio();continue;}
   bool live=login&&login->gameplay_visible()&&login->stage_request()&&login->combat_offer()&&login->combat_state()&&login->combat_state()->players[login->combat_offer()->self.slot]&&login->combat_status()==mgo2win::combat::wire::Status::active;
   if(((Window::debug.enabled&&Window::inspection)||live)&&login&&login->stage_request()&&!Window::debug.confirmReset&&!playerMenu.visible()&&!musicMenu.visible()&&!login->personal_overlay_visible())continue;
   if(a==12&&login&&login->stage_load_request()&&!playerMenu.visible()&&!musicMenu.visible()){Window::proc(window.handle,WM_KEYDOWN,VK_F9,1LL<<25);continue;}
   if(playerMenu.visible()&&a==12&&!playerMenu.capturing()){playerMenu.close(true);player.suspend();continue;}
   if(playerMenu.visible()&&a>=16)continue;
   if(playerMenu.tab_action(a))continue;
   auto k=mgo2win::menu_key(a);if(k)Window::proc(window.handle,WM_KEYDOWN,k,1LL<<25);
  }}
  for(auto cue:Window::uiCues)menuSound(cue);Window::uiCues.clear();
  if(closeAt&&GetTickCount64()>=closeAt)break;
  if(animation){auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();auto target=static_cast<uint32_t>(elapsed*300000.0/1001.0);
   while(animation->ticks()+5<=target&&animation->state()!=4){fade.advance(1);audio.control.gain=fade.gain();if(scripted&&(animation->ticks()+5==100||animation->ticks()+5==900))SendMessageW(window.handle,WM_KEYDOWN,VK_RETURN,0);uint32_t input=Window::pressed;Window::pressed=0;animation->tick(5,input);
    if(animation->state()!=lastState){lastState=animation->state();std::osyncstream(std::cout)<<"{\"actor_state\":"<<lastState<<",\"tick\":"<<animation->ticks()<<"}"<<std::endl;}
   }
   if(animation->state()==4&&loading&&gcx->loading_requested()){
    if(!loadingVisible){loadingVisible=true;std::osyncstream(std::cout)<<"{\"loading_entered\":true,\"layout\":\"loading_MGO\",\"event\":\"0x9ca2fa\"}"<<std::endl;window.title(L"MGO2WIN - Loading | Esc: close");}
    loading->tick(5,0);fade.advance(1);audio.control.gain=fade.gain();
    quads=loading->geometry();for(auto&q:quads)if(q.atlas>=0)q.atlas+=titleTextures;
    if(loadingFrames>=3&&!loadingReady){gcx->loading_ready();loadingReady=true;if(agreement){agreement->start();agreementStarted=true;if(sound&&!lobbyMusicPath.empty()){lobbyAudio.start(lobbyMusicPath,argv[2],23);lobbyMusicStarted=true;}}}
   }else quads=animation->geometry();
   count=static_cast<uint32_t>(quads.size());vertices.clear();for(const auto&q:quads)for(int i:{0,1,2,0,2,3})vertices.push_back(q.vertices[i]);
  }
  if(agreementStarted&&(agreementVisible||(agreement->ready()&&loadingFrames>=30))){
   agreement->report();if(!agreementVisible){agreementVisible=true;Window::agreementActive=true;Window::agreementInput=0;window.title(L"OpenMGO2 - Agreement | Enter: select | Esc: close");menuSound(93);std::osyncstream(std::cout)<<"{\"agreement_visible\":true}"<<std::endl;}
   auto input=Window::agreementInput;Window::agreementInput=0;
   if(scripted){if(agreementFrames==60)input=mgo2win::AgreementScreen::pageDown;if(agreementFrames==90)input=mgo2win::AgreementScreen::home;if(agreementFrames==100&&!scriptedNo)input=mgo2win::AgreementScreen::left;if(agreementFrames==140)input=mgo2win::AgreementScreen::confirm;}
   if(scriptedLogin&&!login&&loginVisits==1){++returnFrames;if(returnFrames==30)input=mgo2win::AgreementScreen::left;if(returnFrames==60)input=mgo2win::AgreementScreen::confirm;}
   bool close=false;int cue=closeAt||login?-1:agreement->input(input,close);if(cue>=0)menuSound(static_cast<unsigned>(cue));if(close)closeAt=GetTickCount64()+1250;
   if(!login&&agreement->accepted()&&!loginPath.empty()){
    std::filesystem::path store;
    if(scripted)store=std::filesystem::path(argv[3]).parent_path()/L"login-test.dat";
    else store=inputPath.parent_path()/L"login.dat";
    login=std::make_unique<mgo2win::LoginScreen>(store,!scripted,mgo2win::authenticate,true,controllerInput,graphics,networkKeys,playtest.enabled,playtest.enabled?playtest.port():0);Window::login=login.get();loginFrames=0;++loginVisits;
    login->character_catalog(characterCatalog.get());
    if(scriptedSlots||scriptedCreation)login->show_character_preview(true);
    if(scriptedCharacters||scriptedAppearance||scriptedSelection)login->show_character_preview(false,scriptedSelection);
    if(scriptedPorts)login->show_port_preview();
    if(scriptedControls||scriptedGraphics)login->show_port_preview();
    if(scriptedStun)login->show_port_preview(true);
    window.title(L"OpenMGO2 - Login | Tab: select | Esc: back");
    std::osyncstream(std::cout)<<"{\"login_visible\":true,\"authentication_started\":false}"<<std::endl;
   }
   if(login){
    if(scriptedSlots){
     if(!slotBegan)slotBegan=GetTickCount64();auto elapsed=GetTickCount64()-slotBegan;
     auto key=[&](WPARAM k){SendMessageW(window.handle,WM_KEYDOWN,k,0);};auto release=[&]{SendMessageW(window.handle,WM_KEYUP,VK_BACK,0);};
     unsigned previous=slotStep;
     if(!modelRotated&&elapsed>=4100){for(int turn=0;turn<5;++turn)key(VK_RIGHT);modelRotated=true;std::osyncstream(std::cout)<<"{\"character_model_rotation_test\":true,\"yaw\":"<<login->model_yaw()<<"}"<<std::endl;}
     if(slotStep==0&&elapsed>=1500){key(VK_DOWN);++slotStep;}
     else if(slotStep==1&&elapsed>=2200){key(VK_UP);++slotStep;}
     else if(slotStep==2&&elapsed>=2500){key(VK_BACK);++slotStep;}
     else if(slotStep==3&&elapsed>=4000){release();++slotStep;}
     else if(slotStep==4&&elapsed>=4300){key(VK_BACK);++slotStep;}
     else if(slotStep==5&&elapsed>=7500){release();++slotStep;}
     else if(slotStep==6&&elapsed>=8500){key(VK_RETURN);++slotStep;}
     else if(slotStep==7&&elapsed>=8800){key(VK_BACK);++slotStep;}
     else if(slotStep==8&&elapsed>=12000){release();++slotStep;}
     else if(slotStep==9&&elapsed>=12700){key(VK_LEFT);key(VK_RETURN);++slotStep;}
     else if(slotStep==10&&elapsed>=13300){for(int i=0;i<4;++i)key(VK_DOWN);key(VK_RETURN);++slotStep;}
     else if(slotStep==11&&elapsed>=14100){key(VK_ESCAPE);++slotStep;}
     if(previous!=slotStep){login->report();std::osyncstream(std::cout)<<"{\"slot_script_step\":"<<slotStep<<",\"elapsed_ms\":"<<elapsed<<"}"<<std::endl;}
    }
    if(scriptedCreation){
     auto key=[&](WPARAM k){SendMessageW(window.handle,WM_KEYDOWN,k,0);};
     auto chars=[&](const wchar_t*s){for(;*s;++s)SendMessageW(window.handle,WM_CHAR,*s,0);};
     if(loginFrames==110){key(VK_DOWN);key(VK_RETURN);}
     if(loginFrames==140)chars(L"試作兵士");
     if(loginFrames==155){key(VK_DELETE);chars(L"한글이름");}
     if(loginFrames==160){key(VK_DOWN);key(VK_RIGHT);key(VK_DOWN);key(VK_RIGHT);key(VK_DOWN);key(VK_RIGHT);}
     if(loginFrames==170){key(VK_DOWN);for(int i=0;i<10;++i)key(VK_LEFT);}
     if(loginFrames==182){for(int i=0;i<20;++i)key(VK_RIGHT);}
     if(loginFrames==200)key(VK_F4);
     if(loginFrames==190){key(VK_F2);key(VK_DOWN);key(VK_RIGHT);key(VK_SPACE);key(VK_RIGHT);}
     if(loginFrames==230){key(VK_F3);key(VK_DOWN);key(VK_DOWN);key(VK_RIGHT);}
     if(loginFrames==270){key(VK_END);key(VK_UP);key(VK_RETURN);}
     if(loginFrames==310)key(VK_RETURN);
     if(loginFrames==340)key(VK_ESCAPE);
     if(loginFrames==365)key(VK_RETURN); // Default NO retains the draft.
     if(loginFrames==385){key(VK_ESCAPE);key(VK_LEFT);key(VK_RETURN);}
     if(loginFrames==410)key(VK_ESCAPE);
    }
    if(scriptedSelection){auto key=[&](WPARAM k){SendMessageW(window.handle,WM_KEYDOWN,k,0);};if(loginFrames==110)key(VK_RETURN);if(loginFrames==190)key(VK_NEXT);if(loginFrames==230||loginFrames==280||loginFrames==340||loginFrames==390||loginFrames==440||loginFrames==490||loginFrames==540)key(VK_RIGHT);if(loginFrames==570)key(VK_RETURN);if(loginFrames==620||loginFrames==660||loginFrames==720||loginFrames==760||loginFrames==770)key(VK_RETURN);if(loginFrames==710)key(VK_DOWN);if(loginFrames==750){for(auto c:L"abc")if(c)SendMessageW(window.handle,WM_CHAR,c,0);}if(loginFrames==830)key(VK_NEXT);if(loginFrames==860)key(VK_F5);if(loginFrames==700||loginFrames==820||loginFrames==920||loginFrames==950||loginFrames==990||loginFrames==1160)key(VK_ESCAPE);if(loginFrames==1040)key(VK_F2);if(loginFrames==1100)key(VK_F3);}
    if(scriptedAppearance){auto key=[&](WPARAM k){SendMessageW(window.handle,WM_KEYDOWN,k,0);};if(loginFrames==150||loginFrames==240||loginFrames==330)key(VK_DOWN);if(loginFrames==430)key(VK_ESCAPE);}
    if(scriptedCharacters){auto key=[&](WPARAM k){SendMessageW(window.handle,WM_KEYDOWN,k,0);};if(loginFrames==110){key(VK_DOWN);key(VK_DOWN);}if(loginFrames==145)key(VK_END);if(loginFrames==160)key(VK_UP);if(loginFrames==230)key(VK_RETURN);if(loginFrames==350)key(VK_ESCAPE);}
    if(scriptedGraphics){
     auto key=[&](WPARAM k){SendMessageW(window.handle,WM_KEYDOWN,k,0);};
     auto click=[&](int x,int y){RECT r{};GetClientRect(window.handle,&r);SendMessageW(window.handle,WM_LBUTTONUP,0,MAKELPARAM(int((Window::viewX+float(x)/1280*Window::viewW)*r.right),int((Window::viewY+float(y)/720*Window::viewH)*r.bottom)));};
     if(loginFrames==10)key(VK_F3);
     if(loginFrames==20){key(VK_DOWN);key(VK_DOWN);key(VK_DOWN);key(VK_RIGHT);graphics->draft.width=1600;graphics->draft.height=900;}
     if(loginFrames==30)click(250,620);
     if(loginFrames==50)key(VK_RETURN);
     if(loginFrames==70){graphics->draft.fullscreen=1;if(!graphics->modes.empty()){auto m=graphics->modes.front();for(auto candidate:graphics->modes)if(candidate.width==1280&&candidate.height==720){m=candidate;break;}graphics->draft.width=m.width;graphics->draft.height=m.height;graphics->draft.refresh_num=m.num;graphics->draft.refresh_den=m.den;}click(250,620);}
     if(loginFrames==100)key(VK_ESCAPE);
     if(loginFrames==120){graphics->draft.width=1280;graphics->draft.height=720;click(250,620);}
    }
    if(scriptedControls){
     auto key=[&](WPARAM k){SendMessageW(window.handle,WM_KEYDOWN,k,0);};
     auto click=[&](int x,int y){RECT r{};GetClientRect(window.handle,&r);SendMessageW(window.handle,WM_LBUTTONUP,0,MAKELPARAM(x*r.right/1280,y*r.bottom/720));};
     if(loginFrames==10)key(VK_F2);
     if(loginFrames==20)click(800,440);
     if(loginFrames==25)key('F');
     if(loginFrames==30)click(220,620);
     if(loginFrames==40)click(520,235);
     if(loginFrames==50)click(800,440);
     if(loginFrames==55)login->controller_sample({true,0,0});
     if(loginFrames==56)login->controller_sample({true,1u<<5,1u<<5});
     if(loginFrames==65)click(220,620);
     if(loginFrames==75)key(VK_NEXT);
     if(loginFrames==85)key(VK_NEXT);
     if(loginFrames==95)key(VK_F1);
     if(loginFrames==105)key(VK_F2);
     if(loginFrames==115){login->show_port_preview();key(VK_F2);}
     if(loginFrames==130){auto actions=controllerInput->actions({true,1u<<8,1u<<8});for(unsigned a=0;a<mgo2win::input_actions;++a)if(actions[a]){auto k=mgo2win::menu_key(a);if(k)Window::proc(window.handle,WM_KEYDOWN,k,1LL<<25);}std::osyncstream(std::cout)<<"{\"controller_scripted_pad_tab\":true,\"physical_device_test\":false}"<<std::endl;}
     if(loginFrames==140)key(VK_F2);
    }
    if(scriptedStun&&loginFrames==20)SendMessageW(window.handle,WM_KEYDOWN,VK_RETURN,0);
    if(scriptedPorts){
     auto key=[&](WPARAM k){SendMessageW(window.handle,WM_KEYDOWN,k,0);};
     auto chars=[&](const wchar_t*s){for(;*s;++s)SendMessageW(window.handle,WM_CHAR,*s,0);};
     if(loginFrames==10){for(int i=0;i<3;++i)key(VK_UP);key(VK_RIGHT);for(int i=0;i<3;++i)key(VK_DOWN);}
     if(loginFrames==20)key(VK_RETURN);
     if(loginFrames==35){key(VK_DOWN);key(VK_RETURN);}
     if(loginFrames==50){for(int i=0;i<3;++i)key(VK_UP);key(VK_DELETE);chars(L"80");key(VK_RETURN);key(VK_RETURN);}
     if(loginFrames==75){key(VK_DELETE);chars(L"5730");key(VK_RETURN);key(VK_RETURN);}
     if(loginFrames==80){key(VK_UP);key(VK_RETURN);key(VK_END);}
     if(loginFrames==90){key(VK_RETURN);key(VK_DOWN);}
     if(loginFrames==100){key(VK_DOWN);key(VK_RETURN);}
     if(loginFrames==130)key(VK_ESCAPE);
     if(loginFrames==150)login->show_port_preview();
     if(loginFrames==170)key(VK_RETURN);
    }
    if(scriptedLogin&&loginVisits==1){
     auto key=[&](WPARAM k){SendMessageW(window.handle,WM_KEYDOWN,k,0);};
     auto chars=[&](const wchar_t*s){for(;*s;++s)SendMessageW(window.handle,WM_CHAR,*s,0);};
     if(loginFrames==20){key(VK_DOWN);key(VK_DOWN);key(VK_RETURN);} // Empty form.
     if(loginFrames==50){chars(L"preview_user");key(VK_TAB);chars(L"local-test-only");key(VK_TAB);}
     if(loginFrames==75){for(int i=0;i<4;++i)key(VK_TAB);key(VK_RETURN);for(int i=0;i<3;++i)key(VK_TAB);}
     if(loginFrames==100)key(VK_RETURN); // Local unavailable notice, no request.
     if(loginFrames==180)key(VK_ESCAPE);
    }
    for(auto c:login->cues())menuSound(c);
    if(voiceAudio&&(!login->creation_visible()||(!scripted&&GetForegroundWindow()!=window.handle)||voiceAudio->result.load()!=-1)){
     if(voiceAudio->result.load()>0)++menuFailures;voiceAudio.reset();
    }
    if(auto request=login->take_audition();request&&sound&&!voiceDirectory.empty()&&(scripted||GetForegroundWindow()==window.handle)){
     if(request->gender>1||request->voice>7)throw std::runtime_error("Character voice range");
     voiceAudio.reset();voiceAudio=std::make_unique<AudioThread>();voiceAudio->control.frequencyRatio=mgo2win::character_voice_ratio(request->pitch);
     auto name=std::to_wstring(request->gender)+L"_"+std::to_wstring(request->voice)+L".gwa";
     voiceAudio->start(voiceDirectory/name,L"10",0,"character_voice");
     std::osyncstream(std::cout)<<"{\"voice_audition_started\":true,\"gender\":"<<request->gender<<",\"voice\":"<<request->voice<<",\"pitch\":"<<request->pitch<<"}"<<std::endl;
    }
    if(login->back()){login->report();Window::login=nullptr;login.reset();agreement->return_from_login();Window::agreementInput=0;
     window.title(L"OpenMGO2 - Agreement | Enter: select | Esc: close");
    }
   }
   if(characterCatalog){
    auto appearance=login?login->preview_appearance():std::nullopt;auto id=login?login->preview_character_id():0;
    if(appearance!=preparedAppearance||id!=preparedId){preparedAppearance=appearance;preparedId=id;characterRenderer.reset();prepared={};selectionComposite.reset();previewBox=false;selectionBlendFrom.clear();previousSelectionKind=mgo2win::SelectionPresentation::Kind::idle;selectionLift=selectionLiftFrom=0;selectionTransitionBegan=0;selectionBlendDuration=0;
     if(appearance){prepared=characterCatalog->assemble(*appearance);if(prepared.ready())characterRenderer=std::make_unique<mgo2win::CharacterRenderer>(device.Get(),prepared.model);modelBegan=GetTickCount64();++appearanceChanges;
      std::osyncstream(std::cout)<<"{\"appearance_assembled\":true,\"parts\":"<<prepared.selectedParts<<",\"missing_models\":"<<prepared.missingModels<<",\"missing_colors\":"<<prepared.missingColors<<",\"defaulted_lower\":"<<(prepared.defaultedLower?"true":"false")<<"}"<<std::endl;
      for(const auto&problem:prepared.issues)std::osyncstream(std::cout)<<"{\"appearance_issue\":true,\"gender\":"<<prepared.gender<<",\"slot\":"<<problem.slot<<",\"item\":"<<problem.id<<",\"color\":"<<problem.color<<",\"texture\":"<<problem.texture<<",\"reason\":\""<<problem.reason<<"\"}"<<std::endl;}
    }
   }
   auto requestedStage=login?login->stage_load_request():std::nullopt;
   if((Window::debug.confirmReset||Window::debug.reset)&&Window::resetRequest!=requestedStage)Window::debug.cancel_reset();
   // Admission starts background loading. Switching between roster and preview
   // must not discard received state or restart preparation for the same round.
   stageAssets.select(login?login->stage_load_request():std::nullopt);
   if(Window::debug.reset&&login&&login->stage_request())stageAssets.reset();
   stageAssets.receive(login?login->stage_placements():std::nullopt);
   if(auto snapshot=login?login->stage_scene():std::nullopt)stageAssets.object_states(*snapshot);
   auto stageResult=stageAssets.result();
   auto navigationNow=GetTickCount64();float navigationSeconds=float(navigationNow-navigationTick)/1000;navigationTick=navigationNow;
   if(movementSource!=stageResult.collision){movementSource=stageResult.collision;movementWorld=mgo2win::stage::movement_collision(movementSource);}navigationWorld=movementWorld;
   auto collisionBase=stageResult.authoredCollision?stageResult.authoredCollision:stageResult.collision;
   if(navigationBase!=collisionBase||navigationRequest!=stageResult.request||navigationGeneration!=stageResult.generation||Window::debug.reset){
    ragdoll.stop();testBody.reset();bodyRenderer.reset();physicsError.clear();Window::testRagdoll=Window::testBody=false;
    navigationWorld=mgo2win::stage::movement_collision(navigationWorld);
    navigationBase=collisionBase;navigationRequest=stageResult.request;navigationGeneration=stageResult.generation;navigation=mgo2win::stage::Navigation{};navigationWater.reset();selfFootsteps.reset();navigationArmed=false;combatRevision=0;yFirstPerson=playerMenu.prone_y_first_person();player=mgo2win::player::Control{yFirstPerson};player.bodyYaw=2.2f;movementRunning=false;motionSeconds=0;playerMenu.close();
    // Local inspection starts near a source GCX initial spawn; this does not
    // bypass the HOST deployment transaction.
    if(stageResult.request){const auto route=std::string(mgo2win::stage::name(stageResult.request->rotation.map));
     try{std::ifstream waterIn(networkKeys.parent_path()/"stage"/(route+".gww"));if(waterIn){navigationWater=std::make_shared<const mgo2win::stage::Water>(mgo2win::stage::Water::read(waterIn));navigation.water(navigationWater);}}catch(...){navigationWater.reset();navigation.water({});}
     try{std::ifstream spawnIn(networkKeys.parent_path()/"stage"/(route+".tdm-spawns-v2.cfg"));auto profile=mgo2win::combat::spawn::StageProfile::read(spawnIn);auto point=profile.group(mgo2win::combat::spawn::Variant::normal,mgo2win::combat::spawn::Kind::initial,0).front().position;point[1]+=3000;if(navigationWorld&&navigation.place(*navigationWorld,point))navigation.facing(2.2f);}catch(...){if(stageResult.request->rotation.map==20&&navigationWorld)navigation.place(*navigationWorld,{-42878.8359f,3000,29781.7969f});}
    }
   }
   hostPlayer.reset();auto combatOffer=login?login->combat_offer():std::nullopt;auto combatState=login?login->combat_state():std::nullopt;
   if(combatOffer&&combatState&&combatState->epoch==combatOffer->epoch)hostPlayer=combatState->players[combatOffer->self.slot];
   bool combatPlayable=hostPlayer&&login->combat_status()==mgo2win::combat::wire::Status::active;
   const uint8_t surveyorLevel=combatPlayable&&hostPlayer->verifiedSkills?hostPlayer->surveyorLevel:0;
   if(surveyorLevel!=lockSurveyorLevel){lockSurveyorLevel=surveyorLevel;playerLock=mgo2win::player_lock::Lock({*mgo2win::original_lock::ak102_parameters(25,0,0.f,lockSurveyorLevel),.65f});}
   bool combatViewing=hostPlayer&&(combatPlayable||login->combat_status()==mgo2win::combat::wire::Status::ended);
   if(combatPlayable&&navigationWorld&&combatState->revision!=combatRevision){
    float error=0;auto feet=navigation.feet();for(unsigned i=0;i<3;++i)error+=(feet[i]-hostPlayer->pose.feet[i])*(feet[i]-hostPlayer->pose.feet[i]);
    if(combatEpoch!=combatState->epoch||combatLife!=hostPlayer->life||!navigation.ready()||error>600.f*600.f){selfFootsteps.reset();navigation.authoritative(*navigationWorld,hostPlayer->pose.feet,hostPlayer->pose.yaw,hostPlayer->pose.pitch,hostPlayer->pose.capsule);if(combatEpoch!=combatState->epoch||combatLife!=hostPlayer->life){combatSequence=0;combatSendAt=0;specialInput.clear();specialClock.clear();combatReloadPending=combatFirePending=combatInputWasActive=false;player=mgo2win::player::Control{playerMenu.prone_y_first_person()};player.bodyYaw=hostPlayer->pose.yaw;navigationArmed=false;}}
    combatEpoch=combatState->epoch;combatRevision=combatState->revision;combatLife=hostPlayer->life;
   }
   if(!combatOffer){combatReloadPending=combatFirePending=combatInputWasActive=false;combatSendAt=0;combatEpoch=combatRevision=0;combatEffects.clear();combatAudio.clear();}
   player.host_reload(combatPlayable?std::optional(hostPlayer->reloadUntil!=0):std::nullopt);
   const auto sopView=login?login->combat_sop():mgo2win::combat::SopView{};
   if(!combatPlayable||!hostPlayer->alive||sopView.recipient!=hostPlayer->identity||sopView.life!=hostPlayer->life)specialInput.clear();
   else specialInput.acknowledge(sopView,navigationNow);
   player.host_special(combatPlayable&&(specialInput.pending()||hostPlayer->specialPhase!=mgo2win::combat::SpecialPhase::none));
   if(combatPlayable)specialSeconds=specialClock.update(combatEpoch,*hostPlayer,navigationNow);else specialClock.clear();
   combatLights.synchronize(combatPlayable&&combatOffer&&combatState?combatOffer->epoch:0,stageResult.generation,combatState?combatState->eventWatermark:0,navigationNow);
   const mgo2win::combat::decals::Scope markScope{combatPlayable&&combatOffer?combatOffer->epoch:0,stageResult.generation};
   bulletMarks.synchronize(markScope,combatState?combatState->eventWatermark:0,navigationNow);materialParticles.synchronize(markScope,combatState?combatState->eventWatermark:0,navigationNow);
   if(login){auto events=login->combat_events();if(stageResult.collision)for(const auto& event:events)if(auto mark=mgo2win::combat::decals::static_impact(event,markScope,*stageResult.collision)){bulletMarks.emit(*mark,navigationNow);materialParticles.emit(*mark,mgo2win::combat::material_effects::verified_kind(mark->material),navigationNow);}if(combatState)combatLights.dispatch(events,*combatState,navigationNow);combatEffects.dispatch(events,navigation.eye(),playCombatSound);}
   combatPointLights=combatLights.sample(navigationNow);
   playerMenu.chat_session(login?login->chat_session():nullptr);
   playerMenu.radio_session(login?login->radio_session():nullptr);
   playerMenu.inventory_session(login?login->inventory_session():nullptr);
   {auto session=login?login->radio_session():nullptr;const auto state=session?session->state():mgo2win::radio::SessionState{};
    if(session!=radioAudioSession||state.generation!=radioAudioGeneration||!state.eligible){radioAudio.reset();radioAudioSession=session;radioAudioGeneration=state.generation;}
    if(radioAudio&&radioAudio->result.load()!=-1)radioAudio.reset();
    if(session)for(const auto& event:session->drain()){
     const auto current=session->state();const auto roster=login->room_host_roster();
     if(current.generation!=state.generation||!current.eligible||!combatPlayable||!hostPlayer||!hostPlayer->alive||hostPlayer->life!=current.life||!combatOffer||!combatState||event.epoch!=combatOffer->epoch||event.epoch!=combatState->epoch||current.self!=combatOffer->self||event.sender.slot>=24||!roster)continue;
     const auto& sender=combatState->players[event.sender.slot];const auto& peer=roster->slots[event.sender.slot];
     if(!sender||sender->identity!=event.sender||sender->life!=event.life||!sender->alive||(requestedStage&&requestedStage->rotation.rule==0?sender->team!=0||hostPlayer->team!=0:sender->team!=hostPlayer->team||!sender->team)||!peer||peer->character!=event.sender.character||peer->instance!=event.sender.instance||peer->slot!=event.sender.slot)continue;
     std::string text;for(const auto& category:mgo2::radio::defaultCategories())for(const auto& message:category.messages)if(message.presetId==event.preset)text=message.text;
     if(auto chat=login->chat_session())chat->receive_radio(event.sender.character,std::move(text),GetTickCount64());
     // Native playback policy: one radio voice, no stale audio backlog. The
     // text still appears when the voice is busy, absent or unsupported.
     if(!sound||radioAudio||!peer->appearance)continue;
     const auto voice=mgo2win::radio_audio::appearance_voice(*peer->appearance);if(!voice)continue;
     const auto cues=mgo2win::radio_audio::resolve(voice->type,event.preset);const auto pitch=mgo2win::radio_audio::pitch_ratio(voice->type,voice->pitchByte);if(!cues||!pitch)continue;
     const auto cue=event.sender==combatOffer->self?cues->self:cues->remote;
     const auto file=mgo2win::radio_audio::asset_path(networkKeys.parent_path()/L"radio",cue);std::error_code error;if(!std::filesystem::is_regular_file(file,error))continue;
     radioAudio=std::make_unique<AudioThread>();radioAudio->control.frequencyRatio=*pitch;radioAudio->start(file,L"30",cue,"preset_radio");
    }
   }
   stageCamera.reset();bool combatInputActive=false;if(login&&login->take_gameplay_options_request())playerMenu.open(mgo2win::player::Menu::settings,true);
   if(((Window::debug.enabled&&Window::inspection)||combatViewing)&&requestedStage&&navigation.ready()&&navigationWorld){
    std::array<float,24> values{};auto&cfg=controllerInput->config;
    bool active=inputActive&&(!combatOffer||combatPlayable)&&(Window::inspection||(login&&login->gameplay_visible()))&&(!combatPlayable||(hostPlayer->alive&&!hostPlayer->stunned))&&!Window::debug.confirmReset&&!playerMenu.visible()&&!musicMenu.visible()&&!Window::invitationGuard.blocks_gameplay(invitation.overlay().visible())&&(!login||!login->personal_overlay_visible())&&(!cfg.device||(pad.connected&&pad.armed));
    if(active){if(cfg.device)values=controllerInput->action_values(pad);else for(unsigned a=4;a<24;++a)values[a]=(GetAsyncKeyState(int(cfg.keyboard[a]))&0x8000)?1.f:0.f;}
    float magnitude=std::min(1.f,std::hypot(values[16]-values[17],values[19]-values[18]));
    movementRunning=active&&mgo2win::input_running(magnitude,movementRunning,cfg);
    if(foreground&&!cfg.device&&(GetAsyncKeyState(VK_SHIFT)&0x8000))movementRunning=false;
    if(yFirstPerson!=playerMenu.prone_y_first_person()){yFirstPerson=playerMenu.prone_y_first_person();player.first_person_on_y(yFirstPerson);}
    auto previousStance=player.stance;player.step(values,navigationSeconds,active&&!ragdoll.active(),movementRunning);
    // Original crouch transition is not yet recovered. The native standing
    // salute needs headroom; collision must approve expansion before request.
    if(player.specialRequested&&combatPlayable){
     if(navigation.shape(*navigationWorld,{350,1700,2})){player.stance=mgo2win::player::Stance::standing;navigation.facing(player.bodyYaw);}
     else player.specialRequested=player.specialHeld=false;
    }
    if(player.stance!=previousStance){using mgo2win::player::Stance;auto shape=player.stance==Stance::standing?mgo2win::stage::Capsule{350,1700,2}:player.stance==Stance::crouching?mgo2win::stage::Capsule{350,1100,2}:mgo2win::stage::Capsule{260,560,2};if(!navigation.shape(*navigationWorld,shape))player.reject_stance(previousStance);}
    if(player.resetView)navigation.reset_view(player.bodyYaw);
    const auto cameraMotion=playerMenu.camera_settings().motion(player.firstPerson,player.aiming,player.turn,player.look);
    const auto cameraRates=playerMenu.camera_settings().rates(player.firstPerson,player.aiming);
    if(active&&!ragdoll.active()&&!player.specialRequested&&!player.special_active())navigation.advance(*navigationWorld,{player.forward,player.right,cameraMotion[0],cameraMotion[1],player.speed,cameraRates[0],cameraRates[1]},navigationSeconds);
    if(active&&ragdoll.active())ragdoll.step(*navigationWorld,std::min(navigationSeconds,.1f));
    if(active&&testBody){testBody->step(*navigationWorld,std::min(navigationSeconds,.1f));auto model=physics_capsule(*testBody);if(bodyRenderer)bodyRenderer->update_vertices(context.Get(),model.vertices);}
    if(std::hypot(player.forward,player.right)>.01f&&!player.dead){float angle=std::atan2(player.right,player.forward);if(player.stance==mgo2win::player::Stance::prone&&player.forward<0)angle=std::atan2(-player.right,-player.forward);player.bodyYaw=navigation.yaw()+angle;}
    if(player.aiming&&player.stance!=mgo2win::player::Stance::prone)player.bodyYaw=navigation.yaw();
    if(player.menu==mgo2win::player::Menu::weapons){login->message(window.handle,WM_KEYDOWN,VK_F4,0);}
    else if(player.menu!=mgo2win::player::Menu::none)playerMenu.open(player.menu);
    combatInputActive=combatPlayable&&active&&!ragdoll.active()&&player.menu==mgo2win::player::Menu::none;
    combatReloadPending|=combatInputActive&&player.reloadStarted;
    combatFirePending|=combatInputActive&&player.firePressed;
    if(combatInputActive&&player.specialRequested){specialInput.press(navigationNow);combatReloadPending=combatFirePending=false;}
    auto lockRoster=login?login->room_host_roster():std::nullopt;
    if(combatInputActive&&combatOffer&&combatState&&lockRoster&&requestedStage){
     mgo2win::player_lock::Input lockInput{login&&login->room_auto_aim()&&hostPlayer&&hostPlayer->weapon==25&&player.autoAim&&player.aiming&&!player.firstPerson&&!player.dead,
      true,combatOffer->self,combatOffer->epoch,stageResult.generation,requestedStage->rotation.rule,navigation.eye(),navigation.direction()};
     auto locked=playerLock.current()?playerLock.update(*combatState,*lockRoster,lockInput,*navigationWorld,stageResult.objectHitCollision.get()):
      playerLock.acquire(*combatState,*lockRoster,lockInput,*navigationWorld,stageResult.objectHitCollision.get());
     if(locked){if(navigation.track_view(locked->aimPoint,navigationSeconds))player.bodyYaw=navigation.yaw();else playerLock.clear();}
    }else playerLock.clear();
    auto eye=navigation.eye(),direction=navigation.direction();
    if(ragdoll.active()){eye=ragdoll.root_position();eye[1]+=700;}
    if(!player.firstPerson){auto target=eye;auto offset=direction;float length=player.aiming?1400.f:2600.f;for(int i=0;i<3;++i)offset[i]*=-length;offset[0]+=std::cos(navigation.yaw())*280;offset[2]-=std::sin(navigation.yaw())*280;
     float distance=std::sqrt(offset[0]*offset[0]+offset[1]*offset[1]+offset[2]*offset[2]);auto ray=offset;for(auto&v:ray)v/=distance;
     if(auto hit=navigationWorld->ray(target,ray,distance+100))distance=std::max(0.f,hit->distance-100.f);
     for(int i=0;i<3;++i)eye[i]=target[i]+ray[i]*distance;
     if(distance>1)for(int i=0;i<3;++i)direction[i]=-ray[i];
    }
    stageCamera=mgo2win::WorldView{eye,direction};
    auto motion=player.motion();if(motion!=lastMotion){lastMotion=motion;motionSeconds=0;}else if(active||(combatPlayable&&motion==mgo2win::player::Motion::reload))motionSeconds+=std::min(.1f,navigationSeconds)*(combatPlayable&&motion==mgo2win::player::Motion::reload?mgo2win::original::rifle_reload_rates[hostPlayer->reloadLevel]:1.f);
   }else{navigationArmed=false;player.suspend();movementRunning=false;if(!login||!login->character_visible())playerMenu.close();ragdoll.stop();testBody.reset();bodyRenderer.reset();Window::testRagdoll=Window::testBody=false;}
   waterEffects.update(combatPlayable?combatEpoch:stageResult.generation,combatPlayable?combatLife:1,navigation.feet(),navigation.water_state(),navigation.ready()&&navigationWorld&&inputActive&&!playerMenu.visible()&&(combatPlayable||Window::inspection),navigationSeconds);
   if(!combatInputActive){combatReloadPending=combatFirePending=false;specialInput.clear();playerLock.clear();}
   const bool inventoryPose=combatPlayable&&inputActive&&playerMenu.inventory_menu()&&hostPlayer&&hostPlayer->alive&&!hostPlayer->stunned;
   if(combatPlayable&&navigation.ready()&&(((combatInputActive||inventoryPose)&&navigationNow>=combatSendAt)||(!combatInputActive&&combatInputWasActive))){
    mgo2win::combat::wire::Input input;input.epoch=combatEpoch;input.life=combatLife;input.sequence=++combatSequence;input.pose={navigation.feet(),navigation.yaw(),navigation.pitch(),navigation.capsule()};input.weapon=hostPlayer->weapon;input.suspended=!combatInputActive&&!inventoryPose;input.reload=hostPlayer->weapon&&combatReloadPending;input.fire=hostPlayer->weapon&&combatInputActive&&player.triggerHeld&&!input.reload;input.firePressed=hostPlayer->weapon&&combatFirePending&&!input.reload;
    input.specialPressed=combatInputActive&&specialInput.edge();input.specialHeld=combatInputActive&&player.specialHeld;
    if(input.specialPressed||input.specialHeld){input.reload=input.fire=input.firePressed=false;}
    login->combat_input(input);specialInput.sent(input.sequence);combatReloadPending=combatFirePending=false;combatSendAt=navigationNow+50;
   }
   combatInputWasActive=combatInputActive;
   auto stageAppearance=login?login->stage_appearance():std::nullopt;
   if(stageAppearance!=avatarAppearance){ragdoll.stop();avatarAppearance=stageAppearance;avatarRenderer.reset();avatar={};if(characterCatalog&&stageAppearance){avatar=characterCatalog->assemble(*stageAppearance);if(avatar.ready())avatarRenderer=std::make_unique<mgo2win::CharacterRenderer>(device.Get(),avatar.model);}}
   motionMissing=false;
   auto remoteRoster=login?login->room_host_roster():std::nullopt;
   if(combatViewing&&stageCamera&&combatOffer&&combatState&&remoteRoster){remoteScene.update(*combatState,*remoteRoster,combatOffer->self,navigationNow);remoteAvatars=remoteScene.sample(navigationNow);}
   else{remoteScene.clear();remoteAvatars.clear();}
   // Original MTSQ event clocks and bone positions; native dry floor and tableId0 policy.
   auto stepSounds=[&](mgo2win::combat::footsteps::Timeline& timeline,const mgo2win::PreparedCharacter& body,
       mgo2win::PlayerMotion action,double seconds,mgo2win::stage::Vec3 feet,float yaw,uint64_t actor,uint64_t life,bool eligible,bool grounded){
    const auto* clip=playerMotions?playerMotions->find(action):nullptr;
    if(!clip||!navigationWorld||!stageResult.request||!clip->loop||clip->fps!=60||
       (clip->sourceIndex==9?clip->frames!=60:clip->sourceIndex==10?clip->frames!=40:true)){timeline.reset();return;}
    auto material=mgo2win::combat::footsteps::dry_floor(*navigationWorld,feet,navigationWater.get());
    auto events=timeline.advance({combatOffer?combatOffer->epoch:1,stageResult.generation,actor,life,clip->sourceKey,clip->sourceIndex,seconds,grounded&&bool(material),eligible});
    for(unsigned i=0;i<events.count;++i){const auto& event=events.values[i];auto bone=body.bone_position(event.bone);if(!bone||!material)continue;
     auto point=mgo2win::combat::footsteps::world_bone(*bone,feet,yaw);auto cue=mgo2win::combat::material_audio::resolve(mgo2win::combat::material_audio::stage_for_map(stageResult.request->rotation.map),event.cue,material->id);
     if(point&&cue&&*cue)combatEffects.play_cue(*cue,*point,navigation.eye(),playCombatSound);
    }
   };
   std::array<bool,24> remotePresent{};
   for(const auto&remote:remoteAvatars){auto slot=remote.identity.slot;remotePresent[slot]=true;auto&cached=remoteModels[slot];
    if(!characterCatalog||!playerMotions){cached.reset();remoteFootsteps[slot].reset();continue;}
    if(!cached||cached->appearance!=remote.appearance){RemoteModel model;model.appearance=remote.appearance;model.body=characterCatalog->assemble(remote.appearance);
     if(model.body.ready()&&!model.body.missingModels&&!model.body.missingColors)model.renderer=std::make_unique<mgo2win::CharacterRenderer>(device.Get(),model.body.model);cached=std::move(model);}
    cached->visible=false;if(cached->renderer){auto pose=specialPose(cached->body.gender,remote.specialPhase,remote.specialSeconds);if(!pose)pose=playerMotions->sample(remote.motion,remote.seconds);if(pose){characterCatalog->pose(cached->body,*pose);cached->renderer->update_vertices(context.Get(),cached->body.model.vertices);cached->visible=true;
     stepSounds(remoteFootsteps[slot],cached->body,remote.motion,remote.seconds,remote.origin,remote.yaw,(uint64_t(remote.identity.instance)<<32)|remote.identity.character,remote.life,
       combatPlayable&&inputActive&&!playerMenu.visible()&&!musicMenu.visible()&&remote.alive&&!remote.stunned,true);
    }}if(!cached->visible)remoteFootsteps[slot].reset();
   }
   for(unsigned i=0;i<24;++i)if(!remotePresent[i]){remoteModels[i].reset();remoteFootsteps[i].reset();}
   bool physicsInputActive=inputActive&&!Window::debug.confirmReset&&!playerMenu.visible()&&!musicMenu.visible()&&Window::debug.enabled&&Window::inspection&&requestedStage==stageResult.request;
   if(Window::testBody&&physicsInputActive&&stageCamera&&navigationWorld){
    mgo2win::physics::RigidBody body;body.radius=150;body.halfLength=200;body.mass=10;
    auto direction=navigation.direction();body.position=navigation.eye();for(int j=0;j<3;++j){body.position[j]+=direction[j]*900;body.velocity[j]=direction[j]*2400;}body.velocity[1]+=1200;body.angularVelocity={1.5f,.5f,2.f};
    auto segment=body.segment();if(navigationWorld->contacts(segment.first,segment.second,body.radius,0).empty()){testBody=body;bodyRenderer=std::make_unique<mgo2win::CharacterRenderer>(device.Get(),physics_capsule(body));physicsError.clear();}else physicsError=L"確認物体の配置先が壁に近すぎます。";
   }
   if(stageCamera&&avatarRenderer&&characterCatalog){
    auto pose=playerMotions?playerMotions->sample(static_cast<mgo2win::PlayerMotion>(player.motion()),motionSeconds):std::nullopt;
    if(combatPlayable)if(auto special=specialPose(avatar.gender,hostPlayer->specialPhase,specialSeconds))pose=std::move(special);
    if(Window::testRagdoll&&physicsInputActive){physicsError.clear();
     if(ragdoll.active()){
      auto hint=ragdoll.root_position();mgo2win::stage::Navigation recovery({260,560,2});
      if(recovery.place(*navigationWorld,hint,10000)&&recovery.feet()[1]<=hint[1]){bool supine=ragdoll.supine();ragdoll.stop();navigation=recovery;navigation.facing(player.bodyYaw);player.knock_down(supine);motionSeconds=0;pose=playerMotions?playerMotions->sample(static_cast<mgo2win::PlayerMotion>(player.motion()),0):std::nullopt;}
      else physicsError=L"起きる場所を確保できません。F5で元の位置へ戻れます。";
     }else if(pose&&ragdoll.start(*characterCatalog,avatar.gender,*pose,navigation.feet(),player.bodyYaw)){
      player.firstPerson=false;auto point=ragdoll.root_position();point[1]+=500;auto direction=navigation.direction();ragdoll.impulse({direction[0]*70000,30000,direction[2]*70000},point);
     }else physicsError=L"このキャラクターの物理姿勢を準備できません。";
    }
    if(ragdoll.active())pose=ragdoll.pose();
    if(pose)characterCatalog->pose(avatar,*pose);else{motionMissing=true;characterCatalog->pose(avatar,motionSeconds);}
    avatarRenderer->update_vertices(context.Get(),avatar.model.vertices);
    if(pose)stepSounds(selfFootsteps,avatar,static_cast<mgo2win::PlayerMotion>(player.motion()),motionSeconds,navigation.feet(),player.bodyYaw,
      combatOffer?(uint64_t(combatOffer->self.instance)<<32)|combatOffer->self.character:1,combatPlayable?combatLife:1,
      combatInputActive&&inputActive&&!playerMenu.visible()&&!musicMenu.visible()&&!player.dead&&!ragdoll.active(),navigation.grounded());
    else selfFootsteps.reset();
   }else selfFootsteps.reset();
   Window::testRagdoll=Window::testBody=false;
   if(login)login->stage_navigation_feedback(bool(stageCamera));
   if(!musicReady&&musicScan.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{musicLibrary=musicScan.get();}catch(...){musicError=L"BGM一覧の読み込みに失敗しました。";}musicReady=true;
    if(auto t=musicLibrary.find("original:bgm_mgo_action01")){musicSelection.choose(musicLibrary,t->id,true,false);musicIndex=size_t(t-musicLibrary.tracks.data());}
   }
   bool hasStage=login&&bool(login->stage_request());
   // Music belongs to the admitted stage, not the currently visible room tab.
   auto musicLoadRequest=login?login->stage_load_request():std::nullopt;
   bool hasMusicStage=bool(musicLoadRequest);
   bool canChooseMusic=login&&login->weapon_music_available();
   if(musicMenu.visible()&&(!canChooseMusic||musicMenuRequest!=login->stage_load_request())){musicMenu.close();musicMenu.take_choice();}
   if(login&&login->take_weapon_music_request()&&musicReady&&canChooseMusic){
    musicLibrary.reload_titles(musicRoot);musicMenuRequest=login->stage_load_request();
    playerMenu.close();musicMenu.open(musicLibrary,musicSelection.selected());player.suspend();
   }
   if(auto selected=musicMenu.take_choice();selected&&canChooseMusic&&musicMenuRequest==login->stage_load_request()){
    if(musicSelection.choose(musicLibrary,*selected,false,true)){musicPlaying=true;musicError.clear();
     if(auto t=musicLibrary.find(*selected))musicIndex=size_t(t-musicLibrary.tracks.data());
    }
   }
   auto musicPreparation=login?login->combat_preparation():std::nullopt;
   bool musicDeployed=false;
   if(const auto& prep=musicPreparation;prep&&prep->self.slot<prep->players.size()){
    const auto& self=prep->players[prep->self.slot];musicDeployed=self&&self->deployed;
   }
   if(hasMusicStage&&musicDeployed&&!wasMusicDeployed){musicSelection.respawn();musicPlaying=true;musicError.clear();}
   wasMusicDeployed=hasMusicStage&&musicDeployed;
   if(musicReady&&Window::debug.enabled&&!debugTitle)musicLibrary.reload_titles(musicRoot);
   if(hasStage&&Window::debug.enabled&&musicReady){
    if(Window::debug.musicStep&&!musicLibrary.tracks.empty()){
     auto n=int(musicLibrary.tracks.size());musicIndex=size_t((int(musicIndex)+Window::debug.musicStep%n+n)%n);
     musicSelection.choose(musicLibrary,musicLibrary.tracks[musicIndex].id,true,false);musicPlaying=true;musicError.clear();
    }
    if(Window::debug.toggleMusic){musicPlaying=!musicPlaying;musicError.clear();}
   }
   if(!hasMusicStage){stageMusic.reset();musicPlayback.clear();musicPlaying=false;musicSelection.force(std::nullopt);}
   auto musicChoice=musicSelection.resolve(musicLibrary);
   mgo2win::stage::RoundMusic roundMusic{musicChoice.track};
   auto roundMusicPhase=mgo2win::stage::tdm_round_music_phase(musicLoadRequest,musicPreparation);
   if(auto phase=roundMusicPhase)
    roundMusic=mgo2win::stage::resolve_round_music(musicLibrary,musicChoice.track,*phase);
   if(!hasMusicStage||!musicPlaying||!sound||!musicChoice.track||musicChoice.track->additional||musicChoice.track->id!="original:bgm_mgo_action01")roundMusicPhase.reset();
   if(roundMusicPhase!=loggedMusicPhase){
    loggedMusicPhase=roundMusicPhase;
    const bool clockKnown=musicPreparation&&musicLoadRequest&&musicPreparation->generation==musicLoadRequest->generation&&musicPreparation->runtimeReady&&musicPreparation->roundClock;
    if(roundMusicPhase)std::osyncstream(std::cout)<<"{\"round_bgm_phase\":\""<<(*roundMusicPhase==mgo2win::stage::RoundMusicPhase::normal?"normal":*roundMusicPhase==mgo2win::stage::RoundMusicPhase::action?"action":"urgent")<<"\",\"clock_known\":"<<(clockKnown?"true":"false")<<",\"round_remaining_ms\":"<<(clockKnown?musicPreparation->roundRemainingMs:0)<<"}"<<std::endl;
   }
   if(hasMusicStage&&musicPlaying&&sound&&roundMusic.track&&musicPlayback.select(roundMusic.track,roundMusic.alternateWave)){
    stageMusic.reset();stageMusic=std::make_unique<AudioThread>();stageMusic->control.loopWhole=true;
    stageMusic->control.alternateWave=roundMusic.alternateWave;
    stageMusic->control.layerMix=mgo2win::stage::action01_mix;
    stageMusic->control.alternate=roundMusic.alternate;
    stageMusic->start(roundMusic.track->path,argv[2],0,"stage_bgm");
   }
   if(stageMusic)stageMusic->control.alternate=roundMusic.alternate;
   if(!musicPlaying){stageMusic.reset();musicPlayback.clear();}
   if(stageMusic&&stageMusic->result.load()!=-1){bool failed=stageMusic->result.load()!=0;stageMusic.reset();musicPlayback.clear();musicPlaying=false;musicError=failed?L"曲を再生できません。別の曲を選んでください。":L"再生終了";}
   if(login){
    std::wstring title=musicReady?(roundMusic.track?roundMusic.track->title:L"再生できるBGMがありません"):L"BGM一覧を読み込み中…";
    if(musicChoice.missingForced)title+=L"（指定曲がないため代替曲）";
    if(!musicError.empty())title+=L" / "+musicError;
    login->weapon_music_feedback(std::move(title),musicReady);
   }
   if(login){std::wstring notice;
   if(Window::debug.enabled&&hasStage){notice=L"F5：ローカル再初期化   F10：歩行確認／全景\nF7 / F8：曲選択   F9：再生／停止\n";
     if(Window::inspection){notice+=L"F11：倒れる／操作へ戻る　Shift+F11：物体落下\n";if(ragdoll.active())notice+=L"物理姿勢：13剛体・12関節（ローカル確認）\n";if(!physicsError.empty())notice+=physicsError+L"\n";}
     if(Window::inspection){if(navigation.ready()){notice+=std::wstring(mgo2win::player::stance_name(player))+L" / "+(player.firstPerson?L"主観":L"三人称")+L"\n";notice+=std::wstring(player.aiming?L"構え ":L"")+(player.firing?L"発射入力 ":L"")+(player.reloading()?L"リロード ":L"")+(player.autoAim?L"AUTO AIM ON":L"AUTO AIM OFF")+L"\n";if(combatPlayable)notice+=L"HP "+std::to_wstring(hostPlayer->hp)+L" / "+std::to_wstring(hostPlayer->maxHp)+L"   AMMO "+std::to_wstring(hostPlayer->ammo)+L"\n";else notice+=L"ローカル操作確認（ホストの出撃許可待ち）\n";if(motionMissing)notice+=L"この動作の原版モーションは未復旧\n";}else notice+=L"歩行確認：床・立ち位置を確認できません\n";}
     if(stageResult.objectSnapshot)notice+=L"物体状態 "+std::to_wstring(stageResult.objectSnapshot->objects.size())+L"件受信済み / 世代 "+std::to_wstring(stageResult.objectSnapshot->request.generation)+L"\n";
     else if(login->scene_status()==mgo2win::stage::SceneSyncStatus::waiting_snapshot)notice+=L"物体の初期状態を受信中…\n";
     if(stageResult.received)notice+=L"受信配置 "+std::to_wstring(stageResult.received->items.size())+L"件 / モデル未対応 "+std::to_wstring(stageResult.missingItemModels)+L"件\n";
     if(stageResult.cboxLayout&&!Window::inspection)notice+=L"箱の配置選択 "+std::to_wstring(stageResult.cboxes.size())+L"件 / 共有世代 "+std::to_wstring(stageResult.request->generation)+(stageResult.objectSnapshot?L"（受信状態を反映）\n":L"（物体状態の受信待ち）\n");
     if(!musicReady)notice+=L"BGM一覧を読み込み中…";
     else if(musicLibrary.tracks.empty())notice+=L"BGMなし（data/bgm にPCM WAVを配置）";
     else{auto&t=musicLibrary.tracks[musicIndex];notice+=(t.additional?L"追加曲 ":L"原曲 ")+std::to_wstring(musicIndex+1)+L" / "+std::to_wstring(musicLibrary.tracks.size())+L"\n"+t.title+L"\n"+(stageMusic?L"再生中":L"停止中");}
     if(musicChoice.missingForced)notice+=L"\n指定BGMがありません。ローカル曲へ復帰。";
     if(!sound)notice+=L"\n音声は無効です。";
     if(!musicError.empty())notice+=L"\n"+musicError;
     if(musicLibrary.rejected||musicLibrary.overflow)notice+=L"\n未対応・重複 "+std::to_wstring(musicLibrary.rejected)+L" / 上限超過 "+std::to_wstring(musicLibrary.overflow);
     if(musicLibrary.playlistErrors)notice+=L"\n曲名ファイルの無効な記述："+std::to_wstring(musicLibrary.playlistErrors);
    }login->stage_debug_feedback(std::move(notice));login->stage_reset_feedback(hasStage&&Window::debug.confirmReset,Window::debug.resetYes);
   }
   Window::debug.clear_actions();
   auto displayStage=Window::debug.enabled&&stageResult.debugModel?stageResult.debugModel:(stageResult.receivedModel?stageResult.receivedModel:(stageResult.objectModel?stageResult.objectModel:stageResult.model));
   if(stageModel!=displayStage){stageModel=displayStage;stageRenderer.reset();if(stageModel){try{stageRenderer=std::make_unique<mgo2win::CharacterRenderer>(device.Get(),*stageModel);}catch(...){stageResult.status=mgo2win::stage::Status::graphics_error;}}}
   if(stageModel&&!stageRenderer)stageResult.status=mgo2win::stage::Status::graphics_error;
   if(login)login->stage_feedback(stageResult);
   if(!stageModel||!login||!login->stage_request()){
    stageAudio.reset();stageAudioIndex=0;Window::stageAudition=false;lobbyAudio.control.gain=1;if(login)login->stage_audio_feedback(L"");
   }else if(Window::stageAudition){
    Window::stageAudition=false;stageAudio.reset();stageAudioIndex=(stageAudioIndex+1)%6;
    // Audition is explicitly user selected. Registration order does not prove
    // the original VLM/SDS listener-region selection or its crossfade behavior.
    constexpr unsigned cues[]={64,67,68,117,115};constexpr const wchar_t*files[]={L"env_s01a30l_01.gwa",L"env_s01a30l_04.gwa",L"env_s01a30l_05.gwa",L"env_s01a30l_07.gwa",L"env_s01a30l_08.gwa"};
    auto path=networkKeys.parent_path()/L"stage/audio"/(stageAudioIndex?files[stageAudioIndex-1]:L"");
    if(stageAudioIndex&&sound&&std::filesystem::is_regular_file(path)){stageAudio=std::make_unique<AudioThread>();stageAudio->start(path,argv[2],cues[stageAudioIndex-1],"stage_ambience_audition");login->stage_audio_feedback(L"試聴 "+std::to_wstring(stageAudioIndex)+L" / 5");}
    else{login->stage_audio_feedback(stageAudioIndex?L"環境音を再生できません（音声設定／ファイルを確認）。":L"試聴停止");stageAudioIndex=0;}
    lobbyAudio.control.gain=stageAudio?0.f:1.f;
   }
   if(stageAudio&&stageAudio->result.load()!=-1){auto failed=stageAudio->result.load()!=0;stageAudio.reset();lobbyAudio.control.gain=1;stageAudioIndex=0;if(login)login->stage_audio_feedback(failed?L"環境音の再生に失敗しました。":L"試聴終了");if(failed)std::osyncstream(std::cout)<<"{\"stage_ambience_error\":true}\n";}
   lobbyAudio.control.gain=(stageAudio||stageMusic)?0.f:1.f;
   if(login){login->model_available(bool(characterRenderer));login->model_partial(prepared.missingModels||prepared.missingColors);
    auto*bank=prepared.gender<2?selectionMotions[prepared.gender].get():nullptr;auto*salute=bank?bank->find(mgo2win::PlayerMotion::SelectionSalute):nullptr;
    login->selection_presentation(salute?uint32_t((uint64_t(salute->frames)*1000+salute->fps-1)/salute->fps):0,false,selectionBox&&bank&&bank->has(mgo2win::PlayerMotion::SelectionBox),400);
    if(login->take_selection_sound()&&sound&&std::filesystem::is_regular_file(selectionSoundPath)){selectionAudio=std::make_unique<AudioThread>();selectionAudio->start(selectionSoundPath,L"10",75,"pc_selection_salute");}
   }
   const void* ui=login?login->draw():agreement->draw();
   if(playerMenu.visible())ui=playerMenu.draw();
   for(auto cue:playerMenu.cues())menuSound(cue);
   if(musicMenu.visible())ui=musicMenu.draw();
   for(auto cue:musicMenu.cues())menuSound(cue);
   // Tags use the same eye/direction as the host-authorized firing pose. The
   // third-person camera offset only affects the final label projection.
   uint32_t enemyClanId=0;std::optional<mgo2win::enemy_tag::Target> enemyTag;
   const auto currentTagRoster=login?login->room_host_roster():std::nullopt;
   const auto currentTagOffer=login?login->combat_offer():std::nullopt;
   const auto currentTagState=login?login->combat_state():std::nullopt;
   if(login&&combatInputActive&&(player.aiming||player.firstPerson)&&stageCamera&&navigationWorld&&combatOffer&&combatState&&currentTagRoster&&requestedStage&&
      stageResult.request==requestedStage&&login->stage_load_request()==requestedStage&&combatState->epoch==combatOffer->epoch&&
      currentTagOffer&&currentTagState&&currentTagOffer->epoch==combatOffer->epoch&&currentTagOffer->self==combatOffer->self&&
      currentTagState->epoch==combatState->epoch&&login->combat_status()==mgo2win::combat::wire::Status::active&&
      currentTagState->players[combatOffer->self.slot]&&currentTagState->players[combatOffer->self.slot]->identity==combatOffer->self&&
      currentTagState->players[combatOffer->self.slot]->alive&&!currentTagState->players[combatOffer->self.slot]->stunned&&
      hostPlayer&&currentTagState->players[combatOffer->self.slot]->life==hostPlayer->life&&
      !playerMenu.visible()&&!musicMenu.visible()&&login->gameplay_visible()){
    if(playerLock.current()){
     mgo2win::player_lock::Input freshLock{login->room_auto_aim()&&currentTagState->players[combatOffer->self.slot]->weapon==25&&player.autoAim&&player.aiming&&!player.firstPerson&&!player.dead,true,combatOffer->self,combatOffer->epoch,stageResult.generation,requestedStage->rotation.rule,navigation.eye(),navigation.direction()};
     playerLock.update(*currentTagState,*currentTagRoster,freshLock,*navigationWorld,stageResult.objectHitCollision.get());
    }
    enemyTag=mgo2win::enemy_tag::select(*currentTagState,*currentTagRoster,combatOffer->self,requestedStage->rotation.rule,
      navigation.eye(),navigation.direction(),*navigationWorld,playerMenu.enemy_name_tags()&&login->room_enemy_name_tags(),stageResult.objectHitCollision.get());
    if(playerMenu.enemy_name_tags()&&login->room_enemy_name_tags())if(const auto&locked=playerLock.current();locked){
     const auto&p=currentTagState->players[locked->identity.slot];const auto&entry=currentTagRoster->slots[locked->identity.slot];
     if(p&&p->identity==locked->identity&&p->life==locked->life&&p->alive&&!p->stunned&&entry&&entry->instance==p->identity.instance&&entry->character==p->identity.character){
      auto head=p->pose.feet;head[1]+=p->pose.capsule.height+90;enemyTag=mgo2win::enemy_tag::Target{p->identity,head,locked->distance};
     }
    }
    if(enemyTag){
     auto presented=std::find_if(remoteAvatars.begin(),remoteAvatars.end(),[&](const auto&a){return a.identity==enemyTag->identity&&a.alive&&a.life==currentTagState->players[enemyTag->identity.slot]->life;});
     const auto&model=remoteModels[enemyTag->identity.slot];
     if(presented==remoteAvatars.end()||!model||!model->visible)enemyTag.reset();
     else{auto&authoritative=*currentTagState->players[enemyTag->identity.slot];enemyTag->head=presented->origin;enemyTag->head[1]+=authoritative.pose.capsule.height+90;
      auto chest=presented->origin;chest[1]+=authoritative.pose.capsule.height*.65f;
      if(!mgo2win::enemy_tag::visible(stageCamera->eye,chest,*navigationWorld,stageResult.objectHitCollision.get())||
         !mgo2win::enemy_tag::visible(stageCamera->eye,enemyTag->head,*navigationWorld,stageResult.objectHitCollision.get()))enemyTag.reset();
      else enemyClanId=currentTagRoster->slots[enemyTag->identity.slot]->clanId;}
    }
   }
   if(login){auto emblem=login->enemy_clan_emblem(enemyClanId);if(!emblem.image||emblem.serial!=enemyTagClanSerial){enemyTagClanSerial=emblem.serial;enemyTagBitmap.reset();if(emblem.image&&emblem.clan==enemyClanId)enemyTagBitmap=std::make_unique<mgo2win::clan::Bitmap>(*emblem.image);}}
   if(enemyTag&&stageCamera){
    if(auto point=mgo2win::enemy_tag::project(enemyTag->head,stageCamera->eye,stageCamera->direction,620,120,616,392)){
     if(ui!=enemyTagSurface.data())std::memcpy(enemyTagSurface.data(),ui,enemyTagSurface.size()*sizeof(uint32_t));
     enemyTagRenderer.paint(enemyTagSurface,1280,720,point->x,point->y,
      currentTagRoster->slots[enemyTag->identity.slot]->name,enemyTagBitmap?enemyTagBitmap->get():nullptr);ui=enemyTagSurface.data();
    }
   }
   if(stageCamera&&navigationWorld&&!playerMenu.visible()&&!musicMenu.visible()&&(combatPlayable||Window::inspection)){
    if(ui!=enemyTagSurface.data())std::memcpy(enemyTagSurface.data(),ui,enemyTagSurface.size()*sizeof(uint32_t));
    if(stageResult.collision){auto marks=bulletMarks.sample(navigationNow);mgo2win::combat::decals::paint(enemyTagSurface,marks,stageCamera->eye,stageCamera->direction,*stageResult.collision,stageResult.objectHitCollision.get());}
    if(stageResult.collision){auto particles=materialParticles.lines(navigationNow);mgo2win::combat::material_effects::paint(enemyTagSurface,particles,stageCamera->eye,stageCamera->direction,*stageResult.collision,stageResult.objectHitCollision.get());}
    auto lines=waterEffects.lines();mgo2win::stage::paint_water(enemyTagSurface,1280,720,lines,stageCamera->eye,stageCamera->direction,*navigationWorld,stageResult.objectHitCollision.get());
    if(login)if(auto session=login->inventory_session()){const auto itemState=session->state();
     if(itemState.context.active&&itemState.world&&combatOffer&&itemState.context.scope.epoch==combatOffer->epoch&&itemState.context.actor.life==combatLife){unsigned painted=0;
      for(const auto& entity:itemState.world->entities){mgo2win::stage::Vec3 point{entity.position.x,entity.position.y+100,entity.position.z};auto delta=mgo2win::enemy_tag::sub(point,stageCamera->eye);if(mgo2win::enemy_tag::dot(delta,delta)>10000.f*10000.f||!mgo2win::enemy_tag::visible(stageCamera->eye,point,*navigationWorld,stageResult.objectHitCollision.get()))continue;
       if(auto p=mgo2win::enemy_tag::project(point,stageCamera->eye,stageCamera->direction,620,120,616,392)){enemyTagRenderer.paint(enemyTagSurface,1280,720,p->x,p->y,entity.kind==mgo2win::items::PlacementKind::installed?"AK102 [PLACED]":"AK102 [PICK UP]",nullptr);if(++painted==24)break;}
      }
     }
    }ui=enemyTagSurface.data();
   }
   invitation.session(login?login->invitation_session():nullptr);invitation.update(GetTickCount64());
   invitation.overlay().set_menu_hint(playerMenu.settings_menu()&&!playerMenu.capturing());
   if(invitation.overlay().available()){
    if(ui!=enemyTagSurface.data())std::memcpy(enemyTagSurface.data(),ui,enemyTagSurface.size()*sizeof(uint32_t));
    invitationFrameModal=invitation.overlay().paint(enemyTagSurface,1280,720,GetTickCount64())&&invitation.overlay().visible();ui=enemyTagSurface.data();
   }
   context->UpdateSubresource(agreementTexture.Get(),0,nullptr,ui,1280*4,0);
   bool portNow=login&&login->port_visible();if(portNow!=portTitle){portTitle=portNow;window.title(portNow?L"OpenMGO2 - Port settings | Esc: back":L"OpenMGO2 - Login | Enter: select | Esc: back");}
   bool charNow=login&&login->character_visible();if(charNow!=charTitle){charTitle=charNow;window.title(charNow?L"OpenMGO2 - Characters | Esc: settings":L"OpenMGO2 - Settings | Esc: back");}
   if(Window::debug.enabled)window.title(L"OpenMGO2 - DEBUG | F12: close | F10: collision walk | F5: local stage reset | F7/F8: BGM | F9: play/stop");
   else if(debugTitle)window.title(L"OpenMGO2 | F12: debug");debugTitle=Window::debug.enabled;
   quads.clear();
   auto appendMotion=[&](mgo2win::TitleAnimation& m){m.tick(5,0);auto v=m.geometry();for(auto&q:v)if(q.atlas>=0)q.atlas+=motionOffset;quads.insert(quads.end(),v.begin(),v.end());};
   if(motionBack)appendMotion(*motionBack);
   const bool loginFrame=login&&!login->port_visible();
   const auto&frame=loginFrame?loginBackground:agreementBackground;
   // l_free_2_bg LA2 nodes 121/122 are the literal ONLINE MODE heading.
   // Keep the original frame; native screen-specific headings replace only those glyphs.
   for(const auto&q:frame)if(loginFrame||(q.node!=121&&q.node!=122))quads.push_back(q);
   if(motionFront)appendMotion(*motionFront);
   if(characterRenderer&&login&&login->model_preview_visible()){
    // A translucent black panel separates the character from the animated backdrop.
    quads.emplace_back();auto& panel=quads.back();panel.atlas=-1;panel.blend=0;
    const float panelXY[4][2]={{720,196},{1160,196},{1160,588},{720,588}};
    for(int i=0;i<4;++i){float values[]={panelXY[i][0],panelXY[i][1],0,0,0,0,0,.38f};std::memcpy(&panel.vertices[i],values,sizeof(Vertex));}
    quads.emplace_back();auto& model=quads.back();model.atlas=-3;model.blend=0;
    // 140% of the former 440x280 preview; preserve its camera/aspect ratio.
    const float xy[4][2]={{632,196},{1248,196},{1248,588},{632,588}},uv[4][2]={{0,0},{1,0},{1,1},{0,1}};
    for(int i=0;i<4;++i){float values[]={xy[i][0],xy[i][1],uv[i][0],uv[i][1],1,1,1,1};std::memcpy(&model.vertices[i],values,sizeof(Vertex));}
   }
   if(stageRenderer&&login&&login->stage_load_request()&&login->stage_load_request()==stageAssets.result().request){
    quads.emplace_back();auto& panel=quads.back();panel.atlas=-1;panel.blend=0;
    const bool briefingBackground=!login->stage_request();
    const float left=briefingBackground?0.f:stageCamera?620.f:785.f,top=briefingBackground?0.f:stageCamera?120.f:292.f,right=briefingBackground?1280.f:stageCamera?1236.f:1130.f,bottom=briefingBackground?720.f:512.f;
    const float xy[4][2]={{left,top},{right,top},{right,bottom},{left,bottom}},uv[4][2]={{0,0},{1,0},{1,1},{0,1}};
    for(int i=0;i<4;++i){float v[]={xy[i][0],xy[i][1],0,0,0,0,0,.7f};std::memcpy(&panel.vertices[i],v,sizeof(Vertex));}
    quads.emplace_back();auto& model=quads.back();model.atlas=-4;model.blend=0;
    for(int i=0;i<4;++i){float v[]={xy[i][0],xy[i][1],uv[i][0],uv[i][1],1,1,1,1};std::memcpy(&model.vertices[i],v,sizeof(Vertex));}
   }
   quads.emplace_back();auto& q=quads.back();q.atlas=-2;q.blend=0;
   const float xy[4][2]={{0,0},{1280,0},{1280,720},{0,720}};const float uv[4][2]={{0,0},{1,0},{1,1},{0,1}};
   for(int i=0;i<4;++i){float values[]={xy[i][0],xy[i][1],uv[i][0],uv[i][1],1,1,1,1};std::memcpy(&q.vertices[i],values,sizeof(Vertex));}
   vertices.clear();for(const auto& draw:quads)for(int i:{0,1,2,0,2,3})vertices.push_back(draw.vertices[i]);count=static_cast<unsigned>(quads.size());
  }
  D3D11_MAPPED_SUBRESOURCE mapped{};check(context->Map(vb.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped));if(!vertices.empty())std::memcpy(mapped.pData,vertices.data(),vertices.size()*sizeof(Vertex));context->Unmap(vb.Get(),0);
  bool modelVisible=characterRenderer&&agreementVisible&&login&&login->model_preview_visible();
  if(modelVisible){if(characterCatalog){auto frame=login->selection_frame();auto*bank=prepared.gender<2?selectionMotions[prepared.gender].get():nullptr;std::optional<mgo2win::MotionPose> selectedPose;
   double boxEnterSeconds=0;if(bank)if(auto*entry=bank->find(mgo2win::PlayerMotion::SelectionBoxEnter))boxEnterSeconds=double(entry->frames)/entry->fps;
   const auto presentationNow=GetTickCount64();
   if(frame.kind!=previousSelectionKind){selectionBlendFrom=prepared.model.vertices;selectionLiftFrom=selectionLift;selectionTransitionBegan=presentationNow;selectionBlendDuration=previousSelectionKind==mgo2win::SelectionPresentation::Kind::box?.2:frame.kind==mgo2win::SelectionPresentation::Kind::box?8./60.:0;previousSelectionKind=frame.kind;}
   const double transitionSeconds=presentationNow>=selectionTransitionBegan?double(presentationNow-selectionTransitionBegan)/1000.:0;
   const bool enteringBox=frame.kind==mgo2win::SelectionPresentation::Kind::box;
   const double liftDuration=enteringBox?std::max(boxEnterSeconds,.2):.2;
   const float liftT=float(std::clamp(transitionSeconds/liftDuration,0.,1.));const float liftSmooth=liftT*liftT*(3-2*liftT);
   selectionLift=selectionLiftFrom+((enteringBox?.12f:0.f)-selectionLiftFrom)*liftSmooth;
   if(bank&&frame.kind!=mgo2win::SelectionPresentation::Kind::idle){auto action=frame.kind==mgo2win::SelectionPresentation::Kind::salute?mgo2win::PlayerMotion::SelectionSalute:frame.kind==mgo2win::SelectionPresentation::Kind::magazine?mgo2win::PlayerMotion::SelectionMagazine:frame.seconds<boxEnterSeconds?mgo2win::PlayerMotion::SelectionBoxEnter:mgo2win::PlayerMotion::SelectionBox;double seconds=frame.seconds;if(action==mgo2win::PlayerMotion::SelectionBox)seconds-=boxEnterSeconds;selectedPose=mgo2win::selection_pose(*bank,action,seconds);}
   if(selectedPose)characterCatalog->pose(prepared,*selectedPose);else characterCatalog->pose(prepared,(GetTickCount64()-modelBegan)/1000.);
   if(selectionBlendDuration>0&&transitionSeconds<selectionBlendDuration&&selectionBlendFrom.size()==prepared.model.vertices.size()){float t=float(transitionSeconds/selectionBlendDuration);t=t*t*(3-2*t);for(size_t i=0;i<prepared.model.vertices.size();++i){auto&v=prepared.model.vertices[i];auto&from=selectionBlendFrom[i];v.x=from.x+(v.x-from.x)*t;v.y=from.y+(v.y-from.y)*t;v.z=from.z+(v.z-from.z)*t;}}
   bool box=frame.kind==mgo2win::SelectionPresentation::Kind::box&&frame.seconds>=boxEnterSeconds&&selectionBox&&selectedPose;
   if(box!=previewBox){previewBox=box;selectionComposite.reset();if(box)selectionComposite= mgo2win::selection_with_prop(prepared.model,*selectionBox,-mgo2win::selection_origin_y(*bank));characterRenderer=std::make_unique<mgo2win::CharacterRenderer>(device.Get(),selectionComposite?*selectionComposite:prepared.model);}
   characterRenderer->preview_vertical_offset(selectionLift);
   if(selectionComposite){std::copy(prepared.model.vertices.begin(),prepared.model.vertices.end(),selectionComposite->vertices.begin());characterRenderer->update_vertices(context.Get(),selectionComposite->vertices);}else characterRenderer->update_vertices(context.Get(),prepared.model.vertices);}characterRenderer->render(context.Get(),login->model_yaw());++modelFrames;login->model_rendered();
   context->IASetInputLayout(input.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->IASetVertexBuffers(0,1,&vptr,&stride,&offset);context->IASetIndexBuffer(nullptr,DXGI_FORMAT_R32_UINT,0);context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);context->PSSetSamplers(0,1,&sp);context->RSSetState(raster.Get());context->RSSetViewports(1,&viewport);context->OMSetRenderTargets(1,&rp,nullptr);context->OMSetDepthStencilState(nullptr,0);
  }else if(login&&login->character_visible())++emptyModelFrames;
  // draw() can adopt a new HOST Frame. Scope the SOP overlay and activation
  // notification to that newest complete state, after the UI update.
  auto sopSnapshot=login?login->combat_state():std::nullopt;auto sopOffer=login?login->combat_offer():std::nullopt;
  const auto sopFrame=login?login->combat_sop():mgo2win::combat::SopView{};
  const bool stageMatches=stageRenderer&&agreementVisible&&login&&login->stage_load_request()&&login->stage_load_request()==stageAssets.result().request;
  const bool sopActive=stageMatches&&stageCamera&&sopSnapshot&&sopOffer&&sopSnapshot->epoch==sopOffer->epoch&&login->combat_status()==mgo2win::combat::wire::Status::active;
  if(sopActive){
   if(sopPresentation.update(stageAssets.result().generation,*sopSnapshot,sopOffer->self,sopFrame,true,GetTickCount64())&&sound&&std::filesystem::is_regular_file(sopSoundPath)){
    sopAudio=std::make_unique<AudioThread>();sopAudio->start(sopSoundPath,L"2",0,"sop_native");
   }
  }else sopPresentation.clear();
  if(!sopActive||sopFrame.jammed||!sopFrame.visibleMask)sopAudio.reset();
  if(stageMatches){
   stageRenderer->render(context.Get(),.35f,true,stageCamera?&*stageCamera:nullptr,nullptr,nullptr,combatPointLights);++stageFrames;
   if(stageCamera&&avatarRenderer&&!player.firstPerson&&!motionMissing){auto origin=ragdoll.active()?ragdoll.origin():navigation.feet();avatarRenderer->render(context.Get(),ragdoll.active()?0:player.bodyYaw,false,&*stageCamera,stageRenderer.get(),&origin,combatPointLights);}
   if(stageCamera&&testBody&&bodyRenderer)bodyRenderer->render(context.Get(),0,false,&*stageCamera,stageRenderer.get(),&testBody->position,combatPointLights);
   if(stageCamera)for(const auto&remote:remoteAvatars)if(auto&model=remoteModels[remote.identity.slot];model&&model->renderer&&model->visible)model->renderer->render(context.Get(),remote.yaw,false,&*stageCamera,stageRenderer.get(),&remote.origin,combatPointLights);
   if(stageCamera&&sopRenderer&&sopActive){
     if(auto scan=sopPresentation.pulse(GetTickCount64()))sopRenderer->scan(context.Get(),*stageRenderer,*stageCamera,{scan->origin,scan->radius,scan->width,scan->opacity},{true,true,true,false});
     for(const auto& remote:remoteAvatars)if(auto& model=remoteModels[remote.identity.slot];model&&model->renderer&&model->visible&&sopPresentation.visible(remote.identity,remote.life,*sopSnapshot))
      sopRenderer->doll(context.Get(),model->body,*stageRenderer,*stageCamera,remote.origin,remote.yaw,{true,true,remote.alive,false});
   }
   context->IASetInputLayout(input.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->IASetVertexBuffers(0,1,&vptr,&stride,&offset);context->IASetIndexBuffer(nullptr,DXGI_FORMAT_R32_UINT,0);context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);context->PSSetSamplers(0,1,&sp);context->RSSetState(raster.Get());context->RSSetViewports(1,&viewport);context->OMSetRenderTargets(1,&rp,nullptr);context->OMSetDepthStencilState(nullptr,0);
  }
  if(modelVisible!=lastModelVisible){lastModelVisible=modelVisible;std::osyncstream(std::cout)<<"{\"character_model_visible\":"<<(modelVisible?"true":"false")<<"}"<<std::endl;}
  const float clear[4]={0,0,0,1};context->ClearRenderTargetView(rt.Get(),clear);
  for(UINT i=0;i<count;i++){const auto&q=quads[i];ID3D11ShaderResourceView*tex=q.atlas==-4?stageRenderer->view():q.atlas==-3?characterRenderer->view():q.atlas==-2?agreementView.Get():textures[q.atlas<0?textureCount:q.atlas].Get();context->PSSetShaderResources(0,1,&tex);context->OMSetBlendState(blend[q.blend].Get(),nullptr,0xffffffff);context->Draw(6,i*6);}
  if(saveCaptures&&frames==0)capture(device.Get(),context.Get(),back.Get(),argv[3]);
  if(saveCaptures&&animation&&captureIndex<6&&animation->ticks()>=captureTicks[captureIndex]){auto out=std::filesystem::path(argv[3]);out.replace_filename(out.stem().wstring()+L"_"+std::to_wstring(captureTicks[captureIndex])+L".bmp");capture(device.Get(),context.Get(),back.Get(),out);std::osyncstream(std::cout)<<"{\"capture_requested_tick\":"<<captureTicks[captureIndex]<<",\"capture_actual_tick\":"<<animation->ticks()<<"}"<<std::endl;++captureIndex;}
  if(saveCaptures&&loadingVisible&&loadingFrames>=3&&!loadingCaptured){auto out=std::filesystem::path(argv[3]);out.replace_filename(L"loading.bmp");capture(device.Get(),context.Get(),back.Get(),out);loadingCaptured=true;}
  if(saveCaptures&&agreementVisible){
   auto shot=[&](const wchar_t* name){auto p=std::filesystem::path(argv[3]);p.replace_filename(name);capture(device.Get(),context.Get(),back.Get(),p);};
   if(motionBack&&(agreementFrames==35||agreementFrames==95))shot(agreementFrames==35?L"agreement_motion_35.bmp":L"agreement_motion_95.bmp");
   if(!agreementCaptured){shot(L"agreement.bmp");agreementCaptured=true;}
   if(scripted&&agreementFrames==61&&!agreementScrolledCaptured){shot(L"agreement_scrolled.bmp");agreementScrolledCaptured=true;}
   if(agreement->accepted()&&!agreementChoiceCaptured){shot(L"agreement_yes.bmp");agreementChoiceCaptured=true;}
  }
  if(saveCaptures&&login&&(loginFrames==0||loginFrames==21||loginFrames==51||loginFrames==101))capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"login_"+std::to_wstring(loginVisits)+L"_"+std::to_wstring(loginFrames)+L".bmp"));
  if(saveCaptures&&scriptedSlots&&slotStep!=slotCaptured&&(slotStep==1||slotStep==2||slotStep==4||slotStep==6||slotStep==10||slotStep==11||slotStep==12)){capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"slots_"+std::to_wstring(slotStep)+L".bmp"));slotCaptured=slotStep;}
  if(saveCaptures&&scriptedCreation&&login&&(loginFrames==120||loginFrames==150||loginFrames==180||loginFrames==210||loginFrames==250||loginFrames==290||loginFrames==350||loginFrames==375||loginFrames==400))capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"creation_"+std::to_wstring(loginFrames)+L".bmp"));
  if(saveCaptures&&scriptedSelection&&login&&(loginFrames==120||loginFrames==175||loginFrames==215||loginFrames==260||loginFrames==310||loginFrames==360||loginFrames==415||loginFrames==465||loginFrames==515||loginFrames==610||loginFrames==645||loginFrames==685||loginFrames==740||loginFrames==800||loginFrames==840||loginFrames==890||loginFrames==935||loginFrames==975||loginFrames==1020||loginFrames==1080||loginFrames==1140||loginFrames==1200))capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"selection_"+std::to_wstring(loginFrames)+L".bmp"));
  if(saveCaptures&&scriptedAppearance&&login&&(loginFrames==90||loginFrames==120||loginFrames==180||loginFrames==270||loginFrames==360||loginFrames==440))capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"appearance_"+std::to_wstring(loginFrames)+L".bmp"));
  if(saveCaptures&&scriptedCharacters&&login&&(loginFrames==10||loginFrames==100||loginFrames==120||loginFrames==240||loginFrames==320||loginFrames==360))capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"characters_"+std::to_wstring(loginFrames)+L".bmp"));
  if(saveCaptures&&scriptedPorts&&login&&(loginFrames==21||loginFrames==36||loginFrames==51||loginFrames==81||loginFrames==101||loginFrames==131||loginFrames==151))capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"ports_"+std::to_wstring(loginFrames)+L".bmp"));
  if(saveCaptures&&scriptedStun&&login&&(loginFrames==21||loginFrames==220))capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"stun_"+std::to_wstring(loginFrames)+L".bmp"));
  if(saveCaptures&&scriptedControls&&login&&(loginFrames==11||loginFrames==21||loginFrames==31||loginFrames==51||loginFrames==66||loginFrames==76||loginFrames==86||loginFrames==116||loginFrames==131))capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"controls_"+std::to_wstring(loginFrames)+L".bmp"));
  if(saveCaptures&&scriptedGraphics&&login&&(loginFrames==11||loginFrames==35||loginFrames==51||loginFrames==85||loginFrames==105||loginFrames==125||loginFrames==1100))capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/(L"graphics_"+std::to_wstring(loginFrames)+L".bmp"));
  if(saveCaptures&&scriptedLogin&&!login&&loginVisits==1&&loginFrames==180){capture(device.Get(),context.Get(),back.Get(),std::filesystem::path(argv[3]).parent_path()/L"login_back.bmp");++loginFrames;}
  if(login)++loginFrames;
  if(agreementVisible)++agreementFrames;
  const auto presented=swap->Present(graphics->active.vsync?1:0,0);check(presented);Window::invitationGuard.painted(invitationFrameModal);if(presented==DXGI_STATUS_OCCLUDED)++occluded;++frames;if(loadingVisible&&!agreementVisible)++loadingFrames;if(animation&&animation->state()==4&&!loadingVisible)running=false;Sleep(1);
 }
 // Preview shutdown policy: allow the final GCX fade request to finish within
 // the existing duration bound. This does not advance an unimplemented actor.
 unsigned drained=0;
 if(animation&&animation->state()==4&&sound)while(fade.remaining()&&GetTickCount64()<deadline){Sleep(17);fade.advance(1);audio.control.gain=fade.gain();++drained;}
 if(drained)std::osyncstream(std::cout)<<"{\"preview_audio_drain_frames\":"<<drained<<",\"gain\":"<<fade.gain()<<"}"<<std::endl;
 for(auto& a:menuAudio)a->stop=true;for(auto& a:menuAudio){if(a->thread.joinable())a->thread.join();if(a->result.load())++menuFailures;}
 if(motionBack)std::osyncstream(std::cout)<<"{\"background_motion_ticks\":"<<motionBack->ticks()<<",\"background_loop_restarts\":"<<(motionBack->loop_restarts()+motionFront->loop_restarts())<<"}"<<std::endl;
 std::osyncstream(std::cout)<<"{\"stage_preview_frames\":"<<stageFrames<<",\"stage_gameplay_ready\":false}"<<std::endl;
 std::osyncstream(std::cout)<<"{\"character_model_frames\":"<<modelFrames<<",\"character_frames_without_model\":"<<emptyModelFrames<<",\"account_appearance_applied\":"<<(characterCatalog&&appearanceChanges?"true":"false")<<",\"appearance_changes\":"<<appearanceChanges<<",\"motion_playing\":"<<(characterCatalog&&modelFrames?"true":"false")<<"}"<<std::endl;
 if(login)login->report();
 std::osyncstream(std::cout)<<"{\"login_visits\":"<<loginVisits<<",\"login_frames\":"<<loginFrames<<",\"original_login_quads\":"<<loginBackground.size()<<"}"<<std::endl;
 if(agreement)std::osyncstream(std::cout)<<"{\"agreement_frames\":"<<agreementFrames<<",\"agreement_accepted\":"<<(agreement->accepted()?"true":"false")<<",\"menu_audio_failures\":"<<menuFailures<<"}"<<std::endl;
 lobbyAudio.stop=true;if(lobbyAudio.thread.joinable())lobbyAudio.thread.join();
 std::osyncstream(std::cout)<<"{\"lobby_music_started\":"<<(lobbyMusicStarted?"true":"false")<<",\"lobby_music_exit\":"<<lobbyAudio.result.load()<<",\"original_background_quads\":"<<agreementBackground.size()<<"}"<<std::endl;
 audio.stop=true;effect.stop=true;if(audio.thread.joinable())audio.thread.join();if(effect.thread.joinable())effect.thread.join();check(device->GetDeviceRemovedReason());
 std::osyncstream(std::cout)<<"{\"start_sound_started\":"<<(seStarted?"true":"false")<<",\"start_sound_exit\":"<<effect.result.load()<<",\"loading_frames\":"<<loadingFrames<<",\"loading_ready\":"<<(loadingReady?"true":"false")<<"}"<<std::endl;
 if(animation)std::osyncstream(std::cout)<<"{\"scope\":\""<<(gcx?"gcx_title_subset_no_full_boot":"title_actor_preview_no_gcx_boot")<<"\",\"ticks\":"<<animation->ticks()<<",\"state\":"<<animation->state()<<",\"start_accepted\":"<<animation->accepted()<<",\"start_rejected\":"<<animation->rejected()<<",\"completion_callbacks\":"<<animation->callbacks()<<",\"accepted_tick\":"<<animation->accepted_tick()<<",\"callback_tick\":"<<animation->callback_tick()<<",\"scripted_input\":"<<(scripted?"true":"false")<<"}"<<std::endl;
 std::osyncstream(std::cout)<<"{\"status\":\"partial_asset_preview\",\"quads\":"<<count<<",\"frames\":"<<frames<<",\"occluded_presents\":"<<occluded<<",\"warp\":"<<(warp?"true":"false")<<",\"audio_exit\":"<<audio.result.load()<<"}"<<std::endl;
 return sound&&((lobbyMusicStarted&&lobbyAudio.result.load()!=0)||menuFailures||audio.result.load()!=0||(seStarted&&effect.result.load()!=0))?1:0;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}

