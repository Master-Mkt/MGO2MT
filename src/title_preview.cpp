#include "chat_view.h"
#include <limits>
#include "original_first_person_lens.h"
#include "first_person_transition.h"
#include "source_coordinates.h"
#include "build_version.h"
#include "title_movie.h"
#include "combat_spawn_profile.h"
#include "water_effects_overlay.h"
#include "bullet_decals_overlay.h"
#include "material_effects_overlay.h"
#include "water_surface_effects.h"
#include "water_audio.h"
#include "water_renderer.h"
#include <set>
#include "local_playtest.h"
#include "combat_audio.h"
#include "footstep_presentation.h"
#include "combat_light_effects.h"
#include "remote_avatar.h"
#include "sop_visuals.h"
#include "sop_presentation.h"
#include "special_action_motion.h"
#include "enemy_tag_target.h"
#include "weapon_aim_presentation.h"
#include "weapon_hand_renderer.h"
#include "gameplay_presentation.h"
#include "mounted_input.h"
#include "mounted_renderer.h"
#include "mortar_shell_renderer.h"
#include "mounted_flight_presentation.h"
#include "player_lock.h"
#include "enemy_name_tag.h"
#include "menu_audio.h"
#include "selection_model.h"
#include "music_menu.h"
#include "invitation_presenter.h"
#include "invitation_input_guard.h"
#include "notification_bridge.h"
#include "alert_media.h"
#include "server_disconnect_screen.h"
// Partial title preview with optional native GCX subset; full game boot is pending.
#include <windows.h>
#include "multi_ui_layer.h"
#include "multi_ui_game_events.h"
#include "gameplay_fingerprint.h"
#include "multi_ui_state.h"
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
#include "shadow_renderer.h"
#include "render_effects.h"
#include "render_profiler.h"
#include "world_depth.h"
#include "stage_assets.h"
#include "stage_music.h"
#include "round_music.h"
#include "stage_debug.h"
#include "debug_frame_rate.h"
#include "stage_navigation.h"
#include "player_control.h"
#include "player_menu.h"
#include "menu_input_wait.h"
#include "menu_font.h"
#include "system_ui_icons.h"
#include "hold_inventory_bridge.h"
#include "native_loadout.h"
#include "weapon_visual_policy.h"
#include "gekko_salute_audio.h"
#include "preset_radio_audio.h"
#include "player_motion.h"
#include "motion_blend_presentation.h"
#include "motion_blend_dialog.h"
#include "evade_input.h"
#include "evade_motion.h"
#include "cover_input.h"
#include "cover_presentation.h"
#include "cover_hud.h"
#include "special_pc_input.h"
#include "special_pc_clock.h"
#include "gekko_motion.h"
#include "gekko_locomotion.h"
#include "gekko_climb.h"
#include "gekko_traversal_motion.h"
#include "gekko_greeting.h"
#include "ladder_input.h"
#include "round_items.h"
#include "item_menu_shortcut.h"
#include "combat_particle_effects.h"
#include "weapon_effect_audio.h"
#include "combat_tracer.h"
#include "tracer_renderer.h"
#include "item_box_presentation.h"
#include "item_pickup_feedback.h"
#include "combat_runtime_effects_overlay.h"
#include "combat_particle_renderer.h"
#include "first_person_sight.h"
#include "item_drop_physics.h"
#include "stage_weather_renderer.h"
#include "stage_environment.h"
#include "stage_precipitation.h"
#include "physics_debug_render.h"
#include "player_motion_mapping.h"
#include "player_ragdoll.h"
#include "foot_ik.h"
#include "combat_presentation.h"
#include "combat_death.h"
#include "body_hit_presentation.h"
#include "blast_pose.h"
#include "combat_kill_feed.h"
#include "combat_attack_input.h"
#include "combat_action_presentation.h"
#include "weapon_motion_events.h"
#include "weapon_reload_adapter_policy.h"
#include "installed_weapon_model.h"
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
int run_audio_probe(int,wchar_t**,const std::atomic_bool*,const mgo2mt::AudioControl*);
static void check(HRESULT hr) { if(FAILED(hr)) throw std::runtime_error("D3D HRESULT " + std::to_string(static_cast<unsigned long>(hr))); }
static std::vector<char> file(const std::filesystem::path& path) {
 std::ifstream f(path,std::ios::binary|std::ios::ate); if(!f) throw std::runtime_error("Cannot open asset");
 auto n=f.tellg();if(n<0||n>128*1024*1024)throw std::runtime_error("Asset size limit");
 std::vector<char>b(static_cast<size_t>(n));f.seekg(0);f.read(b.data(),n);if(!f)throw std::runtime_error("Asset read");return b;
}
static uint32_t u32(const std::vector<char>& b,size_t p){if(p+4>b.size())throw std::runtime_error("Truncated asset");uint32_t v;std::memcpy(&v,b.data()+p,4);return v;}
using mgo2mt::Vertex;
using mgo2mt::Quad;
static_assert(sizeof(Quad)==140);
struct Window {
 static inline bool localTest=false;
 static inline mgo2mt::multi_ui::Layer* customUi=nullptr;
 static inline bool customUiInteractive=false;
 std::wstring instanceLabel;
 void title(const wchar_t* value){SetWindowTextW(handle,mgo2mt::versioned_title(instanceLabel.empty()?std::wstring(value):instanceLabel+L" | "+value).c_str());}
 static inline uint32_t pressed=0;
 static inline bool stageAudition=false;
 static inline bool inspection=false;
 static inline bool gameplayMode=false;
 static inline bool testRagdoll=false,testBody=false;
 static inline mgo2mt::PlayerMenu* playerMenu=nullptr;
 static inline mgo2mt::MusicMenu* musicMenu=nullptr;
 static inline mgo2mt::invitation_ui::Presenter* invitation=nullptr;
 static inline bool invitationChar=false;
 static inline mgo2mt::invitation_ui::InputGuard invitationGuard;
 static inline std::vector<unsigned> uiCues;
 static void cue(unsigned c){if(uiCues.size()<32)uiCues.push_back(c);}
 static inline mgo2mt::stage::DebugControls debug;
 static inline std::optional<mgo2mt::host::LoadRequest> resetRequest;
 static inline unsigned agreementInput=0;
 static inline bool agreementActive=false;
 static inline uint64_t titleActivity=0;
 static inline bool titleMovieActive=false,titleMovieSkip=false;
 static inline LPARAM titleMousePosition=0;
 static inline mgo2mt::LoginScreen* login=nullptr;
 static inline mgo2mt::ControllerInput* input=nullptr;
 static inline mgo2mt::MenuInputWait menuWait;
 static bool menu_wait_enabled(){
  if(playerMenu&&playerMenu->hold_visible())return false;
  return !login||!login->gameplay_visible()||login->combat_status()!=mgo2mt::combat::wire::Status::active||(playerMenu&&playerMenu->visible())||
   (musicMenu&&musicMenu->visible())||(invitation&&invitation->overlay().visible())||debug.confirmReset;
 }
 static void sync_menu_wait(){
  const uint64_t context=(login?login->input_context():0)|((playerMenu?playerMenu->input_context():0)<<32)|
   (uint64_t(agreementActive)<<52)|(uint64_t(musicMenu&&musicMenu->visible())<<53)|
   (uint64_t(invitation&&invitation->overlay().visible())<<54)|(uint64_t(debug.confirmReset)<<55)|
   (uint64_t(titleMovieActive)<<56)|(uint64_t(menu_wait_enabled())<<57);
  menuWait.context(context,GetTickCount64());
 }
 static bool menu_action(mgo2mt::MenuInputWait::Kind kind,bool repeat=false){
  sync_menu_wait();return !menu_wait_enabled()||menuWait.accept(kind,GetTickCount64(),repeat);
 }
 static inline float viewX=0,viewY=0,viewW=1,viewH=1;
 HWND handle=nullptr;
 ~Window(){customUi=nullptr;customUiInteractive=false;login=nullptr;input=nullptr;playerMenu=nullptr;musicMenu=nullptr;invitation=nullptr;invitationGuard.reset();uiCues.clear();if(handle)DestroyWindow(handle);}
 static constexpr UINT StartMenuMessage=WM_APP+73;
 static LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
  // Observe synchronous and frame-driven transitions with one clock shared by
  // keyboard, mouse and pad. A rejected event is never replayed later.
  sync_menu_wait();struct SyncMenuAfterMessage{~SyncMenuAfterMessage(){sync_menu_wait();}} syncMenuAfterMessage;
  if(GetForegroundWindow()==hwnd){
   if(msg==WM_KEYDOWN||msg==WM_SYSKEYDOWN||msg==WM_LBUTTONDOWN||msg==WM_RBUTTONDOWN||msg==WM_MBUTTONDOWN||msg==WM_MOUSEWHEEL||msg==WM_SIZING||msg==WM_ENTERSIZEMOVE||msg==WM_EXITSIZEMOVE||(msg==WM_MOUSEMOVE&&lp!=titleMousePosition))titleActivity=GetTickCount64();
   if(msg==WM_MOUSEMOVE)titleMousePosition=lp;
  }
  if(titleMovieActive){
   if(GetForegroundWindow()==hwnd&&msg==WM_KEYDOWN&&!(lp&(1LL<<30))&&(wp==VK_RETURN||wp==VK_ESCAPE||(input&&!input->config.device&&wp==input->config.keyboard[12])))titleMovieSkip=true;
   if((msg>=WM_KEYFIRST&&msg<=WM_KEYLAST&&msg!=WM_SYSKEYDOWN&&msg!=WM_SYSKEYUP)||(msg>=WM_MOUSEFIRST&&msg<=WM_MOUSELAST))return 0;
  }
  if(localTest&&GetForegroundWindow()!=hwnd&&!(msg==WM_KEYDOWN&&(lp&(1LL<<25)))&&((msg>=WM_KEYFIRST&&msg<=WM_KEYLAST)||(msg>=WM_MOUSEFIRST&&msg<=WM_MOUSELAST)))return 0;
  if(msg==WM_KILLFOCUS){menuWait.wait(GetTickCount64());if(customUi)customUi->cancel_actions("focus lost");}
  const bool capturing=(playerMenu&&playerMenu->capturing())||(login&&login->capturing());
  if(!capturing&&menu_wait_enabled()){
   if(msg==WM_KEYDOWN){
    const bool textEntry=(playerMenu&&playerMenu->text_entry())||(!(playerMenu&&playerMenu->visible())&&login&&login->text_entry());
    auto key=unsigned(wp);
    if(!textEntry&&input&&!(lp&(1LL<<25))&&key!=VK_ESCAPE&&key!=VK_RETURN&&key!=VK_TAB&&!(key>=VK_LEFT&&key<=VK_DOWN)){
     if(auto mapped=input->keyboard_menu(key))key=mapped;
    }
    if(!menu_action(mgo2mt::MenuInputWait::key(key,textEntry),bool(lp&(1LL<<30))))return 0;
   }
   if(msg==WM_LBUTTONUP&&!menu_action(mgo2mt::MenuInputWait::Kind::decision)){
    // Always release the modal's gesture ownership, even for a discarded click.
    invitationGuard.left_button(false,invitation&&invitation->overlay().visible());if(customUi)customUi->pointer(-1,-1,false);return 0;
   }
   if(msg==WM_MOUSEWHEEL&&!menu_action(mgo2mt::MenuInputWait::Kind::move))return 0;
  }
  if(msg==WM_MOUSEMOVE||msg==WM_LBUTTONDOWN||msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(r.right&&r.bottom){float x=float(short(LOWORD(lp)))/r.right,y=float(short(HIWORD(lp)))/r.bottom;if(x<viewX||y<viewY||x>=viewX+viewW||y>=viewY+viewH){if(customUi)customUi->pointer(-1,-1,msg==WM_LBUTTONDOWN||(msg==WM_MOUSEMOVE&&(wp&MK_LBUTTON)));return 0;}lp=MAKELPARAM(int((x-viewX)/viewW*r.right),int((y-viewY)/viewH*r.bottom));}}
  const bool wasInvitationModal=invitation&&invitation->overlay().visible();
  if(invitation)invitation->update(GetTickCount64());
  const bool invitationModal=invitation&&invitation->overlay().visible();
  if(msg==StartMenuMessage){
   if(!login||!login->stage_load_request()||(playerMenu&&playerMenu->capturing())||debug.confirmReset)return 0;
   if(invitationModal){invitation->overlay().close();cue(mgo2mt::menu_audio::Cancel);return 0;}
   if(login->personal_overlay_visible()){login->message(hwnd,WM_KEYDOWN,VK_ESCAPE,1LL<<25);return 0;}
   const bool child=(playerMenu&&playerMenu->visible())||(musicMenu&&musicMenu->visible());
   if(musicMenu&&musicMenu->visible())musicMenu->close();
   if(playerMenu&&playerMenu->visible())playerMenu->close(true);
   if(!child||login->gameplay_visible())login->message(hwnd,WM_KEYDOWN,VK_F9,1LL<<25);
   return 0;
  }
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
   playerMenu->close();invitationChar=true;cue(mgo2mt::menu_audio::Confirm);return 0;
  }
  if(invitation&&invitation->overlay().visible()){
   auto& overlay=invitation->overlay();
   auto confirm=[&]{if(auto response=overlay.confirm(GetTickCount64())){const auto queued=invitation->respond(*response,GetTickCount64());cue(queued?mgo2mt::menu_audio::Confirm:mgo2mt::menu_audio::Cancel);}};
   if(msg==WM_KEYDOWN){invitationChar=true;if(lp&(1LL<<30))return 0;
    if(input&&!(lp&(1LL<<25))&&wp!=VK_ESCAPE&&wp!=VK_RETURN&&!(wp>=VK_LEFT&&wp<=VK_DOWN)){if(wp==input->config.keyboard[12])wp=VK_ESCAPE;else if(auto key=input->keyboard_menu(unsigned(wp)))wp=key;}
    if(wp==VK_ESCAPE){overlay.close();cue(mgo2mt::menu_audio::Cancel);}
    else if(wp==VK_RETURN)confirm();
    else if(wp==VK_LEFT||wp==VK_UP||wp==VK_RIGHT||wp==VK_DOWN){overlay.move(wp==VK_LEFT||wp==VK_UP?-1:1);cue(mgo2mt::menu_audio::Cursor);}return 0;
   }
   if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(r.right&&r.bottom)if(auto accept=overlay.hit_test(int(short(LOWORD(lp)))*1280/r.right,int(short(HIWORD(lp)))*720/r.bottom)){overlay.choose(*accept);confirm();}return 0;}
   if(msg==WM_KEYUP||msg==WM_CHAR||msg==WM_LBUTTONDOWN||msg==WM_MOUSEWHEEL||msg==WM_IME_STARTCOMPOSITION||msg==WM_IME_COMPOSITION||msg==WM_IME_ENDCOMPOSITION)return 0;
  }
  if(login&&login->stage_load_request()&&input&&!input->config.device&&msg==WM_KEYDOWN&&!(lp&(1LL<<30))&&wp==input->config.keyboard[12]&&(!playerMenu||(!playerMenu->capturing()&&!playerMenu->text_entry()))){return proc(hwnd,StartMenuMessage,0,0);}
  if(playerMenu&&playerMenu->visible()&&!playerMenu->capturing()&&!playerMenu->text_entry()&&input&&!input->config.device&&msg==WM_KEYDOWN&&!(lp&(1LL<<30))&&wp==input->config.keyboard[12]){playerMenu->close(true);return 0;}
  // Hold-panel A is sampled as drop. Do not map it to the menu Escape action.
  if(playerMenu&&playerMenu->hold_consumes_action(5)&&input&&!input->config.device&&!(lp&(1LL<<25))&&(msg==WM_KEYDOWN||msg==WM_KEYUP)&&wp==input->config.keyboard[5])return 0;
  if((!login||(musicMenu&&musicMenu->visible())||(playerMenu&&playerMenu->visible()&&!playerMenu->capturing()&&!playerMenu->text_entry()))&&input&&msg==WM_KEYDOWN&&!(lp&(1LL<<25))&&wp!=VK_ESCAPE&&wp!=VK_RETURN&&!(wp>=VK_LEFT&&wp<=VK_DOWN)){auto mapped=input->keyboard_menu(unsigned(wp));if(mapped)wp=mapped;}
  if(msg==WM_KILLFOCUS){debug.focus_lost();testRagdoll=testBody=false;}
  if(msg==WM_KEYDOWN&&wp==VK_F5&&!(lp&(1LL<<30))&&login&&playerMenu&&!debug.confirmReset&&!login->personal_overlay_visible()&&(!musicMenu||!musicMenu->visible())&&!playerMenu->visible())if(auto session=login->chat_session();session&&session->state().joined){playerMenu->chat_session(session);playerMenu->open(mgo2mt::player::Menu::chat);return 0;}
  if(musicMenu&&musicMenu->visible()&&musicMenu->message(hwnd,msg,wp,lp))return 0;
  if(playerMenu&&playerMenu->visible()&&playerMenu->message(hwnd,msg,wp,lp))return 0;
  if(playerMenu&&playerMenu->text_entry()&&(msg==WM_KEYDOWN||msg==WM_KEYUP||msg==WM_CHAR||msg==WM_IME_STARTCOMPOSITION||msg==WM_IME_COMPOSITION||msg==WM_IME_ENDCOMPOSITION))return DefWindowProcW(hwnd,msg,wp,lp);
  auto currentStage=login?login->stage_request():std::nullopt;
  if((debug.confirmReset||debug.reset)&&resetRequest!=currentStage)debug.cancel_reset();
  if(msg==WM_KEYDOWN){bool wasOpen=debug.confirmReset,wasYes=debug.resetYes,wasEnabled=debug.enabled;int oldStep=debug.musicStep;bool oldToggle=debug.toggleMusic;
   if(debug.key(unsigned(wp),bool(lp&(1LL<<30)),bool(currentStage))){
    if(!wasOpen&&debug.confirmReset){resetRequest=currentStage;cue(mgo2mt::menu_audio::Confirm);}
    else if(wasOpen&&!debug.confirmReset)cue(debug.reset?mgo2mt::menu_audio::Confirm:mgo2mt::menu_audio::Cancel);
    else if(wasYes!=debug.resetYes||oldStep!=debug.musicStep)cue(mgo2mt::menu_audio::Cursor);
    else if(wasEnabled!=debug.enabled)cue(debug.enabled?mgo2mt::menu_audio::Confirm:mgo2mt::menu_audio::Cancel);
    else if(oldToggle!=debug.toggleMusic)cue(mgo2mt::menu_audio::Confirm);
    return 0;
   }}
  if(debug.confirmReset){
   if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(r.right&&r.bottom){debug.click(int(short(LOWORD(lp)))*1280/r.right,int(short(HIWORD(lp)))*720/r.bottom);if(!debug.confirmReset)cue(debug.reset?mgo2mt::menu_audio::Confirm:mgo2mt::menu_audio::Cancel);}return 0;}
   if(msg==WM_LBUTTONDOWN||msg==WM_MOUSEWHEEL||msg==WM_CHAR||msg==WM_KEYUP)return 0;
  }
  if(debug.enabled&&currentStage&&msg==WM_KEYDOWN&&wp==VK_F10){if(!(lp&(1LL<<30)))inspection=!inspection;return 0;}
  if(debug.enabled&&inspection&&currentStage&&msg==WM_KEYDOWN&&wp==VK_F11){if(!(lp&(1LL<<30))){if(GetKeyState(VK_SHIFT)&0x8000)testBody=true;else testRagdoll=true;}return 0;}
  if(debug.enabled&&inspection&&currentStage&&input&&!input->config.device){
   if(msg==WM_KEYDOWN||msg==WM_KEYUP)for(unsigned a=4;a<24;++a)if(wp==input->config.keyboard[a])return 0;
   if(msg==WM_CHAR)return 0;
  }
  if(login&&msg==WM_KEYDOWN&&wp==VK_F6&&!(lp&(1LL<<30))&&login->stage_request()){stageAudition=true;return 0;}
  if((msg==WM_MOUSEMOVE||msg==WM_LBUTTONDOWN||msg==WM_LBUTTONUP)&&customUi&&customUiInteractive&&!capturing&&GetForegroundWindow()==hwnd){RECT bounds{};GetClientRect(hwnd,&bounds);if(bounds.right>0&&bounds.bottom>0&&customUi->pointer(float(short(LOWORD(lp)))*1280/bounds.right,float(short(HIWORD(lp)))*720/bounds.bottom,msg==WM_LBUTTONDOWN||(msg==WM_MOUSEMOVE&&(wp&MK_LBUTTON))))return 0;}
  if(msg==WM_KEYDOWN&&customUi&&customUiInteractive&&!capturing&&!(login&&login->text_entry())&&!(playerMenu&&playerMenu->text_entry())&&GetForegroundWindow()==hwnd){const bool repeat=lp&(1LL<<30);if((!repeat||(wp>=VK_LEFT&&wp<=VK_DOWN))&&customUi->key(unsigned(wp)))return 0;}
  if(login&&login->message(hwnd,msg,wp,lp))return 0;
  if(!login&&agreementActive&&msg==WM_KEYDOWN&&wp==VK_ESCAPE){if(!(lp&(1LL<<30)))agreementInput|=mgo2mt::AgreementScreen::cancel;return 0;}
  if(msg==WM_CLOSE || (msg==WM_KEYDOWN&&wp==VK_ESCAPE)){PostQuitMessage(0);return 0;}
  if(msg==WM_MOUSEWHEEL){agreementInput |= static_cast<short>(HIWORD(wp))>0?8:16;return 0;}
  if(msg==WM_KEYDOWN&&!(lp&(1LL<<30))){switch(wp){case VK_LEFT:agreementInput|=1;break;case VK_RIGHT:agreementInput|=2;break;case VK_RETURN:agreementInput|=4;break;case VK_UP:agreementInput|=8;break;case VK_DOWN:agreementInput|=16;break;case VK_PRIOR:agreementInput|=32;break;case VK_NEXT:agreementInput|=64;break;case VK_HOME:agreementInput|=128;break;case VK_END:agreementInput|=256;break;case 'R':agreementInput|=512;break;}}
  if(msg==WM_KEYDOWN&&wp==VK_RETURN&&!(lp&(1LL<<30))){pressed|=8;return 0;}
  return DefWindowProcW(hwnd,msg,wp,lp);
 }
 void create(){WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"MGO2MT_AssetPreview";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
  if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("Register window");
  RECT rect{0,0,1280,720};AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
  handle=CreateWindowW(wc.lpszClassName,(mgo2mt::versioned_title(L"MGO2MT - Partial title asset preview (not game boot)")+instanceLabel).c_str(),WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,wc.hInstance,nullptr);
  if(!handle)throw std::runtime_error("Create window");ShowWindow(handle,SW_SHOW);
 }
};
struct AudioThread {
 std::atomic_bool stop{false}; std::atomic_int result{-1}; std::thread thread;
 mgo2mt::AudioControl control;
 void finish(){stop=true;if(thread.joinable())thread.join();}
 ~AudioThread(){finish();}
 void start(const std::filesystem::path& wav,const std::wstring& seconds,unsigned cue=0,const char* stream=nullptr){finish();stop=false;result=-1;control.cue=cue;control.stream=stream?stream:(wav.stem()==L"lobby"||wav.stem()==L"bgm_mgo_lobby01")?"lobby_bgm":"other";thread=std::thread([this,wav,seconds]{
  std::vector<std::wstring> args{L"audio",wav.wstring(),seconds};std::vector<wchar_t*> pointers;for(auto& a:args)pointers.push_back(a.data());result=run_audio_probe(static_cast<int>(args.size()),pointers.data(),&stop,&control);
 });}
};
// An orange inspection capsule; no original object or network identity is implied.
static mgo2mt::CharacterModel physics_capsule(const mgo2mt::physics::RigidBody& body){
 using namespace mgo2mt;using namespace mgo2mt::physics;CharacterModel model;
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
 if(argc<4)throw std::runtime_error("Usage: mgo2mt_title_preview scene seconds capture.bmp [--audio] [--scripted-input] [--gcx file --entry procedure --wav file]");
 double seconds=std::stod(argv[2]);if(!(seconds>0&&seconds<=600))throw std::runtime_error("Duration limit");
 mgo2mt::LocalPlaytest playtest;
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
  else if(auto test=mgo2mt::LocalPlaytest::parse(arg);test.enabled){if(playtest.enabled)throw std::runtime_error("Duplicate local playtest role");playtest=test;}
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
 if(!networkKeys.empty()){
  const auto root=networkKeys.parent_path();std::string error;
  const bool font=mgo2mt::menu_font_resources().load(root/L"fonts");
  const bool ui=mgo2mt::system_ui_icons().load(root/L"system-ui/index.tsv",error);
  std::osyncstream(std::cout)<<"{\"original_menu_font_loaded\":"<<(font?"true":"false")<<",\"original_system_ui_loaded\":"<<(ui?"true":"false")<<"}\n";
 }
 auto path=std::filesystem::absolute(argv[1]);auto bytes=file(path);
 std::unique_ptr<mgo2mt::TitleGcx> gcx;
 if(!gcxPath.empty()){gcx=std::make_unique<mgo2mt::TitleGcx>(file(gcxPath),std::cout);gcx->start(entry);}
 std::unique_ptr<mgo2mt::TitleAnimation> animation;uint32_t textureCount=0,count=0;std::vector<Quad>quads;
 if(bytes.size()>=4&&!std::memcmp(bytes.data(),"M2AN",4)){animation=std::make_unique<mgo2mt::TitleAnimation>(bytes,gcx?gcx->timeout():18000);textureCount=animation->texture_count();quads=animation->geometry();count=static_cast<uint32_t>(quads.size());}
 else{if(scripted)throw std::runtime_error("Scripted input requires animation");if(bytes.size()<16||std::memcmp(bytes.data(),"M2PV",4)||u32(bytes,4)!=1||!u32(bytes,12)||u32(bytes,12)>64)throw std::runtime_error("Preview header");textureCount=u32(bytes,12);count=u32(bytes,8);if(count>10000||bytes.size()!=16+size_t(count)*sizeof(Quad))throw std::runtime_error("Preview extent");quads.resize(count);std::memcpy(quads.data(),bytes.data()+16,count*sizeof(Quad));}
 std::unique_ptr<mgo2mt::AgreementScreen> agreement;
 if(!policyUrl.empty()){if(loadingPath.empty()||menuCancelPath.empty()||menuConfirmPath.empty()||menuMovePath.empty())throw std::runtime_error("Agreement needs loading and menu sounds");agreement=std::make_unique<mgo2mt::AgreementScreen>(policyUrl);}
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
 std::unique_ptr<mgo2mt::TitleAnimation> motionBack,motionFront;
 if(!motionPath.empty()){auto b=file(motionPath);motionBack=std::make_unique<mgo2mt::TitleAnimation>(b);motionFront=std::make_unique<mgo2mt::TitleAnimation>(b,18000,true);if(motionBack->texture_count()!=8)throw std::runtime_error("Background motion textures");}
 std::vector<Vertex>vertices;
 if(gcx&&!animation)throw std::runtime_error("GCX requires animated title asset");
 std::unique_ptr<mgo2mt::TitleAnimation> loading;
 if(!loadingPath.empty()){
  if(!gcx)throw std::runtime_error("Loading requires GCX");
  loading=std::make_unique<mgo2mt::TitleAnimation>(file(loadingPath));
  if(loading->texture_count()!=2)throw std::runtime_error("Loading textures");
 }
 if(animation)animation->set_callbacks([&](uint32_t result){std::osyncstream(std::cout)<<"{\"host_callback\":\"selected\",\"result\":"<<result<<",\"tick\":"<<animation->ticks()<<"}"<<std::endl;if(gcx)gcx->callback(false,result);},[&](uint32_t result){std::osyncstream(std::cout)<<"{\"host_callback\":\"completed\",\"result\":"<<result<<",\"tick\":"<<animation->ticks()<<"}"<<std::endl;if(gcx)gcx->callback(true,result);});
 const auto initialTitle=animation?std::optional(*animation):std::nullopt;
 const auto initialLoading=loading?std::optional(*loading):std::nullopt;
 for(const auto&q:quads){if(q.atlas< -1||q.atlas>=static_cast<int>(textureCount)||q.blend<0||q.blend>1)throw std::runtime_error("Preview draw range");for(const auto&v:q.vertices){float values[8];std::memcpy(values,&v,sizeof(v));for(float value:values)if(!std::isfinite(value))throw std::runtime_error("Nonfinite vertex");}for(int i:{0,1,2,0,2,3})vertices.push_back(q.vertices[i]);}
 if(vertices.empty())throw std::runtime_error("Empty scene");
 std::filesystem::path inputPath;if(scripted)inputPath=std::filesystem::path(argv[3]).parent_path()/L"input.cfg";else{wchar_t local[32768];DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);if(!n||n>=32768)throw std::runtime_error("Input settings directory unavailable");inputPath=playtest.profile(std::filesystem::path(local)/L"MGO2MT")/L"input.cfg";}
 mgo2mt::LocalPlaytestSession playtestSession;playtestSession.prepare(playtest,inputPath.parent_path());
 auto controllerInput=std::make_shared<mgo2mt::ControllerInput>(inputPath);
 Window window;window.instanceLabel=playtest.label();Window::localTest=playtest.enabled;window.create();Window::input=controllerInput.get();if(animation)window.title(L"MGO2MT - Title actor preview | Enter: START | Esc: close | No GCX boot");ComPtr<ID3D11Device>device;ComPtr<ID3D11DeviceContext>context;ComPtr<IDXGISwapChain>swap;
 if(gcx)window.title(L"MGO2MT - Title | Enter: START | Esc: close");
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
 std::unique_ptr<mgo2mt::CharacterCatalog> characterCatalog; mgo2mt::PreparedCharacter prepared;
 std::optional<mgo2mt::CharacterModel> selectionComposite,selectionBox;bool previewBox=false;mgo2mt::motion_blend::Lane selectionBlend;auto previousSelectionKind=mgo2mt::SelectionPresentation::Kind::idle;float selectionLift=0,selectionLiftFrom=0;float selectionLiftProgress=1;
 const auto selectionBoxPath=networkKeys.parent_path()/L"stage"/L"objects"/L"cbox_a_sk.gwm";
 if(std::filesystem::is_regular_file(selectionBoxPath))try{selectionBox.emplace(file(selectionBoxPath));}catch(...){std::osyncstream(std::cout)<<"{\"selection_box_load_failed\":true}\n";}
 std::optional<std::array<uint8_t,28>> preparedAppearance;uint32_t preparedId=0;ULONGLONG modelBegan=0;unsigned appearanceChanges=0;
 if(!catalogPath.empty()){characterCatalog=std::make_unique<mgo2mt::CharacterCatalog>(file(catalogPath));std::osyncstream(std::cout)<<"{\"character_catalog_loaded\":true,\"meshes\":"<<characterCatalog->mesh_count()<<",\"motion_clip\":"<<characterCatalog->clip()<<"}"<<std::endl;}
 mgo2mt::stage::Assets stageAssets(networkKeys.empty()?std::filesystem::path{}:networkKeys.parent_path()/"stage");
 std::unique_ptr<mgo2mt::CharacterRenderer> stageSkyRenderer;std::shared_ptr<const mgo2mt::CharacterModel> stageSkyModel;uint64_t skyStartedAt=0,skyGeneration=0;
 mgo2mt::shadows::Renderer shadowRenderer;
 std::unique_ptr<mgo2mt::effects::Renderer> renderEffects;
 auto reflectionMaterials=mgo2mt::render_reflections::defaults();
 try{reflectionMaterials=mgo2mt::render_reflections::load(path.parent_path().parent_path()/L"render_materials.json");}catch(const std::exception&e){reflectionMaterials={};std::osyncstream(std::cout)<<"render_materials: invalid profile, native reflection disabled: "<<e.what()<<'\n';}
 mgo2mt::render_profiler::Profiler gpuProfiler;bool profilerInitialized=false;
 std::optional<std::chrono::steady_clock::time_point> cpuPreviousPresent;
 using GpuStage=mgo2mt::render_profiler::Stage;
 ID3D11ShaderResourceView* presentedStageView=nullptr;
 std::unique_ptr<mgo2mt::CharacterRenderer> stageRenderer;std::shared_ptr<const mgo2mt::CharacterModel> stageModel;unsigned stageFrames=0;
 std::optional<mgo2mt::gameplay::Config> gameplayConfig;
 mgo2mt::mounted::Registry mountedRegistry;
 {std::string error;const auto root=networkKeys.parent_path();const auto startupHash=mgo2mt::gameplay::fingerprint(root);if(!startupHash)throw std::runtime_error("Gameplay configuration fingerprint failed");
  if(std::filesystem::exists(root/"gameplay.json")){mgo2mt::gameplay::Config config;if(!config.load(root/"gameplay.json",error))throw std::runtime_error(error);gameplayConfig=std::move(config);}
  if(std::filesystem::exists(root/"mounted_weapons.json")&&!mountedRegistry.load(root/"mounted_weapons.json",error))throw std::runtime_error(error);
  if(!mgo2mt::freeze_client_gameplay_configuration(root,*startupHash))throw std::runtime_error("Gameplay configuration changed while loading; restart the client");
 }
 const auto* gameplay=gameplayConfig?&*gameplayConfig:nullptr;
 std::unique_ptr<mgo2mt::mounted::Renderer> mountedRenderer;std::unique_ptr<mgo2mt::mortar_shells::Renderer> mortarShellRenderer;
 for(const auto&t:mountedRegistry.types)if(t.kind==mgo2mt::mounted::Kind::mortar&&!t.projectileModel.empty()&&gameplay&&gameplay->find(t.weapon)){mortarShellRenderer=std::make_unique<mgo2mt::mortar_shells::Renderer>(device.Get(),networkKeys.parent_path()/t.projectileModel,gameplay->find(t.weapon)->weapon.range);break;}
 if(!mountedRegistry.types.empty())mountedRenderer=std::make_unique<mgo2mt::mounted::Renderer>(device.Get(),networkKeys.parent_path(),mountedRegistry);
 mgo2mt::items::PickupFeedback pickupFeedback;
 mgo2mt::item_box::Presentation itemBoxes;std::unique_ptr<mgo2mt::item_box::Renderer> itemBoxRenderer;
 mgo2mt::items::DropPhysics debugItemShapes;bool debugItemShapesReady=false;
 {mgo2mt::weapons::Catalog catalog;std::string error;if(catalog.load(networkKeys.parent_path()/L"weapon_catalog.tsv",error)){debugItemShapes.catalog(catalog);debugItemShapesReady=true;}}
 {std::string error;if(itemBoxes.load_catalog(networkKeys.parent_path()/L"weapon_catalog.tsv",error))try{itemBoxRenderer=std::make_unique<mgo2mt::item_box::Renderer>(device.Get(),networkKeys.parent_path()/L"stage"/L"items");}catch(const std::exception& e){std::osyncstream(std::cout)<<"item_box_renderer_failed: "<<e.what()<<'\n';}}
 mgo2mt::stage::Navigation navigation;std::shared_ptr<const mgo2mt::stage::Collision> navigationWorld,navigationBase;
 std::optional<mgo2mt::host::LoadRequest> navigationRequest;uint64_t navigationGeneration=0;
 std::shared_ptr<const mgo2mt::stage::Collision> movementSource,movementWorld,queryWorld; mgo2mt::stage::WaterEffects waterEffects;
 std::shared_ptr<const mgo2mt::stage::Water> navigationWater;
 std::shared_ptr<const mgo2mt::stage::WaterSurface> contactWater;
 mgo2mt::stage::WaterSurfaceEffects waterContacts;
 std::map<uint64_t,mgo2mt::stage::WaterEffects> remoteWaterEffects;
 std::unique_ptr<mgo2mt::water_visuals::Renderer> waterRenderer;
 mgo2mt::water_audio::Steps waterSteps;
 struct WaterVoice {uint64_t identity,life,epoch,scene;std::unique_ptr<AudioThread> audio;};std::vector<WaterVoice> waterVoices;
 mgo2mt::combat::decals::Pool bulletMarks({1024,120000,10000,32.f,1.f});
 mgo2mt::combat::material_effects::Pool materialParticles;
 mgo2mt::FirstPersonTransition firstPersonTransition;mgo2mt::first_person_sight::Controller firstPersonSight;std::array<uint64_t,4> firstPersonScope{};std::optional<mgo2mt::WorldView> stageCamera;auto navigationTick=GetTickCount64();bool navigationArmed=false;
 mgo2mt::player::Control player;bool movementRunning=false;double motionSeconds=0;bool yFirstPerson=false;
 auto lastMotion=mgo2mt::player::Motion::idle;bool motionMissing=false;
 mgo2mt::player::EvadeInput evadeInput;float evadeMovementYaw=0;uint32_t localEvadeCounter=0,predictedEvadeSerial=0;bool evadeAcknowledged=false;
 mgo2mt::player::CoverInput coverInput;mgo2mt::combat::cover::State coverState;bool coverAvailable=false,coverLeft=false,coverRight=false;
 mgo2mt::cover::Timeline coverTimeline;std::array<mgo2mt::cover::Timeline,24> remoteCoverTimelines;
 mgo2mt::cover_hud::Renderer coverHud;mgo2mt::cover::FreeLean freeLean;std::array<mgo2mt::cover::FreeLean,24> remoteFreeLeans;
 std::array<uint64_t,24> coverActorKeys{};std::array<uint32_t,24> coverActorLives{};uint64_t coverRemoteEpoch=0;
 std::unique_ptr<mgo2mt::cover::CoverMotionBank> coverMotions;
 const auto coverMotionPath=catalogPath.parent_path()/L"cover.gwmot";
 if(std::filesystem::is_regular_file(coverMotionPath))try{coverMotions=std::make_unique<mgo2mt::cover::CoverMotionBank>(file(coverMotionPath));}catch(...){std::osyncstream(std::cout)<<"{\"cover_motion_load_failed\":true}\n";}
 mgo2mt::special_pc::Input specialPcInput;mgo2mt::gekko_locomotion::State gekkoLocomotion;bool gekkoActive=false,gekkoWasTraversing=false;auto avatarKind=mgo2mt::special_pc::Kind::human;
 std::unique_ptr<mgo2mt::CharacterCatalog> gekkoCatalog;std::unique_ptr<mgo2mt::special_pc::GekkoMotionBank> gekkoMotions;mgo2mt::special_pc::Clock gekkoClock;
 std::unique_ptr<mgo2mt::special_pc::GekkoGreeting> gekkoGreeting;
 std::unique_ptr<mgo2mt::special_pc::GekkoTraversalMotionBank> gekkoTraversal;
 try{auto p=networkKeys.parent_path()/"special/gekko_traversal.gwmot";if(std::filesystem::is_regular_file(p))gekkoTraversal=std::make_unique<mgo2mt::special_pc::GekkoTraversalMotionBank>(file(p));}catch(...){std::osyncstream(std::cout)<<"{\"gekko_traversal_load_failed\":true}\n";}
 try{auto p=networkKeys.parent_path()/"special/gekko_salute.gwmot";if(std::filesystem::is_regular_file(p))gekkoGreeting=std::make_unique<mgo2mt::special_pc::GekkoGreeting>(file(p));}catch(...){std::osyncstream(std::cout)<<"{\"gekko_greeting_load_failed\":true}\n";}
 mgo2mt::ItemMenuShortcut itemMenuShortcut;
 mgo2mt::mounted::Input mountedInput;uint16_t mountedCurrent=0,mountedNearby=0,flightCurrent=0;
 mgo2mt::mounted::FlightPosition flightPosition;std::array<mgo2mt::mounted::FlightPresentation,24> flightClocks;std::array<std::optional<mgo2mt::mounted::FlightFrame>,24> flightFrames;uint8_t mountedMap=0;
 mgo2mt::ladder::Input ladderInput;std::vector<mgo2mt::ladder::Anchor> ladderAnchors;uint64_t ladderScene=0;float ladderAxis=0;uint16_t ladderCurrent=0;bool ladderNearby=false;
 const auto gekkoMotionPath=networkKeys.parent_path()/L"special"/L"gekko.gwmot";
 if(std::filesystem::is_regular_file(gekkoMotionPath))try{gekkoMotions=std::make_unique<mgo2mt::special_pc::GekkoMotionBank>(file(gekkoMotionPath));}catch(...){std::osyncstream(std::cout)<<"{\"gekko_motion_load_failed\":true}\n";}
 auto gekkoMotion=[](mgo2mt::special_pc::Action action,bool moving,bool running){using G=mgo2mt::special_pc::GekkoMotion;using A=mgo2mt::special_pc::Action;return action==A::jump?G::jump:action==A::kick?G::kick:!moving?G::idle:running?G::run:G::walk;};
 auto gekkoSample=[&](mgo2mt::special_pc::Action action,mgo2mt::special_pc::GekkoMotion motion,double seconds)->std::optional<mgo2mt::special_pc::GekkoSample>{
  using A=mgo2mt::special_pc::Action;
  if(action==A::salute)return gekkoGreeting?gekkoGreeting->sample(seconds):std::nullopt;
  if(action==A::climb)return gekkoTraversal&&gekkoMotions?gekkoTraversal->sample_climb(*gekkoMotions,seconds):std::nullopt;
  return gekkoMotions?gekkoMotions->sample_gameplay(motion,seconds):std::nullopt;
 };
 const auto gekkoCatalogPath=networkKeys.parent_path()/L"special"/L"gekko.gwc";
 if(std::filesystem::is_regular_file(gekkoCatalogPath))try{gekkoCatalog=std::make_unique<mgo2mt::CharacterCatalog>(file(gekkoCatalogPath));}catch(...){std::osyncstream(std::cout)<<"{\"gekko_model_load_failed\":true}\n";}
 auto shotEye=[&]{if(mountedCurrent)if(auto*i=mountedRegistry.find(mountedMap,mountedCurrent))if(auto*t=mountedRegistry.find(i->type))return mgo2mt::mounted::muzzle_position(*i,*t,navigation.yaw(),navigation.pitch());return mgo2mt::combat::cover::eye(navigation.feet(),navigation.capsule(),coverState,navigation.yaw());};
 auto evadeKind=[](mgo2mt::player::Evade kind){using E=mgo2mt::player::Evade;return kind==E::roll?mgo2mt::combat::EvadeKind::roll:kind==E::backstep?mgo2mt::combat::EvadeKind::backstep:kind==E::rollLeft?mgo2mt::combat::EvadeKind::rollLeft:kind==E::rollRight?mgo2mt::combat::EvadeKind::rollRight:mgo2mt::combat::EvadeKind::none;};
 std::unique_ptr<mgo2mt::player::EvadeMotionBank> evadeMotions;
 const auto evadeMotionPath=catalogPath.parent_path()/L"evade.gwmot";
 if(std::filesystem::is_regular_file(evadeMotionPath))try{evadeMotions=std::make_unique<mgo2mt::player::EvadeMotionBank>(file(evadeMotionPath));}catch(...){std::osyncstream(std::cout)<<"{\"evade_motion_load_failed\":true}\n";}
 std::unique_ptr<mgo2mt::weapon_hand::Bank> handMotions;
 std::unique_ptr<mgo2mt::weapon_hand::Models> heldModels;
 mgo2mt::weapon_hand::Actor avatarWeapon;
 mgo2mt::combat::presentation::Actions weaponActions;
 const auto weaponSoundEvents=[] {std::map<std::pair<uint32_t,uint32_t>,std::vector<std::pair<unsigned,unsigned>>> result;for(const auto&e:mgo2mt::weapon_hand::soundEvents)result[{e.weapon,e.index}].emplace_back(e.tick,e.cue);return result;}();
 std::map<std::array<float,5>,std::unique_ptr<mgo2mt::CharacterRenderer>> installedWeaponModels;
 try{const auto root=networkKeys.parent_path();handMotions=std::make_unique<mgo2mt::weapon_hand::Bank>(file(root/(gameplay?gameplay->resources().handsPath:"weapons/hands.gwh")));heldModels=gameplay?std::make_unique<mgo2mt::weapon_hand::Models>(root,*gameplay):std::make_unique<mgo2mt::weapon_hand::Models>(root/"weapons");}catch(const std::exception&e){std::osyncstream(std::cout)<<"weapon_hand_load_failed: "<<e.what()<<'\n';}
 std::unique_ptr<mgo2mt::PlayerMotionBank> playerMotions;
 auto playerMotionPath=catalogPath.parent_path()/L"player.gwmot";
 if(std::filesystem::is_regular_file(playerMotionPath)){try{playerMotions=std::make_unique<mgo2mt::PlayerMotionBank>(file(playerMotionPath));}catch(...){std::osyncstream(std::cout)<<"{\"player_motion_load_failed\":true}\n";}}
 std::unique_ptr<mgo2mt::player::SpecialMotionBank> specialMotions;
 const auto specialMotionPath=catalogPath.parent_path()/L"special_male.gwmot";
 if(std::filesystem::is_regular_file(specialMotionPath))try{specialMotions=std::make_unique<mgo2mt::player::SpecialMotionBank>(file(specialMotionPath));}catch(...){std::osyncstream(std::cout)<<"{\"special_motion_load_failed\":true}\n";}
 auto specialPose=[&](unsigned gender,mgo2mt::combat::SpecialPhase phase,double seconds)->std::optional<mgo2mt::MotionPose>{
  // Female and alternate special-action archives are not recovered here.
  if(!specialMotions||gender!=0||phase==mgo2mt::combat::SpecialPhase::none)return {};
  return specialMotions->sample(mgo2mt::player::SpecialPhase(unsigned(phase)-1),seconds);
 };
 std::array<std::unique_ptr<mgo2mt::PlayerMotionBank>,2> selectionMotions;
 for(unsigned gender=0;gender<2;++gender){auto path=catalogPath.parent_path()/(L"selection"+std::to_wstring(gender)+L".gwmot");if(std::filesystem::is_regular_file(path))try{selectionMotions[gender]=std::make_unique<mgo2mt::PlayerMotionBank>(file(path));}catch(...){std::osyncstream(std::cout)<<"{\"selection_motion_load_failed\":true}\n";}}
 const auto selectionSoundPath=networkKeys.parent_path()/L"audio"/L"salute.gwa";
 std::unique_ptr<AudioThread> selectionAudio;
 const auto sopSoundPath=networkKeys.parent_path()/L"audio"/L"sop_native.wav";
 std::unique_ptr<AudioThread> sopAudio;
 mgo2mt::PreparedCharacter avatar;std::optional<std::array<uint8_t,28>> avatarAppearance;
 mgo2mt::motion_blend::Lane avatarBlend;mgo2mt::foot_ik::Solver avatarFootIk;std::optional<mgo2mt::MotionPose> avatarGroundPose;std::array<float,3> avatarDrawOrigin{};float avatarDrawYaw=0;bool avatarWasDrawn=false,avatarWasRagdoll=false;uint64_t avatarModelGeneration=0;
 std::unique_ptr<mgo2mt::CharacterRenderer> avatarRenderer,avatarArmsRenderer,avatarFadeRenderer;
 mgo2mt::remote::Scene remoteScene;
 mgo2mt::sop::Presentation sopPresentation;mgo2mt::sop::SpecialInput specialInput;mgo2mt::sop::PhaseClock specialClock;
 std::unique_ptr<mgo2mt::sop::Renderer> sopRenderer;
 try{sopRenderer=std::make_unique<mgo2mt::sop::Renderer>(device.Get());}catch(...){std::osyncstream(std::cout)<<"{\"sop_renderer_load_failed\":true}\n";}
 double specialSeconds=0;
 struct RemoteModel {mgo2mt::player::Ragdoll corpse;mgo2mt::combat::presentation::Death death;mgo2mt::special_pc::Kind kind=mgo2mt::special_pc::Kind::human;std::array<uint8_t,28> appearance{};mgo2mt::PreparedCharacter body;std::unique_ptr<mgo2mt::CharacterRenderer> renderer;mgo2mt::weapon_hand::Actor weapon;mgo2mt::motion_blend::Lane blend;mgo2mt::foot_ik::Solver footIk;std::optional<mgo2mt::MotionPose> groundPose;std::array<float,3> drawOrigin{};float drawYaw=0;bool hasDrawFrame=false,visible=false;};
 std::array<std::optional<RemoteModel>,24> remoteModels;
 std::vector<mgo2mt::remote::Avatar> remoteAvatars;
 std::shared_ptr<const mgo2mt::weapon_effect::Config> weaponEffects;
 if(gameplay&&!gameplay->resources().weaponEffectsManifest.empty()){
  auto config=std::make_shared<mgo2mt::weapon_effect::Config>();std::string error;
  if(!config->load(networkKeys.parent_path()/std::filesystem::u8path(gameplay->resources().weaponEffectsManifest),error))throw std::runtime_error(error);
  weaponEffects=std::move(config);
 }
 mgo2mt::combat::LightEffects combatLights;combatLights.configure(weaponEffects);
 mgo2mt::combat::KillFeed killFeed;
 mgo2mt::combat::particles::Pool projectileParticles;projectileParticles.configure(weaponEffects);
 if(gameplay)for(const auto&definition:gameplay->definitions()){
  const auto id=definition.weapon.id,source=definition.visual.effectId?definition.visual.effectId:id;
  projectileParticles.effect_source(id,source);combatLights.effect_source(id,source);
  projectileParticles.textures(id,definition.visual.flashTexture,definition.visual.smokeTexture);
 }
 bool sentDebugPhysics=false;
 mgo2mt::combat::tracers::Pool bulletTracers;
 std::unique_ptr<mgo2mt::combat::tracers::Renderer> tracerRenderer;
 std::unique_ptr<mgo2mt::combat::particles::Renderer> particleRenderer;
 const auto effectResource=networkKeys.parent_path()/(gameplay?gameplay->resources().effectsManifest:"fx/original.gwfx");if(std::filesystem::is_regular_file(effectResource))particleRenderer=std::make_unique<mgo2mt::combat::particles::Renderer>(device.Get(),effectResource);
 if(particleRenderer&&gameplay&&!gameplay->resources().damageEffectsManifest.empty())particleRenderer->add_bundle(networkKeys.parent_path()/std::filesystem::u8path(gameplay->resources().damageEffectsManifest));
 if(particleRenderer&&weaponEffects)particleRenderer->add_textures(*weaponEffects,networkKeys.parent_path());
 std::unique_ptr<mgo2mt::stage::weather::Renderer> weatherRenderer;mgo2mt::stage::weather::Controller weatherController;
 mgo2mt::stage::weather::SurfaceController weatherSurface; mgo2mt::stage::EnvironmentCache stageEnvironment; mgo2mt::environment::Config environmentConfig; std::shared_ptr<const mgo2mt::stage::Lighting> environmentLighting;
 uint64_t weatherStartedAt=GetTickCount64();std::optional<std::array<bool,5>> weatherKey;std::optional<mgo2mt::environment::Sound> environmentSound;
 if(std::filesystem::is_regular_file(networkKeys.parent_path()/"fx/weather.gwfx"))weatherRenderer=std::make_unique<mgo2mt::stage::weather::Renderer>(device.Get(),networkKeys.parent_path()/"fx/weather.gwfx");
 std::vector<mgo2mt::DynamicPointLight> combatPointLights;
 // AUTO AIM remains an opt-in action.
 // Current AK102 geometry in the native camera/torso adapter. Original weapon
 // actor flags and C05C38 state are absent here: explicitly neutral. Surveyor
 // remains unequipped (level 0) until a trusted round loadout is available;
 // never infer it from room flags or local HUD. Original aim-frame parity is pending.
 uint8_t lockSurveyorLevel=0;uint16_t lockWeapon=25;
 mgo2mt::player_lock::Lock playerLock({*mgo2mt::original_lock::ak102_parameters(25,0,0.f,0),.65f});
 mgo2mt::hud::EnemyNameTagRenderer enemyTagRenderer;
 std::vector<uint32_t> enemyTagSurface(1280*720);
 mgo2mt::reticle::Presentation weaponReticle;mgo2mt::reticle::Recoil weaponRecoil;
 std::unique_ptr<mgo2mt::clan::Bitmap> enemyTagBitmap;
 uint64_t enemyTagClanSerial=~uint64_t(0);
 mgo2mt::combat::presentation::Death avatarDeath;std::array<mgo2mt::combat::presentation::Reload,24> reloadPresentation;
 mgo2mt::player::Ragdoll ragdoll;std::optional<mgo2mt::physics::RigidBody> testBody;
 std::unique_ptr<mgo2mt::CharacterRenderer> bodyRenderer;std::wstring physicsError;
 std::unique_ptr<mgo2mt::CharacterRenderer> characterRenderer;unsigned modelFrames=0,emptyModelFrames=0;bool lastModelVisible=false;
 if(!modelPath.empty()){mgo2mt::CharacterModel model(file(modelPath));characterRenderer=std::make_unique<mgo2mt::CharacterRenderer>(device.Get(),model);std::osyncstream(std::cout)<<"{\"character_model_loaded\":true,\"vertices\":"<<model.vertices.size()<<",\"triangles\":"<<model.indices.size()/3<<",\"textures\":"<<model.textures.size()<<"}"<<std::endl;}
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
 auto graphics=std::make_shared<mgo2mt::GraphicsSettings>(inputPath.parent_path()/L"graphics.cfg");
 ComPtr<IDXGIOutput> output;std::vector<DXGI_MODE_DESC> displayModes;
 if(SUCCEEDED(swap->GetContainingOutput(&output))){UINT n=0;if(SUCCEEDED(output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM,0,&n,nullptr))&&n<=4096){displayModes.resize(n);if(SUCCEEDED(output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM,0,&n,displayModes.data()))){displayModes.resize(n);for(auto m:displayModes){mgo2mt::GraphicsConfig test;test.width=m.Width;test.height=m.Height;test.refresh_num=m.RefreshRate.Numerator;test.refresh_den=m.RefreshRate.Denominator;if(mgo2mt::valid_graphics(test)&&m.ScanlineOrdering!=DXGI_MODE_SCANLINE_ORDER_UPPER_FIELD_FIRST&&m.ScanlineOrdering!=DXGI_MODE_SCANLINE_ORDER_LOWER_FIELD_FIRST)graphics->modes.push_back({m.Width,m.Height,m.RefreshRate.Numerator,m.RefreshRate.Denominator});}}}}
 std::osyncstream(std::cout)<<"{\"graphics_display_modes\":"<<graphics->modes.size()<<"}"<<std::endl;
 graphics->apply=[&](const mgo2mt::GraphicsConfig& cfg){
  if(playtest.enabled&&cfg.fullscreen)return false;
  if(!shadowRenderer.configure(device.Get(),cfg.shadows()))return false;
  try{mgo2mt::render_backend::Options options{cfg.anisotropy,bool(cfg.mipmaps),bool(cfg.linearColor),bool(cfg.hdr),bool(cfg.softParticles),bool(cfg.lod),bool(cfg.reflections)};options.reflectionMaterials=reflectionMaterials;mgo2mt::render_backend::Device(device.Get()).configure(options);}catch(...){return false;}
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
 std::optional<mgo2mt::stage::RoundMusicPhase> loggedMusicPhase;
 mgo2mt::stage::MusicLibrary musicLibrary;mgo2mt::stage::MusicSelection musicSelection;
 auto musicRoot=networkKeys.empty()?std::filesystem::path{}:networkKeys.parent_path()/"bgm";
 auto musicScan=std::async(std::launch::async,[musicRoot]{return musicRoot.empty()?mgo2mt::stage::MusicLibrary{}:mgo2mt::stage::MusicLibrary::scan(musicRoot);});
 bool musicReady=false,musicPlaying=false,debugTitle=false;size_t musicIndex=0;mgo2mt::stage::MusicPlayback musicPlayback;std::wstring musicError;
 const auto startSound=[&](uint32_t cue){
  if(seStarted)throw std::runtime_error("Duplicate START sound");seStarted=true;
  std::osyncstream(std::cout)<<"{\"start_sound\":true,\"cue\":"<<cue<<",\"tick\":"<<animation->ticks()<<"}"<<std::endl;
  effect.start(sePath,L"600",18999);
 };
 if(gcx&&sound&&!sePath.empty())gcx->set_se_handler(startSound);
 std::vector<std::unique_ptr<AudioThread>> menuAudio;unsigned menuFailures=0;
 std::unique_ptr<AudioThread> voiceAudio;
 mgo2mt::special_pc::SaluteAudio saluteAudio;
 mgo2mt::combat::Effects combatEffects;combatEffects.load_manifest(networkKeys.parent_path()/(gameplay?gameplay->resources().audioManifest:"sfx/combat.txt"));
 mgo2mt::weapon_effect::Audio weaponEffectAudio;{std::string error;if(!weaponEffectAudio.configure(weaponEffects,networkKeys.parent_path(),combatEffects,error))throw std::runtime_error(error);}
 std::vector<std::unique_ptr<AudioThread>> combatAudio;
 std::array<mgo2mt::special_pc::GekkoFootsteps,24> gekkoFootsteps;
 mgo2mt::combat::footsteps::Timeline selfFootsteps({.5});
 std::vector<mgo2mt::combat::footsteps::Timeline> remoteFootsteps(24,mgo2mt::combat::footsteps::Timeline({.5}));
 auto playCombatSound=[&](const mgo2mt::combat::Sound& s){
  if(!sound)return;std::erase_if(combatAudio,[](const auto&a){return a->result.load()!=-1;});if(combatAudio.size()>=24)return;
  auto voice=std::make_unique<AudioThread>();voice->control.gain=s.gain;voice->start(s.file,L"30",s.cue,"combat_se");combatAudio.push_back(std::move(voice));
 };
 std::unique_ptr<AudioThread> radioAudio;std::shared_ptr<mgo2mt::radio::Session> radioAudioSession;uint64_t radioAudioGeneration=0;
 uint64_t combatEpoch=0,combatRevision=0,combatSendAt=0;uint32_t combatSequence=0,combatLife=0;bool combatReloadPending=false,combatFirePending=false,combatInputWasActive=false;
 std::optional<mgo2mt::combat::Player> hostPlayer;

 auto menuSound=[&](unsigned cue){if(!sound||mgo2mt::menu_audio::asset(cue).empty())return;for(auto it=menuAudio.begin();it!=menuAudio.end();){if((*it)->result.load()!=-1){if((*it)->result.load())++menuFailures;it=menuAudio.erase(it);}else ++it;}
  if(menuAudio.size()>=8)menuAudio.erase(menuAudio.begin());
  const auto& cuePath=cue==mgo2mt::menu_audio::Confirm?menuConfirmPath:cue==mgo2mt::menu_audio::Cancel?menuCancelPath:menuMovePath;
  if(cuePath.empty())return;auto a=std::make_unique<AudioThread>();a->start(cuePath,L"10",cue);menuAudio.push_back(std::move(a));
  std::osyncstream(std::cout)<<"{\"menu_sound_requested\":"<<cue<<"}"<<std::endl;
 };
 ComPtr<ID3D11Texture2D> agreementTexture;ComPtr<ID3D11ShaderResourceView> agreementView;
 mgo2mt::multi_ui::Layer customUi(networkKeys.parent_path()/L"ui/layout.json");
 Window::customUi=&customUi;
 struct UnbindCustomUi{~UnbindCustomUi(){Window::customUi=nullptr;Window::customUiInteractive=false;}} unbindCustomUi;
 ComPtr<ID3D11Texture2D> customUiTexture;ComPtr<ID3D11ShaderResourceView> customUiView;
 std::string customUiState;unsigned customUiFrames=0;
 if(agreement){D3D11_TEXTURE2D_DESC td{};td.Width=1280;td.Height=720;td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;check(device->CreateTexture2D(&td,nullptr,&agreementTexture));check(device->CreateShaderResourceView(agreementTexture.Get(),nullptr,&agreementView));}
 bool agreementStarted=false,agreementVisible=false,agreementCaptured=false,agreementScrolledCaptured=false,agreementChoiceCaptured=false;
 std::unique_ptr<mgo2mt::LoginScreen> login;unsigned loginFrames=0,loginVisits=0,returnFrames=0;
 mgo2mt::PlayerMenu playerMenu(inputPath,controllerInput,graphics);playerMenu.hold_assets(networkKeys.parent_path());Window::playerMenu=&playerMenu;
 mgo2mt::MusicMenu musicMenu;Window::musicMenu=&musicMenu;
 mgo2mt::invitation_ui::Presenter invitation;Window::invitation=&invitation;
 mgo2mt::notifications::Presentation notificationPresentation;
 mgo2mt::notifications::Renderer notificationRenderer;
 mgo2mt::alert_media::Player alertMedia(networkKeys.parent_path()/L"alerts");
 std::unique_ptr<AudioThread> notificationAudio;
 const auto notificationSoundPath=networkKeys.parent_path()/L"audio"/L"notification_native.wav";
 mgo2mt::LobbyDisconnectReason titleDisconnect=mgo2mt::LobbyDisconnectReason::none;
 std::optional<mgo2mt::host::LoadRequest> musicMenuRequest;bool wasMusicDeployed=false;
 bool portTitle=false,charTitle=false;ULONGLONG slotBegan=0;unsigned slotStep=0,slotCaptured=0;bool modelRotated=false;
 struct LoginReset{~LoginReset(){Window::login=nullptr;}} loginReset;
 unsigned agreementFrames=0;ULONGLONG closeAt=0;
 mgo2mt::AudioFade fade;
 const auto titleFade=[&](int duration){fade.stop(duration);audio.control.gain=fade.gain();std::osyncstream(std::cout)<<"{\"bgm_fade\":true,\"tick\":"<<animation->ticks()<<",\"argument\":"<<duration<<",\"remaining_frames\":"<<fade.remaining()<<",\"gain\":"<<fade.gain()<<"}"<<std::endl;};
 if(gcx&&sound)gcx->set_fade_handler(titleFade);
 auto began=std::chrono::steady_clock::now();auto deadline=GetTickCount64()+static_cast<ULONGLONG>(seconds*1000);bool running=true,loadingVisible=false,loadingCaptured=false,loadingReady=false;unsigned frames=0,occluded=0,lastState=0,captureIndex=0,loadingFrames=0;const uint32_t captureTicks[]={5,300,625,800,1000,1200};
 // User-selected local attract movie. This is separate from the original GCX
 // timeout/reset branch and never starts agreement/login on a movie skip.
 enum class MoviePhase {menu,fadeOut,opening,playing,returning};
 MoviePhase moviePhase=MoviePhase::menu;
 const auto titleMoviePath=std::filesystem::absolute(networkKeys.parent_path()/L"movie_01.mp4");
 std::error_code movieFileError;
 const bool attractAvailable=!scripted&&gcx&&animation&&initialTitle&&initialLoading&&std::filesystem::is_regular_file(titleMoviePath,movieFileError);
 bool attractFailed=!std::filesystem::is_regular_file(titleMoviePath);uint64_t titleIdleSince=0,moviePhaseAt=0,movieLastTick=GetTickCount64();
 std::unique_ptr<mgo2mt::title_movie::Player> titleMovie;
 std::optional<mgo2mt::TitleAnimation> movieLoading;
 std::optional<mgo2mt::GraphicsConfig> moviePreviousGraphics;
 std::optional<LONG_PTR> moviePreviousStyle;
 const auto movieApplyGraphics=[&](const mgo2mt::GraphicsConfig& requested){
  if(graphics->apply(requested))return true;
  // apply() can release the old backbuffer before a DXGI mode change fails.
  // Recreate a usable target before the movie error/loading path draws again.
  if(graphics->apply(graphics->active))return false;
  mgo2mt::GraphicsConfig safe;
  if(!graphics->apply(safe))throw std::runtime_error("Display unavailable after movie mode change");
  graphics->active=graphics->draft=safe; // session recovery only; no settings save
  std::osyncstream(std::cout)<<"{\"title_movie_display_safe_recovery\":true}\n";
  return false;
 };
 if(attractAvailable)animation->wait_for_start(true);
 Window::titleActivity=GetTickCount64();Window::titleMovieActive=Window::titleMovieSkip=false;
 struct MovieInputReset{~MovieInputReset(){Window::titleMovieActive=Window::titleMovieSkip=false;}} movieInputReset;
 const auto blendSettingsPath=networkKeys.parent_path()/L"motion_blend.cfg";
 auto blendSettings=mgo2mt::motion_blend::load(blendSettingsPath);mgo2mt::motion_blend::Dialog blendDialog;
 auto blendTick=GetTickCount64();
 mgo2mt::DebugFrameRate debugFrameRate;
 while(running&&GetTickCount64()<deadline){gpuProfiler.poll(context.Get());Window::sync_menu_wait();MSG msg{};while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)running=false;if(blendDialog.message(msg))continue;TranslateMessage(&msg);DispatchMessage(&msg);}if(!running)break;bool invitationFrameModal=false;
  if(IsIconic(window.handle))debugFrameRate.reset();
  if(login&&login->lobby_monitor().reason!=mgo2mt::LobbyDisconnectReason::none){
   titleDisconnect=login->lobby_monitor().reason;
   Window::login=nullptr;login.reset(); // transport owner joins after watchdog shutdown
   playerMenu.close();musicMenu.close();blendDialog.close();player.suspend();
   playerMenu.cancel_hold_selection();playerMenu.chat_session(nullptr);playerMenu.radio_session(nullptr);playerMenu.inventory_session(nullptr);
   controllerInput->reset();specialPcInput.cancel();gekkoActive=false;coverInput.cancel();coverState={};coverTimeline.clear();specialInput.clear();specialClock.clear();evadeInput.cancel();evadeAcknowledged=false;predictedEvadeSerial=0;
   combatReloadPending=combatFirePending=combatInputWasActive=false;combatSendAt=0;
   invitation.session(nullptr);invitation.update(GetTickCount64());Window::invitationGuard.reset();
   notificationPresentation.reset();notificationAudio.reset();alertMedia.clear();
   stageAudio.reset();stageMusic.reset();musicPlayback.clear();musicPlaying=false;loggedMusicPhase.reset();
   radioAudio.reset();radioAudioSession.reset();voiceAudio.reset();selectionAudio.reset();sopAudio.reset();
   waterVoices.clear();combatAudio.clear();menuAudio.clear();audio.finish();lobbyAudio.finish();effect.finish();
   stageAssets.select(std::nullopt);stageSkyRenderer.reset();stageSkyModel.reset();stageRenderer.reset();stageModel.reset();stageCamera.reset();remoteScene.clear();remoteAvatars.clear();
   navigationRequest.reset();navigationWorld.reset();navigationBase.reset();hostPlayer.reset();combatEpoch=combatRevision=combatLife=0;
   weaponReticle.reset();weaponRecoil.reset();combatEffects.clear();sopPresentation.clear();
   agreement->return_from_login();agreementStarted=agreementVisible=false;agreementFrames=loginFrames=returnFrames=0;
   loadingVisible=loadingReady=false;loadingFrames=lastState=0;seStarted=lobbyMusicStarted=false;portTitle=charTitle=false;
   Window::agreementActive=false;Window::pressed=Window::agreementInput=0;Window::gameplayMode=Window::inspection=false;Window::debug={};Window::uiCues.clear();
   fade=mgo2mt::AudioFade{};audio.control.gain=1.f;lobbyAudio.control.gain=1.f;
   if(initialTitle){*animation=*initialTitle;animation->wait_for_start(true);}
   if(initialLoading)*loading=*initialLoading;
   if(gcx){gcx=std::make_unique<mgo2mt::TitleGcx>(file(gcxPath),std::cout);gcx->start(entry);if(sound){gcx->set_fade_handler(titleFade);if(!sePath.empty())gcx->set_se_handler(startSound);}}
   if(sound)audio.start(wavPath.empty()?path.parent_path().parent_path()/L"audio/bgm_mgo_title01.wav":wavPath,argv[2],23);
   began=std::chrono::steady_clock::now();window.title(L"MGO2MT - 接続解除 | STARTで入り直す");
   std::osyncstream(std::cout)<<"{\"lobby_disconnected_to_start\":true,\"reason\":"<<unsigned(titleDisconnect)<<"}\n";
  }
  if(!login){notificationPresentation.reset();notificationAudio.reset();alertMedia.clear();}
  if(!Window::debug.enabled)blendDialog.close();
  if(Window::debug.openMotionBlend){Window::debug.openMotionBlend=false;blendDialog.open(window.handle,blendSettings,blendSettingsPath);}
  auto blendNow=GetTickCount64();const double blendDelta=blendNow>=blendTick&&blendNow-blendTick<=250?double(blendNow-blendTick)/1000.:0.;blendTick=blendNow;
  const float blendRate=float(blendSettings.rate_per_second());
  graphics->tick(GetTickCount64(),GetForegroundWindow()==window.handle);
  {
   RECT client{};GetClientRect(window.handle,&client);D3D11_TEXTURE2D_DESC buffer{};back->GetDesc(&buffer);
   const unsigned width=unsigned(std::max(0L,client.right)),height=unsigned(std::max(0L,client.bottom));
   if(width&&height&&width<=8192&&height<=8192&&(width!=buffer.Width||height!=buffer.Height)){
    context->OMSetRenderTargets(0,nullptr,nullptr);rt.Reset();back.Reset();context->Flush();
    check(swap->ResizeBuffers(2,width,height,DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));
    check(swap->GetBuffer(0,IID_PPV_ARGS(&back)));check(device->CreateRenderTargetView(back.Get(),nullptr,&rt));rp=rt.Get();back->GetDesc(&buffer);
   }
  }
  if(login&&login->stage_load_request()){
   D3D11_TEXTURE2D_DESC buffer{};back->GetDesc(&buffer);viewport={0,0,float(buffer.Width),float(buffer.Height),0,1};Window::viewX=Window::viewY=0;Window::viewW=Window::viewH=1;
  }else{
   D3D11_TEXTURE2D_DESC buffer{};back->GetDesc(&buffer);const float scale=std::min(float(buffer.Width)/1280,float(buffer.Height)/720);
   viewport={(buffer.Width-1280*scale)/2,(buffer.Height-720*scale)/2,1280*scale,720*scale,0,1};
   Window::viewX=viewport.TopLeftX/buffer.Width;Window::viewY=viewport.TopLeftY/buffer.Height;Window::viewW=viewport.Width/buffer.Width;Window::viewH=viewport.Height/buffer.Height;
  }
  context->RSSetViewports(1,&viewport);context->OMSetRenderTargets(1,&rp,nullptr);

  const bool foreground=GetForegroundWindow()==window.handle;
  playtest.enforce_input(controllerInput->config);
  const bool inputActive=!blendDialog.visible()&&playtest.gameplay_active(foreground,scripted,controllerInput->config.device);
  const bool invitationModalBeforePoll=invitation.overlay().visible();
  invitation.session(login?login->invitation_session():nullptr);invitation.update(GetTickCount64());
  const bool invitationClosedBeforePoll=invitationModalBeforePoll&&!invitation.overlay().visible();
  auto pad=playtest.enabled&&!playtest.pad?mgo2mt::PadSample{}:controllerInput->poll(!blendDialog.visible()&&playtest.pad_active(foreground,scripted),playtest.enabled&&playtest.pad?playtest.slot:playerMenu.slot(login?login->input_slot():controllerInput->config.slot));
  const auto movieNow=GetTickCount64();
  const bool movieFocused=foreground&&!IsIconic(window.handle)&&!blendDialog.visible();
  if(movieFocused&&pad.raw_held)Window::titleActivity=movieNow;
  if(moviePhase==MoviePhase::menu){
   const bool idleEligible=attractAvailable&&!attractFailed&&!login&&!agreementStarted&&!loadingVisible&&animation->ready_for_start()&&titleDisconnect==mgo2mt::LobbyDisconnectReason::none;
   if(!idleEligible||!movieFocused)titleIdleSince=0;
   else{
    if(!titleIdleSince)titleIdleSince=movieNow;
    titleIdleSince=std::max(titleIdleSince,Window::titleActivity);
    // START received on the boundary wins over the attract transition.
    if(movieNow-titleIdleSince>=30000&&!Window::pressed){
     moviePhase=MoviePhase::fadeOut;moviePhaseAt=movieNow;Window::titleMovieActive=true;Window::titleMovieSkip=false;Window::agreementInput=Window::pressed=0;
     std::osyncstream(std::cout)<<"{\"title_movie_fade\":true,\"idle_ms\":30000}\n";
    }
   }
  }
  if(moviePhase!=MoviePhase::menu){
   auto returnToTitle=[&](bool failed){
    if(titleMovie){titleMovie->visible(false);titleMovie->stop();}
    audio.finish();audio.control.gain=0;
    attractFailed=attractFailed||failed;moviePhase=MoviePhase::returning;moviePhaseAt=movieNow;
    movieLoading=*initialLoading;movieLoading->tick(5,0);Window::titleMovieSkip=false;Window::pressed=Window::agreementInput=0;
    window.title(L"MGO2MT - Loading");
   };
   if(titleMovie){titleMovie->pump();titleMovie->pause(!movieFocused);}
   if(!movieFocused&&movieNow>=movieLastTick)moviePhaseAt+=movieNow-movieLastTick;
   const auto movieActions=controllerInput->actions(pad);
   if(movieFocused&&(Window::titleMovieSkip||(pad.armed&&movieActions[12]))&&moviePhase!=MoviePhase::returning)returnToTitle(false);
   if(moviePhase==MoviePhase::fadeOut&&movieNow-moviePhaseAt>=600&&movieFocused){
    audio.finish();audio.control.gain=0;
    try{
     // MFPlay presents to a child HWND. Temporarily leave exclusive DXGI mode
     // and restore it on return; persistent graphics preferences are untouched.
     if(graphics->active.fullscreen){
      moviePreviousGraphics=graphics->active;MONITORINFO monitor{sizeof(monitor)};
      if(!GetMonitorInfoW(MonitorFromWindow(window.handle,MONITOR_DEFAULTTONEAREST),&monitor))throw std::runtime_error("Movie monitor unavailable");
      auto cfg=graphics->active;cfg.fullscreen=0;if(!movieApplyGraphics(cfg))throw std::runtime_error("Movie windowed presentation unavailable");
      moviePreviousStyle=GetWindowLongPtrW(window.handle,GWL_STYLE);
      SetWindowLongPtrW(window.handle,GWL_STYLE,(*moviePreviousStyle&~LONG_PTR(WS_OVERLAPPEDWINDOW))|WS_POPUP|WS_CLIPCHILDREN);
      if(!SetWindowPos(window.handle,nullptr,monitor.rcMonitor.left,monitor.rcMonitor.top,monitor.rcMonitor.right-monitor.rcMonitor.left,monitor.rcMonitor.bottom-monitor.rcMonitor.top,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED))throw std::runtime_error("Movie borderless presentation unavailable");
     }
     if(!titleMovie)titleMovie=std::make_unique<mgo2mt::title_movie::Player>(window.handle);
     titleMovie->open(titleMoviePath);titleMovie->mute(!sound);titleMovie->visible(false);titleMovie->play();moviePhase=MoviePhase::opening;moviePhaseAt=movieNow;
     window.title(L"MGO2MT - Movie | START: return");
    }catch(const std::exception&){if(!back||!rt)throw;std::osyncstream(std::cout)<<"{\"title_movie_open_failed\":true}\n";returnToTitle(true);}
   }
   if(moviePhase==MoviePhase::opening&&titleMovie&&titleMovie->status()==mgo2mt::title_movie::Status::playing){moviePhase=MoviePhase::playing;std::osyncstream(std::cout)<<"{\"title_movie_playing\":true}\n";}
   if((moviePhase==MoviePhase::opening||moviePhase==MoviePhase::playing)&&titleMovie){
    if(titleMovie->status()==mgo2mt::title_movie::Status::failed||titleMovie->status()==mgo2mt::title_movie::Status::ended){const bool failed=titleMovie->status()==mgo2mt::title_movie::Status::failed;std::osyncstream(std::cout)<<"{\"title_movie_finished\":true,\"failed\":"<<(failed?"true":"false")<<"}\n";returnToTitle(failed);}
    else if(moviePhase==MoviePhase::opening&&movieNow-moviePhaseAt>15000){std::osyncstream(std::cout)<<"{\"title_movie_open_timeout\":true}\n";returnToTitle(true);}
   }
   movieLastTick=movieNow;
   if(moviePhase==MoviePhase::returning&&movieNow-moviePhaseAt>=750&&movieFocused){
    titleMovie.reset();movieLoading.reset();
    if(moviePreviousStyle){SetWindowLongPtrW(window.handle,GWL_STYLE,*moviePreviousStyle);moviePreviousStyle.reset();SetWindowPos(window.handle,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);}
    if(moviePreviousGraphics){const auto restore=*moviePreviousGraphics;moviePreviousGraphics.reset();if(movieApplyGraphics(restore))graphics->active=graphics->draft=restore;else std::osyncstream(std::cout)<<"{\"title_movie_display_restore_failed\":true}\n";}
    *animation=*initialTitle;animation->wait_for_start(true);if(initialLoading)*loading=*initialLoading;
    gcx=std::make_unique<mgo2mt::TitleGcx>(file(gcxPath),std::cout);gcx->start(entry);if(sound){gcx->set_fade_handler(titleFade);if(!sePath.empty())gcx->set_se_handler(startSound);}
    fade=mgo2mt::AudioFade{};audio.control.gain=1;seStarted=false;loadingVisible=loadingReady=false;loadingFrames=lastState=0;
    if(sound)audio.start(wavPath.empty()?path.parent_path().parent_path()/L"audio/bgm_mgo_title01.wav":wavPath,argv[2],23);
    Window::pressed=Window::agreementInput=0;Window::titleMovieActive=Window::titleMovieSkip=false;Window::titleActivity=movieNow;controllerInput->reset();
    titleIdleSince=0;moviePhase=MoviePhase::menu;began=std::chrono::steady_clock::now();window.title(L"MGO2MT - Title | Enter: START | Esc: close");
    std::osyncstream(std::cout)<<"{\"title_movie_returned_to_start\":true}\n";continue;
   }
   if(titleMovie){RECT bounds{};GetClientRect(window.handle,&bounds);const float scale=std::min(float(bounds.right)/1280,float(bounds.bottom)/720);const int w=int(1280*scale),h=int(720*scale);titleMovie->place((bounds.right-w)/2,(bounds.bottom-h)/2,w,h);titleMovie->visible(moviePhase==MoviePhase::playing&&!IsIconic(window.handle));}
   std::vector<Quad> movieQuads;
   if(moviePhase==MoviePhase::fadeOut){movieQuads=animation->geometry();const float remaining=1-std::clamp(float(movieNow-moviePhaseAt)/600.f,0.f,1.f);audio.control.gain=remaining;for(auto&q:movieQuads)for(auto&v:q.vertices){v.r*=remaining;v.g*=remaining;v.b*=remaining;}}
   else if(moviePhase==MoviePhase::returning&&movieLoading){const auto target=uint32_t(double(movieNow-moviePhaseAt)*300./1001.);if(movieFocused)while(movieLoading->ticks()+5<=target)movieLoading->tick(5,0);movieQuads=movieLoading->geometry();for(auto&q:movieQuads)if(q.atlas>=0)q.atlas+=titleTextures;}
   std::vector<Vertex> movieVertices;for(const auto&q:movieQuads)for(int i:{0,1,2,0,2,3})movieVertices.push_back(q.vertices[i]);
  D3D11_MAPPED_SUBRESOURCE mapped{};check(context->Map(vb.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped));if(!movieVertices.empty())std::memcpy(mapped.pData,movieVertices.data(),movieVertices.size()*sizeof(Vertex));context->Unmap(vb.Get(),0);
   context->IASetInputLayout(input.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->IASetVertexBuffers(0,1,&vptr,&stride,&offset);context->IASetIndexBuffer(nullptr,DXGI_FORMAT_R32_UINT,0);context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);context->PSSetSamplers(0,1,&sp);context->RSSetState(raster.Get());context->RSSetViewports(1,&viewport);context->OMSetRenderTargets(1,&rp,nullptr);context->OMSetDepthStencilState(nullptr,0);
   const float black[4]={0,0,0,1};context->ClearRenderTargetView(rt.Get(),black);
   for(UINT i=0;i<movieQuads.size();++i){const auto&q=movieQuads[i];ID3D11ShaderResourceView*tex=textures[q.atlas<0?textureCount:q.atlas].Get();context->PSSetShaderResources(0,1,&tex);context->OMSetBlendState(blend[q.blend].Get(),nullptr,0xffffffff);context->Draw(6,i*6);}
   // EVR owns the child while playing; avoid presenting the parent's black
   // swapchain over it. WM_PAINT/UpdateVideo refresh the child surface.
   if(moviePhase!=MoviePhase::playing){check(swap->Present(graphics->active.vsync?1:0,0));++frames;}
   Window::pressed=Window::agreementInput=0;cpuPreviousPresent.reset();Sleep(8);continue;
  }
  movieLastTick=movieNow;
  const bool radioDpad=playerMenu.radio_visible();if(inputActive&&radioDpad&&controllerInput->config.device&&pad.connected&&pad.armed&&!controllerInput->actions(pad)[12]){const auto directions=pad.held&15;if((pad.pressed&directions)==directions)playerMenu.radio_digital_mask(directions);}
  bool consumed=blendDialog.visible()?true:(invitationClosedBeforePoll||Window::invitationGuard.stale(invitation.overlay().visible()))?true:invitation.overlay().visible()?false:musicMenu.visible()?false:playerMenu.visible()?playerMenu.sample(pad):(login&&login->controller_sample(pad));
  if(inputActive&&!consumed){auto actions=controllerInput->actions(pad);
   if(actions[12]&&login&&login->stage_load_request()&&!playerMenu.capturing()){if(Window::menu_action(mgo2mt::MenuInputWait::Kind::decision))Window::proc(window.handle,Window::StartMenuMessage,0,0);actions.fill(false);}
   for(unsigned a=0;a<mgo2mt::input_actions;++a)if(actions[a]){
   if(invitation.overlay().visible()){auto k=a==12?VK_ESCAPE:mgo2mt::menu_key(a);if(k){Window::proc(window.handle,WM_KEYDOWN,k,1LL<<25);break;}continue;}
   if(playerMenu.hold_consumes_action(a))continue;
   if(a==7&&playerMenu.settings_menu()&&!playerMenu.capturing()&&invitation.overlay().available()){Window::proc(window.handle,WM_KEYDOWN,VK_F6,1LL<<25);break;}
   if(radioDpad&&a<4)continue;
   if(a==13&&playerMenu.chat_menu()){if(Window::menu_action(mgo2mt::MenuInputWait::Kind::decision))playerMenu.select_chat_radio();continue;}
   if(a==13&&!Window::debug.confirmReset&&login&&!login->personal_overlay_visible()&&login->chat_session()&&login->chat_session()->state().joined&&!playerMenu.visible()&&!musicMenu.visible()){if(Window::menu_action(mgo2mt::MenuInputWait::Kind::decision)){playerMenu.chat_session(login->chat_session());playerMenu.select_chat_radio();}continue;}
   if(a==12&&login&&login->stage_load_request()&&!playerMenu.visible()&&!musicMenu.visible()){Window::proc(window.handle,WM_KEYDOWN,VK_F9,1LL<<25);continue;}
   bool live=login&&login->gameplay_visible()&&login->stage_request()&&login->combat_offer()&&login->combat_state()&&login->combat_state()->players[login->combat_offer()->self.slot]&&login->combat_status()==mgo2mt::combat::wire::Status::active;
   if(((Window::debug.enabled&&Window::inspection)||live)&&login&&login->stage_request()&&!Window::debug.confirmReset&&!playerMenu.visible()&&!musicMenu.visible()&&!login->personal_overlay_visible())continue;
   if(playerMenu.visible()&&a==12&&!playerMenu.capturing()){if(Window::menu_action(mgo2mt::MenuInputWait::Kind::decision)){playerMenu.close(true);player.suspend();}continue;}
   if(playerMenu.visible()&&a>=16)continue;
   if(playerMenu.settings_menu()&&(a==8||a==9)){if(Window::menu_action(mgo2mt::MenuInputWait::Kind::decision))playerMenu.tab_action(a);continue;}
   auto k=mgo2mt::menu_key(a);if(k)Window::proc(window.handle,WM_KEYDOWN,k,1LL<<25);
  }}
  for(auto cue:Window::uiCues)menuSound(cue);Window::uiCues.clear();
  if(closeAt&&GetTickCount64()>=closeAt)break;
  if(animation){auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();auto target=static_cast<uint32_t>(elapsed*300000.0/1001.0);
   while(animation->ticks()+5<=target&&animation->state()!=4){fade.advance(1);audio.control.gain=fade.gain();if(scripted&&(animation->ticks()+5==100||animation->ticks()+5==900))SendMessageW(window.handle,WM_KEYDOWN,VK_RETURN,0);uint32_t input=Window::pressed;Window::pressed=0;animation->tick(5,input);
    if(animation->accepted()&&titleDisconnect!=mgo2mt::LobbyDisconnectReason::none){titleDisconnect=mgo2mt::LobbyDisconnectReason::none;animation->wait_for_start(attractAvailable);}
    if(animation->state()!=lastState){lastState=animation->state();std::osyncstream(std::cout)<<"{\"actor_state\":"<<lastState<<",\"tick\":"<<animation->ticks()<<"}"<<std::endl;}
   }
   if(animation->state()==4&&loading&&gcx->loading_requested()){
    if(!loadingVisible){loadingVisible=true;std::osyncstream(std::cout)<<"{\"loading_entered\":true,\"layout\":\"loading_MGO\",\"event\":\"0x9ca2fa\"}"<<std::endl;window.title(L"MGO2MT - Loading | Esc: close");}
    loading->tick(5,0);fade.advance(1);audio.control.gain=fade.gain();
    quads=loading->geometry();for(auto&q:quads)if(q.atlas>=0)q.atlas+=titleTextures;
    if(loadingFrames>=3&&!loadingReady){gcx->loading_ready();loadingReady=true;if(agreement){agreement->start();agreementStarted=true;if(sound&&!lobbyMusicPath.empty()){lobbyAudio.start(lobbyMusicPath,argv[2],23);lobbyMusicStarted=true;}}}
   }else quads=animation->geometry();
   count=static_cast<uint32_t>(quads.size());vertices.clear();for(const auto&q:quads)for(int i:{0,1,2,0,2,3})vertices.push_back(q.vertices[i]);
  }
  if(agreementStarted&&(agreementVisible||(agreement->ready()&&loadingFrames>=30))){
   agreement->report();if(!agreementVisible){agreementVisible=true;Window::agreementActive=true;Window::agreementInput=0;window.title(L"OpenMGO2 - Agreement | Enter: select | Esc: close");menuSound(93);std::osyncstream(std::cout)<<"{\"agreement_visible\":true}"<<std::endl;}
   auto input=Window::agreementInput;Window::agreementInput=0;
   if(scripted){if(agreementFrames==60)input=mgo2mt::AgreementScreen::pageDown;if(agreementFrames==90)input=mgo2mt::AgreementScreen::home;if(agreementFrames==100&&!scriptedNo)input=mgo2mt::AgreementScreen::left;if(agreementFrames==140)input=mgo2mt::AgreementScreen::confirm;}
   if(scriptedLogin&&!login&&loginVisits==1){++returnFrames;if(returnFrames==30)input=mgo2mt::AgreementScreen::left;if(returnFrames==60)input=mgo2mt::AgreementScreen::confirm;}
   bool close=false;int cue=closeAt||login?-1:agreement->input(input,close);if(cue>=0)menuSound(static_cast<unsigned>(cue));if(close)closeAt=GetTickCount64()+1250;
   if(!login&&agreement->accepted()&&!loginPath.empty()){
    std::filesystem::path store;
    if(scripted)store=std::filesystem::path(argv[3]).parent_path()/L"login-test.dat";
    else store=inputPath.parent_path()/L"login.dat";
    login=std::make_unique<mgo2mt::LoginScreen>(store,!scripted,mgo2mt::authenticate,true,controllerInput,graphics,networkKeys,playtest.enabled,playtest.enabled?playtest.port():0);Window::login=login.get();loginFrames=0;++loginVisits;
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
     if(loginFrames==130){auto actions=controllerInput->actions({true,1u<<8,1u<<8});for(unsigned a=0;a<mgo2mt::input_actions;++a)if(actions[a]){auto k=mgo2mt::menu_key(a);if(k)Window::proc(window.handle,WM_KEYDOWN,k,1LL<<25);}std::osyncstream(std::cout)<<"{\"controller_scripted_pad_tab\":true,\"physical_device_test\":false}"<<std::endl;}
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
     voiceAudio.reset();voiceAudio=std::make_unique<AudioThread>();voiceAudio->control.frequencyRatio=mgo2mt::character_voice_ratio(request->pitch);
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
    if(appearance!=preparedAppearance||id!=preparedId){preparedAppearance=appearance;preparedId=id;characterRenderer.reset();prepared={};selectionComposite.reset();previewBox=false;selectionBlend.reset();previousSelectionKind=mgo2mt::SelectionPresentation::Kind::idle;selectionLift=selectionLiftFrom=0;selectionLiftProgress=1;
     if(appearance){prepared=characterCatalog->assemble(*appearance);if(prepared.ready())characterRenderer=std::make_unique<mgo2mt::CharacterRenderer>(device.Get(),prepared.model);modelBegan=GetTickCount64();++appearanceChanges;
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
   if(ladderScene!=stageResult.generation){ladderScene=stageResult.generation;ladderAnchors.clear();ladderInput.cancel();if(stageResult.request)try{auto p=networkKeys.parent_path()/"stage"/(std::string(mgo2mt::stage::name(stageResult.request->rotation.map))+"_ladders.cfg");if(std::filesystem::is_regular_file(p))ladderAnchors=mgo2mt::ladder::load(p);}catch(...){std::osyncstream(std::cout)<<"{\"ladder_config_load_failed\":true}\n";}}
   auto navigationNow=GetTickCount64();float navigationSeconds=float(navigationNow-navigationTick)/1000;navigationTick=navigationNow;
   if(movementSource!=stageResult.collision){movementSource=stageResult.collision;auto augmented=movementSource&&stageResult.request?std::make_shared<const mgo2mt::stage::Collision>(mgo2mt::mounted::with_collision(*movementSource,mountedRegistry,stageResult.request->rotation.map)):movementSource;queryWorld=augmented;movementWorld=mgo2mt::stage::movement_collision(augmented);}navigationWorld=movementWorld;
   auto collisionBase=stageResult.authoredCollision?stageResult.authoredCollision:stageResult.collision;
   if(navigationBase!=collisionBase||navigationRequest!=stageResult.request||navigationGeneration!=stageResult.generation||Window::debug.reset){
    ragdoll.stop();testBody.reset();bodyRenderer.reset();physicsError.clear();Window::testRagdoll=Window::testBody=false;
    navigationWorld=mgo2mt::stage::movement_collision(navigationWorld);
    navigationBase=collisionBase;navigationRequest=stageResult.request;navigationGeneration=stageResult.generation;navigation=mgo2mt::stage::Navigation{};coverState={};coverInput.cancel();coverTimeline.clear();freeLean.clear();navigationWater.reset();contactWater.reset();waterContacts.reset();waterEffects.reset();remoteWaterEffects.clear();waterSteps.reset();waterVoices.clear();waterRenderer.reset();selfFootsteps.reset();avatarBlend.reset();avatarFootIk.reset();avatarGroundPose.reset();avatarWasDrawn=false;navigationArmed=false;combatRevision=0;yFirstPerson=playerMenu.prone_y_first_person();player=mgo2mt::player::Control{yFirstPerson};player.bodyYaw=2.2f;movementRunning=false;motionSeconds=0;playerMenu.close();
    // Local inspection starts near a source GCX initial spawn; this does not
    // bypass the HOST deployment transaction.
    if(stageResult.request){const auto route=std::string(mgo2mt::stage::name(stageResult.request->rotation.map));
     try{std::ifstream waterIn(networkKeys.parent_path()/"stage"/(route+".gww"));if(waterIn){navigationWater=std::make_shared<const mgo2mt::stage::Water>(mgo2mt::stage::Water::read(waterIn));navigation.water(navigationWater);}}catch(...){navigationWater.reset();navigation.water({});}
     if(stageResult.request->rotation.map==1)try{std::ifstream surfaceIn(networkKeys.parent_path()/"stage/n001a_surface.gws",std::ios::binary);if(surfaceIn)contactWater=std::make_shared<const mgo2mt::stage::WaterSurface>(mgo2mt::stage::WaterSurface::read(surfaceIn));}catch(...){contactWater.reset();}
     // Prefer the verified finite original MDN surface; AA can also use its
     // reviewed finite GEOM contact shape. Never invent a plane from FIELD bounds.
     try{
      std::shared_ptr<const mgo2mt::stage::WaterSurface> visualWater=contactWater;
      std::ifstream surfaceIn(networkKeys.parent_path()/"stage"/(route+"_render.gws"),std::ios::binary);
      if(surfaceIn)visualWater=std::make_shared<const mgo2mt::stage::WaterSurface>(mgo2mt::stage::WaterSurface::read(surfaceIn));
      if(visualWater&&!visualWater->triangles().empty())waterRenderer=std::make_unique<mgo2mt::water_visuals::Renderer>(device.Get(),mgo2mt::water_visuals::mesh(nullptr,visualWater.get()));
     }catch(...){waterRenderer.reset();std::osyncstream(std::cout)<<"{\"water_surface_unavailable\":true}"<<std::endl;}
     try{std::ifstream spawnIn(networkKeys.parent_path()/"stage"/(route+".tdm-spawns-v2.cfg"));auto profile=mgo2mt::combat::spawn::StageProfile::read(spawnIn);auto point=profile.group(mgo2mt::combat::spawn::Variant::normal,mgo2mt::combat::spawn::Kind::initial,0).front().position;point[1]+=3000;if(navigationWorld&&navigation.place(*navigationWorld,point))navigation.facing(2.2f);}catch(...){if(stageResult.request->rotation.map==20&&navigationWorld)navigation.place(*navigationWorld,{-42878.8359f,3000,29781.7969f});}
    }
   }
   hostPlayer.reset();auto combatOffer=login?login->combat_offer():std::nullopt;auto combatState=login?login->combat_state():std::nullopt;
   if(combatOffer&&combatState&&combatState->epoch==combatOffer->epoch)hostPlayer=combatState->players[combatOffer->self.slot];
   bool combatPlayable=hostPlayer&&login->combat_status()==mgo2mt::combat::wire::Status::active;
   const uint8_t surveyorLevel=combatPlayable&&hostPlayer->verifiedSkills?hostPlayer->surveyorLevel:0;
   const uint16_t equippedLockWeapon=hostPlayer?hostPlayer->weapon:25;
   if(surveyorLevel!=lockSurveyorLevel||equippedLockWeapon!=lockWeapon){
    lockSurveyorLevel=surveyorLevel;lockWeapon=equippedLockWeapon;playerLock.clear();
    if(auto parameters=mgo2mt::gameplay::lock(gameplay,lockWeapon,lockSurveyorLevel))playerLock=mgo2mt::player_lock::Lock({*parameters,.65f});
    else playerLock=mgo2mt::player_lock::Lock({lockWeapon==128||lockWeapon==129?8000.f:0.f,.98f,.65f}); // Explicit native Gekko policy, independent of previously equipped gun.
   }
   bool combatViewing=hostPlayer&&(combatPlayable||login->combat_status()==mgo2mt::combat::wire::Status::ended);
   gekkoActive=combatViewing&&hostPlayer->specialPc.kind==mgo2mt::special_pc::Kind::gekko;
   if(specialPcInput.scope(combatOffer?combatOffer->epoch:0,combatOffer?combatOffer->self:mgo2mt::combat::Identity{},hostPlayer?hostPlayer->life:0,gekkoActive?mgo2mt::special_pc::Kind::gekko:mgo2mt::special_pc::Kind::human)){
    coverInput.cancel();coverState={};coverTimeline.clear();freeLean.clear();evadeInput.cancel();specialInput.clear();specialClock.clear();gekkoClock.clear();gekkoLocomotion.reset();ragdoll.stop();player=mgo2mt::player::Control{playerMenu.prone_y_first_person()};player.bodyYaw=hostPlayer?hostPlayer->pose.yaw:player.bodyYaw;
    if(hostPlayer&&!gekkoActive)player.stance=hostPlayer->pose.capsule.height==560?mgo2mt::player::Stance::prone:hostPlayer->pose.capsule.height==1100?mgo2mt::player::Stance::crouching:mgo2mt::player::Stance::standing;
   }
   // Retire the previous life before its physics can affect this frame's camera.
   const uint64_t blendIdentity=combatOffer?mgo2mt::stage::water_surface_identity(combatOffer->self.slot,combatOffer->self.instance,combatOffer->self.character):1;
   if(avatarWasDrawn&&!avatarBlend.matches({combatOffer?combatOffer->epoch:1,stageResult.generation,blendIdentity,combatViewing?hostPlayer->life:1,avatarModelGeneration})){avatarBlend.reset();avatarFootIk.reset();avatarGroundPose.reset();avatarWasDrawn=false;ragdoll.stop();}
   const bool gekkoActionEnded=gekkoActive&&hostPlayer->specialPc.action==mgo2mt::special_pc::Action::none&&gekkoWasTraversing;
   gekkoWasTraversing=gekkoActive&&(hostPlayer->specialPc.action==mgo2mt::special_pc::Action::jump||hostPlayer->specialPc.action==mgo2mt::special_pc::Action::climb);
   if(combatPlayable&&navigationWorld&&combatState->revision!=combatRevision){
    float error=0;auto feet=navigation.feet();for(unsigned i=0;i<3;++i)error+=(feet[i]-hostPlayer->pose.feet[i])*(feet[i]-hostPlayer->pose.feet[i]);
    if(combatEpoch!=combatState->epoch||combatLife!=hostPlayer->life||!navigation.ready()||navigation.capsule().height!=hostPlayer->pose.capsule.height||navigation.capsule().radius!=hostPlayer->pose.capsule.radius||hostPlayer->ladderAnchor||ladderCurrent||hostPlayer->mountedId!=mountedCurrent||hostPlayer->flightId||flightCurrent||(gekkoActive&&hostPlayer->specialPc.action!=mgo2mt::special_pc::Action::none)||gekkoActionEnded||(gekkoActive&&!combatInputWasActive)||error>600.f*600.f){gekkoLocomotion.reset();selfFootsteps.reset();waterContacts.reset();navigation.authoritative(*navigationWorld,hostPlayer->pose.feet,(gekkoActive||flightCurrent)&&combatEpoch==combatState->epoch&&combatLife==hostPlayer->life&&navigation.ready()?navigation.yaw():hostPlayer->pose.yaw,(gekkoActive||flightCurrent)&&combatEpoch==combatState->epoch&&combatLife==hostPlayer->life&&navigation.ready()?navigation.pitch():hostPlayer->pose.pitch,hostPlayer->pose.capsule);if(combatEpoch!=combatState->epoch||combatLife!=hostPlayer->life){combatSequence=0;combatSendAt=0;specialInput.clear();specialClock.clear();combatReloadPending=combatFirePending=combatInputWasActive=false;player=mgo2mt::player::Control{playerMenu.prone_y_first_person()};player.bodyYaw=hostPlayer->pose.yaw;navigationArmed=false;}}
    combatEpoch=combatState->epoch;combatRevision=combatState->revision;combatLife=hostPlayer->life;
   }
   if(!combatOffer){combatReloadPending=combatFirePending=combatInputWasActive=false;combatSendAt=0;combatEpoch=combatRevision=0;combatEffects.clear();combatAudio.clear();}
   player.host_reload(combatPlayable&&hostPlayer->alive?std::optional(hostPlayer->reloadUntil!=0):std::nullopt);
   std::array<std::vector<unsigned>,24> reloadCues;
   for(unsigned slot=0;slot<24;++slot){
    const auto* p=combatViewing&&combatState&&combatState->players[slot]?&*combatState->players[slot]:nullptr;
    auto hand=p&&handMotions&&p->reloadUntil?mgo2mt::gameplay::hand(gameplay,handMotions.get(),p->weapon,mgo2mt::PlayerMotion::Reload,0,false,-1,-1,p->pose.capsule.height==560?2:p->pose.capsule.height==1100?1:0):std::nullopt;
    auto duration=hand?handMotions->duration(mgo2mt::gameplay::motion_id(gameplay,p->weapon),hand->index):std::nullopt;
    std::span<const std::pair<unsigned,unsigned>> cues;
    if(hand)if(auto it=weaponSoundEvents.find({mgo2mt::gameplay::motion_id(gameplay,p->weapon),hand->index});it!=weaponSoundEvents.end())cues=it->second;
    double reloadDuration=p?mgo2mt::combat::native_reload_presentation_ms(p->weapon):0;
    if(p&&gameplay)if(const auto*definition=gameplay->find(p->weapon))if(auto timing=mgo2mt::combat::weapon_reload_timing(definition->weapon,*p))reloadDuration=double(timing->endMs);
    reloadCues[slot]=reloadPresentation[slot].update(combatViewing&&combatOffer?combatOffer->epoch:0,p,navigationNow,hand?hand->index:~0u,duration.value_or(0),cues,reloadDuration);
   }
   avatarDeath.scope(ragdoll,combatViewing&&combatOffer?combatOffer->epoch:0,stageResult.generation,combatOffer?combatOffer->self:mgo2mt::combat::Identity{},hostPlayer?hostPlayer->life:0);
   if(combatViewing&&!hostPlayer->alive){player.dead=true;player.firstPerson=player.aiming=false;player.cancel_evade();playerMenu.close();combatReloadPending=combatFirePending=false;}

   ladderCurrent=combatPlayable&&hostPlayer?hostPlayer->ladderAnchor:0;
   flightCurrent=combatPlayable&&hostPlayer?hostPlayer->flightId:0;if(!flightCurrent)flightPosition={};
   for(unsigned slot=0;slot<24;++slot)flightFrames[slot]=flightClocks[slot].update(combatViewing&&combatState?combatState->epoch:0,combatViewing&&combatState&&combatState->players[slot]?&*combatState->players[slot]:nullptr,navigationNow);
   mountedCurrent=combatPlayable&&hostPlayer?hostPlayer->mountedId:0;mountedMap=stageResult.request?stageResult.request->rotation.map:0;
   if(mountedCurrent||flightCurrent){player.stance=flightCurrent&&hostPlayer->blastFlight?(hostPlayer->pose.capsule.height==560?mgo2mt::player::Stance::prone:hostPlayer->pose.capsule.height==1100?mgo2mt::player::Stance::crouching:mgo2mt::player::Stance::standing):mgo2mt::player::Stance::standing;player.cancel_evade();coverState={};coverInput.cancel();}
   mountedInput.scope(combatPlayable?combatOffer->epoch:0,combatPlayable?mgo2mt::stage::water_surface_identity(combatOffer->self.slot,combatOffer->self.instance,combatOffer->self.character):0,combatPlayable?hostPlayer->life:0);
   const auto sopView=login?login->combat_sop():mgo2mt::combat::SopView{};specialPcInput.acknowledge(sopView,navigationNow);
   ladderInput.scope(combatPlayable?combatOffer->epoch:0,combatPlayable?mgo2mt::stage::water_surface_identity(combatOffer->self.slot,combatOffer->self.instance,combatOffer->self.character):0,combatPlayable?hostPlayer->life:0);if(combatPlayable&&sopView.recipient==hostPlayer->identity&&sopView.life==hostPlayer->life)ladderInput.acknowledge(sopView.inputSequence,sopView.inputSequenced);
   if(combatPlayable&&sopView.recipient==hostPlayer->identity&&sopView.life==hostPlayer->life)mountedInput.acknowledge(sopView.inputSequence,sopView.inputSequenced);
   if(coverInput.scope(combatOffer?combatOffer->epoch:0,combatOffer?combatOffer->self:mgo2mt::combat::Identity{},hostPlayer?hostPlayer->life:0)){coverState={};coverTimeline.clear();}
   if(combatPlayable){coverInput.acknowledge(sopView,navigationNow);coverState=hostPlayer->cover;if(!hostPlayer->alive||hostPlayer->stunned){coverInput.cancel();coverState={};}}
   coverAvailable=coverLeft=coverRight=false;
   const mgo2mt::reticle::Scope reticleScope{combatOffer?combatOffer->epoch:0,stageResult.generation,combatOffer?combatOffer->self:mgo2mt::combat::Identity{},hostPlayer?hostPlayer->life:0,hostPlayer?hostPlayer->weapon:uint16_t(0)};
   auto reticleVisible=[&]{return player.aiming&&!player.reloading()&&combatPlayable&&combatOffer&&combatState&&hostPlayer->identity==combatOffer->self&&hostPlayer->alive&&!hostPlayer->stunned&&!flightCurrent&&hostPlayer->weapon&&
    requestedStage&&stageResult.request==requestedStage&&stageResult.collision&&navigation.ready()&&login->gameplay_visible()&&
    !playerMenu.visible()&&!musicMenu.visible()&&!Window::debug.confirmReset&&!Window::invitationGuard.blocks_gameplay(invitation.overlay().visible())&&!login->personal_overlay_visible()&&
    hostPlayer->specialPhase==mgo2mt::combat::SpecialPhase::none&&hostPlayer->evadeKind==mgo2mt::combat::EvadeKind::none&&!player.special_active()&&player.evade_active()==mgo2mt::player::Evade::none&&!ragdoll.active();};
   if(!combatPlayable||!hostPlayer->alive||sopView.recipient!=hostPlayer->identity||sopView.life!=hostPlayer->life)specialInput.clear();
   else specialInput.acknowledge(sopView,navigationNow);
   if(evadeInput.scope(combatOffer?combatOffer->epoch:0,combatOffer?combatOffer->self:mgo2mt::combat::Identity{},hostPlayer?hostPlayer->life:0)){player.cancel_evade();evadeAcknowledged=false;predictedEvadeSerial=0;}
   if(!combatPlayable||!hostPlayer->alive||hostPlayer->stunned){evadeInput.cancel();evadeAcknowledged=false;predictedEvadeSerial=0;}
   else{
    const auto ack=evadeInput.acknowledge(sopView,*hostPlayer,combatOffer->epoch,navigationNow);
    if(ack==mgo2mt::player::EvadeInput::Ack::accepted)evadeAcknowledged=true;
    if(ack==mgo2mt::player::EvadeInput::Ack::rejected||ack==mgo2mt::player::EvadeInput::Ack::expired||
       (evadeAcknowledged&&(hostPlayer->evadeKind==mgo2mt::combat::EvadeKind::none||hostPlayer->evadeSerial!=predictedEvadeSerial))){player.cancel_evade();evadeAcknowledged=false;predictedEvadeSerial=0;}
   }
   player.host_special(combatPlayable&&(specialInput.pending()||hostPlayer->specialPhase!=mgo2mt::combat::SpecialPhase::none));
   if(combatPlayable)specialSeconds=specialClock.update(combatEpoch,*hostPlayer,navigationNow);else specialClock.clear();
   projectileParticles.synchronize(combatPlayable&&combatOffer&&combatState?combatOffer->epoch:0,stageResult.generation,combatState?combatState->eventWatermark:0,navigationNow);
   {const auto* self=combatState&&combatOffer&&combatOffer->self.slot<24&&combatState->players[combatOffer->self.slot]?&*combatState->players[combatOffer->self.slot]:nullptr;
    bulletTracers.synchronize(combatPlayable&&combatOffer&&self&&self->identity==combatOffer->self?combatOffer->epoch:0,stageResult.generation,combatOffer?combatOffer->self:mgo2mt::combat::Identity{},self?self->life:0,combatState?combatState->eventWatermark:0,navigationNow);}
   combatLights.synchronize(combatPlayable&&combatOffer&&combatState?combatOffer->epoch:0,stageResult.generation,combatState?combatState->eventWatermark:0,navigationNow);
   weaponEffectAudio.synchronize(combatPlayable&&combatOffer&&combatState?combatOffer->epoch:0,stageResult.generation,combatState?combatState->eventWatermark:0,navigationNow);
   const mgo2mt::combat::decals::Scope markScope{combatPlayable&&combatOffer?combatOffer->epoch:0,stageResult.generation};
   bulletMarks.synchronize(markScope,combatState?combatState->eventWatermark:0,navigationNow);materialParticles.synchronize(markScope,combatState?combatState->eventWatermark:0,navigationNow);
   std::vector<mgo2mt::combat::Event> frameCombatEvents;
   if(login){auto events=login->combat_events();frameCombatEvents=events;killFeed.scope(combatOffer?combatOffer->epoch:0);if(auto roster=login->room_host_roster())killFeed.roster(*roster);for(const auto&entry:killFeed.consume(events,navigationNow))std::osyncstream(std::cout)<<"combat_kill epoch="<<entry.event.epoch<<" event="<<entry.event.id<<" source="<<entry.event.source.character<<" target="<<entry.event.target.character<<" source_life="<<entry.event.sourceLife<<" target_life="<<entry.event.targetLife<<" weapon="<<entry.event.weapon<<" source_name="<<std::quoted(entry.source)<<" target_name="<<std::quoted(entry.target)<<'\n';if(combatPlayable&&combatState&&combatOffer){if(pickupFeedback.update(*combatState,combatOffer->self,events))menuSound(mgo2mt::menu_audio::Confirm);}else pickupFeedback.clear();weaponRecoil.update(reticleScope,reticleVisible(),combatState?combatState->eventWatermark:0,events,navigationSeconds);if(stageResult.collision)for(const auto& event:events)if(auto mark=mgo2mt::combat::decals::static_impact(event,markScope,*stageResult.collision)){bulletMarks.emit(*mark,navigationNow);const auto kind=stageResult.request?mgo2mt::combat::material_effects::verified_kind(stageResult.request->rotation.map,mark->material):mgo2mt::combat::material_effects::Kind::unknown;materialParticles.emit(*mark,kind,navigationNow);}if(combatState){bulletTracers.dispatch(events,*combatState,navigationNow);}}else{weaponRecoil.reset();pickupFeedback.clear();}
   combatPointLights=combatLights.sample(navigationNow);
   if(combatPlayable&&combatState){for(auto origin:saluteAudio.update(*combatState,stageResult.generation)){
    float squared=0;const auto listener=navigation.eye();for(unsigned i=0;i<3;++i)squared+=(origin[i]-listener[i])*(origin[i]-listener[i]);
    playCombatSound({0xff0001,networkKeys.parent_path()/"special/gekko_salute.wav",.7f/(1.f+squared/144000000.f),origin});
   }}else saluteAudio.clear();
   playerMenu.chat_session(login?login->chat_session():nullptr);
   playerMenu.radio_session(login?login->radio_session():nullptr);
   weaponActions.update(combatState.value_or(mgo2mt::combat::Snapshot{}),frameCombatEvents,navigationNow);
   playerMenu.inventory_session(login?login->inventory_session():nullptr);
   {auto session=login?login->radio_session():nullptr;const auto state=session?session->state():mgo2mt::radio::SessionState{};
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
     const auto voice=mgo2mt::radio_audio::appearance_voice(*peer->appearance);if(!voice)continue;
     const auto cues=mgo2mt::radio_audio::resolve(voice->type,event.preset);const auto pitch=mgo2mt::radio_audio::pitch_ratio(voice->type,voice->pitchByte);if(!cues||!pitch)continue;
     const auto cue=event.sender==combatOffer->self?cues->self:cues->remote;
     const auto file=mgo2mt::radio_audio::asset_path(networkKeys.parent_path()/L"radio",cue);std::error_code error;if(!std::filesystem::is_regular_file(file,error))continue;
     radioAudio=std::make_unique<AudioThread>();radioAudio->control.frequencyRatio=*pitch;radioAudio->start(file,L"30",cue,"preset_radio");
    }
   }
   stageCamera.reset();bool combatInputActive=false;if(login&&login->take_gameplay_options_request())playerMenu.open(mgo2mt::player::Menu::settings,true);
   std::array<float,24> selectionValues{};
   const auto& selectionConfig=controllerInput->config;
   if(inputActive){if(selectionConfig.device)selectionValues=controllerInput->action_values(pad);else for(unsigned a=0;a<24;++a)selectionValues[a]=(GetAsyncKeyState(int(selectionConfig.keyboard[a]))&0x8000)?1.f:0.f;}
   auto inventorySession=login?login->inventory_session():nullptr;
   const auto inventoryState=inventorySession?inventorySession->state():mgo2mt::items::ClientState{};
   const bool inventoryEligible=!mountedCurrent&&!flightCurrent&&!mountedInput.pending()&&(!gekkoActive||hostPlayer->specialPc.action==mgo2mt::special_pc::Action::none)&&combatPlayable&&hostPlayer&&hostPlayer->alive&&!hostPlayer->stunned&&!hostPlayer->reloadUntil&&hostPlayer->evadeKind==mgo2mt::combat::EvadeKind::none&&hostPlayer->specialPhase==mgo2mt::combat::SpecialPhase::none&&login->gameplay_visible()&&
    combatOffer&&inventoryState.context.scope.epoch==combatOffer->epoch&&inventoryState.context.actor.slot==combatOffer->self.slot&&inventoryState.context.actor.instance==combatOffer->self.instance&&inventoryState.context.actor.character==combatOffer->self.character&&inventoryState.context.actor.life==hostPlayer->life;
   const bool itemMenuEligible=!gekkoActive&&inventoryEligible&&inputActive&&!ladderCurrent&&!ladderInput.pending()&&(!playerMenu.visible()||playerMenu.hold_visible())&&!playerMenu.capturing()&&!musicMenu.visible()&&!Window::debug.enabled&&!Window::debug.confirmReset&&!Window::invitationGuard.blocks_gameplay(invitation.overlay().visible())&&(!login||!login->personal_overlay_visible());
   const bool itemMenuKey=(GetAsyncKeyState(VK_F7)&0x8000)!=0,itemMenuChord=selectionValues[15]>.12f&&selectionValues[6]>.12f;
   if(itemMenuShortcut.step(combatPlayable?combatOffer->epoch:0,combatPlayable?mgo2mt::stage::water_surface_identity(combatOffer->self.slot,combatOffer->self.instance,combatOffer->self.character):0,combatPlayable?hostPlayer->life:0,itemMenuKey,itemMenuChord,itemMenuEligible)){playerMenu.cancel_hold_selection();playerMenu.open(mgo2mt::player::Menu::equipment);}
   const auto holdings=mgo2mt::hold_selection::inventory_snapshot(inventoryState,inventoryEligible);
   if(auto choice=playerMenu.hold_selection_step(holdings,{selectionValues[14]>.12f,selectionValues[15]>.12f,selectionValues[2]>.12f,selectionValues[3]>.12f,inputActive,(playerMenu.visible()&&!playerMenu.hold_visible())||musicMenu.visible()||Window::debug.confirmReset||Window::invitationGuard.blocks_gameplay(invitation.overlay().visible())||bool(login&&login->personal_overlay_visible()),selectionValues[5]>.12f});choice&&inventorySession){
    if(!mgo2mt::hold_selection::submit_inventory_selection(*inventorySession,*choice))menuSound(mgo2mt::menu_audio::Cancel);
   }

   const auto inventoryDelivery=inventorySession?inventorySession->state().delivery:mgo2mt::items::Delivery::none;
   const bool inventoryPending=mgo2mt::hold_selection::inventory_blocks_gameplay(inventoryDelivery);
   if(((Window::debug.enabled&&Window::inspection)||combatViewing)&&requestedStage&&navigation.ready()&&navigationWorld){
    std::array<float,24> values{};auto&cfg=controllerInput->config;
    bool active=inputActive&&(!combatOffer||combatPlayable)&&(Window::inspection||(login&&login->gameplay_visible()))&&(!combatPlayable||(hostPlayer->alive&&!hostPlayer->stunned))&&!Window::debug.confirmReset&&!playerMenu.visible()&&!playerMenu.hold_blocks_gameplay()&&!inventoryPending&&!musicMenu.visible()&&!Window::invitationGuard.blocks_gameplay(invitation.overlay().visible())&&(!login||!login->personal_overlay_visible())&&(!cfg.device||(pad.connected&&pad.armed));
    if(active){if(cfg.device)values=controllerInput->action_values(pad);else for(unsigned a=0;a<24;++a)values[a]=(GetAsyncKeyState(int(cfg.keyboard[a]))&0x8000)?1.f:0.f;}
    mountedNearby=0;float nearestMounted=std::numeric_limits<float>::max();
    if(active&&combatPlayable&&!gekkoActive&&!ladderCurrent&&!mountedCurrent&&!flightCurrent&&!hostPlayer->reloadUntil&&navigation.capsule().height==1700&&hostPlayer->pose.capsule.height==1700&&!hostPlayer->faceSubmerged&&!coverState.attached&&!coverState.lean&&hostPlayer->specialPhase==mgo2mt::combat::SpecialPhase::none&&!player.special_active()&&player.evade_active()==mgo2mt::player::Evade::none)for(const auto&i:mountedRegistry.placements){
     if(i.map!=mountedMap)continue;const auto*t=mountedRegistry.find(i.type);if(!t)continue;
     if(std::any_of(combatState->players.begin(),combatState->players.end(),[&](const auto&p){return p&&p->mountedId==i.id;}))continue;
     const auto at=mgo2mt::mounted::operator_position(i,*t),feet=navigation.feet();const float distance=std::hypot(at[0]-feet[0],at[1]-feet[1],at[2]-feet[2]);
     if(distance<nearestMounted&&mgo2mt::mounted::within_use_range(i,*t,feet)&&mgo2mt::enemy_tag::visible(navigation.eye(),mgo2mt::stage::Vec3{at[0],at[1]+navigation.capsule().height-150,at[2]},*queryWorld,stageResult.objectHitCollision.get())){mountedNearby=i.id;nearestMounted=distance;}
    }
    if(mountedInput.step(values[mgo2mt::mounted::action_button]>.12f,active&&combatPlayable&&!gekkoActive&&!ladderCurrent&&!flightCurrent,mountedCurrent,mountedNearby,navigationNow))combatSendAt=0;
    if(mountedCurrent||mountedNearby||mountedInput.consumes_action())values[mgo2mt::mounted::action_button]=0;
    if(mountedCurrent||mountedInput.pending())for(unsigned a:{4u,5u,6u,7u,14u,15u,16u,17u,18u,19u})values[a]=0;
    if(mountedInput.pending())values[10]=values[11]=0;
    if(flightCurrent){for(unsigned a:{4u,5u,6u,7u,8u,10u,11u,14u,15u,16u,17u,18u,19u})values[a]=0;player.firstPerson=player.aiming=false;combatReloadPending=combatFirePending=false;}
    ladderAxis=values[16]-values[17];uint16_t nearLadder=0;ladderNearby=false;
    if(active&&!gekkoActive&&!mountedCurrent&&!flightCurrent&&!mountedInput.pending()&&!mountedNearby&&!ladderCurrent)for(const auto&a:ladderAnchors)if(mgo2mt::ladder::enter(a,navigation.feet(),navigation.capsule(),*navigationWorld)){nearLadder=a.id;ladderNearby=true;break;}
    if(ladderInput.step(values[7]>.12f,active&&!gekkoActive&&!mountedCurrent&&!flightCurrent&&!mountedInput.pending(),ladderCurrent,nearLadder,navigationNow))combatSendAt=0;
    if(ladderCurrent||ladderInput.pending()||ladderNearby)values[7]=0;
    if(ladderCurrent||ladderInput.pending())for(unsigned a:{4u,5u,6u,7u,8u,10u,11u,14u,15u,16u,17u,18u,19u})values[a]=0;
    if(gekkoActive){
     const bool climbAvailable=active&&values[5]>.12f&&values[16]-values[17]>.12f&&hostPlayer->specialPc.action==mgo2mt::special_pc::Action::none&&bool(mgo2mt::special_pc::begin_climb(navigation.feet(),player.bodyYaw,*navigationWorld));
     if(specialPcInput.step(values,active&&hostPlayer->alive&&!hostPlayer->stunned,hostPlayer->specialPc.action,navigationNow,climbAvailable))combatSendAt=0;
     for(unsigned action:{4u,5u,7u,8u,14u,15u})values[action]=0;
     if(specialPcInput.pending()||hostPlayer->specialPc.action!=mgo2mt::special_pc::Action::none)values[10]=values[11]=0;
     player.stance=mgo2mt::player::Stance::standing;player.firstPerson=false;player.dead=!hostPlayer->alive;player.cancel_evade();coverState={};coverInput.cancel();
    }
    // Trigger holds and START are consumed by their presentation routes.
    values[12]=values[14]=values[15]=0;
    if(combatPlayable&&!mountedCurrent&&mgo2mt::weapons::native_loadout::held_only(hostPlayer->weapon))values[4]=values[10]=0;
    if(navigation.water_state().proneBlocked)values[16]=values[17]=values[18]=values[19]=0;
    float magnitude=std::min(1.f,std::hypot(values[16]-values[17],values[19]-values[18]));
    movementRunning=active&&mgo2mt::input_running(magnitude,movementRunning,cfg);
    if(foreground&&!cfg.device&&(GetAsyncKeyState(VK_SHIFT)&0x8000))movementRunning=false;
    if(yFirstPerson!=playerMenu.prone_y_first_person()){yFirstPerson=playerMenu.prone_y_first_person();player.first_person_on_y(yFirstPerson);}
    coverAvailable=!mountedCurrent&&!flightCurrent&&!mountedInput.pending()&&!gekkoActive&&active&&!ragdoll.active()&&!player.special_active()&&!player.reloading()&&player.evade_active()==mgo2mt::player::Evade::none&&!coverState.attached&&
     navigation.water_state().foot!=mgo2mt::stage::WaterFoot::inWater&&bool(mgo2mt::combat::cover::acquire(*navigationWorld,navigation.feet(),navigation.capsule(),navigation.yaw()));
    player.cover_context(coverAvailable||coverInput.pending(),coverState.attached);
    auto previousStance=player.stance;player.step(values,navigationSeconds,active&&!ragdoll.active(),movementRunning,navigation.grounded());
    if(gekkoActive){
     if(!active||specialPcInput.pending()||hostPlayer->specialPc.action!=mgo2mt::special_pc::Action::none){gekkoLocomotion.reset();player.forward=player.right=player.speed=0;player.running=false;}
     else{
      const float inputMagnitude=std::min(1.f,std::hypot(player.forward,player.right));
      const float requestedSpeed=inputMagnitude*(player.running?mgo2mt::special_pc::native_gekko.runSpeed:mgo2mt::special_pc::native_gekko.walkSpeed);
      const float requestedYaw=inputMagnitude>.01f?navigation.yaw()+mgo2mt::source_movement_angle(player.right,player.forward):player.bodyYaw;
      auto policy=mgo2mt::gekko_locomotion::Policy{};policy.maximumGapMs=100;
      const mgo2mt::gekko_locomotion::Scope scope{combatEpoch,stageResult.generation,hostPlayer->identity.character,hostPlayer->life,hostPlayer->identity.instance,hostPlayer->identity.slot};
      auto step=gekkoLocomotion.update(scope,{requestedYaw,requestedSpeed},navigationNow,policy);
      if(step.rebaselined){gekkoLocomotion.reset();step=gekkoLocomotion.update(scope,{player.bodyYaw,0},navigationNow,policy);}
      const float distance=std::hypot(step.displacement[0],step.displacement[1]);
      player.speed=navigationSeconds>0?std::min(mgo2mt::special_pc::native_gekko.runSpeed,distance/navigationSeconds):0;
      const float travelYaw=distance>.001f?std::atan2(step.displacement[0],step.displacement[1]):step.yaw;
      player.forward=distance>.001f?std::cos(travelYaw-navigation.yaw()):0;
      player.right=distance>.001f?mgo2mt::source_screen_x*std::sin(travelYaw-navigation.yaw()):0;
      player.running=player.speed>mgo2mt::special_pc::native_gekko.walkSpeed+100;
      player.bodyYaw=step.yaw;
     }
    }
    if(active&&!ragdoll.active()&&!gekkoActive){
     using CA=mgo2mt::combat::cover::Action;
     const bool detach=coverState.attached&&(player.coverRequested||player.forward<-.12f);
     const bool attach=!coverState.attached&&player.coverRequested&&coverAvailable;
     if(detach||attach){
      if(combatPlayable){if(coverInput.press(detach?CA::detach:CA::attach,navigationNow))combatSendAt=0;}
      else if(detach)coverState={};
      else if(auto wall=mgo2mt::combat::cover::acquire(*navigationWorld,navigation.feet(),navigation.capsule(),navigation.yaw()))coverState={true,0,std::atan2(wall->normal[0],wall->normal[2])};
     }
     if(coverInput.pending()){player.forward=player.right=player.speed=0;player.triggerHeld=player.firePressed=player.firing=player.reloadStarted=false;}
     if(coverState.attached){player.forward=0;player.speed=mgo2mt::combat::cover::native_policy.slideSpeed;player.running=false;}
    }
    if(player.evadeRequested!=mgo2mt::player::Evade::none&&evadeMotions){
     auto kind=evadeKind(player.evadeRequested);
     bool admitted=!combatPlayable||evadeInput.press(kind,navigationNow);
     if(admitted){
      evadeMovementYaw=navigation.yaw();const float face=evadeMovementYaw+(mgo2mt::combat::is_roll(kind)?mgo2mt::source_movement_angle(player.evadeRight,player.evadeForward):0.f);
      const float length=std::hypot(player.evadeForward,player.evadeRight);if(length>.001f){player.evadeForward/=length;player.evadeRight/=length;}
      if(player.begin_reviewed_evade(player.evadeRequested,std::remainder(face,6.283185307f))){
       if(++localEvadeCounter==0)++localEvadeCounter;player.firstPerson=false;predictedEvadeSerial=combatPlayable?evadeInput.request():0;evadeAcknowledged=false;combatSendAt=0;combatReloadPending=combatFirePending=false;playerLock.clear();
      }else if(combatPlayable)evadeInput.cancel();
     }
    }
    // Original crouch transition is not yet recovered. The native standing
    // salute needs headroom; collision must approve expansion before request.
    if(player.specialRequested&&combatPlayable){
     if(navigation.shape(*navigationWorld,{350,1700,2})){player.stance=mgo2mt::player::Stance::standing;navigation.facing(player.bodyYaw);}
     else player.specialRequested=player.specialHeld=false;
    }
    if(player.stance!=previousStance){using mgo2mt::player::Stance;auto shape=player.stance==Stance::standing?mgo2mt::stage::Capsule{350,1700,2}:player.stance==Stance::crouching?mgo2mt::stage::Capsule{350,1100,2}:mgo2mt::stage::Capsule{260,560,2};if(!navigation.shape(*navigationWorld,shape))player.reject_stance(previousStance);}
    if(player.resetView)navigation.reset_view(player.bodyYaw);
    const auto cameraMotion=playerMenu.camera_settings().motion(player.firstPerson,player.aiming,player.turn,player.look);
    const auto cameraRates=playerMenu.camera_settings().rates(player.firstPerson,player.aiming);
    if(!gekkoActive&&!coverState.attached&&!mountedCurrent&&!flightCurrent&&!ladderCurrent&&player.evade_active()==mgo2mt::player::Evade::none&&hostPlayer&&gameplay)
     if(const auto*definition=gameplay->find(hostPlayer->weapon))player.speed*=mgo2mt::combat::weapon_move_scale(definition->weapon);
    if(active&&!ragdoll.active()&&!player.specialRequested&&!player.special_active()){
     mgo2mt::stage::WalkInput move{player.forward,player.right,cameraMotion[0],cameraMotion[1],coverInput.pending()?0.f:player.speed,cameraRates[0],cameraRates[1],player.evade_active()!=mgo2mt::player::Evade::none?std::optional<float>(evadeMovementYaw):gekkoActive?std::optional<float>(navigation.yaw()):std::nullopt};
     if(mountedCurrent||flightCurrent||mountedInput.pending()||ladderCurrent||ladderInput.pending())navigation.rotate_view({0,0,cameraMotion[0],cameraMotion[1],0,cameraRates[0],cameraRates[1]},navigationSeconds);
     else if(coverState.attached)navigation.advance_cover(*navigationWorld,move,navigationSeconds,coverState);
     else if(!gekkoActive||hostPlayer->specialPc.action==mgo2mt::special_pc::Action::none)navigation.advance(*navigationWorld,move,navigationSeconds);
     else navigation.rotate_view({0,0,cameraMotion[0],cameraMotion[1],0,cameraRates[0],cameraRates[1]},navigationSeconds);
     coverState=mgo2mt::combat::cover::evaluate(*navigationWorld,navigation.feet(),navigation.capsule(),navigation.yaw(),coverState,int8_t(player.lean),player.firstPerson);
     if(coverState.attached){
      coverLeft=mgo2mt::combat::cover::evaluate(*navigationWorld,navigation.feet(),navigation.capsule(),navigation.yaw(),coverState,-1,true).lean==-1;
      coverRight=mgo2mt::combat::cover::evaluate(*navigationWorld,navigation.feet(),navigation.capsule(),navigation.yaw(),coverState,1,true).lean==1;
     }
    }else if(!active){if(!combatPlayable)coverState={};coverInput.cancel();}
    if(mountedCurrent)if(auto*i=mountedRegistry.find(mountedMap,mountedCurrent))if(auto*t=mountedRegistry.find(i->type)){
     float yaw=navigation.yaw(),pitch=navigation.pitch();mgo2mt::mounted::clamp_aim(*i,*t,yaw,pitch);
     auto feet=hostPlayer->pose.feet;bool blocked=false;
     if(t->kind!=mgo2mt::mounted::Kind::catapult)for(const auto&next:mgo2mt::mounted::operator_path(*i,*t,hostPlayer->pose.yaw,yaw)){
      auto probe=next;probe[1]+=12;auto floor=navigationWorld->ray(probe,{0,-1,0},32);if(!floor||std::abs(floor->normal[1])<.7f){blocked=true;break;}
      std::array<float,3>delta{};for(unsigned n=0;n<3;++n)delta[n]=next[n]-feet[n];
      for(const auto&collision:{navigationWorld,stageResult.objectHitCollision})if(collision){if(!collision->clear(next,hostPlayer->pose.capsule,mgo2mt::stage::query::player)){blocked=true;break;}if(auto hit=collision->sweep(feet,delta,hostPlayer->pose.capsule,mgo2mt::stage::query::player);hit&&hit->fraction<.9999f){blocked=true;break;}}
      for(const auto&other:combatState->players)if(other&&other->alive&&other->identity!=hostPlayer->identity){const auto&q=other->pose;if(next[1]>=q.feet[1]+q.capsule.height||q.feet[1]>=next[1]+hostPlayer->pose.capsule.height)continue;const float length=delta[0]*delta[0]+delta[2]*delta[2],along=length>0?std::clamp(((q.feet[0]-feet[0])*delta[0]+(q.feet[2]-feet[2])*delta[2])/length,0.f,1.f):0;if(std::hypot(feet[0]+delta[0]*along-q.feet[0],feet[2]+delta[2]*along-q.feet[2])<q.capsule.radius+hostPlayer->pose.capsule.radius){blocked=true;break;}}
      if(blocked)break;feet=next;
     }
     if(blocked){yaw=hostPlayer->pose.yaw;feet=hostPlayer->pose.feet;}
     navigation.authoritative(*navigationWorld,feet,yaw,pitch,hostPlayer->pose.capsule);player.bodyYaw=yaw;player.forward=player.right=player.speed=0;player.running=false;player.aiming=true;
    }
    if(flightCurrent){auto visible=flightPosition.update(combatEpoch,*hostPlayer,navigationNow);auto delta=visible;for(unsigned i=0;i<3;++i)delta[i]-=navigation.feet()[i];if(auto hit=navigationWorld->sweep(navigation.feet(),delta,hostPlayer->pose.capsule);hit&&hit->fraction<.9999f)visible=hostPlayer->pose.feet;navigation.authoritative(*navigationWorld,visible,navigation.yaw(),navigation.pitch(),hostPlayer->pose.capsule);player.bodyYaw=hostPlayer->pose.yaw;player.forward=player.right=player.speed=0;player.running=player.aiming=player.firstPerson=false;}
    if(ragdoll.active())ragdoll.step(*navigationWorld,std::min(navigationSeconds,.1f));
    if(active&&testBody){testBody->step(*navigationWorld,std::min(navigationSeconds,.1f));auto model=physics_capsule(*testBody);if(bodyRenderer)bodyRenderer->update_vertices(context.Get(),model.vertices);}
    if(!gekkoActive&&std::hypot(player.forward,player.right)>.01f&&!player.dead&&player.evade_active()==mgo2mt::player::Evade::none){float angle=mgo2mt::source_movement_angle(player.right,player.forward);if(player.stance==mgo2mt::player::Stance::prone&&player.forward<0)angle=mgo2mt::source_movement_angle(-player.right,-player.forward);player.bodyYaw=navigation.yaw()+angle;}
    if((player.aiming||player.firstPerson)&&player.stance!=mgo2mt::player::Stance::prone)player.bodyYaw=navigation.yaw();
    if(coverState.attached)player.bodyYaw=std::remainder(coverState.normalYaw+3.14159265359f,6.283185307f);
    if(gekkoActive&&hostPlayer->specialPc.action!=mgo2mt::special_pc::Action::none)player.bodyYaw=hostPlayer->pose.yaw;
    if(player.menu==mgo2mt::player::Menu::weapons){login->message(window.handle,WM_KEYDOWN,VK_F4,0);}
    else if(player.menu==mgo2mt::player::Menu::settings&&login&&login->stage_load_request())Window::proc(window.handle,WM_KEYDOWN,VK_F9,1LL<<25);else if(player.menu!=mgo2mt::player::Menu::none)playerMenu.open(player.menu);
    combatInputActive=combatPlayable&&active&&!ragdoll.active()&&player.menu==mgo2mt::player::Menu::none;
    combatReloadPending|=combatInputActive&&player.reloadStarted;
    combatFirePending|=combatInputActive&&player.firePressed;
    if(combatInputActive&&player.firePressed&&hostPlayer)weaponEffectAudio.click(*hostPlayer,navigationNow);
    if(combatInputActive&&player.specialRequested){specialInput.press(navigationNow);combatReloadPending=combatFirePending=false;}
    auto lockRoster=login?login->room_host_roster():std::nullopt;
    if(combatInputActive&&combatOffer&&combatState&&lockRoster&&requestedStage){
     mgo2mt::player_lock::Input lockInput{login&&login->room_auto_aim()&&hostPlayer&&mgo2mt::gameplay::auto_aim(gameplay,hostPlayer->weapon)&&player.autoAim&&player.aiming&&!player.firstPerson&&!player.dead,
      true,combatOffer->self,combatOffer->epoch,stageResult.generation,requestedStage->rotation.rule,shotEye(),navigation.direction()};
     auto locked=playerLock.current()?playerLock.update(*combatState,*lockRoster,lockInput,*queryWorld,stageResult.objectHitCollision.get()):
      playerLock.acquire(*combatState,*lockRoster,lockInput,*queryWorld,stageResult.objectHitCollision.get());
     if(locked){if(navigation.track_view(locked->aimPoint,navigationSeconds))player.bodyYaw=navigation.yaw();else playerLock.clear();}
    }else playerLock.clear();
    auto eye=shotEye(),direction=navigation.direction();
    if(ragdoll.active()){eye=ragdoll.root_position();eye[1]+=700;}
    if(!player.firstPerson){auto target=eye;auto offset=direction;float length=gekkoActive?7800.f:player.aiming?1400.f:2600.f;for(int i=0;i<3;++i)offset[i]*=-length;offset[0]+=mgo2mt::source_screen_x*std::cos(navigation.yaw())*280;offset[2]-=mgo2mt::source_screen_x*std::sin(navigation.yaw())*280;
     float distance=std::sqrt(offset[0]*offset[0]+offset[1]*offset[1]+offset[2]*offset[2]);auto ray=offset;for(auto&v:ray)v/=distance;
     if(auto hit=queryWorld->ray(target,ray,distance+100,mgo2mt::stage::query::camera))distance=std::max(0.f,hit->distance-100.f);
     for(int i=0;i<3;++i)eye[i]=target[i]+ray[i]*distance;
     if(distance>1)for(int i=0;i<3;++i)direction[i]=-ray[i];
    }
    if(reticleVisible())direction=weaponRecoil.direction(direction);else weaponRecoil.reset();
    const std::array<uint64_t,4> viewScope{stageResult.generation,combatEpoch,combatLife,uint64_t(gekkoActive||(hostPlayer&&!hostPlayer->alive))};
    if(firstPersonScope!=viewScope){firstPersonTransition.reset();firstPersonSight.reset();firstPersonScope=viewScope;}
    const float aspect=viewport.Width/viewport.Height;
    const float lens=player.firstPerson&&!gekkoActive?mgo2mt::original_first_person::vertical_fov_degrees(hostPlayer?hostPlayer->weapon:25,hostPlayer&&hostPlayer->verifiedSkills?hostPlayer->hawkeyeLevel:0,aspect,mgo2mt::original_first_person::scope_capable(hostPlayer?hostPlayer->weapon:25),false)*.0174532925199433f:1.f;
    // The current frame's posed CNP points are available after avatar update.
    // Keep a baseline here; finalize sight placement and travel below it.
    stageCamera=mgo2mt::WorldView{eye,direction,aspect,lens};
    auto motion=player.motion();if(motion!=lastMotion){lastMotion=motion;motionSeconds=0;}else if(active||(combatPlayable&&motion==mgo2mt::player::Motion::reload))motionSeconds+=std::min(.1f,navigationSeconds)*(combatPlayable&&motion==mgo2mt::player::Motion::reload?mgo2mt::original::rifle_reload_rates[hostPlayer->reloadLevel]:1.f);
    if(combatPlayable&&hostPlayer->reloadUntil)motionSeconds=reloadPresentation[hostPlayer->identity.slot].seconds();
   }else{firstPersonTransition.reset();firstPersonSight.reset();navigationArmed=false;player.suspend();movementRunning=false;if(!login||!login->character_visible())playerMenu.close();ragdoll.stop();testBody.reset();bodyRenderer.reset();Window::testRagdoll=Window::testBody=false;}
   waterEffects.update(combatPlayable?combatEpoch:stageResult.generation,combatPlayable?combatLife:1,navigation.feet(),navigation.water_state(),navigation.ready()&&navigationWorld&&inputActive&&!playerMenu.visible()&&(!hostPlayer||hostPlayer->alive)&&(combatPlayable||Window::inspection),navigationSeconds);
   if(!combatInputActive){mountedInput.cancel();gekkoLocomotion.reset();specialPcInput.cancel();coverInput.cancel();combatReloadPending=combatFirePending=false;specialInput.clear();evadeInput.cancel();evadeAcknowledged=false;predictedEvadeSerial=0;playerLock.clear();}
   const bool inventoryPose=combatPlayable&&inputActive&&(playerMenu.inventory_menu()||playerMenu.hold_blocks_gameplay()||inventoryPending)&&hostPlayer&&hostPlayer->alive&&!hostPlayer->stunned;
   if(combatPlayable&&navigation.ready()&&(((combatInputActive||inventoryPose||Window::debug.enabled||sentDebugPhysics!=Window::debug.enabled)&&navigationNow>=combatSendAt)||(!combatInputActive&&combatInputWasActive))){
    mgo2mt::combat::wire::Input input;input.epoch=combatEpoch;input.life=combatLife;input.sequence=++combatSequence;input.pose={navigation.feet(),player.evade_active()!=mgo2mt::player::Evade::none?player.bodyYaw:navigation.yaw(),navigation.pitch(),navigation.capsule()};input.weapon=hostPlayer->weapon;input.suspended=!combatInputActive&&!inventoryPose;input.reload=hostPlayer->weapon&&combatReloadPending;input.fire=(hostPlayer->weapon||mountedCurrent)&&combatInputActive&&player.triggerHeld&&!input.reload;input.firePressed=(hostPlayer->weapon||mountedCurrent)&&combatFirePending&&!input.reload;
    input.specialPressed=combatInputActive&&specialInput.edge();input.specialHeld=combatInputActive&&player.specialHeld;
    input.cover=coverInput.intent(player.lean,player.firstPerson,combatInputActive);
    input.specialPc=specialPcInput.intent(combatInputActive&&gekkoActive);
    input.mounted=mountedInput.intent(combatInputActive);
    input.ladder=ladderInput.intent(ladderCurrent,ladderAxis,combatInputActive&&!gekkoActive);
    if(ladderCurrent||ladderInput.pending()){input.pose=hostPlayer->pose;input.fire=input.firePressed=input.reload=input.specialPressed=input.specialHeld=false;input.cover={};input.evadeKind=mgo2mt::combat::EvadeKind::none;input.evadeRequest=0;}
    if(gekkoActive){input.pose.yaw=navigation.yaw();if(hostPlayer->specialPc.action!=mgo2mt::special_pc::Action::none){input.pose.yaw=hostPlayer->pose.yaw;input.pose.pitch=hostPlayer->pose.pitch;}if(specialPcInput.pending()||hostPlayer->specialPc.action!=mgo2mt::special_pc::Action::none)input.fire=input.firePressed=false;input.reload=input.specialPressed=input.specialHeld=false;input.cover={};input.evadeKind=mgo2mt::combat::EvadeKind::none;input.evadeRequest=0;}
    if(coverState.attached&&!coverState.lean)input.fire=input.firePressed=false;
    if(input.cover.request){input.fire=input.firePressed=input.reload=input.specialPressed=input.specialHeld=false;}
    if(combatInputActive){evadeInput.apply(input);if(input.evadeRequest)specialInput.clear();}
    if(input.specialPressed||input.specialHeld||player.evade_active()!=mgo2mt::player::Evade::none){input.reload=input.fire=input.firePressed=false;}
    input.aiming=(player.aiming||player.firstPerson)&&input.weapon&&!input.suspended&&!input.reload&&!input.specialPressed&&!input.specialHeld&&!input.evadeRequest&&!input.cover.request&&!input.specialPc.request&&input.ladder==mgo2mt::ladder::Intent{};
    if(!mountedCurrent&&!flightCurrent)mgo2mt::combat::select_attack(input,player.aiming,player.firstPerson,gekkoActive,coverState.attached);
    if(mountedCurrent||mountedInput.pending()){input.reload=input.meleePressed=input.specialPressed=input.specialHeld=false;input.cover={};input.specialPc={};input.ladder={};input.evadeKind=mgo2mt::combat::EvadeKind::none;input.evadeRequest=0;if(mountedCurrent){input.pose.feet=hostPlayer->pose.feet;input.pose.capsule=hostPlayer->pose.capsule;input.aiming=input.weapon&&!input.suspended;}}
    if(flightCurrent){input.pose=hostPlayer->pose;input.fire=input.firePressed=input.reload=input.aiming=input.meleePressed=input.specialPressed=input.specialHeld=false;input.cover={};input.specialPc={};input.ladder={};input.mounted={};input.evadeKind=mgo2mt::combat::EvadeKind::none;input.evadeRequest=0;}
    if(input.mounted.action!=mgo2mt::mounted::Action::none){input.fire=input.firePressed=input.reload=input.aiming=input.meleePressed=input.specialPressed=input.specialHeld=false;input.cover={};input.specialPc={};input.ladder={};input.evadeKind=mgo2mt::combat::EvadeKind::none;input.evadeRequest=0;}
    input.debugPhysics=Window::debug.enabled;login->combat_input(input);sentDebugPhysics=input.debugPhysics;specialInput.sent(input.sequence);ladderInput.sent(input.sequence);mountedInput.sent(input.sequence);combatReloadPending=combatFirePending=false;combatSendAt=navigationNow+50;
   }
   combatInputWasActive=combatInputActive;
   auto stageAppearance=login?login->stage_appearance():std::nullopt;
   const auto nextAvatarKind=gekkoActive?mgo2mt::special_pc::Kind::gekko:mgo2mt::special_pc::Kind::human;
   if(stageAppearance!=avatarAppearance||avatarKind!=nextAvatarKind){ragdoll.stop();avatarBlend.reset();avatarFootIk.reset();avatarGroundPose.reset();avatarWasDrawn=false;++avatarModelGeneration;avatarAppearance=stageAppearance;avatarKind=nextAvatarKind;avatarRenderer.reset();avatarArmsRenderer.reset();avatarFadeRenderer.reset();avatarWeapon.clear();avatar={};
    auto* catalog=gekkoActive?gekkoCatalog.get():characterCatalog.get();if(catalog&&stageAppearance){avatar=catalog->assemble(gekkoActive?std::array<uint8_t,28>{}:*stageAppearance);if(avatar.ready())avatarRenderer=std::make_unique<mgo2mt::CharacterRenderer>(device.Get(),avatar.model);}}
   // Prepare both original-mesh subsets before the first camera toggle.
   if(avatarRenderer&&!gekkoActive&&characterCatalog&&!avatarArmsRenderer){auto arms=mgo2mt::weapon_hand::first_person_arms(avatar,characterCatalog->skeleton(avatar.gender));auto remainder=mgo2mt::weapon_hand::first_person_body(avatar,characterCatalog->skeleton(avatar.gender));if(!arms.indices.empty())avatarArmsRenderer=std::make_unique<mgo2mt::CharacterRenderer>(device.Get(),arms);if(!remainder.indices.empty())avatarFadeRenderer=std::make_unique<mgo2mt::CharacterRenderer>(device.Get(),remainder);}
   auto* avatarCatalog=gekkoActive?gekkoCatalog.get():characterCatalog.get();
   motionMissing=false;
   auto remoteRoster=login?login->room_host_roster():std::nullopt;
   if(combatViewing&&stageCamera&&combatOffer&&combatState&&remoteRoster){remoteScene.update(*combatState,*remoteRoster,combatOffer->self,navigationNow);remoteAvatars=remoteScene.sample(navigationNow);}
   else{remoteScene.clear();remoteAvatars.clear();}
   // Original MTSQ event clocks and bone positions; native dry floor and tableId0 policy.
   auto stepSounds=[&](mgo2mt::combat::footsteps::Timeline& timeline,const mgo2mt::PreparedCharacter& body,
       mgo2mt::PlayerMotion action,double seconds,mgo2mt::stage::Vec3 feet,float yaw,uint64_t actor,uint64_t life,bool eligible,bool grounded){
    const auto* clip=playerMotions?playerMotions->find(action):nullptr;
    if(!clip||!navigationWorld||!stageResult.request||!clip->loop||clip->fps!=60||
       (clip->sourceIndex==9?clip->frames!=60:clip->sourceIndex==10?clip->frames!=40:true)){timeline.reset();return;}
    auto material=mgo2mt::combat::footsteps::dry_floor(*navigationWorld,feet,navigationWater.get());
    auto events=timeline.advance({combatOffer?combatOffer->epoch:1,stageResult.generation,actor,life,clip->sourceKey,clip->sourceIndex,seconds,grounded&&bool(material),eligible});
    for(unsigned i=0;i<events.count;++i){const auto& event=events.values[i];auto bone=body.bone_position(event.bone);if(!bone||!material)continue;
     auto point=mgo2mt::combat::footsteps::world_bone(*bone,feet,yaw);auto cue=mgo2mt::combat::material_audio::resolve(mgo2mt::combat::material_audio::stage_for_map(stageResult.request->rotation.map),event.cue,material->id);
     if(point&&cue&&*cue)combatEffects.play_cue(*cue,*point,navigation.eye(),playCombatSound);
    }
   };
   auto gekkoStep=[&](const mgo2mt::PreparedCharacter& body,const mgo2mt::special_pc::GekkoSample* sample,uint8_t slot,uint64_t life,mgo2mt::stage::Vec3 feet,float yaw,bool eligible){
    if(slot>=24)return;auto origin=feet;origin[1]+=mgo2mt::special_pc::gekko_model_feet_offset;
    bool ground=false;if(navigationWorld){auto ray=feet;ray[1]+=50;auto hit=navigationWorld->ray(ray,{0,-1,0},100);ground=hit&&hit->normal[1]>.7f&&std::abs(hit->position[1]-feet[1])<20;}
    if(!gekkoFootsteps[slot].update((combatOffer&&combatState&&combatState->players[slot]?mgo2mt::stage::water_surface_identity(slot,combatState->players[slot]->identity.instance,combatState->players[slot]->identity.character)^combatOffer->epoch:0),life,sample?sample->sourceKey:0,sample?sample->phaseSeconds:0,eligible&&ground))return;
    auto l=body.bone_position(0x1B7DF1),r=body.bone_position(0xFB7DF0);if(!l||!r)return;auto pos=mgo2mt::combat::footsteps::world_bone((*l)[1]<(*r)[1]?*l:*r,origin,yaw);if(!pos)return;
    const auto path=networkKeys.parent_path()/"special/gekko_step.wav";if(!std::filesystem::is_regular_file(path))return;auto delta=mgo2mt::enemy_tag::sub(*pos,navigation.eye());float gain=.65f*std::clamp(1.f-std::sqrt(mgo2mt::enemy_tag::dot(delta,delta))/30000.f,0.f,1.f);if(gain>0)playCombatSound({0,path,gain,*pos});
   };
   std::array<bool,24> remotePresent{};
   if(coverRemoteEpoch!=(combatOffer?combatOffer->epoch:0)){coverRemoteEpoch=combatOffer?combatOffer->epoch:0;coverActorKeys={};coverActorLives={};}
   for(const auto&remote:remoteAvatars){auto slot=remote.identity.slot;remotePresent[slot]=true;auto&cached=remoteModels[slot];
    const auto coverActorKey=mgo2mt::stage::water_surface_identity(slot,remote.identity.instance,remote.identity.character);
    if(coverActorKeys[slot]!=coverActorKey||coverActorLives[slot]!=remote.life){remoteCoverTimelines[slot].clear();remoteFreeLeans[slot].clear();coverActorKeys[slot]=coverActorKey;coverActorLives[slot]=remote.life;}
    if(!characterCatalog||!playerMotions){cached.reset();remoteFootsteps[slot].reset();continue;}
    const bool remoteGekko=remote.specialPc.kind==mgo2mt::special_pc::Kind::gekko;auto* remoteCatalog=remoteGekko?gekkoCatalog.get():characterCatalog.get();
    if(!remoteCatalog){cached.reset();continue;}
    if(!cached||cached->appearance!=remote.appearance||cached->kind!=remote.specialPc.kind){remoteCoverTimelines[slot].clear();remoteFreeLeans[slot].clear();RemoteModel model;model.kind=remote.specialPc.kind;model.appearance=remote.appearance;model.body=remoteCatalog->assemble(remoteGekko?std::array<uint8_t,28>{}:remote.appearance);
     if(model.body.ready()&&(remoteGekko||(!model.body.missingModels&&!model.body.missingColors)))model.renderer=std::make_unique<mgo2mt::CharacterRenderer>(device.Get(),model.body.model);cached=std::move(model);}
    if(remoteGekko){
     cached->weapon.clear();cached->visible=false;if(cached->renderer){
      const auto action=gekkoMotion(remote.specialPc.action,remote.motion==mgo2mt::PlayerMotion::Walk||remote.motion==mgo2mt::PlayerMotion::Run,remote.motion==mgo2mt::PlayerMotion::Run);
      const double seconds=remote.specialPc.action==mgo2mt::special_pc::Action::none?remote.seconds:remote.specialPcSeconds;
      auto sampled=gekkoSample(remote.specialPc.action,action,seconds);auto pose=sampled?remoteCatalog->complete_pose(0,sampled->pose):remoteCatalog->sample_pose(0,seconds);
      const uint64_t source=0x2000000000000000ull|(uint64_t(remote.specialPc.serial)<<24)|(sampled?(uint64_t(sampled->phase)<<56)|sampled->sourceKey:0x600+unsigned(action));
      auto drawOrigin=remote.origin;drawOrigin[1]+=mgo2mt::special_pc::native_gekko.modelFeetOffset;
      const mgo2mt::motion_blend::Scope scope{combatState->epoch,stageResult.generation,coverActorKey,remote.life,2};
      if(cached->hasDrawFrame&&cached->blend.matches(scope)&&!cached->blend.same_source(source))cached->blend.rebase(cached->drawOrigin,cached->drawYaw,drawOrigin,remote.yaw,remoteCatalog->skeleton(0).front().position);
      const auto& shown=cached->blend.sample(scope,source,sampled?sampled->phaseSeconds:seconds,pose,blendDelta,blendRate);cached->drawOrigin=drawOrigin;cached->drawYaw=remote.yaw;cached->hasDrawFrame=true;remoteCatalog->pose(cached->body,shown);cached->renderer->update_vertices(context.Get(),cached->body.model.vertices);cached->visible=true;gekkoStep(cached->body,sampled?&*sampled:nullptr,slot,remote.life,remote.origin,remote.yaw,combatPlayable&&remote.alive&&inputActive&&!playerMenu.visible());}
     remoteFootsteps[slot].reset();continue;
    }
    cached->visible=false;if(cached->renderer){auto pose=specialPose(cached->body.gender,remote.specialPhase,remote.specialSeconds);const bool special=bool(pose);
     auto evasion=evadeMotions&&remote.alive&&!remote.stunned?evadeMotions->sample(remote.evadeKind,remote.evadeSeconds):std::nullopt;
     auto coverPose=remoteCoverTimelines[slot].sample(coverMotions.get(),remote.cover,remote.motion==mgo2mt::PlayerMotion::CrouchIdle||remote.motion==mgo2mt::PlayerMotion::CrouchWalk,float(remote.coverMove),blendDelta);
     auto leanBase=playerMotions->sample(remote.motion==mgo2mt::PlayerMotion::Walk||remote.motion==mgo2mt::PlayerMotion::Run?remote.motion:mgo2mt::PlayerMotion::Aim,remote.seconds);
     auto leanPose=remoteFreeLeans[slot].sample(remote.alive&&!remote.stunned&&!remote.cover.attached&&leanBase?&*leanBase:nullptr,remote.cover.lean,blendDelta);
     if(leanPose)pose=leanPose;else if(coverPose)pose=coverPose->pose;else if(evasion)pose=evasion->pose;else if(!pose)pose=playerMotions->sample(remote.motion,remote.seconds);{
     const uint64_t identity=mgo2mt::stage::water_surface_identity(remote.identity.slot,remote.identity.instance,remote.identity.character);
     const auto& remotePlayer=combatState->players[slot];
     if(mountedRenderer&&remotePlayer&&remotePlayer->mountedId)if(auto held=mountedRenderer->pose(mountedMap,remotePlayer->mountedId,cached->body.gender,weaponActions.seconds(*remotePlayer,false,navigationNow)))pose=std::move(held);
     if(flightFrames[slot]){const auto&f=*flightFrames[slot];if(f.blast){if(auto airborne=mgo2mt::combat::presentation::blast_pose(*playerMotions,f))pose=std::move(airborne);}else if(mountedRenderer)if(auto airborne=mountedRenderer->action_pose(mountedMap,f.instance,cached->body.gender,f.landing?mgo2mt::PlayerMotion::Roll:mgo2mt::PlayerMotion::Run,f.seconds))pose=std::move(airborne);}
     
     if(cached->death.scope(cached->corpse,combatState->epoch,stageResult.generation,remote.identity,remote.life)){cached->blend.reset();cached->footIk.reset();cached->groundPose.reset();cached->hasDrawFrame=false;}
     if(remotePlayer&&navigationWorld){auto seed=cached->groundPose?*cached->groundPose:cached->blend.pose()?*cached->blend.pose():*playerMotions->sample(mgo2mt::PlayerMotion::Idle,0);cached->death.update(cached->corpse,*characterCatalog,cached->body.gender,seed,remote.origin,remote.yaw,*remotePlayer,*combatState,frameCombatEvents,navigationNow);}
     if(cached->corpse.active()&&navigationWorld){cached->corpse.step(*navigationWorld,std::min(navigationSeconds,.1f));pose=mgo2mt::motion_blend::local_physics_pose(cached->corpse.pose(),remote.yaw);}
     const auto corpseOrigin=cached->corpse.active()?cached->corpse.origin():remote.origin;
     const auto reloadSeconds=remotePlayer&&remotePlayer->reloadUntil?reloadPresentation[slot].seconds():remote.seconds;

     auto hand=handMotions&&remotePlayer&&remote.alive&&!remote.stunned&&!remotePlayer->mountedId&&!flightFrames[slot]&&!special&&!evasion?mgo2mt::gameplay::hand(gameplay,handMotions.get(),remotePlayer->weapon,remotePlayer->reloadUntil?mgo2mt::PlayerMotion::Reload:remote.motion,reloadSeconds,remotePlayer->aiming,weaponActions.seconds(*remotePlayer,false,navigationNow),weaponActions.seconds(*remotePlayer,true,navigationNow),remotePlayer->pose.capsule.height==560?2:remotePlayer->pose.capsule.height==1100?1:0):std::nullopt;
     if(hand&&pose&&!coverPose&&!leanPose)mgo2mt::weapon_hand::upper_body(*pose,*hand,characterCatalog->skeleton(cached->body.gender));
     auto complete=pose?characterCatalog->complete_pose(cached->body.gender,*pose):characterCatalog->sample_pose(cached->body.gender,remote.seconds);
     const mgo2mt::motion_blend::Scope remoteScope{combatState->epoch,stageResult.generation,identity,remote.life,cached->body.gender};
     const uint64_t remoteFlightSource=flightFrames[slot]?((uint64_t(flightFrames[slot]->instance)|0x8000ull)<<48)|((flightFrames[slot]->landing?0x4000:0x2000)|(flightFrames[slot]->blast?0x8000:0)):0;
     const uint64_t remoteSource=remoteFlightSource|(uint64_t(remotePlayer?remotePlayer->mountedId:0)<<48)|(uint64_t(hand?hand->weapon:0)<<32)|(uint64_t(hand?hand->index+1:0)<<40)|(cached->corpse.active()?0x300:leanPose?0x500+unsigned(remoteFreeLeans[slot].side()+1):coverPose?0x400+unsigned(coverPose->action):evasion?(0x1000000000000000ull|(uint64_t(remote.evadeSerial)<<8)|unsigned(evasion->phase)):special?0x100+unsigned(remote.specialPhase):pose?1+unsigned(remote.motion):0x200);
     // Preserve the displayed world pose when a side-roll changes its body frame,
     // then blend to the same original forward clip in the new heading.
     if(cached->hasDrawFrame&&cached->blend.matches(remoteScope)&&!cached->blend.same_source(remoteSource))cached->blend.rebase(cached->drawOrigin,cached->drawYaw,corpseOrigin,remote.yaw,characterCatalog->skeleton(cached->body.gender).front().position);
     const auto& displayed=cached->blend.sample(remoteScope,remoteSource,coverPose?remoteCoverTimelines[slot].seconds():evasion?remote.evadeSeconds:special?remote.specialSeconds:hand?hand->seconds:remote.seconds,complete,blendDelta,blendRate);
     cached->drawOrigin=corpseOrigin;cached->drawYaw=remote.yaw;cached->hasDrawFrame=true;
     cached->groundPose=cached->footIk.solve(*characterCatalog,cached->body,cached->corpse.active()?complete:displayed,queryWorld.get(),corpseOrigin,remote.yaw,blendDelta,
       graphics->active.footIk&&remote.alive&&!remote.stunned&&!cached->corpse.active()&&!special&&!evasion&&!coverPose&&remotePlayer&&!remotePlayer->ladderAnchor&&!remotePlayer->mountedId&&!flightFrames[slot]&&remotePlayer->pose.capsule.height>560&&mgo2mt::foot_ik::eligible(remote.motion)&&mgo2mt::foot_ik::grounded(navigationWorld.get(),corpseOrigin,remotePlayer->pose.capsule),remoteScope);
     characterCatalog->pose(cached->body,*cached->groundPose);if(remotePlayer&&remotePlayer->mountedId){if(auto*i=mountedRegistry.find(mountedMap,remotePlayer->mountedId))if(auto*t=mountedRegistry.find(i->type))mgo2mt::mounted::operator_pitch(cached->body,characterCatalog->skeleton(cached->body.gender),*t,remotePlayer->pose.pitch);}if(remotePlayer&&!remotePlayer->mountedId&&!flightFrames[slot]&&remotePlayer->aiming&&!cached->corpse.active()&&!special&&!evasion&&!remotePlayer->reloadUntil)mgo2mt::weapon_hand::aim_pitch(cached->body,characterCatalog->skeleton(cached->body.gender),remotePlayer->pose.pitch);cached->renderer->update_vertices(context.Get(),cached->body.model.vertices);cached->visible=true;
     if(hand&&hand->cqc)cached->weapon.hide();else if(hand&&heldModels)cached->weapon.update(device.Get(),context.Get(),*heldModels,cached->body,*hand,blendDelta,blendRate);else cached->weapon.clear();
     stepSounds(remoteFootsteps[slot],cached->body,remote.motion,remote.seconds,remote.origin,remote.yaw,mgo2mt::stage::water_surface_identity(remote.identity.slot,remote.identity.instance,remote.identity.character),remote.life,
       pose&&!coverPose&&!evasion&&combatPlayable&&inputActive&&!playerMenu.visible()&&!musicMenu.visible()&&remote.alive&&!remote.stunned,true);
    }}if(!cached->visible)remoteFootsteps[slot].reset();
   }
   for(unsigned i=0;i<24;++i)if(!remotePresent[i]){remoteModels[i].reset();remoteCoverTimelines[i].clear();remoteFreeLeans[i].clear();coverActorKeys[i]=0;remoteFootsteps[i].reset();}
   // Native water presentation uses current admitted identities/lives. No
   // snapshot cue/path can start a sound. Effects disappear on scope loss.
   {
    std::vector<mgo2mt::water_audio::Actor> actors;
    const bool waterActive=combatPlayable&&combatState&&combatOffer&&hostPlayer&&combatState->epoch==combatOffer->epoch&&requestedStage==stageResult.request&&navigationWorld&&inputActive&&!playerMenu.visible()&&!musicMenu.visible();
    if(waterActive&&hostPlayer->alive&&navigation.ready())actors.push_back({mgo2mt::stage::water_surface_identity(hostPlayer->identity.slot,hostPlayer->identity.instance,hostPlayer->identity.character),hostPlayer->life,navigation.feet(),navigation.water_state(),true,navigation.grounded()});
    std::set<uint64_t> presentWater;
    if(waterActive)for(const auto& remote:remoteAvatars){
     const auto slot=remote.identity.slot;if(slot>=24||!remote.alive||!remoteModels[slot]||!remoteModels[slot]->visible)continue;
     const auto& p=combatState->players[slot];if(!p||p->identity!=remote.identity||p->life!=remote.life||!p->alive)continue;
     const uint64_t identity=mgo2mt::stage::water_surface_identity(remote.identity.slot,remote.identity.instance,remote.identity.character);
     auto wet=mgo2mt::water_gameplay::sample(navigationWater.get(),*navigationWorld,remote.origin,p->pose.capsule);
     presentWater.insert(identity);remoteWaterEffects[identity].update(combatOffer->epoch,remote.life,remote.origin,wet,true,navigationSeconds);
     actors.push_back({identity,remote.life,remote.origin,wet,true,wet.level&&std::abs(remote.origin[1]-wet.floorY)<10});
    }
    std::erase_if(remoteWaterEffects,[&](const auto& entry){return !presentWater.contains(entry.first);});
    auto events=waterSteps.update({waterActive?combatOffer->epoch:0,stageResult.generation},actors,navigationNow,waterActive);
    std::erase_if(waterVoices,[&](const auto& voice){return !waterActive||voice.epoch!=combatOffer->epoch||voice.scene!=stageResult.generation||voice.audio->result.load()!=-1||std::none_of(actors.begin(),actors.end(),[&](const auto& actor){return actor.identity==voice.identity&&actor.life==voice.life&&mgo2mt::water_audio::valid_wet(actor);});});
    if(sound)for(const auto& event:events)if(waterVoices.size()<8)if(auto step=mgo2mt::water_audio::make_sound(event,networkKeys.parent_path()/"sfx"/mgo2mt::water_audio::filename,navigation.eye())){
     auto voice=std::make_unique<AudioThread>();voice->control.gain=step->gain;voice->start(step->file,L"30",0,"native_water_step");waterVoices.push_back({event.identity,event.life,combatOffer->epoch,stageResult.generation,std::move(voice)});
    }
   }
   // Finite AA water surfaces are cosmetic contacts, separate from GWW depth.
   {std::vector<mgo2mt::stage::WaterSurfaceActor> actors;
    const bool eligible=combatPlayable&&combatState&&combatOffer&&hostPlayer&&hostPlayer->identity==combatOffer->self&&combatState->epoch==combatOffer->epoch&&stageResult.request==requestedStage&&inputActive&&!playerMenu.visible()&&!musicMenu.visible();
    if(eligible&&hostPlayer&&hostPlayer->alive&&navigation.ready()){auto p=navigation.feet();p[1]+=hostPlayer->pose.capsule.height*.5f;actors.push_back({mgo2mt::stage::water_surface_identity(hostPlayer->identity.slot,hostPlayer->identity.instance,hostPlayer->identity.character),hostPlayer->life,p});}
    if(eligible)for(const auto& remote:remoteAvatars){const auto slot=remote.identity.slot;if(slot>=24||!remote.alive||!remoteModels[slot]||!remoteModels[slot]->visible)continue;const auto& p=combatState->players[slot];if(!p||p->identity!=remote.identity||p->life!=remote.life||!p->alive)continue;auto point=remote.origin;point[1]+=p->pose.capsule.height*.5f;actors.push_back({mgo2mt::stage::water_surface_identity(remote.identity.slot,remote.identity.instance,remote.identity.character),remote.life,point});}
    waterContacts.update({eligible?combatOffer->epoch:0,stageResult.generation},actors,contactWater.get(),navigationNow,eligible);
   }
   bool physicsInputActive=inputActive&&!Window::debug.confirmReset&&!playerMenu.visible()&&!musicMenu.visible()&&Window::debug.enabled&&Window::inspection&&requestedStage==stageResult.request;
   if(Window::testBody&&physicsInputActive&&stageCamera&&navigationWorld){
    mgo2mt::physics::RigidBody body;body.radius=150;body.halfLength=200;body.mass=10;
    auto direction=navigation.direction();body.position=navigation.eye();for(int j=0;j<3;++j){body.position[j]+=direction[j]*900;body.velocity[j]=direction[j]*2400;}body.velocity[1]+=1200;body.angularVelocity={1.5f,.5f,2.f};
    auto segment=body.segment();if(navigationWorld->contacts(segment.first,segment.second,body.radius,0).empty()){testBody=body;bodyRenderer=std::make_unique<mgo2mt::CharacterRenderer>(device.Get(),physics_capsule(body));physicsError.clear();}else physicsError=L"確認物体の配置先が壁に近すぎます。";
   }
   if(gekkoActive)avatarWeapon.clear();
   if(stageCamera&&avatarRenderer&&gekkoActive&&avatarCatalog){
    const uint64_t identity=combatOffer?mgo2mt::stage::water_surface_identity(combatOffer->self.slot,combatOffer->self.instance,combatOffer->self.character):1;
    const auto action=gekkoMotion(hostPlayer->specialPc.action,std::hypot(player.forward,player.right)>.01f,player.running);
    const auto actionTime=gekkoClock.sample(combatEpoch,hostPlayer->identity,hostPlayer->life,hostPlayer->specialPc,navigationNow);
    const double seconds=hostPlayer->specialPc.action==mgo2mt::special_pc::Action::none?motionSeconds:actionTime;
    auto sampled=gekkoSample(hostPlayer->specialPc.action,action,seconds);auto pose=sampled?avatarCatalog->complete_pose(0,sampled->pose):avatarCatalog->sample_pose(0,seconds);
    const uint64_t source=0x2000000000000000ull|(uint64_t(hostPlayer->specialPc.serial)<<24)|(sampled?(uint64_t(sampled->phase)<<56)|sampled->sourceKey:0x600+unsigned(action));
    auto drawOrigin=navigation.feet();drawOrigin[1]+=mgo2mt::special_pc::native_gekko.modelFeetOffset;
    if(avatarWasDrawn&&!avatarBlend.same_source(source))avatarBlend.rebase(avatarDrawOrigin,avatarDrawYaw,drawOrigin,player.bodyYaw,avatarCatalog->skeleton(0).front().position);
    const auto& shown=avatarBlend.sample({combatOffer?combatOffer->epoch:1,stageResult.generation,identity,hostPlayer?hostPlayer->life:1,avatarModelGeneration},source,sampled?sampled->phaseSeconds:seconds,pose,blendDelta,blendRate);
    avatarCatalog->pose(avatar,shown);avatarRenderer->update_vertices(context.Get(),avatar.model.vertices);avatarWasDrawn=true;avatarDrawOrigin=drawOrigin;avatarDrawYaw=player.bodyYaw;selfFootsteps.reset();gekkoStep(avatar,sampled?&*sampled:nullptr,hostPlayer->identity.slot,hostPlayer->life,navigation.feet(),player.bodyYaw,combatPlayable&&hostPlayer->alive&&inputActive&&!playerMenu.visible());
   }else if(stageCamera&&avatarRenderer&&characterCatalog){
    const uint64_t identity=combatOffer?mgo2mt::stage::water_surface_identity(combatOffer->self.slot,combatOffer->self.instance,combatOffer->self.character):1;
    const mgo2mt::motion_blend::Scope avatarScope{combatOffer?combatOffer->epoch:1,stageResult.generation,identity,combatViewing?hostPlayer->life:1,avatarModelGeneration};
    if(avatarWasDrawn&&!avatarBlend.matches(avatarScope)){avatarBlend.reset();avatarFootIk.reset();avatarGroundPose.reset();avatarWasDrawn=false;ragdoll.stop();}
    if(combatViewing&&hostPlayer&&navigationWorld&&playerMotions){auto seed=avatarGroundPose?*avatarGroundPose:avatarBlend.pose()?*avatarBlend.pose():*playerMotions->sample(mgo2mt::PlayerMotion::Idle,0);if(avatarDeath.update(ragdoll,*characterCatalog,avatar.gender,seed,navigation.feet(),player.bodyYaw,*hostPlayer,*combatState,frameCombatEvents,navigationNow)){player.firstPerson=player.aiming=false;}}
    auto pose=playerMotions?playerMotions->sample(mgo2mt::render_motion(player.motion()),motionSeconds):std::nullopt;
    auto evasion=evadeMotions?evadeMotions->sample(evadeKind(player.evade_active()),player.evade_elapsed()):std::nullopt;if(evasion)pose=evasion->pose;
    auto coverPose=coverTimeline.sample(coverMotions.get(),coverState,player.stance==mgo2mt::player::Stance::crouching,player.right,blendDelta);if(coverPose)pose=coverPose->pose;
    auto leanBase=playerMotions?playerMotions->sample(std::hypot(player.forward,player.right)>.01f?mgo2mt::render_motion(player.motion()):mgo2mt::PlayerMotion::Aim,motionSeconds):std::nullopt;
    auto leanPose=freeLean.sample(!coverState.attached&&!player.dead&&!player.special_active()&&!ragdoll.active()&&player.evade_active()==mgo2mt::player::Evade::none&&leanBase?&*leanBase:nullptr,coverState.lean,blendDelta);if(leanPose)pose=leanPose;
    bool specialApplied=false;if(combatPlayable)if(auto special=specialPose(avatar.gender,hostPlayer->specialPhase,specialSeconds)){pose=std::move(special);specialApplied=true;}
    if(mountedRenderer&&mountedCurrent)if(auto held=mountedRenderer->pose(mountedMap,mountedCurrent,avatar.gender,weaponActions.seconds(*hostPlayer,false,navigationNow)))pose=std::move(held);
    const auto flightFrame=hostPlayer&&hostPlayer->identity.slot<24?flightFrames[hostPlayer->identity.slot]:std::nullopt;
    if(flightFrame){const auto&f=*flightFrame;if(f.blast&&playerMotions){if(auto airborne=mgo2mt::combat::presentation::blast_pose(*playerMotions,f))pose=std::move(airborne);}else if(mountedRenderer)if(auto airborne=mountedRenderer->action_pose(mountedMap,f.instance,avatar.gender,f.landing?mgo2mt::PlayerMotion::Roll:mgo2mt::PlayerMotion::Run,f.seconds))pose=std::move(airborne);}
    if(Window::testRagdoll&&physicsInputActive&&!avatarDeath.dead()){physicsError.clear();player.cancel_evade();evadeInput.cancel();evadeAcknowledged=false;evasion.reset();
     if(ragdoll.active()){
      auto hint=ragdoll.root_position();mgo2mt::stage::Navigation recovery({260,560,2});
      if(recovery.place(*navigationWorld,hint,10000)&&recovery.feet()[1]<=hint[1]){bool supine=ragdoll.supine();ragdoll.stop();navigation=recovery;navigation.facing(player.bodyYaw);player.knock_down(supine);motionSeconds=0;specialApplied=false;pose=playerMotions?playerMotions->sample(mgo2mt::render_motion(player.motion()),0):std::nullopt;}
      else physicsError=L"起きる場所を確保できません。F5で元の位置へ戻れます。";
     }else if(pose&&ragdoll.start(*characterCatalog,avatar.gender,avatarGroundPose?*avatarGroundPose:avatarBlend.pose()?*avatarBlend.pose():*pose,navigation.feet(),player.bodyYaw)){
      player.firstPerson=false;auto point=ragdoll.root_position();point[1]+=500;auto direction=navigation.direction();ragdoll.impulse({direction[0]*70000,30000,direction[2]*70000},point);
     }else physicsError=L"このキャラクターの物理姿勢を準備できません。";
    }
    if(ragdoll.active())pose=mgo2mt::motion_blend::local_physics_pose(ragdoll.pose(),player.bodyYaw);
    const auto drawOrigin=ragdoll.active()?ragdoll.origin():navigation.feet();
    auto hand=handMotions&&hostPlayer&&hostPlayer->alive&&!hostPlayer->stunned&&!mountedCurrent&&!flightFrame&&!specialApplied&&!evasion&&!ragdoll.active()?mgo2mt::gameplay::hand(gameplay,handMotions.get(),hostPlayer->weapon,hostPlayer->reloadUntil?mgo2mt::PlayerMotion::Reload:mgo2mt::render_motion(player.motion()),hostPlayer->reloadUntil?reloadPresentation[hostPlayer->identity.slot].seconds():motionSeconds,player.aiming||player.firstPerson,weaponActions.seconds(*hostPlayer,false,navigationNow),weaponActions.seconds(*hostPlayer,true,navigationNow),hostPlayer->pose.capsule.height==560?2:hostPlayer->pose.capsule.height==1100?1:0):std::nullopt;
    if(hand&&pose&&!coverPose&&!leanPose)mgo2mt::weapon_hand::upper_body(*pose,*hand,characterCatalog->skeleton(avatar.gender));
    auto complete=pose?characterCatalog->complete_pose(avatar.gender,*pose):characterCatalog->sample_pose(avatar.gender,motionSeconds);
    uint64_t source=(flightFrame?((uint64_t(flightFrame->instance)|0x8000ull)<<48)|((flightFrame->landing?0x4000:0x2000)|(flightFrame->blast?0x8000:0)):0)|(uint64_t(mountedCurrent)<<48)|(uint64_t(hand?hand->weapon:0)<<32)|(uint64_t(hand?hand->index+1:0)<<40)|(ragdoll.active()?0x300:leanPose?0x500+unsigned(freeLean.side()+1):coverPose?0x400+unsigned(coverPose->action):evasion?(0x1000000000000000ull|(uint64_t(localEvadeCounter)<<8)|unsigned(evasion->phase)):specialApplied?0x100+unsigned(hostPlayer->specialPhase):pose?1+unsigned(player.motion()):0x200);
    double clipTime=ragdoll.active()?double(navigationNow)/1000.:coverPose?coverTimeline.seconds():evasion?player.evade_elapsed():specialApplied?specialSeconds:hand?hand->seconds:motionSeconds;
    // Preserve the prior world pose when the next motion also changes heading,
    // including the first walking frame after an evasion's idle ending frame.
    if(avatarWasDrawn&&(avatarWasRagdoll!=ragdoll.active()||!avatarBlend.same_source(source)))avatarBlend.rebase(avatarDrawOrigin,avatarDrawYaw,drawOrigin,player.bodyYaw,characterCatalog->skeleton(avatar.gender).front().position);
    const auto& displayed=avatarBlend.sample(avatarScope,source,clipTime,complete,blendDelta,blendRate);
    avatarGroundPose=avatarFootIk.solve(*characterCatalog,avatar,ragdoll.active()?complete:displayed,queryWorld.get(),drawOrigin,player.bodyYaw,blendDelta,
      graphics->active.footIk&&navigation.grounded()&&!player.dead&&!ragdoll.active()&&!specialApplied&&!evasion&&!coverPose&&!ladderCurrent&&!mountedCurrent&&!flightCurrent&&!(hostPlayer&&hostPlayer->stunned)&&player.stance!=mgo2mt::player::Stance::prone&&mgo2mt::foot_ik::eligible(mgo2mt::render_motion(player.motion())),avatarScope);
    characterCatalog->pose(avatar,*avatarGroundPose);motionMissing=!pose;
    if(mountedCurrent){if(auto*i=mountedRegistry.find(mountedMap,mountedCurrent))if(auto*t=mountedRegistry.find(i->type))mgo2mt::mounted::operator_pitch(avatar,characterCatalog->skeleton(avatar.gender),*t,navigation.pitch());}
    if(!mountedCurrent&&!flightCurrent&&(player.aiming||player.firstPerson)&&!ragdoll.active()&&!specialApplied&&!evasion&&!(hostPlayer&&hostPlayer->reloadUntil))mgo2mt::weapon_hand::aim_pitch(avatar,characterCatalog->skeleton(avatar.gender),navigation.pitch());
    if(hand&&hand->cqc)avatarWeapon.hide();else if(hand&&heldModels)avatarWeapon.update(device.Get(),context.Get(),*heldModels,avatar,*hand,blendDelta,blendRate);else avatarWeapon.clear();
    avatarDrawOrigin=drawOrigin;avatarDrawYaw=player.bodyYaw;avatarWasRagdoll=ragdoll.active();avatarWasDrawn=true;
    avatarRenderer->update_vertices(context.Get(),avatar.model.vertices);
    if(player.firstPerson||firstPersonTransition.split_body()){if(!avatarArmsRenderer){auto arms=mgo2mt::weapon_hand::first_person_arms(avatar,characterCatalog->skeleton(avatar.gender));if(!arms.indices.empty())avatarArmsRenderer=std::make_unique<mgo2mt::CharacterRenderer>(device.Get(),arms);auto body=mgo2mt::weapon_hand::first_person_body(avatar,characterCatalog->skeleton(avatar.gender));if(!body.indices.empty())avatarFadeRenderer=std::make_unique<mgo2mt::CharacterRenderer>(device.Get(),body);}if(avatarArmsRenderer)avatarArmsRenderer->update_vertices(context.Get(),avatar.model.vertices);if(avatarFadeRenderer)avatarFadeRenderer->update_vertices(context.Get(),avatar.model.vertices);}
    if(pose)stepSounds(selfFootsteps,avatar,mgo2mt::render_motion(player.motion()),motionSeconds,navigation.feet(),player.bodyYaw,
      combatOffer?(uint64_t(combatOffer->self.instance)<<32)|combatOffer->self.character:1,combatPlayable?combatLife:1,
      combatInputActive&&inputActive&&!playerMenu.visible()&&!musicMenu.visible()&&!player.dead&&!ragdoll.active()&&!evasion&&!coverPose,navigation.grounded());
    else selfFootsteps.reset();
   }else selfFootsteps.reset();
   if(stageCamera&&navigationWorld){
    auto target=*stageCamera;
    if(player.firstPerson&&!gekkoActive&&!mountedCurrent&&!flightCurrent&&!player.dead&&!ragdoll.active()){
     const auto weapon=hostPlayer?hostPlayer->weapon:25u;
     const bool stable=hostPlayer&&hostPlayer->alive&&!hostPlayer->stunned&&!hostPlayer->reloadUntil&&player.evade_active()==mgo2mt::player::Evade::none;
     auto axis=stable?avatarWeapon.sight_axis(player.bodyYaw,navigation.feet()):std::nullopt;
     target=firstPersonSight.update(target,weapon,axis).view;
    }else firstPersonSight.reset();
    if(player.firstPerson&&mountedCurrent)if(auto*i=mountedRegistry.find(mountedMap,mountedCurrent))if(auto*t=mountedRegistry.find(i->type)){auto offset=t->eye;for(unsigned j=0;j<3;++j)offset[j]-=t->pivot[j];offset=mgo2mt::mounted::rotate(offset,navigation.yaw(),navigation.pitch()-t->bindPitch);auto pivot=mgo2mt::mounted::pivot_position(*i,*t);for(unsigned j=0;j<3;++j)target.eye[j]=pivot[j]+offset[j];}
    auto view=firstPersonTransition.update(target,player.firstPerson,navigationNow);
    auto pivot=shotEye();if(ragdoll.active()){pivot=ragdoll.root_position();pivot[1]+=700;}
    auto delta=view.eye;float length=0;for(unsigned i=0;i<3;++i){delta[i]-=pivot[i];length+=delta[i]*delta[i];}
    length=std::sqrt(length);if(length>1){for(auto&v:delta)v/=length;if(auto hit=queryWorld->ray(pivot,delta,length+100,mgo2mt::stage::query::camera)){const auto safe=std::max(0.f,hit->distance-100.f);if(safe<length)for(unsigned i=0;i<3;++i)view.eye[i]=pivot[i]+delta[i]*safe;}}
    firstPersonTransition.presented(view);stageCamera=view;
   }
   if(mortarShellRenderer)mortarShellRenderer->update(context.Get(),frameCombatEvents,combatViewing&&combatState?&*combatState:nullptr,mountedRegistry,mountedMap,queryWorld,navigationNow,stageResult.generation);
   if(combatViewing&&combatState&&combatOffer){
    auto muzzle=[&](const mgo2mt::combat::Player&p)->std::optional<std::array<float,3>>{if(p.mountedId){if(auto*i=mountedRegistry.find(mountedMap,p.mountedId))if(auto*t=mountedRegistry.find(i->type))return mgo2mt::mounted::muzzle_position(*i,*t,p.identity==combatOffer->self?navigation.yaw():p.pose.yaw,p.identity==combatOffer->self?navigation.pitch():p.pose.pitch);return {};}if(p.identity==combatOffer->self)return avatarWeapon.muzzle(player.bodyYaw,navigation.feet());if(p.identity.slot<24&&remoteModels[p.identity.slot]){const auto&m=*remoteModels[p.identity.slot];return m.weapon.muzzle(m.drawYaw,m.drawOrigin);}return {};};
    auto visualEvents=frameCombatEvents;for(auto&e:visualEvents)if(e.kind==mgo2mt::combat::EventKind::shot&&e.source.slot<24){const auto&p=combatState->players[e.source.slot];if(p&&p->identity==e.source&&p->life==e.sourceLife)if(auto point=muzzle(*p))e.position=*point;}
    for(auto&e:visualEvents)if(e.kind==mgo2mt::combat::EventKind::damage&&e.hpDamage&&e.target.slot<24){
     const auto&p=combatState->players[e.target.slot];if(!p||p->identity!=e.target||p->life!=e.targetLife)continue;
     const mgo2mt::CharacterModel* body=nullptr;std::array<float,3> origin{};float yaw=0;
     if(e.target==combatOffer->self&&avatarWasDrawn){body=&avatar.model;origin=avatarDrawOrigin;yaw=avatarDrawYaw;}
     else if(const auto&model=remoteModels[e.target.slot];model&&model->visible){body=&model->body.model;origin=model->drawOrigin;yaw=model->drawYaw;}
     if(body)if(auto point=mgo2mt::combat::body_hit::surface(e,*body,origin,yaw,std::min(6000.f,std::max(600.f,p->pose.capsule.radius*2.5f))))e.position=*point;
    }
    auto casing=[&](const mgo2mt::combat::Event&e)->std::optional<mgo2mt::combat::particles::CasingEmission>{
     if(e.source.slot>=24)return {};const auto& p=combatState->players[e.source.slot];if(!p||p->identity!=e.source||p->life!=e.sourceLife)return {};
     if(p->mountedId){if(auto*i=mountedRegistry.find(mountedMap,p->mountedId))if(auto*t=mountedRegistry.find(i->type);t&&t->ejectionDirection!=mgo2mt::stage::Vec3{}){const float yaw=e.source==combatOffer->self?navigation.yaw():p->pose.yaw,pitch=e.source==combatOffer->self?navigation.pitch():p->pose.pitch;auto position=t->ejection;for(unsigned n=0;n<3;++n)position[n]-=t->pivot[n];position=mgo2mt::mounted::rotate(position,yaw,pitch-t->bindPitch);auto pivot=mgo2mt::mounted::pivot_position(*i,*t);for(unsigned n=0;n<3;++n)position[n]+=pivot[n];return mgo2mt::combat::particles::CasingEmission{position,mgo2mt::mounted::rotate(t->ejectionDirection,yaw,pitch-t->bindPitch)};}return {};}
     const mgo2mt::weapon_hand::Actor* held=nullptr;float yaw=0;std::array<float,3> origin{};
     if(e.source==combatOffer->self){held=&avatarWeapon;yaw=player.bodyYaw;origin=navigation.feet();}
     else if(const auto& model=remoteModels[e.source.slot];model&&model->visible){held=&model->weapon;yaw=model->drawYaw;origin=model->drawOrigin;}
     if(!held||held->weapon_id()!=p->weapon)return {};auto axis=held->connection(0x443037,yaw,origin);if(!axis)return {};
     auto direction=axis->front;for(unsigned i=0;i<3;++i)direction[i]-=axis->rear[i];return mgo2mt::combat::particles::CasingEmission{axis->rear,direction};
    };
    auto originalAudio=weaponEffectAudio.dispatch(visualEvents,*combatState,navigationNow);
    combatEffects.dispatch(originalAudio,navigation.eye(),playCombatSound);
    combatLights.dispatch(visualEvents,*combatState,navigationNow);projectileParticles.dispatch(visualEvents,*combatState,navigationNow,casing);combatPointLights=combatLights.sample(navigationNow);
    if(queryWorld)for(const auto&contact:projectileParticles.casing_contacts(*combatState,navigationNow,*queryWorld))weaponEffectAudio.casing(contact,*combatState,navigationNow);
    weaponEffectAudio.reloads(*combatState,navigationNow);
    for(unsigned slot=0;slot<24;++slot)if(const auto&p=combatState->players[slot];p&&!weaponEffectAudio.replaces_reload(p->weapon))for(auto cue:reloadCues[slot]){auto origin=p->pose.feet;origin[1]+=1100;combatEffects.play_cue(cue,origin,navigation.eye(),playCombatSound);}
    weaponEffectAudio.sample(*combatState,navigationNow,navigation.eye(),combatEffects,playCombatSound);
   }
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
   mgo2mt::stage::RoundMusic roundMusic{musicChoice.track};
   auto roundMusicPhase=mgo2mt::stage::tdm_round_music_phase(musicLoadRequest,musicPreparation);
   if(auto phase=roundMusicPhase)
    roundMusic=mgo2mt::stage::resolve_round_music(musicLibrary,musicChoice.track,*phase);
   if(!hasMusicStage||!musicPlaying||!sound||!musicChoice.track||musicChoice.track->additional||musicChoice.track->id!="original:bgm_mgo_action01")roundMusicPhase.reset();
   if(roundMusicPhase!=loggedMusicPhase){
    loggedMusicPhase=roundMusicPhase;
    const bool clockKnown=musicPreparation&&musicLoadRequest&&musicPreparation->generation==musicLoadRequest->generation&&musicPreparation->runtimeReady&&musicPreparation->roundClock;
    if(roundMusicPhase)std::osyncstream(std::cout)<<"{\"round_bgm_phase\":\""<<(*roundMusicPhase==mgo2mt::stage::RoundMusicPhase::normal?"normal":*roundMusicPhase==mgo2mt::stage::RoundMusicPhase::action?"action":"urgent")<<"\",\"clock_known\":"<<(clockKnown?"true":"false")<<",\"round_remaining_ms\":"<<(clockKnown?musicPreparation->roundRemainingMs:0)<<"}"<<std::endl;
   }
   if(hasMusicStage&&musicPlaying&&sound&&roundMusic.track&&musicPlayback.select(roundMusic.track,roundMusic.alternateWave)){
    stageMusic.reset();stageMusic=std::make_unique<AudioThread>();stageMusic->control.loopWhole=true;
    stageMusic->control.alternateWave=roundMusic.alternateWave;
    stageMusic->control.layerMix=mgo2mt::stage::action01_mix;
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
   if(Window::debug.enabled&&hasStage){
     if(auto fps=debugFrameRate.fps()){wchar_t value[64]{};swprintf_s(value,L"FPS：%.1f（描画）\n",*fps);notice=value;}else notice=L"FPS：計測中…\n";
     if(graphics->active.shadowEnabled){const auto& stats=shadowRenderer.statistics();wchar_t detail[200]{};swprintf_s(detail,L"影（直前フレーム）：%u分割 / %up / %.1f MiB / %llu draw / %llu tris%s\n",stats.cascades,stats.resolution,double(stats.allocatedBytes)/(1024*1024),stats.drawCalls,stats.triangles,stats.reduced?L"（GPU上限）":L"");notice+=detail;}else notice+=L"影：OFF（Legacy）\n";
     // Use the same admitted-stage feet origin as the local character model,
     // not the eye/camera or the previous stage's last navigation position.
     if(navigation.ready()&&requestedStage==stageResult.request){
      const auto feet=navigation.feet();
      if(std::all_of(feet.begin(),feet.end(),[](float v){return std::isfinite(v);})){wchar_t position[192]{};swprintf_s(position,L"本人座標（足元・ワールド座標）\nX %.2f   Y %.2f   Z %.2f\n",double(feet[0]),double(feet[1]),double(feet[2]));notice+=position;}
      else notice+=L"本人座標：X —   Y —   Z —\n";
     }else notice+=L"本人座標：X —   Y —   Z —（未配置）\n";
     wchar_t blendNotice[192]{};swprintf_s(blendNotice,L"F4：モーションブレンド %u%%/秒（約%.3f秒）\n現在の合成率：自己 %.0f%%\n",blendSettings.percentPerSecond,blendSettings.completion_seconds(),avatarBlend.progress()*100);notice+=blendNotice;notice+=L"F5：ローカル再初期化   F10：歩行確認／全景\nF7 / F8：曲選択   F9：再生／停止\n";
     if(Window::inspection){notice+=L"F11：倒れる／操作へ戻る　Shift+F11：物体落下\n";if(ragdoll.active())notice+=L"物理姿勢：13剛体・12関節（ローカル確認）\n";if(!physicsError.empty())notice+=physicsError+L"\n";}
     if(Window::inspection){if(navigation.ready()){notice+=std::wstring(mgo2mt::player::stance_name(player))+L" / "+(player.firstPerson?L"主観":L"三人称")+L"\n";notice+=std::wstring(player.aiming?L"構え ":L"")+(player.firing?L"発射入力 ":L"")+(player.reloading()?L"リロード ":L"")+(player.autoAim?L"AUTO AIM ON":L"AUTO AIM OFF")+L"\n";if(combatPlayable)notice+=L"HP "+std::to_wstring(hostPlayer->hp)+L" / "+std::to_wstring(hostPlayer->maxHp)+L"   AMMO "+std::to_wstring(hostPlayer->ammo)+L"\n";else notice+=L"ローカル操作確認（ホストの出撃許可待ち）\n";if(motionMissing)notice+=L"この動作の原版モーションは未復旧\n";}else notice+=L"歩行確認：床・立ち位置を確認できません\n";}
     if(stageRenderer)notice+=L"LOD："+std::to_wstring(stageRenderer->lod_triangles())+L" / "+std::to_wstring(stageRenderer->original_triangles())+L" tris、簡略化 "+std::to_wstring(stageRenderer->simplified_parts())+L" 部品\n";
     notice+=stageResult.skyModel?L"天球：読込済 / F12診断diffuse（原shader未対応）\n":L"天球：選択経路未確定・未読込\n";
     if(stageResult.objectSnapshot)notice+=L"物体状態 "+std::to_wstring(stageResult.objectSnapshot->objects.size())+L"件受信済み / 世代 "+std::to_wstring(stageResult.objectSnapshot->request.generation)+L"\n";
     else if(login->scene_status()==mgo2mt::stage::SceneSyncStatus::waiting_snapshot)notice+=L"物体の初期状態を受信中…\n";
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
   environmentConfig=login&&login->environment_settings()?login->environment_settings()->config:mgo2mt::environment::Config{};
   stageEnvironment.update(displayStage,stageResult.lighting,environmentConfig);displayStage=stageEnvironment.model();environmentLighting=stageEnvironment.lighting();
   const std::array<bool,5> nextWeatherKey{environmentConfig.weatherOverride,environmentConfig.fog,environmentConfig.rain,environmentConfig.snow,environmentConfig.sandstorm};
   if(!weatherKey||*weatherKey!=nextWeatherKey){weatherKey=nextWeatherKey;weatherStartedAt=GetTickCount64();}
   if(stageModel!=displayStage){stageModel=displayStage;stageRenderer.reset();if(stageModel){try{stageRenderer=std::make_unique<mgo2mt::CharacterRenderer>(device.Get(),*stageModel);}catch(...){stageResult.status=mgo2mt::stage::Status::graphics_error;}}}
   if(skyGeneration!=stageResult.generation){skyGeneration=stageResult.generation;skyStartedAt=GetTickCount64();weatherStartedAt=skyStartedAt;weatherSurface.reset();}
   if(stageSkyModel!=stageResult.skyModel){stageSkyModel=stageResult.skyModel;stageSkyRenderer.reset();if(stageSkyModel){try{stageSkyRenderer=std::make_unique<mgo2mt::CharacterRenderer>(device.Get(),*stageSkyModel,true);}catch(...){stageResult.status=mgo2mt::stage::Status::graphics_error;}}}
   if(stageSkyModel&&!stageSkyRenderer)stageResult.status=mgo2mt::stage::Status::graphics_error;
   if(stageModel&&!stageRenderer)stageResult.status=mgo2mt::stage::Status::graphics_error;
   if(login)login->stage_feedback(stageResult);
   if(!stageModel||!login||!login->stage_request()){
    stageAudio.reset();stageAudioIndex=0;Window::stageAudition=false;lobbyAudio.control.gain=1;if(login)login->stage_audio_feedback(L"");
   }else if(environmentConfig.sound!=mgo2mt::environment::Sound::original){
    stageAudio.reset();stageAudioIndex=0;const bool requested=Window::stageAudition;Window::stageAudition=false;
    if(!environmentSound||*environmentSound!=environmentConfig.sound||requested){
     constexpr const wchar_t* names[]={L"ステージの設定",L"雨",L"吹雪",L"砂嵐",L"森",L"無音"};
     login->stage_audio_feedback(environmentConfig.sound==mgo2mt::environment::Sound::silent?L"環境音：無音":std::wstring(L"環境音（")+names[unsigned(environmentConfig.sound)]+L"）は未収録です。");
     std::osyncstream(std::cout)<<"{\"host_environment_sound\":"<<unsigned(environmentConfig.sound)<<",\"available\":"<<(environmentConfig.sound==mgo2mt::environment::Sound::silent?"true":"false")<<"}\n";
    }
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
   if(environmentSound&&*environmentSound!=environmentConfig.sound&&environmentConfig.sound==mgo2mt::environment::Sound::original&&login)login->stage_audio_feedback(L"環境音：ステージの設定");
   environmentSound=environmentConfig.sound;
   if(stageAudio&&stageAudio->result.load()!=-1){auto failed=stageAudio->result.load()!=0;stageAudio.reset();lobbyAudio.control.gain=1;stageAudioIndex=0;if(login)login->stage_audio_feedback(failed?L"環境音の再生に失敗しました。":L"試聴終了");if(failed)std::osyncstream(std::cout)<<"{\"stage_ambience_error\":true}\n";}
   lobbyAudio.control.gain=(stageAudio||stageMusic)?0.f:1.f;
   if(login){login->model_available(bool(characterRenderer));login->model_partial(prepared.missingModels||prepared.missingColors);
    auto*bank=prepared.gender<2?selectionMotions[prepared.gender].get():nullptr;auto*salute=bank?bank->find(mgo2mt::PlayerMotion::SelectionSalute):nullptr;
    login->selection_presentation(salute?uint32_t((uint64_t(salute->frames)*1000+salute->fps-1)/salute->fps):0,false,selectionBox&&bank&&bank->has(mgo2mt::PlayerMotion::SelectionBox),400);
    if(login->take_selection_sound()&&sound&&std::filesystem::is_regular_file(selectionSoundPath)){selectionAudio=std::make_unique<AudioThread>();selectionAudio->start(selectionSoundPath,L"10",75,"pc_selection_salute");}
   }
   const void* ui=login?login->draw():agreement->draw();
   if(playerMenu.visible())ui=playerMenu.draw();
   for(auto cue:playerMenu.cues())menuSound(cue);
   if(musicMenu.visible())ui=musicMenu.draw();
   for(auto cue:musicMenu.cues())menuSound(cue);
   // Tags use the same eye/direction as the host-authorized firing pose. The
   // third-person camera offset only affects the final label projection.
   uint32_t enemyClanId=0;std::optional<mgo2mt::enemy_tag::Target> enemyTag;
   const auto currentTagRoster=login?login->room_host_roster():std::nullopt;
   const auto currentTagOffer=login?login->combat_offer():std::nullopt;
   const auto currentTagState=login?login->combat_state():std::nullopt;
   if(login&&combatInputActive&&(player.aiming||player.firstPerson)&&stageCamera&&navigationWorld&&combatOffer&&combatState&&currentTagRoster&&requestedStage&&
      stageResult.request==requestedStage&&login->stage_load_request()==requestedStage&&combatState->epoch==combatOffer->epoch&&
      currentTagOffer&&currentTagState&&currentTagOffer->epoch==combatOffer->epoch&&currentTagOffer->self==combatOffer->self&&
      currentTagState->epoch==combatState->epoch&&login->combat_status()==mgo2mt::combat::wire::Status::active&&
      currentTagState->players[combatOffer->self.slot]&&currentTagState->players[combatOffer->self.slot]->identity==combatOffer->self&&
      currentTagState->players[combatOffer->self.slot]->alive&&!currentTagState->players[combatOffer->self.slot]->stunned&&
      hostPlayer&&currentTagState->players[combatOffer->self.slot]->life==hostPlayer->life&&
      !playerMenu.visible()&&!musicMenu.visible()&&login->gameplay_visible()){
    if(playerLock.current()){
     mgo2mt::player_lock::Input freshLock{login->room_auto_aim()&&mgo2mt::gameplay::auto_aim(gameplay,currentTagState->players[combatOffer->self.slot]->weapon)&&player.autoAim&&player.aiming&&!player.firstPerson&&!player.dead,true,combatOffer->self,combatOffer->epoch,stageResult.generation,requestedStage->rotation.rule,shotEye(),navigation.direction()};
     playerLock.update(*currentTagState,*currentTagRoster,freshLock,*queryWorld,stageResult.objectHitCollision.get());
    }
    enemyTag=mgo2mt::enemy_tag::select(*currentTagState,*currentTagRoster,combatOffer->self,requestedStage->rotation.rule,
      shotEye(),navigation.direction(),*queryWorld,playerMenu.enemy_name_tags()&&login->room_enemy_name_tags(),stageResult.objectHitCollision.get());
    if(playerMenu.enemy_name_tags()&&login->room_enemy_name_tags())if(const auto&locked=playerLock.current();locked){
     const auto&p=currentTagState->players[locked->identity.slot];const auto&entry=currentTagRoster->slots[locked->identity.slot];
     if(p&&p->identity==locked->identity&&p->life==locked->life&&p->alive&&!p->stunned&&p->specialPc.nameVisible&&entry&&entry->instance==p->identity.instance&&entry->character==p->identity.character){
      auto head=p->pose.feet;head[1]+=p->pose.capsule.height+90;enemyTag=mgo2mt::enemy_tag::Target{p->identity,head,locked->distance};
     }
    }
    if(enemyTag){
     auto presented=std::find_if(remoteAvatars.begin(),remoteAvatars.end(),[&](const auto&a){return a.identity==enemyTag->identity&&a.alive&&a.life==currentTagState->players[enemyTag->identity.slot]->life;});
     const auto&model=remoteModels[enemyTag->identity.slot];
     if(presented==remoteAvatars.end()||!model||!model->visible)enemyTag.reset();
     else{auto&authoritative=*currentTagState->players[enemyTag->identity.slot];enemyTag->head=presented->origin;enemyTag->head[1]+=authoritative.pose.capsule.height+90;
      auto chest=presented->origin;chest[1]+=authoritative.pose.capsule.height*.65f;
      if(!mgo2mt::enemy_tag::visible(stageCamera->eye,chest,*queryWorld,stageResult.objectHitCollision.get())||
         !mgo2mt::enemy_tag::visible(stageCamera->eye,enemyTag->head,*queryWorld,stageResult.objectHitCollision.get()))enemyTag.reset();
      else enemyClanId=currentTagRoster->slots[enemyTag->identity.slot]->clanId;}
    }
   }
   if(login){auto emblem=login->enemy_clan_emblem(enemyClanId);if(!emblem.image||emblem.serial!=enemyTagClanSerial){enemyTagClanSerial=emblem.serial;enemyTagBitmap.reset();if(emblem.image&&emblem.clan==enemyClanId)enemyTagBitmap=std::make_unique<mgo2mt::clan::Bitmap>(*emblem.image);}}
   if(enemyTag&&stageCamera){
    if(auto point=mgo2mt::enemy_tag::project(enemyTag->head,stageCamera->eye,stageCamera->direction,0,0,1280,720,stageCamera->aspect,stageCamera->verticalFov)){
     if(ui!=enemyTagSurface.data())std::memcpy(enemyTagSurface.data(),ui,enemyTagSurface.size()*sizeof(uint32_t));
     enemyTagRenderer.paint(enemyTagSurface,1280,720,point->x,point->y,
      currentTagRoster->slots[enemyTag->identity.slot]->name,enemyTagBitmap?enemyTagBitmap->get():nullptr,
      mgo2mt::hud::EnemyVitals{currentTagRoster->slots[enemyTag->identity.slot]->level,currentTagState->players[enemyTag->identity.slot]->hp,currentTagState->players[enemyTag->identity.slot]->maxHp});ui=enemyTagSurface.data();
    }
   }
   if(!mountedCurrent&&!flightCurrent&&player.autoAim&&combatInputActive&&stageCamera&&navigationWorld&&combatState&&combatOffer){
    if(const auto& locked=playerLock.current();locked){
     const auto& target=combatState->players[locked->identity.slot];
     if(target&&target->identity==locked->identity&&target->life==locked->life&&target->alive){
      auto point=target->pose.feet;point[1]+=target->pose.capsule.height*.6f;
      if(mgo2mt::enemy_tag::visible(stageCamera->eye,point,*queryWorld,stageResult.objectHitCollision.get()))
       if(auto screen=mgo2mt::enemy_tag::project(point,stageCamera->eye,stageCamera->direction,0,0,1280,720,stageCamera->aspect,stageCamera->verticalFov)){
        if(ui!=enemyTagSurface.data())std::memcpy(enemyTagSurface.data(),ui,enemyTagSurface.size()*sizeof(uint32_t));
        char distance[48]{};std::snprintf(distance,sizeof(distance),"%.1f m",double(locked->distance)/1000.0);
        enemyTagRenderer.paint(enemyTagSurface,1280,720,screen->x+90,screen->y,distance,nullptr);ui=enemyTagSurface.data();
       }
     }
    }
   }
   if(stageCamera&&navigationWorld&&!playerMenu.visible()&&!musicMenu.visible()&&(combatPlayable||Window::inspection)){
    if(ui!=enemyTagSurface.data())std::memcpy(enemyTagSurface.data(),ui,enemyTagSurface.size()*sizeof(uint32_t));
    if(stageResult.collision){auto marks=bulletMarks.sample(navigationNow);mgo2mt::combat::decals::paint(enemyTagSurface,marks,stageCamera->eye,stageCamera->direction,*stageResult.collision,stageResult.objectHitCollision.get(),{0,0,1280,720,stageCamera->aspect,stageCamera->verticalFov});}
    if(stageResult.collision){auto particles=materialParticles.lines(navigationNow);mgo2mt::combat::material_effects::paint(enemyTagSurface,particles,stageCamera->eye,stageCamera->direction,*stageResult.collision,stageResult.objectHitCollision.get(),{0,0,1280,720,stageCamera->aspect,stageCamera->verticalFov});}
    if(stageResult.collision&&combatPlayable&&combatState){auto parts=projectileParticles.sample(*combatState,navigationNow);auto runtimeLines=mgo2mt::combat::runtime_effects::lines(parts,*combatState,navigationNow,bool(particleRenderer));mgo2mt::combat::material_effects::paint(enemyTagSurface,runtimeLines,stageCamera->eye,stageCamera->direction,*stageResult.collision,stageResult.objectHitCollision.get(),{0,0,1280,720,stageCamera->aspect,stageCamera->verticalFov});}
    if(inventoryEligible&&!playerMenu.visible()&&!musicMenu.visible())enemyTagRenderer.paint(enemyTagSurface,1280,720,640,685,"F7 / LT+X アイテム操作",nullptr);
    if(flightCurrent)enemyTagRenderer.paint(enemyTagSurface,1280,720,640,622,hostPlayer&&hostPlayer->blastFlight?"爆風で吹き飛ばされています":"カタパルト：飛行中",nullptr);
    else if(mountedNearby||mountedCurrent||mountedInput.pending()){
     auto*i=mountedRegistry.find(mountedMap,mountedCurrent?mountedCurrent:mountedNearby);auto*t=i?mountedRegistry.find(i->type):nullptr;
     const auto& binding=controllerInput->config;
     const auto key=[&](unsigned action){return mgo2mt::chat::utf8(mgo2mt::input_name(binding.device?binding.gamepad[action]:binding.keyboard[action],binding.device!=0));};
     const std::string name=t?t->name:"重武器",action=key(mgo2mt::mounted::action_button),fire=key(10);
     const std::string label=mountedInput.pending()?name+"：アクション確認中…":mountedCurrent?
      "Armed  "+name+"  ["+fire+"] "+(t&&t->kind==mgo2mt::mounted::Kind::catapult?"射出":"発射")+" / ["+action+"] 解除":
      "["+action+"] アクション："+name+(t&&t->kind==mgo2mt::mounted::Kind::catapult?"に乗る":"を構える");
     enemyTagRenderer.paint(enemyTagSurface,1280,720,640,622,label,nullptr);
    }
    if(ladderNearby||ladderCurrent)enemyTagRenderer.paint(enemyTagSurface,1280,720,640,654,ladderCurrent?"はしご：前後で上下 / Yで出口":"Y：はしごにつかまる",nullptr);
    unsigned waterPixelBudget=12288;
    auto lines=waterEffects.lines();if(stageResult.collision)mgo2mt::stage::paint_water(enemyTagSurface,1280,720,lines,stageCamera->eye,stageCamera->direction,*stageResult.collision,stageResult.objectHitCollision.get(),{0,0,1280,720,stageCamera->aspect,stageCamera->verticalFov},&waterPixelBudget);
    if(stageResult.collision)for(const auto& entry:remoteWaterEffects){auto remoteLines=entry.second.lines();mgo2mt::stage::paint_water(enemyTagSurface,1280,720,remoteLines,stageCamera->eye,stageCamera->direction,*stageResult.collision,stageResult.objectHitCollision.get(),{0,0,1280,720,stageCamera->aspect,stageCamera->verticalFov},&waterPixelBudget);}
    auto contactLines=waterContacts.lines(navigationNow);if(stageResult.collision)mgo2mt::stage::paint_water(enemyTagSurface,1280,720,contactLines,stageCamera->eye,stageCamera->direction,*stageResult.collision,stageResult.objectHitCollision.get(),{0,0,1280,720,stageCamera->aspect,stageCamera->verticalFov},&waterPixelBudget);
    if(login)if(auto session=login->inventory_session()){const auto itemState=session->state();
     if(itemState.context.active&&itemState.world&&combatOffer&&itemState.context.scope.epoch==combatOffer->epoch&&itemState.context.actor.life==combatLife){unsigned painted=0;
      for(const auto& entity:itemState.world->entities){mgo2mt::stage::Vec3 point{entity.position.x,entity.position.y+100,entity.position.z};auto delta=mgo2mt::enemy_tag::sub(point,stageCamera->eye);if(mgo2mt::enemy_tag::dot(delta,delta)>10000.f*10000.f||!mgo2mt::enemy_tag::visible(stageCamera->eye,point,*queryWorld,stageResult.objectHitCollision.get()))continue;
       if(auto p=mgo2mt::enemy_tag::project(point,stageCamera->eye,stageCamera->direction,0,0,1280,720,stageCamera->aspect,stageCamera->verticalFov)){enemyTagRenderer.paint(enemyTagSurface,1280,720,p->x,p->y,mgo2mt::items::item_label(entity.contents.domain,entity.contents.item)+(entity.kind==mgo2mt::items::PlacementKind::installed?" [PLACED]":" [PICK UP]"),nullptr);if(++painted==24)break;}
      }
     }
    }ui=enemyTagSurface.data();
   }
   mgo2mt::reticle::Model reticleModel;
   if(reticleVisible()&&stageCamera&&navigationWorld&&sopView.recipient==reticleScope.identity&&sopView.life==reticleScope.life){
    if(auto target=mgo2mt::reticle::aim_point(shotEye(),navigation.direction(),*queryWorld,stageResult.objectHitCollision.get(),*combatState,reticleScope.identity)){
     auto point=mgo2mt::enemy_tag::project(*target,stageCamera->eye,stageCamera->direction,0,0,1280,720,stageCamera->aspect,stageCamera->verticalFov);
     auto angle=mgo2mt::reticle::camera_angle(float(sopView.spreadMilliRadians)/1000.f,shotEye(),*target,stageCamera->eye,stageCamera->direction);
     if(point&&angle){reticleModel.scope=reticleScope;reticleModel.spreadRadians=*angle;reticleModel.shotWatermark=combatState->eventWatermark;reticleModel.centerX=float(point->x);reticleModel.centerY=float(point->y);reticleModel.viewport={0,0,1280,720,stageCamera->aspect,stageCamera->verticalFov};reticleModel.gameplay=reticleModel.active=reticleModel.alive=true;}
    }
   }
   if(auto reticle=weaponReticle.update(reticleModel,navigationSeconds)){
    if(ui!=enemyTagSurface.data())std::memcpy(enemyTagSurface.data(),ui,enemyTagSurface.size()*sizeof(uint32_t));
    if(const auto*style=weaponEffects?weaponEffects->reticle(reticleScope.weapon):nullptr)mgo2mt::reticle::paint(enemyTagSurface,1280,720,*reticle,*style);
    else mgo2mt::reticle::paint(enemyTagSurface,1280,720,*reticle);ui=enemyTagSurface.data();
   }
   if(stageCamera&&navigation.ready()&&!playerMenu.visible()&&!musicMenu.visible()&&((login&&login->gameplay_visible())||Window::inspection)){
    if(ui!=enemyTagSurface.data())std::memcpy(enemyTagSurface.data(),ui,enemyTagSurface.size()*sizeof(uint32_t));
    const auto& binding=controllerInput->config;const auto action=mgo2mt::input_name(binding.device?binding.gamepad[7]:binding.keyboard[7],binding.device!=0);
    coverHud.paint(enemyTagSurface,{coverAvailable,coverState.attached,coverInput.pending(),coverState.lean,coverLeft,coverRight,player.firstPerson,action});ui=enemyTagSurface.data();
   }
   killFeed.expire(navigationNow);
   if(combatState&&login&&login->gameplay_visible()&&!killFeed.entries.empty()){
    if(ui!=enemyTagSurface.data())std::memcpy(enemyTagSurface.data(),ui,enemyTagSurface.size()*sizeof(uint32_t));
    int y=85;for(const auto&entry:killFeed.entries){enemyTagRenderer.paint(enemyTagSurface,1280,720,1020,y,entry.text,nullptr);y+=44;}ui=enemyTagSurface.data();
   }
   invitation.session(login?login->invitation_session():nullptr);invitation.update(GetTickCount64());
   invitation.overlay().set_menu_hint(playerMenu.settings_menu()&&!playerMenu.capturing());
   if(invitation.overlay().available()){
    if(ui!=enemyTagSurface.data())std::memcpy(enemyTagSurface.data(),ui,enemyTagSurface.size()*sizeof(uint32_t));
    invitationFrameModal=invitation.overlay().paint(enemyTagSurface,1280,720,GetTickCount64())&&invitation.overlay().visible();ui=enemyTagSurface.data();
   }
   {
    const auto now=GetTickCount64();const auto session=login?login->notification_session():nullptr;
    const auto state=session?session->state():mgo2mt::notices::State{};
    const auto inviteSession=login?login->invitation_session():nullptr;
    const auto entries=inviteSession?inviteSession->view(now):std::vector<mgo2mt::invitations::Entry>{};
    const auto utc=uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    const auto view=mgo2mt::notifications::update_session(notificationPresentation,state,entries,now,utc);
    if(notificationPresentation.take_ping()&&sound&&std::filesystem::is_regular_file(notificationSoundPath)){
     notificationAudio=std::make_unique<AudioThread>();notificationAudio->start(notificationSoundPath,L"2",0,"notification_native");
    }
    if(!state.connected)notificationAudio.reset();
    alertMedia.select(state.scope,state.connected&&state.ready?state.snapshot.alert:std::nullopt,mgo2mt::notifications::server_now(state,now,utc),now,sound);
    if(state.connected){
     if(ui!=enemyTagSurface.data())std::memcpy(enemyTagSurface.data(),ui,enemyTagSurface.size()*sizeof(uint32_t));
     notificationRenderer.paint(enemyTagSurface,1280,720,view,now);
     alertMedia.paint(enemyTagSurface,1280,720);
     mgo2mt::paint_lobby_ping(enemyTagSurface,login->lobby_monitor().pingMs);ui=enemyTagSurface.data();
    }
   }
   if(Window::debug.enabled&&stageCamera&&navigationWorld){
    if(ui!=enemyTagSurface.data())std::memcpy(enemyTagSurface.data(),ui,enemyTagSurface.size()*sizeof(uint32_t));
    mgo2mt::physics_debug::Frame wires;
    auto hitVolumes=[&](const mgo2mt::combat::Player& p){if(!p.alive||p.specialPc.kind!=mgo2mt::special_pc::Kind::human)return;const auto stance=p.stunned||p.pose.capsule.height==560?mgo2mt::host_hit::Stance::prone:p.pose.capsule.height==1100?mgo2mt::host_hit::Stance::crouching:mgo2mt::host_hit::Stance::standing;wires.hit_regions(p.pose.feet,p.cover.attached?std::remainder(p.cover.normalYaw+3.14159265359f,6.28318530718f):p.pose.yaw,stance,mgo2mt::combat::cover::eye_offset(p.cover,p.pose.yaw));};
    if(hostPlayer)hitVolumes(*hostPlayer);
    if(navigation.ready())wires.standing(navigation.feet(),navigation.capsule());
    for(const auto& body:ragdoll.bodies())wires.rigid(body);
    if(testBody)wires.rigid(*testBody);
    if(avatar.ready()&&avatarCatalog)wires.skeleton(avatar,avatarCatalog->skeleton(avatar.gender),avatarDrawOrigin,avatarDrawYaw);
    if(auto axis=avatarWeapon.sight_axis(player.bodyYaw,navigation.feet()))wires.line(axis->rear,axis->front,mgo2mt::physics_debug::Kind::attachment);
    for(const auto& [key,axis]:avatarWeapon.connection_frames(player.bodyYaw,navigation.feet()))wires.line(axis.rear,axis.front,mgo2mt::physics_debug::Kind::attachment);
    if(combatState)for(const auto& remote:remoteAvatars)if(remote.identity.slot<24){
     const auto& model=remoteModels[remote.identity.slot];const auto& state=combatState->players[remote.identity.slot];
     if(state&&state->identity==remote.identity&&state->alive){wires.standing(state->pose.feet,state->pose.capsule);hitVolumes(*state);}
     if(model&&model->visible){for(const auto& body:model->corpse.bodies())wires.rigid(body);auto* catalog=model->kind==mgo2mt::special_pc::Kind::gekko?gekkoCatalog.get():characterCatalog.get();if(catalog)wires.skeleton(model->body,catalog->skeleton(model->body.gender),model->drawOrigin,model->drawYaw);for(const auto& [key,axis]:model->weapon.connection_frames(model->drawYaw,model->drawOrigin))wires.line(axis.rear,axis.front,mgo2mt::physics_debug::Kind::attachment);}
    }
    if(combatState)for(const auto& line:projectileParticles.sample(*combatState,navigationNow))if(line.kind==mgo2mt::combat::particles::Kind::casing)wires.line(line.from,line.to,mgo2mt::physics_debug::Kind::visual);
    if(login)if(auto flights=login->debug_flights())for(const auto& flight:flights->flights){
     wires.line(flight.traceFrom,flight.traceTo,mgo2mt::physics_debug::Kind::rigid);
     for(unsigned axis=0;axis<3;++axis){auto a=flight.position,b=flight.position;a[axis]-=60;b[axis]+=60;wires.line(a,b,mgo2mt::physics_debug::Kind::rigid);}
    }
    if(debugItemShapesReady&&login)if(auto session=login->inventory_session()){
     const auto state=session->state();if(state.world)for(const auto& item:state.world->entities)if(item.kind!=mgo2mt::items::PlacementKind::installed){
      const auto extent=debugItemShapes.extent(item.contents);wires.box({item.position.x,item.position.y+extent[1],item.position.z},extent);
     }
    }
    wires.terrain(*queryWorld,stageCamera->eye);
    const auto stats=mgo2mt::physics_debug::paint(enemyTagSurface,1280,720,wires,{stageCamera->eye,stageCamera->direction,0,0,1280,720,stageCamera->aspect,stageCamera->verticalFov});
    enemyTagRenderer.paint(enemyTagSurface,1280,720,640,674,"GREEN: PLAYER / BLUE: NON-BLOCKING / ORANGE: DON'T FALL / CYAN: BODY / YELLOW: BONES",nullptr);
    enemyTagRenderer.paint(enemyTagSurface,1280,720,640,702,"F12  LINES "+std::to_string(stats.lines)+"  OMITTED "+std::to_string(stats.omitted),nullptr);ui=enemyTagSurface.data();
   }
   if(Window::debug.enabled&&graphics->active.gpuTiming){
    if(ui!=enemyTagSurface.data())std::memcpy(enemyTagSurface.data(),ui,enemyTagSurface.size()*sizeof(uint32_t));
    mgo2mt::render_profiler::paint(enemyTagSurface,1280,720,gpuProfiler,{16,365,1248,280});ui=enemyTagSurface.data();
   }
   context->UpdateSubresource(agreementTexture.Get(),0,nullptr,ui,1280*4,0);
   bool portNow=login&&login->port_visible();if(portNow!=portTitle){portTitle=portNow;window.title(portNow?L"OpenMGO2 - Port settings | Esc: back":L"OpenMGO2 - Login | Enter: select | Esc: back");}
   bool charNow=login&&login->character_visible();if(charNow!=charTitle){charTitle=charNow;window.title(charNow?L"OpenMGO2 - Characters | Esc: settings":L"OpenMGO2 - Settings | Esc: back");}
   if(Window::debug.enabled){auto title=L"OpenMGO2 - DEBUG MENU | F4: Blend "+std::to_wstring(blendSettings.percentPerSecond)+L"%/s | F12: close | F10: walk | F5: reset | F7/F8/F9: BGM";window.title(title.c_str());}
   else if(debugTitle)window.title(L"OpenMGO2 | F12: debug");debugTitle=Window::debug.enabled;
   quads.clear();
   auto appendMotion=[&](mgo2mt::TitleAnimation& m){m.tick(5,0);auto v=m.geometry();for(auto&q:v)if(q.atlas>=0)q.atlas+=motionOffset;quads.insert(quads.end(),v.begin(),v.end());};
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
    const float left=0,top=0,right=1280,bottom=720;
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
  if(titleDisconnect!=mgo2mt::LobbyDisconnectReason::none&&agreementTexture){
   std::fill(enemyTagSurface.begin(),enemyTagSurface.end(),0);mgo2mt::paint_server_disconnect(enemyTagSurface,titleDisconnect);
   context->UpdateSubresource(agreementTexture.Get(),0,nullptr,enemyTagSurface.data(),1280*4,0);
   Quad q{};q.atlas=-2;const float xy[4][2]={{0,0},{1280,0},{1280,720},{0,720}},uv[4][2]={{0,0},{1,0},{1,1},{0,1}};
   for(int i=0;i<4;++i)q.vertices[i]={xy[i][0],xy[i][1],uv[i][0],uv[i][1],1,1,1,1};quads.push_back(q);
   vertices.clear();for(const auto&draw:quads)for(int i:{0,1,2,0,2,3})vertices.push_back(draw.vertices[i]);count=unsigned(quads.size());
  }
  {
   auto state=agreementVisible?(login?login->ui_presentation():mgo2mt::multi_ui::presentation(mgo2mt::multi_ui::Route::agreement,!agreement->ready())):
      mgo2mt::multi_ui::presentation(loadingVisible?mgo2mt::multi_ui::Route::loading:mgo2mt::multi_ui::Route::title);
   if(playerMenu.visible()||musicMenu.visible())state.flags.insert("modal");
   if(state.screen=="hud"){if(player.firstPerson)state.flags.insert("first_person");if(!mountedCurrent&&!flightCurrent&&player.autoAim)state.flags.insert("auto_aim");}
   if(titleDisconnect!=mgo2mt::LobbyDisconnectReason::none){state=mgo2mt::multi_ui::presentation(mgo2mt::multi_ui::Route::title);state.state="disconnected";state.flags.insert("modal");}
   const bool uiFocused=GetForegroundWindow()==window.handle&&!IsIconic(window.handle)&&!Window::titleMovieActive;
   Window::customUiInteractive=uiFocused&&!state.flags.contains("modal")&&!state.flags.contains("loading");
   if(const auto* rgba=customUi.frame(GetTickCount64(),state,uiFocused,sound)){
    if(!customUiTexture){D3D11_TEXTURE2D_DESC td{};td.Width=1280;td.Height=720;td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;check(device->CreateTexture2D(&td,nullptr,&customUiTexture));check(device->CreateShaderResourceView(customUiTexture.Get(),nullptr,&customUiView));}
    context->UpdateSubresource(customUiTexture.Get(),0,nullptr,rgba->data(),1280*4,0);
    Quad q{};q.atlas=-5;q.blend=0;const float xy[4][2]={{0,0},{1280,0},{1280,720},{0,720}},uv[4][2]={{0,0},{1,0},{1,1},{0,1}};
    for(int i=0;i<4;++i)q.vertices[i]={xy[i][0],xy[i][1],uv[i][0],uv[i][1],1,1,1,1};quads.push_back(q);
    vertices.clear();for(const auto&draw:quads)for(int i:{0,1,2,0,2,3})vertices.push_back(draw.vertices[i]);count=unsigned(quads.size());++customUiFrames;
    const auto key=state.screen+"/"+state.state;if(key!=customUiState){customUiState=key;std::osyncstream(std::cout)<<"multi_ui_state: "<<key<<'\n';}
   }
   if(customUi.busy())Window::titleActivity=GetTickCount64();
   for(const auto& event:customUi.take_events()){
    const auto action=mgo2mt::multi_ui::game_action(event,state,Window::customUiInteractive);
    if(action==mgo2mt::multi_ui::GameAction::start){Window::pressed|=8;Window::titleActivity=GetTickCount64();}
    else if(action==mgo2mt::multi_ui::GameAction::confirm||action==mgo2mt::multi_ui::GameAction::back){
     const auto key=action==mgo2mt::multi_ui::GameAction::confirm?VK_RETURN:VK_ESCAPE;
     if(login)login->message(window.handle,WM_KEYDOWN,key,1LL<<25);
     else if(agreementVisible)Window::agreementInput|=action==mgo2mt::multi_ui::GameAction::confirm?mgo2mt::AgreementScreen::confirm:mgo2mt::AgreementScreen::cancel;
    }
    if(event.kind=="failed")std::osyncstream(std::cout)<<"multi_ui_action_failed\n";
    else if(event.kind=="event")std::osyncstream(std::cout)<<"multi_ui_event: "<<event.name<<" routed="<<unsigned(action)<<'\n';
   }
  }
   const bool profileThisFrame=Window::debug.enabled&&graphics->active.gpuTiming&&!IsIconic(window.handle);
  if(profileThisFrame&&!profilerInitialized){gpuProfiler.initialize(device.Get());profilerInitialized=true;}
  const auto cpuSubmitStart=std::chrono::steady_clock::now();
  if(profileThisFrame)gpuProfiler.begin_frame(context.Get(),frames);
  auto gpuBegin=[&](GpuStage stage){if(profileThisFrame)gpuProfiler.begin_stage(context.Get(),stage);};
  auto gpuEnd=[&](GpuStage stage){if(profileThisFrame)gpuProfiler.end_stage(context.Get(),stage);};
  presentedStageView=nullptr;
  D3D11_MAPPED_SUBRESOURCE mapped{};check(context->Map(vb.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped));if(!vertices.empty())std::memcpy(mapped.pData,vertices.data(),vertices.size()*sizeof(Vertex));context->Unmap(vb.Get(),0);
  bool modelVisible=characterRenderer&&agreementVisible&&login&&login->model_preview_visible();
  if(modelVisible){if(characterCatalog){auto frame=login->selection_frame();auto*bank=prepared.gender<2?selectionMotions[prepared.gender].get():nullptr;std::optional<mgo2mt::MotionPose> selectedPose;
   double boxEnterSeconds=0;if(bank)if(auto*entry=bank->find(mgo2mt::PlayerMotion::SelectionBoxEnter))boxEnterSeconds=double(entry->frames)/entry->fps;
   const auto presentationNow=GetTickCount64();
   if(frame.kind!=previousSelectionKind){selectionLiftFrom=selectionLift;selectionLiftProgress=0;previousSelectionKind=frame.kind;}else selectionLiftProgress=std::min(1.f,selectionLiftProgress+float(blendDelta)*blendRate);
   const bool enteringBox=frame.kind==mgo2mt::SelectionPresentation::Kind::box;
   selectionLift=selectionLiftFrom+((enteringBox?.12f:0.f)-selectionLiftFrom)*selectionLiftProgress;
   uint64_t selectionSource=0x200;double selectionTime=(presentationNow-modelBegan)/1000.;
   if(bank&&frame.kind!=mgo2mt::SelectionPresentation::Kind::idle){auto action=frame.kind==mgo2mt::SelectionPresentation::Kind::salute?mgo2mt::PlayerMotion::SelectionSalute:frame.kind==mgo2mt::SelectionPresentation::Kind::magazine?mgo2mt::PlayerMotion::SelectionMagazine:frame.seconds<boxEnterSeconds?mgo2mt::PlayerMotion::SelectionBoxEnter:mgo2mt::PlayerMotion::SelectionBox;double seconds=frame.seconds;if(action==mgo2mt::PlayerMotion::SelectionBox)seconds-=boxEnterSeconds;selectedPose=mgo2mt::selection_pose(*bank,action,seconds);selectionSource=0x400+unsigned(action);selectionTime=seconds;}
   auto complete=selectedPose?characterCatalog->complete_pose(prepared.gender,*selectedPose):characterCatalog->sample_pose(prepared.gender,selectionTime);
   const auto& displayed=selectionBlend.sample({1,appearanceChanges,preparedId,1,prepared.gender},selectionSource,selectionTime,complete,blendDelta,blendRate);characterCatalog->pose(prepared,displayed);
   bool box=frame.kind==mgo2mt::SelectionPresentation::Kind::box&&frame.seconds>=boxEnterSeconds&&selectionBox&&selectedPose;
   if(box!=previewBox){previewBox=box;selectionComposite.reset();if(box)selectionComposite= mgo2mt::selection_with_prop(prepared.model,*selectionBox,-mgo2mt::selection_origin_y(*bank));characterRenderer=std::make_unique<mgo2mt::CharacterRenderer>(device.Get(),selectionComposite?*selectionComposite:prepared.model);}
   characterRenderer->preview_vertical_offset(selectionLift);
   if(selectionComposite){std::copy(prepared.model.vertices.begin(),prepared.model.vertices.end(),selectionComposite->vertices.begin());characterRenderer->update_vertices(context.Get(),selectionComposite->vertices);}else characterRenderer->update_vertices(context.Get(),prepared.model.vertices);}characterRenderer->render(context.Get(),login->model_yaw());++modelFrames;login->model_rendered();
   context->IASetInputLayout(input.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->IASetVertexBuffers(0,1,&vptr,&stride,&offset);context->IASetIndexBuffer(nullptr,DXGI_FORMAT_R32_UINT,0);context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);context->PSSetSamplers(0,1,&sp);context->RSSetState(raster.Get());context->RSSetViewports(1,&viewport);context->OMSetRenderTargets(1,&rp,nullptr);context->OMSetDepthStencilState(nullptr,0);
  }else if(login&&login->character_visible())++emptyModelFrames;
  // draw() can adopt a new HOST Frame. Scope the SOP overlay and activation
  // notification to that newest complete state, after the UI update.
  auto sopSnapshot=login?login->combat_state():std::nullopt;auto sopOffer=login?login->combat_offer():std::nullopt;
  const auto sopFrame=login?login->combat_sop():mgo2mt::combat::SopView{};
  const bool stageMatches=stageRenderer&&agreementVisible&&login&&login->stage_load_request()&&login->stage_load_request()==stageAssets.result().request;
  const bool sopActive=stageMatches&&stageCamera&&sopSnapshot&&sopOffer&&sopSnapshot->epoch==sopOffer->epoch&&login->combat_status()==mgo2mt::combat::wire::Status::active;
  if(sopActive){
   if(sopPresentation.update(stageAssets.result().generation,*sopSnapshot,sopOffer->self,sopFrame,true,GetTickCount64())&&sound&&std::filesystem::is_regular_file(sopSoundPath)){
    sopAudio=std::make_unique<AudioThread>();sopAudio->start(sopSoundPath,L"2",0,"sop_native");
   }
  }else sopPresentation.clear();
  if(!sopActive||sopFrame.jammed||!sopFrame.visibleMask)sopAudio.reset();
  if(stageMatches){
   const auto& quality=graphics->active;
   mgo2mt::effects::Settings effectSettings;effectSettings.hdr=quality.hdr!=0;effectSettings.fxaa=quality.aa!=0;effectSettings.ssao=quality.ao!=0;effectSettings.ssr=quality.reflections!=0;effectSettings.bloom=quality.bloom!=0;effectSettings.exposure=quality.exposureMilli/1000.f;
   const bool needsEffects=quality.hdr||quality.aa||quality.ao||quality.reflections||quality.bloom;
   if(needsEffects&&!renderEffects)renderEffects=std::make_unique<mgo2mt::effects::Renderer>(device.Get());
   if(!needsEffects)renderEffects.reset();
   if(quality.lod&&stageModel)stageRenderer->prepare_lod(device.Get(),*stageModel);
   std::vector<mgo2mt::item_box::Box> visibleBoxes;
   std::vector<mgo2mt::shadows::Caster> installedWeapons;std::set<std::array<float,5>> installedKeys;
   if(stageCamera&&itemBoxRenderer&&sopActive&&sopSnapshot&&sopOffer&&stageAssets.result().collision){
    auto session=login->inventory_session();auto state=session?session->state():mgo2mt::items::ClientState{};const auto& self=sopSnapshot->players[sopOffer->self.slot];
    if(self&&self->identity==sopOffer->self&&state.context.scope.epoch==sopOffer->epoch&&state.context.actor.slot==self->identity.slot&&state.context.actor.instance==self->identity.instance&&state.context.actor.character==self->identity.character&&state.context.actor.life==self->life){
     visibleBoxes=itemBoxes.update(state,*stageAssets.result().collision,stageAssets.result().generation,GetTickCount64());
     if(heldModels&&state.context.active&&state.status==mgo2mt::items::ClientStatus::ready&&state.world&&state.world->scope==state.context.scope)for(const auto&e:state.world->entities){
      if(e.kind!=mgo2mt::items::PlacementKind::installed||e.key.scope!=state.context.scope||e.contents.domain!=mgo2mt::items::Domain::weapon)continue;
      const auto id=e.contents.item;if(id!=64&&id!=65&&id!=66&&id!=67&&id!=69)continue;
      std::array<float,3> origin{e.position.x,e.position.y,e.position.z};auto delta=mgo2mt::enemy_tag::sub(origin,stageCamera->eye);
      if(!mgo2mt::enemy_tag::finite(origin)||!std::isfinite(e.position.yaw)||mgo2mt::enemy_tag::dot(delta,delta)>40000.f*40000.f)continue;
      if(auto*model=heldModels->find(id)){std::array<float,5> key{float(id),e.position.nx,e.position.ny,e.position.nz,e.position.yaw};installedKeys.insert(key);auto&renderer=installedWeaponModels[key];if(!renderer)renderer=std::make_unique<mgo2mt::CharacterRenderer>(device.Get(),mgo2mt::installed_weapon_model(*model,{e.position.nx,e.position.ny,e.position.nz},e.position.yaw));
       installedWeapons.push_back({renderer.get(),0,origin});if(installedWeapons.size()>=128)break;
      }
     }
    }else itemBoxes.clear();
   }else itemBoxes.clear();
   std::erase_if(installedWeaponModels,[&](const auto&entry){return !installedKeys.contains(entry.first);});
   if(mountedRenderer&&stageAssets.result().request){auto rendered=sopSnapshot;if(rendered&&sopOffer&&mountedCurrent)if(auto&me=rendered->players[sopOffer->self.slot];me&&me->identity==sopOffer->self&&me->mountedId==mountedCurrent){me->pose.yaw=navigation.yaw();me->pose.pitch=navigation.pitch();}mountedRenderer->update(context.Get(),stageAssets.result().request->rotation.map,rendered?&*rendered:nullptr);mountedRenderer->shadow_casters(installedWeapons);}if(mortarShellRenderer)mortarShellRenderer->shadow_casters(installedWeapons);
   const mgo2mt::shadows::Renderer* worldShadows=nullptr;
   auto shadowStage=stageAssets.result();shadowStage.lighting=environmentLighting;
   auto actorEnvironment=[&](const std::array<float,3>& origin){return shadowStage.lighting?shadowStage.lighting->environment(origin):mgo2mt::EnvironmentLight{};};
   if(stageCamera&&shadowStage.lighting&&graphics->active.shadowEnabled&&shadowRenderer.configure(device.Get(),graphics->active.shadows())){
    gpuBegin(GpuStage::Shadow);
    std::vector<mgo2mt::shadows::Caster> casters{{stageRenderer.get(),0,{}}};
    if(avatarRenderer){auto origin=ragdoll.active()?ragdoll.origin():navigation.feet();if(gekkoActive)origin[1]+=mgo2mt::special_pc::native_gekko.modelFeetOffset;casters.push_back({avatarRenderer.get(),player.bodyYaw,origin});avatarWeapon.shadow_casters(casters,player.bodyYaw,origin);}
    if(testBody&&bodyRenderer)casters.push_back({bodyRenderer.get(),0,testBody->position});
    for(const auto&remote:remoteAvatars)if(auto&model=remoteModels[remote.identity.slot];model&&model->renderer&&model->visible){casters.push_back({model->renderer.get(),remote.yaw,model->drawOrigin});model->weapon.shadow_casters(casters,remote.yaw,model->drawOrigin);}
    if(itemBoxRenderer)itemBoxRenderer->shadow_casters(casters,visibleBoxes);
    casters.insert(casters.end(),installedWeapons.begin(),installedWeapons.end());
    if(shadowRenderer.render(context.Get(),*stageCamera,shadowStage.lighting->direction,shadowStage.lighting->direct,stageRenderer->bounds(),casters))worldShadows=&shadowRenderer;
    gpuEnd(GpuStage::Shadow);
   }
   const auto internal=mgo2mt::render_backend::internal_extent(unsigned(viewport.Width),unsigned(viewport.Height),graphics->active.renderScale,needsEffects);
   gpuBegin(GpuStage::Opaque);
   stageRenderer->resize_target(device.Get(),internal.width,internal.height,quality.hdr!=0,quality.reflections!=0);
   stageRenderer->render(context.Get(),.35f,true,stageCamera?&*stageCamera:nullptr,nullptr,nullptr,combatPointLights,nullptr,worldShadows,nullptr,1.f,stageCamera?mgo2mt::CharacterPass::opaque:mgo2mt::CharacterPass::all);++stageFrames;
   if(stageCamera&&stageSkyRenderer){auto settings=stageAssets.result().skySettings;std::optional<mgo2mt::SkyFrame> sky;if(settings){auto pose=settings->sample(double(GetTickCount64()-skyStartedAt)/1000.0);sky=mgo2mt::SkyFrame{pose.position,pose.degrees,settings->color,settings->fogColor,settings->fog,pose.cloudU};}stageSkyRenderer->render(context.Get(),0,false,&*stageCamera,stageRenderer.get(),nullptr,{},nullptr,nullptr,nullptr,1.f,mgo2mt::CharacterPass::all,sky?&*sky:nullptr);}
   if(stageCamera&&itemBoxRenderer&&!visibleBoxes.empty())itemBoxRenderer->draw(context.Get(),visibleBoxes,*stageRenderer,*stageCamera,combatPointLights,worldShadows,shadowStage.lighting.get());
   if(stageCamera)for(const auto&object:installedWeapons){auto environment=actorEnvironment(object.origin);object.renderer->render(context.Get(),object.yaw,false,&*stageCamera,stageRenderer.get(),&object.origin,combatPointLights,nullptr,worldShadows,shadowStage.lighting?&environment:nullptr);}
   if(stageCamera&&avatarRenderer){auto origin=ragdoll.active()?ragdoll.origin():navigation.feet();if(gekkoActive)origin[1]+=mgo2mt::special_pc::native_gekko.modelFeetOffset;auto environment=actorEnvironment(origin);const auto* env=shadowStage.lighting?&environment:nullptr;if(!firstPersonTransition.split_body()||gekkoActive)avatarRenderer->render(context.Get(),player.bodyYaw,false,&*stageCamera,stageRenderer.get(),&origin,combatPointLights,nullptr,worldShadows,env);else{if(avatarFadeRenderer&&firstPersonTransition.body_opacity()>0)avatarFadeRenderer->render(context.Get(),player.bodyYaw,false,&*stageCamera,stageRenderer.get(),&origin,combatPointLights,nullptr,worldShadows,env,firstPersonTransition.body_opacity());if(avatarArmsRenderer)avatarArmsRenderer->render(context.Get(),player.bodyYaw,false,&*stageCamera,stageRenderer.get(),&origin,combatPointLights,nullptr,worldShadows,env);}avatarWeapon.draw(context.Get(),player.bodyYaw,*stageCamera,stageRenderer.get(),origin,combatPointLights,worldShadows,env);}
   if(stageCamera&&testBody&&bodyRenderer){auto environment=actorEnvironment(testBody->position);bodyRenderer->render(context.Get(),0,false,&*stageCamera,stageRenderer.get(),&testBody->position,combatPointLights,nullptr,worldShadows,shadowStage.lighting?&environment:nullptr);}
   if(stageCamera)for(const auto&remote:remoteAvatars)if(auto&model=remoteModels[remote.identity.slot];model&&model->renderer&&model->visible){auto origin=model->drawOrigin;auto environment=actorEnvironment(origin);const auto* env=shadowStage.lighting?&environment:nullptr;model->renderer->render(context.Get(),remote.yaw,false,&*stageCamera,stageRenderer.get(),&origin,combatPointLights,nullptr,worldShadows,env);model->weapon.draw(context.Get(),remote.yaw,*stageCamera,stageRenderer.get(),origin,combatPointLights,worldShadows,env);}
   gpuEnd(GpuStage::Opaque);
   if(stageCamera&&renderEffects&&(quality.ao||quality.reflections)){
    gpuBegin(GpuStage::AmbientReflection);
    mgo2mt::effects::Camera projection;DirectX::XMFLOAT4X4 matrix;
    const auto lens=mgo2mt::world_projection(stageCamera->aspect,stageCamera->verticalFov);
    DirectX::XMStoreFloat4x4(&matrix,lens);std::memcpy(projection.projection.data(),&matrix,64);
    DirectX::XMStoreFloat4x4(&matrix,DirectX::XMMatrixInverse(nullptr,lens));std::memcpy(projection.inverseProjection.data(),&matrix,64);
    const auto composed=renderEffects->opaque(context.Get(),{stageRenderer->view(),stageRenderer->depth_view(),stageRenderer->reflection_view(),projection},effectSettings);
    if(composed!=stageRenderer->view())renderEffects->copy_to(context.Get(),composed,stageRenderer->target_view());
    gpuEnd(GpuStage::AmbientReflection);
   }
   gpuBegin(GpuStage::TransparentFx);
   if(stageCamera)stageRenderer->render(context.Get(),0,false,&*stageCamera,stageRenderer.get(),nullptr,combatPointLights,nullptr,worldShadows,nullptr,1.f,mgo2mt::CharacterPass::alpha);
   if(stageCamera&&waterRenderer)waterRenderer->render(context.Get(),*stageRenderer,*stageCamera);
   if(sopActive&&sopSnapshot&&stageCamera){auto streaks=bulletTracers.sample(*sopSnapshot,GetTickCount64());if(!streaks.empty()){if(!tracerRenderer)tracerRenderer=std::make_unique<mgo2mt::combat::tracers::Renderer>(device.Get());tracerRenderer->render(context.Get(),*stageRenderer,*stageCamera,streaks);}}else bulletTracers.clear();
   if(particleRenderer&&sopActive&&sopSnapshot&&stageCamera){auto sprites=projectileParticles.sprites(*sopSnapshot,GetTickCount64());particleRenderer->render(context.Get(),*stageRenderer,*stageCamera,sprites);}
   if(stageCamera&&shadowStage.request){
    const auto stageName=mgo2mt::stage::name(shadowStage.request->rotation.map);const auto weatherSettings=mgo2mt::stage::environment_weather(stageName,environmentConfig);const auto weatherNow=GetTickCount64();
    const bool fog=environmentConfig.weatherOverride?environmentConfig.fog:weatherSettings.preset==mgo2mt::stage::weather::Preset::fog_sand;
    const bool sand=environmentConfig.weatherOverride?environmentConfig.sandstorm:weatherSettings.preset==mgo2mt::stage::weather::Preset::fog_sand;
    auto weather=weatherController.sample(fog,sand,double(weatherNow-weatherStartedAt)/1000.0,stageCamera->eye,shadowStage.collision.get());
    weather.surface=weatherSurface.sample(weatherSettings,double(weatherNow)/1000.0,stageCamera->eye,shadowStage.collision,shadowStage.objectSnapshot?shadowStage.objectSnapshot->revision:0);
    if(weatherRenderer)weatherRenderer->render(context.Get(),*stageRenderer,*stageCamera,weather);
    auto precipitation=mgo2mt::stage::weather::precipitation(weatherSettings,double(weatherNow)/1000.0,stageCamera->eye,shadowStage.collision.get());
    if(!precipitation.empty()){if(!tracerRenderer)tracerRenderer=std::make_unique<mgo2mt::combat::tracers::Renderer>(device.Get());tracerRenderer->render(context.Get(),*stageRenderer,*stageCamera,precipitation);}
   }
   if(stageCamera&&sopRenderer&&sopActive){
     if(auto scan=sopPresentation.pulse(GetTickCount64()))sopRenderer->scan(context.Get(),*stageRenderer,*stageCamera,{scan->origin,scan->radius,scan->width,scan->opacity},{true,true,true,false});
     for(const auto& remote:remoteAvatars)if(auto& model=remoteModels[remote.identity.slot];model&&model->renderer&&model->visible&&remote.specialPc.kind==mgo2mt::special_pc::Kind::human&&sopPresentation.visible(remote.identity,remote.life,*sopSnapshot))
      sopRenderer->doll(context.Get(),model->body,*stageRenderer,*stageCamera,remote.origin,remote.yaw,{true,true,remote.alive,false});
   }
   gpuEnd(GpuStage::TransparentFx);
   presentedStageView=stageRenderer->view();
   if(renderEffects&&(quality.hdr||quality.bloom||quality.aa)){
    gpuBegin(GpuStage::Post);presentedStageView=renderEffects->finish(context.Get(),stageRenderer->view(),effectSettings,stageRenderer->hdr());gpuEnd(GpuStage::Post);
   }
   context->IASetInputLayout(input.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->IASetVertexBuffers(0,1,&vptr,&stride,&offset);context->IASetIndexBuffer(nullptr,DXGI_FORMAT_R32_UINT,0);context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);context->PSSetSamplers(0,1,&sp);context->RSSetState(raster.Get());context->RSSetViewports(1,&viewport);context->OMSetRenderTargets(1,&rp,nullptr);context->OMSetDepthStencilState(nullptr,0);
  }
  if(modelVisible!=lastModelVisible){lastModelVisible=modelVisible;std::osyncstream(std::cout)<<"{\"character_model_visible\":"<<(modelVisible?"true":"false")<<"}"<<std::endl;}
  gpuBegin(GpuStage::Ui);
  const float clear[4]={0,0,0,1};context->ClearRenderTargetView(rt.Get(),clear);
  for(UINT i=0;i<count;i++){const auto&q=quads[i];ID3D11ShaderResourceView*tex=q.atlas==-5?customUiView.Get():q.atlas==-4?(presentedStageView?presentedStageView:stageRenderer->view()):q.atlas==-3?characterRenderer->view():q.atlas==-2?agreementView.Get():textures[q.atlas<0?textureCount:q.atlas].Get();context->PSSetShaderResources(0,1,&tex);context->OMSetBlendState(blend[q.blend].Get(),nullptr,0xffffffff);context->Draw(6,i*6);}
  gpuEnd(GpuStage::Ui);if(profileThisFrame)gpuProfiler.end_frame(context.Get());
  const auto cpuSubmitEnd=std::chrono::steady_clock::now();
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
  const auto cpuPresentStart=std::chrono::steady_clock::now();
  const auto presented=swap->Present(graphics->active.vsync?1:0,0);check(presented);
  const auto cpuFrameEnd=std::chrono::steady_clock::now();
  if(profileThisFrame){auto ms=[](auto a,auto b){return std::chrono::duration<double,std::milli>(b-a).count();};gpuProfiler.record_cpu(frames,{cpuPreviousPresent&&presented==S_OK?ms(*cpuPreviousPresent,cpuFrameEnd):mgo2mt::render_profiler::Missing,ms(cpuSubmitStart,cpuSubmitEnd),ms(cpuPresentStart,cpuFrameEnd)});}Window::invitationGuard.painted(invitationFrameModal);
  if(presented==S_OK&&!IsIconic(window.handle))cpuPreviousPresent=cpuFrameEnd;else cpuPreviousPresent.reset();
  if(presented==S_OK&&!IsIconic(window.handle))debugFrameRate.present(std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count());else debugFrameRate.reset();
  if(presented==DXGI_STATUS_OCCLUDED)++occluded;++frames;if(loadingVisible&&!agreementVisible)++loadingFrames;if(animation&&animation->state()==4&&!loadingVisible)running=false;Sleep(1);
 }
 // Preview shutdown policy: allow the final GCX fade request to finish within
 // the existing duration bound. This does not advance an unimplemented actor.
 unsigned drained=0;
 if(animation&&animation->state()==4&&sound)while(fade.remaining()&&GetTickCount64()<deadline){Sleep(17);fade.advance(1);audio.control.gain=fade.gain();++drained;}
 if(drained)std::osyncstream(std::cout)<<"{\"preview_audio_drain_frames\":"<<drained<<",\"gain\":"<<fade.gain()<<"}"<<std::endl;
 for(auto& a:menuAudio)a->stop=true;for(auto& a:menuAudio){if(a->thread.joinable())a->thread.join();if(a->result.load())++menuFailures;}
 if(motionBack)std::osyncstream(std::cout)<<"{\"background_motion_ticks\":"<<motionBack->ticks()<<",\"background_loop_restarts\":"<<(motionBack->loop_restarts()+motionFront->loop_restarts())<<"}"<<std::endl;
 std::osyncstream(std::cout)<<"{\"stage_preview_frames\":"<<stageFrames<<",\"stage_gameplay_ready\":false}"<<std::endl;
 std::osyncstream(std::cout)<<"{\"multi_ui_frames\":"<<customUiFrames<<",\"multi_ui_ready\":"<<(customUi.ready()?"true":"false")<<"}"<<std::endl;
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

