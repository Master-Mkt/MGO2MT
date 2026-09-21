#include "product_identity.h"
#include "menu_font.h"
#include "round_items.h"
#include "chat_view.h"
#include <imm.h>
#include <algorithm>
#include "player_menu.h"
#include "menu_theme.h"
#include "menu_audio.h"
#include "native_loadout.h"
#include "hold_selection_layout.h"
#include <stdexcept>
#include <cstring>
#include <fstream>
namespace mgo2mt {
namespace {
bool save_settings(const std::filesystem::path& path,const std::string& text){
 auto temporary=path;temporary+=L".tmp."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetCurrentThreadId())+L"."+std::to_wstring(GetTickCount64());
 HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file==INVALID_HANDLE_VALUE)return false;
 DWORD written=0;bool ok=WriteFile(file,text.data(),DWORD(text.size()),&written,nullptr)&&written==text.size()&&FlushFileBuffers(file);
 if(!CloseHandle(file))ok=false;
 if(ok)ok=MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
 if(!ok)DeleteFileW(temporary.c_str());
 return ok;
}
}
PlayerMenu::PlayerMenu(std::filesystem::path path,std::shared_ptr<ControllerInput> input,std::shared_ptr<GraphicsSettings> graphics):graphics_(std::move(graphics)),input_(input){
 gameplayPath_=path.parent_path()/L"player.cfg";{std::error_code error;const auto size=std::filesystem::file_size(gameplayPath_,error);if(!error&&size<=128){std::ifstream f(gameplayPath_);std::string magic,extra;unsigned version=0,y=0,tags=1;
  if(f>>magic>>version>>y&&magic==mgo2mt::brand::Format{"MGO2MT.PLAYER"}&&y<=1&&(version==1||(version==2&&f>>tags&&tags<=1))&&!(f>>extra)){yFirstPerson_=y!=0;enemyNameTags_=tags!=0;}
 }}
 cameraPath_=path.parent_path()/L"camera.cfg";{std::error_code error;const auto size=std::filesystem::file_size(cameraPath_,error);if(!error&&size<=128){std::ifstream f(cameraPath_,std::ios::binary);std::string text{std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};if(auto loaded=camera::decode(text))cameraSettings_=*loaded;}}
 controls_=std::make_unique<ControllerPanel>(path,std::move(input));controls_->return_label(L"ゲームへ戻る");dc_=CreateCompatibleDC(nullptr);
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=1280;info.bmiHeader.biHeight=-720;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
 bitmap_=CreateDIBSection(dc_,&info,DIB_RGB_COLORS,&pixels_,nullptr,0);if(!dc_||!bitmap_)throw std::runtime_error("Player menu surface");old_=SelectObject(dc_,bitmap_);
 for(int size:{30,23,20,17})fonts_.push_back(create_menu_font(size,FW_NORMAL));
}
PlayerMenu::~PlayerMenu(){SelectObject(dc_,old_);DeleteObject(bitmap_);DeleteDC(dc_);for(auto f:fonts_)DeleteObject(f);}
void PlayerMenu::open(player::Menu kind,bool controlsOnly){holdSelection_.cancel();if(kind==player::Menu::none){close();return;}if(kind_==kind&&controlsOnly_==controlsOnly)return;collect_cues();kind_=kind;radioMenu_.reset();radioSelected_.reset();if(kind==player::Menu::chat)radioMenu_.select();controlsOnly_=controlsOnly;tab_=0;gameplayFocus_=cameraFocus_=0;controls_->discard_changes();notice_.clear();pendingHigh_=0;cue(menu_audio::Confirm);}
void PlayerMenu::close(bool feedback){if(!visible())return;holdSelection_.cancel();cancel_composition();collect_cues();kind_=player::Menu::none;radioMenu_.reset();radioSelected_.reset();controls_->discard_changes();graphics_->cancel();graphics_->draft=graphics_->active;pendingHigh_=0;composition_.clear();composing_=imeEnterGuard_=false;if(feedback)cue(menu_audio::Cancel);}
void PlayerMenu::hold_assets(const std::filesystem::path& dataRoot){std::string error;holdNames_.load(dataRoot/L"item_drop_policy.json",error);holdIcons_.load(dataRoot/L"weapon-icons/index.tsv",error);holdGlyphs_.load(dataRoot/L"hold-font/index.tsv",error);holdEquipmentIcons_.load(dataRoot/L"equipment-icons",error);}
std::optional<hold_selection::Request> PlayerMenu::hold_selection_step(const hold_selection::Snapshot& snapshot,hold_selection::Input input){
 const auto before=holdSelection_.selected();input.otherMenu|=kind_!=player::Menu::none;const auto event=holdSelection_.step(snapshot,input);
 if(event.opened){holdSelectionFrom_=holdSelection_.selected();holdTransitionAt_=0;}
 if(event.cursor){holdSelectionFrom_=before;holdTransitionAt_=GetTickCount64();}
 if(event.opened)notice_.clear();if(event.cursor)cue(menu_audio::Cursor);if(event.cancelled)cue(menu_audio::Cancel);
 return event.confirm;
}
void PlayerMenu::select_tab(unsigned tab){if(tab>=4||tab==tab_||(controlsOnly_&&tab==1))return;collect_cues();tab_=tab;notice_.clear();cue(menu_audio::Cursor);}
bool PlayerMenu::tab_action(unsigned action){
 if(kind_!=player::Menu::settings||(action!=8&&action!=9))return false;
 if(controls_->capturing()||graphics_->pending())return true;
 auto next=(tab_+(action==8?3:1))%4;if(controlsOnly_&&next==1)next=(next+(action==8?3:1))%4;
 select_tab(next);return true;
}
void PlayerMenu::change_gameplay(bool confirm){
 const bool y=gameplayFocus_==0?!yFirstPerson_:yFirstPerson_,tags=gameplayFocus_==1?!enemyNameTags_:enemyNameTags_;
 const auto text=std::string("MGO2MT.PLAYER 2 ")+char('0'+y)+' '+char('0'+tags)+'\n';
 if(save_settings(gameplayPath_,text)){yFirstPerson_=y;enemyNameTags_=tags;notice_=L"保存しました。";cue(confirm?menu_audio::Confirm:menu_audio::Cursor);}
 else notice_=L"設定を保存できませんでした。変更前の設定を維持します。";
}
void PlayerMenu::cancel_composition(){
 const bool wasComposing=composing_;composing_=false;composition_.clear();pendingHigh_=0;
 if(wasComposing&&chatHwnd_&&IsWindow(chatHwnd_))if(auto im=ImmGetContext(chatHwnd_)){ImmNotifyIME(im,NI_COMPOSITIONSTR,CPS_CANCEL,0);ImmReleaseContext(chatHwnd_,im);}
}
void PlayerMenu::chat_session(std::shared_ptr<chat::Session> session){
 if(session!=chatSession_){if(chat_menu())close();chatSession_=std::move(session);chatState_={};chat_.clear();composition_.clear();composing_=imeEnterGuard_=false;radioMenu_.reset();radioSelected_.reset();chatSerial_=0;}
 sync_chat();
}
void PlayerMenu::sync_chat(){
 const auto state=chatSession_?chatSession_->state():chat::State{};
 if(state.generation!=chatState_.generation){if(chat_menu())close();chat_.clear();composition_.clear();composing_=imeEnterGuard_=false;pendingHigh_=0;chatSerial_=0;chatChannel_=0;notice_.clear();}
 if(chatSerial_&&state.serial==chatSerial_){
  if(state.delivery==chat::Delivery::echo_received){if(chat::utf8(chat_)==state.submittedText)chat_.clear();notice_=L"自分のメッセージの受信を確認しました。";chatSerial_=0;}
  else if(state.delivery==chat::Delivery::unconfirmed){notice_=L"送信を確認できません。自動再送はしていません。";chatSerial_=0;}
 }
 if(chatState_.joined&&!state.joined&&chat_menu())close();chatState_=state;
}
void PlayerMenu::select_chat_radio(){
 sync_chat();if(composing_||imeEnterGuard_)return;
 if(!visible()){open(player::Menu::chat);return;}if(!chat_menu())return;
 radioMenu_.select();radioSelected_.reset();notice_.clear();cue(menu_audio::Cursor);
}
void PlayerMenu::inventory_session(std::shared_ptr<items::ClientSession> session){
 const auto state=session?session->state():items::ClientState{};
 if(session!=inventorySession_||state.connection!=inventoryState_.connection){inventoryFocus_=inventorySlot_=0;notice_.clear();}
 else if(state.delivery!=inventoryState_.delivery){if(state.delivery==items::Delivery::confirmed){notice_=L"操作を反映しました。";cue(menu_audio::Confirm);}else if(state.delivery==items::Delivery::rejected){notice_=L"操作できません。距離・所持品・部屋の設定を確認してください。";cue(menu_audio::Cancel);}else if(state.delivery==items::Delivery::unconfirmed)notice_=L"結果を確認できません。次の出撃まで再操作を停止します。";}
 inventorySession_=std::move(session);inventoryState_=state;
}
void PlayerMenu::inventory_action(){
 if(!inventorySession_)return;auto state=inventorySession_->state();const auto action=items::wire::Action(inventoryFocus_+1);uint64_t target=0;float nearest=1500.f*1500.f;uint8_t selected=uint8_t(inventorySlot_);
 if(action==items::wire::Action::pickup||action==items::wire::Action::recover||action==items::wire::Action::use){
  if(state.world&&state.held)for(const auto& entity:state.world->entities){if((action==items::wire::Action::pickup&&entity.kind==items::PlacementKind::installed)||((action==items::wire::Action::recover||action==items::wire::Action::use)&&entity.kind!=items::PlacementKind::installed))continue;
   uint8_t candidate=255;for(uint8_t i=0;i<state.held->slots.size();++i)if(!state.held->slots[i].contents.item&&(entity.contents.domain==items::Domain::equipment?i==3:i<3)){candidate=i;break;}if(candidate==255)continue;
   auto p=state.context.position;float x=p.x-entity.position.x,y=p.y-entity.position.y,z=p.z-entity.position.z;auto distance=x*x+y*y+z*z;if(distance<nearest){nearest=distance;target=entity.key.id;selected=candidate;}}
 }
 if(inventorySession_->submit(action,selected,target)){notice_=L"HOSTの結果を待っています…";}else notice_=L"操作できません。対応する設置物・空きスロット・HOSTの状態を確認してください。";
}
void PlayerMenu::radio_session(std::shared_ptr<radio::Session> session){
 const auto state=session?session->state():radio::SessionState{};
 if(session!=radioSession_||state.generation!=radioState_.generation){if(radio_visible())close();radioSelected_.reset();}
 radioSession_=std::move(session);radioState_=state;
 if(radioSelected_){if(state.delivery==radio::DeliveryState::confirmed)notice_=L"無線の受信を確認しました。";
 else if(state.delivery==radio::DeliveryState::unconfirmed)notice_=L"無線の送信を確認できません。";}
}
void PlayerMenu::radio_direction(mgo2::radio::Direction direction){
 if(!radio_visible())return;
 if(auto selected=radioMenu_.digital(direction)){
  radioSelected_=*selected;
  if(radioSession_&&radioSession_->submit(radioState_.generation,selected->presetId,GetTickCount64())){notice_=L"無線を送信しています…";cue(menu_audio::Confirm);}
  else if(radioState_.status==radio::Status::probing)notice_=L"HOSTの無線対応を確認しています。";
  else if(radioState_.status!=radio::Status::ready)notice_=L"このHOSTでは無線を利用できません。";
  else if(!radioState_.eligible)notice_=L"出撃後に味方へ無線を送信できます。";
  else notice_=L"前の無線を確認しています。少し待ってください。";
 }else cue(menu_audio::Cursor);
}
void PlayerMenu::radio_digital_mask(uint32_t mask){
 // XInput physical D-pad only; analog mappings and diagonal chords do not select.
 if(mask==1)radio_direction(mgo2::radio::Direction::up);else if(mask==2)radio_direction(mgo2::radio::Direction::down);else if(mask==4)radio_direction(mgo2::radio::Direction::left);else if(mask==8)radio_direction(mgo2::radio::Direction::right);
}
void PlayerMenu::send_chat(){
 sync_chat();if(composing_||imeEnterGuard_)return;
 const auto result=chatSession_?chatSession_->submit(chatState_.generation,chat::utf8(chat_),chatChannel_==1,GetTickCount64()):chat::Submit::not_joined;
 switch(result){
 case chat::Submit::accepted:chatSerial_=chatSession_->state().serial;notice_=L"送信待ちです…";cue(menu_audio::Confirm);break;
 case chat::Submit::not_joined:notice_=L"部屋への参加が完了してから送信できます。";break;
 case chat::Submit::busy:notice_=L"前の送信を確認しています。少し待ってください。";break;
 case chat::Submit::unsupported_team:notice_=L"この接続ではチーム宛てを利用できません。";break;
 case chat::Submit::unknown_encoding:notice_=L"この接続では半角英数字・記号のみ送信できます。";break;
 case chat::Submit::command_disabled:notice_=L"スラッシュで始まるコマンドは利用できません。";break;
 default:notice_=L"送信できる文字種と126バイト以内の長さを確認してください。";break;
 }
}
void PlayerMenu::append_chat(wchar_t value){
 if(value==VK_BACK){pendingHigh_=0;if(!chat_.empty()){auto ch=chat_.back();chat_.pop_back();if(ch>=0xdc00&&ch<=0xdfff&&!chat_.empty()&&chat_.back()>=0xd800&&chat_.back()<=0xdbff)chat_.pop_back();}}
 else if(value>=0xd800&&value<=0xdbff)pendingHigh_=value;
 else if(value>=0xdc00&&value<=0xdfff){if(pendingHigh_&&chat_.size()+2<=128){chat_+=pendingHigh_;chat_+=value;}pendingHigh_=0;}
 else{pendingHigh_=0;if(value>=32&&value!=127&&chat_.size()<128)chat_+=value;}
}
void PlayerMenu::change_camera(bool confirm,bool reset,int direction){
 auto changed=cameraSettings_;if(reset)changed={};else if(cameraFocus_%3==2){auto& value=changed.speed[cameraFocus_/3];value=unsigned(std::clamp(int(value)+direction,1,10));}else{const auto index=(cameraFocus_/3)*2+cameraFocus_%3;changed.reversed[index]=!changed.reversed[index];}
 if(save_settings(cameraPath_,camera::encode(changed))){cameraSettings_=changed;notice_=reset?L"カメラ操作を初期設定に戻しました。":L"保存しました。";cue(confirm?menu_audio::Confirm:menu_audio::Cursor);}
 else notice_=L"設定を保存できませんでした。変更前の設定を維持します。";
}
bool PlayerMenu::message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 if(!visible())return false;
 if(hold_visible()){
  if(msg==WM_KILLFOCUS||(msg==WM_ACTIVATEAPP&&!wp)){holdSelection_.cancel();return false;}
  if(msg==WM_KEYDOWN&&wp==VK_ESCAPE){holdSelection_.cancel();if(!(lp&(1LL<<30)))cue(menu_audio::Cancel);return true;}
  // Direction levels and release confirmation are processed once by the frame
  // sampler; Win32 key repeats/Enter must not emit another selection or equip.
  return msg==WM_KEYDOWN||msg==WM_KEYUP||msg==WM_CHAR||msg==WM_LBUTTONDOWN||msg==WM_LBUTTONUP||msg==WM_MOUSEWHEEL;
 }
 if(chat_menu()&&msg==WM_KILLFOCUS){if(radio_visible()){close();return false;}}
 if(chat_menu()&&msg==WM_KEYDOWN&&wp==VK_F5){if(!(lp&(1LL<<30)))select_chat_radio();return true;}
 if(radio_visible()){
  if(msg==WM_KEYDOWN){if(lp&(1LL<<30))return true;
   if(wp==VK_ESCAPE){radioMenu_.cancel();radioSelected_.reset();if(radioMenu_.state()==mgo2::radio::MenuState::closed)close(true);else cue(menu_audio::Cancel);}
   else if(wp==VK_UP)radio_direction(mgo2::radio::Direction::up);else if(wp==VK_RIGHT)radio_direction(mgo2::radio::Direction::right);else if(wp==VK_DOWN)radio_direction(mgo2::radio::Direction::down);else if(wp==VK_LEFT)radio_direction(mgo2::radio::Direction::left);
   return true;
  }
  if(msg==WM_CHAR||msg==WM_KEYUP||msg==WM_LBUTTONUP||msg==WM_MOUSEWHEEL)return true;
 }
 if(text_entry()){
  if(hwnd)chatHwnd_=hwnd;
  sync_chat();if(!visible())return true;
  if(msg==WM_KILLFOCUS){const bool wasComposing=composing_;cancel_composition();imeEnterGuard_=wasComposing;return false;}
  if(msg==WM_IME_STARTCOMPOSITION){composing_=true;imeEnterGuard_=true;composition_.clear();}
  if((msg==WM_IME_STARTCOMPOSITION||msg==WM_IME_COMPOSITION)&&hwnd){if(auto im=ImmGetContext(hwnd)){
   RECT r{};GetClientRect(hwnd,&r);COMPOSITIONFORM form{};form.dwStyle=CFS_POINT;form.ptCurrentPos={168*r.right/1280,510*r.bottom/720};ImmSetCompositionWindow(im,&form);
   CANDIDATEFORM candidate{};candidate.dwStyle=CFS_CANDIDATEPOS;candidate.ptCurrentPos=form.ptCurrentPos;ImmSetCandidateWindow(im,&candidate);
   auto get=[&](DWORD type){const LONG bytes=ImmGetCompositionStringW(im,type,nullptr,0);if(bytes<=0||bytes>4096||bytes%2)return std::wstring{};std::wstring out(size_t(bytes)/2,0);if(ImmGetCompositionStringW(im,type,out.data(),DWORD(bytes))!=bytes)return std::wstring{};return out;};
   if(msg==WM_IME_COMPOSITION){if(lp&GCS_RESULTSTR){for(auto c:get(GCS_RESULTSTR))append_chat(c);composition_.clear();imeEnterGuard_=true;}if(lp&GCS_COMPSTR)composition_=get(GCS_COMPSTR);}
   ImmReleaseContext(hwnd,im);
  }return true;}
  if(msg==WM_IME_ENDCOMPOSITION){composing_=false;composition_.clear();return true;}
  if(msg==WM_IME_CHAR)return true;
  if(msg==WM_KEYUP&&wp==VK_RETURN){imeEnterGuard_=false;return true;}
  if(msg==WM_KEYDOWN&&composing_)return wp==VK_PROCESSKEY?false:true;
  if(msg==WM_KEYDOWN&&wp==VK_PROCESSKEY){imeEnterGuard_=true;return false;}
 }
 if(msg!=WM_KEYDOWN&&msg!=WM_KEYUP&&msg!=WM_CHAR&&msg!=WM_LBUTTONUP&&msg!=WM_LBUTTONDOWN&&msg!=WM_MOUSEWHEEL)return false;
 if(kind_==player::Menu::settings){
  if(msg==WM_KEYDOWN&&!controls_->capturing()&&!graphics_->pending()&&wp>=VK_F1&&wp<=VK_F4){select_tab(unsigned(wp-VK_F1));return true;}
  if(msg==WM_LBUTTONUP&&!controls_->capturing()&&!graphics_->pending()){RECT r{};GetClientRect(hwnd,&r);int x=r.right?short(LOWORD(lp))*1280/r.right:0,y=r.bottom?short(HIWORD(lp))*720/r.bottom:0;if(y>=132&&y<=176){for(unsigned i=0;i<4;++i)if(x>=150+245*int(i)&&x<385+245*int(i))select_tab(i);return true;}}
  if(tab_==0){controls_->message(hwnd,msg,wp,lp);if(controls_->back()){controls_->clear_back();close();}}else if(tab_==1){graphics_->message(hwnd,msg,wp,lp);if(graphics_->back())close();}else if(tab_==3){
   if(msg==WM_KEYDOWN&&!(lp&(1LL<<30))){if(wp==VK_ESCAPE)close(true);else if(wp==VK_UP||wp==VK_DOWN||wp==VK_TAB){cameraFocus_=(cameraFocus_+(wp==VK_UP?8:1))%9;cue(menu_audio::Cursor);}else if(wp==VK_LEFT||wp==VK_RIGHT||wp==VK_RETURN)change_camera(wp==VK_RETURN,false,wp==VK_LEFT?-1:1);else if(wp==VK_HOME)change_camera(true,true);}
   else if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(r.right&&r.bottom){const int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;if(x>=150&&x<1080&&y>=230&&y<536&&(y-230)%34<32){cameraFocus_=unsigned(y-230)/34;change_camera(true);}}}
  }else if(msg==WM_KEYDOWN&&!(lp&(1LL<<30))){
   if(wp==VK_ESCAPE)close(true);
   else if(wp==VK_UP||wp==VK_DOWN||wp==VK_TAB){gameplayFocus_^=1;cue(menu_audio::Cursor);}
   else if(wp==VK_LEFT||wp==VK_RIGHT||wp==VK_RETURN)change_gameplay(wp==VK_RETURN);
  }else if(tab_==2&&msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(r.right&&r.bottom){const int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;if(x>=150&&x<1080){if(y>=250&&y<298){gameplayFocus_=0;change_gameplay(true);}else if(y>=314&&y<362){gameplayFocus_=1;change_gameplay(true);}}}}return true;
 }
 if(msg==WM_KEYDOWN){if(wp==VK_ESCAPE){close(true);return true;}if(lp&(1LL<<30))return true;
  if(kind_==player::Menu::chat){if(wp==VK_LEFT||wp==VK_RIGHT||wp==VK_UP||wp==VK_DOWN){chatChannel_^=1;cue(menu_audio::Cursor);}if(wp==VK_RETURN)send_chat();}
  if(kind_==player::Menu::equipment){if(wp==VK_UP){inventoryFocus_=(inventoryFocus_+4)%5;cue(menu_audio::Cursor);}else if(wp==VK_DOWN){inventoryFocus_=(inventoryFocus_+1)%5;cue(menu_audio::Cursor);}else if(wp==VK_LEFT){inventorySlot_=(inventorySlot_+items::held_slot_count-1)%items::held_slot_count;cue(menu_audio::Cursor);}else if(wp==VK_RIGHT){inventorySlot_=(inventorySlot_+1)%items::held_slot_count;cue(menu_audio::Cursor);}else if(wp==VK_RETURN)inventory_action();}
 }else if(kind_==player::Menu::chat&&msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(r.right&&r.bottom){int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;if(y>=165&&y<209){int channel=x>=150&&x<450?0:x>=460&&x<760?1:-1;if(channel>=0&&unsigned(channel)!=chatChannel_){chatChannel_=unsigned(channel);cue(menu_audio::Cursor);}}}
 }else if(kind_==player::Menu::chat&&msg==WM_CHAR){if(!composing_)append_chat(wchar_t(wp));}

 return true;
}
const void* PlayerMenu::draw(){
 sync_chat();
 std::memset(pixels_,0,1280*720*4);SetBkMode(dc_,TRANSPARENT);
 auto text=[&](const std::wstring&s,int x,int y,int w=920,int h=45){SelectObject(dc_,fonts_[1]);SetTextColor(dc_,RGB(235,241,236));RECT r{x,y,x+w,y+h};DrawTextW(dc_,s.c_str(),int(s.size()),&r,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);};
 if(hold_visible()){
  const bool weaponsMenu=holdSelection_.kind()==hold_selection::Kind::weapons;
  const auto& rows=holdSelection_.choices();
  auto hasGlyphs=[&](std::wstring_view value){return std::all_of(value.begin(),value.end(),[&](wchar_t c){return c>=32&&c<=126&&holdGlyphs_.find(uint16_t(c));});};
  const auto selected=holdSelection_.selected();
  // Video t15/t30: selection stays at the lower outer corner; adjacent
  // entries occupy the vertical and inward horizontal arms. The 160 ms
  // smoothstep is native interpolation, not a measured original LA2 curve.
  double slide=holdTransitionAt_?std::clamp(double(GetTickCount64()-holdTransitionAt_)/160.,0.,1.):1.;slide=slide*slide*(3.-2.*slide);
  auto position=[&](size_t index){const auto to=hold_selection::card_position(weaponsMenu,rows.size(),selected,index);const auto from=hold_selection::card_position(weaponsMenu,rows.size(),holdSelectionFrom_,index);return hold_selection::CardPosition{int(from.x+(to.x-from.x)*slide),int(from.y+(to.y-from.y)*slide)};};
  const int anchor=weaponsMenu?1000:70;
  // This plain translucent frame follows the recording. Its original LA2
  // frame/gradient is not resolved; no synthetic icon is used in its place.
  menu_rect(dc_,anchor,598,210,78,RGB(99,75,47));
  menu_rect(dc_,weaponsMenu?1210:0,598,70,78,RGB(255,187,83));
  auto label=[&](std::wstring_view value,int x,int y,int w,int h,bool center=false){SelectObject(dc_,fonts_[2]);SetTextColor(dc_,RGB(255,187,83));RECT r{x,y,x+w,y+h};DrawTextW(dc_,value.data(),int(value.size()),&r,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS|(center?DT_CENTER:DT_LEFT));};
  if(rows.empty())label(weaponsMenu?L"所持している武器がありません。":L"所持している装備がありません。",330,230,620,40,true);
  const auto domain=weaponsMenu?items::Domain::weapon:items::Domain::equipment;
  auto name=[&](const hold_selection::Owned& row){if(!weaponsMenu&&row.item==67)return std::wstring(L"装備 #67");const auto* entry=holdNames_.find(row.item,domain);return entry?chat::wide(entry->name):(weaponsMenu?L"武器 #":L"装備 #")+std::to_wstring(row.item);};
  if(const auto* row=holdSelection_.choice();row&&!hasGlyphs(name(*row)))label(name(*row),330,190,620,40,true);
  if(weaponsMenu)if(const auto* item=holdSelection_.choice();item&&item->item<=65535&&!weapons::native_loadout::attack_supported(uint16_t(item->item)))label(L"使用準備中",330,365,620,30,true);
  const auto& bindings=input_->config;const auto& codes=bindings.device?bindings.gamepad:bindings.keyboard;const auto trigger=input_name(codes[weaponsMenu?14:15],bindings.device!=0);
  const auto dropButton=input_name(codes[5],bindings.device!=0);
  if(rows.empty())label(trigger+L"を離す：戻る",330,310,620,32,true);
  else{label(trigger+L"を押したまま ← →：選択",330,275,620,32,true);label(trigger+L"を離す：装備する    "+dropButton+L"：捨てる",330,310,620,32,true);}
  // Missing/unresolved original Latin glyphs fall back explicitly to the
  // existing readable font. Equipment IDs never use the weapon icon namespace.
  for(size_t i=0;i<rows.size();++i){const auto p=position(i);const auto title=name(rows[i]);if(!hasGlyphs(title))label(title,p.x+10,p.y+27,190,28,true);const auto& row=rows[i];const auto quantity=row.ammunition?std::to_wstring(row.magazine)+L" / "+std::to_wstring(row.reserve):std::to_wstring(row.quantity);if(!hasGlyphs(quantity))label(quantity,p.x+68,p.y+58,126,19);}
  finish_menu_surface(pixels_);
  auto pixels=std::span(static_cast<uint32_t*>(pixels_),1280*720);
  // Dim the complete scene/HUD, including black/transparent pixels. Existing
  // straight-alpha foreground is composed over black rather than made opaque.
  for(auto& pixel:pixels){const unsigned alpha=pixel>>24,out=alpha+(112*(255-alpha)+127)/255;uint32_t value=out<<24;for(unsigned k=0;k<3;++k)value|=((((pixel>>(8*k))&255)*alpha+out/2)/out)<<(8*k);pixel=value;}
  auto originalText=[&](std::wstring_view value,int x,int y,int width,int height,bool centered){
   if(!hasGlyphs(value)||value.empty())return;
   const int advance=std::min(int(height*32/26),width/int(value.size()));if(advance<1)return;
   if(centered)x+=(width-advance*int(value.size()))/2;
   for(auto c:value){auto glyph=*holdGlyphs_.find(uint16_t(c));for(auto& px:glyph.bgra)px=(px&0xff000000)|0xffbb53;weapons::paint_icon(glyph,pixels,1280,720,x,y,advance,height);x+=advance;}
  };
  if(const auto* row=holdSelection_.choice())originalText(name(*row),330,198,620,24,true);
  for(size_t i=0;i<rows.size();++i){const auto& row=rows[i];const auto p=position(i);
   if(weaponsMenu&&row.item<=65535)if(const auto* icon=holdIcons_.find(uint16_t(row.item)))weapons::paint_icon(*icon,pixels,1280,720,p.x+32,p.y+9,146,51);
   if(!weaponsMenu&&row.item<=65535)holdEquipmentIcons_.paint(uint16_t(row.item),pixels,p.x+5,p.y+2,200,60);
   originalText(name(row),p.x+8,p.y+31,194,16,true);
   const auto quantity=row.ammunition?std::to_wstring(row.magazine)+L" / "+std::to_wstring(row.reserve):std::to_wstring(row.quantity);
   if(hasGlyphs(quantity))originalText(quantity,p.x+68,p.y+62,126,14,false);
  }
  return pixels_;
 }
 menu_rect(dc_,95,100,1090,576,RGB(48,59,61));
 if(kind_==player::Menu::settings){menu_heading(dc_,fonts_[0],L"OPTION");constexpr const wchar_t* tabs[]{L"コントローラー [F1]",L"画質 [F2]",L"ゲーム操作 [F3]",L"カメラ [F4]"};for(unsigned i=0;i<4;++i)if(!controlsOnly_||i!=1){const int x=150+245*int(i);menu_tab(dc_,x,132,235,44,tab_==i);SelectObject(dc_,fonts_[2]);SetTextColor(dc_,RGB(235,241,236));RECT tabRect{x+12,141,x+225,169};DrawTextW(dc_,tabs[i],-1,&tabRect,DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);}
  if(tab_<2){auto p=tab_==0?controls_->draw(dc_,fonts_):graphics_->draw(dc_,fonts_);menu_focus_guides(dc_,p.x,p.y);}
  else if(tab_==3){text(L"カメラ操作カスタマイズ",169,194);constexpr const wchar_t* rows[]{L"通常カメラ・上下",L"通常カメラ・左右",L"通常カメラ・移動速度",L"肩越しカメラ・上下",L"肩越しカメラ・左右",L"肩越しカメラ・移動速度",L"主観カメラ・上下",L"主観カメラ・左右",L"主観カメラ・移動速度"};for(unsigned i=0;i<9;++i){const int y=230+34*int(i);menu_row(dc_,150,y,930,32,i,cameraFocus_==i);text(rows[i],169,y+2,610,30);const auto value=i%3==2?L"◀  "+std::to_wstring(cameraSettings_.speed[i/3])+L"  ▶":cameraSettings_.reversed[(i/3)*2+i%3]?L"◀ リバース ▶":L"◀ ノーマル ▶";text(value,805,y+2,265,30);}
   text(cameraFocus_%3==2?L"数値を大きくするとカメラが速く動きます。初期設定は5です。":cameraFocus_%3==1?L"選択したカメラの左右操作を反転します。":L"選択したカメラの上下操作を反転します。",150,548,970,33);text(L"上下：項目  左右：保存  Home：初期設定  キャンセル：戻る",150,585,970,33);text(notice_,150,624,970,38);menu_focus_guides(dc_,150,230+34*int(cameraFocus_));}

  else{menu_row(dc_,150,250,930,48,0,gameplayFocus_==0);text(L"匍匐中のY短押し",169,259);text(yFirstPerson_?L"主観／三人称":L"うつ伏せ／仰向け",680,259,370);
   menu_row(dc_,150,314,930,48,1,gameplayFocus_==1);text(L"敵のネームタグ表示",169,323);text(enemyNameTags_?L"ON":L"OFF",680,323,370);
   if(gameplayFocus_==0){text(yFirstPerson_?L"主観切替（補助）でうつ伏せ／仰向けを切り替えます。":L"主観切替（補助）で主観／三人称を切り替えます。",150,392);text(L"伏せ中のY長押し：死んだふり    立ち・しゃがみのY：敬礼",150,438);}
   else{text(L"照準を合わせた敵のクラン・名前・LV・体力を表示します。",150,392);text(L"ルームで敵ネームタグが許可されている場合に表示します。",150,438);}
   text(L"上下：項目    左右・決定：変更して保存    キャンセル：戻る",150,493);text(notice_,150,551);menu_focus_guides(dc_,150,gameplayFocus_?314:250);}
 }
 else{menu_heading(dc_,fonts_[0],radio_visible()?L"PRESET RADIO":kind_==player::Menu::chat?L"CHAT":L"EQUIPMENT");
  if(text_entry()){menu_tab(dc_,150,165,300,44,chatChannel_==0);menu_tab(dc_,460,165,300,44,chatChannel_==1);text(L"全員",170,171);text(chatState_.teamSupported?L"チーム":L"チーム（未対応）",480,171);
   chat::draw_history(dc_,fonts_[2],chatState_,{150,224,1080,454},GetTickCount64());
   menu_section(dc_,fonts_[3],L"MESSAGE",150,462,930);menu_row(dc_,150,495,930,69,0,true);text(chat_.empty()&&composition_.empty()?L"キーボードでメッセージを入力":chat_+composition_+L"｜",168,501,896,62);
   SelectObject(dc_,fonts_[3]);SetTextColor(dc_,RGB(238,218,181));RECT hint{150,573,1090,599};const auto label=chatState_.encoding==chat::Encoding::utf8?L"UTF-8・126バイト以内  /  Enter：送信  Esc：閉じる":chatState_.encoding==chat::Encoding::latin1?L"英欧文・126バイト以内  /  Enter：送信  Esc：閉じる":L"半角英数字・記号・126バイト以内  /  Enter：送信  Esc：閉じる";DrawTextW(dc_,label,-1,&hint,DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);menu_focus_guides(dc_,150,495);}

  else if(radio_visible()){
   menu_section(dc_,fonts_[1],radioMenu_.category()?chat::wide(mgo2::radio::defaultCategories()[unsigned(*radioMenu_.category())].text).c_str():L"CATEGORY",150,150,930);
   const auto& categories=mgo2::radio::defaultCategories();
   if(radioSelected_){const auto& choice=categories[unsigned(radioSelected_->category)].messages[unsigned(radioSelected_->choice)];text(chat::wide(choice.text),220,300,840,100);}
   else{
    constexpr const wchar_t* arrows[]{L"↑",L"→",L"↓",L"←"};constexpr int xs[]{440,810,440,140},ys[]{220,330,440,330};
    for(unsigned i=0;i<4;++i){const auto title=radioMenu_.category()?categories[unsigned(*radioMenu_.category())].messages[i].text:categories[i].text;menu_row(dc_,xs[i],ys[i],300,80,i,false);text(std::wstring(arrows[i])+L" "+chat::wide(title),xs[i]+12,ys[i]+12,274,65);}
    text(radioMenu_.category()?L"十字キーでメッセージを選択":L"十字キーでカテゴリーを選択",150,540,950,40);
   }
   text(L"SELECT / F5：チャット    Esc：戻る",150,588,950,40);
  }
  else{
   if(inventoryState_.held){const auto& c=inventoryState_.held->slots[inventorySlot_].contents;auto label=L"←  "+std::to_wstring(inventorySlot_+1)+L"  "+(c.item?chat::wide(items::item_label(c.domain,c.item)):L"空き")+L"  →";if(c.resource==items::Resource::ammunition)label+=L"   "+std::to_wstring(c.magazine)+L" / "+std::to_wstring(c.reserve);text(label,150,190);}
   else text(inventoryState_.status==items::ClientStatus::probing?L"HOSTの設置機能を確認しています…":L"この状態では設置・回収を利用できません。",150,190);
   const wchar_t* labels[]={L"捨てる",L"地面に設置する",L"近くのアイテムを空き枠に拾う",L"近くの設置武器を回収する",L"近くの設置武器を装備して使う"};
   for(unsigned i=0;i<5;++i){menu_row(dc_,150,248+48*i,930,42,i,inventoryFocus_==i);text(labels[i],168,254+48*i,890,36);}
   text(L"↑↓：操作  ←→：所持枠  Enter：決定  キャンセル：戻る",150,503,960,40);
  }
  if(!notice_.empty())text(notice_,150,chat_menu()?628:555,910,chat_menu()?42:90);
 }
 finish_menu_surface(pixels_);if(kind_==player::Menu::settings&&tab_==0)controls_->paint_original(pixels_);return pixels_;
}
void PlayerMenu::collect_cues(){for(auto c:controls_->cues())cue(c);for(auto c:graphics_->cues())cue(c);}
std::vector<unsigned> PlayerMenu::cues(){collect_cues();auto out=std::move(cues_);cues_.clear();return out;}
}
