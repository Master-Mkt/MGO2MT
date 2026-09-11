#include "menu_theme.h"
#include "login_screen.h"
#include "login_store.h"
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <syncstream>
#include <memory>
namespace mgo2win {
// Deployed account-only server SHA 972ba7f3380042643ec13d736851141e0eb1e06cc779cb47e287b2dfdddcb467 verified 2026-09-10.
void LoginScreen::open_characters(){if(!authenticated_||networkKeys_.empty())return;auto session=std::make_shared<AuthReply>(reply_);auto path=networkKeys_;characters_=std::make_unique<CharacterScreen>([session,path](const std::atomic_bool& cancel){return fetch_characters(path,*session,cancel);});characters_->catalog(catalog_);characters_->room_guard(roomJoinUncertain_);characters_->registration([session,path](const CharacterCreateRequest& request,const std::atomic_bool& cancel){return create_character(path,*session,request,cancel);},registrationState_);characters_->selection([session,path](uint32_t id,const std::atomic_bool& cancel){return select_character(path,*session,id,cancel,CharacterSelectionContract::channel_snapshot_v1);},selectionState_);characters_->rooms([session,path,udp=ports_?ports_->game_socket():~uintptr_t(0)](uint32_t id,const GameLobbyEntry& lobby,const std::atomic_bool& cancel,std::atomic_bool& refresh,const RoomPublish& publish,RoomRequests& requests){run_game_lobby(path,*session,id,lobby,cancel,refresh,publish,requests,udp);});}
void LoginScreen::show_character_preview(bool slotMode,bool selectionMode){if(authEnabled_)throw std::runtime_error("Character preview requires offline mode");show_port_preview();characters_=std::make_unique<CharacterScreen>([slotMode](const std::atomic_bool& cancel){for(int i=0;i<80&&!cancel;++i)Sleep(10);CharacterReply r;r.status=cancel?CharacterStatus::cancelled:CharacterStatus::success;r.stage=CharacterStage::list;r.list.slots=slotMode?3:8;for(int i=0;i<(slotMode?1:8);++i){CharacterEntry e;e.id=100+i;e.name=i?L"テストキャラクター"+std::to_wstring(i):L"Preview MAIN";e.main=!i;e.appearance[0]=i==2?1:0;e.appearance[1]=i%3;e.appearance[2]=i==1?15:i==2?11:11+i%3;e.appearance[5]=i==2?14:0;e.appearance[6]=i==1?5:0;e.appearance[3]=(!slotMode&&(i==0||i==2))?0:22;e.appearance[15]=46;e.appearance[17]=57;if(!slotMode){e.appearance[18]=103;e.appearance[25]=i==2?2:0;}r.list.entries.push_back(e);}return r;});characters_->catalog(catalog_);if(slotMode)characters_->registration([](const CharacterCreateRequest&,const std::atomic_bool&){CharacterCreateReply r;r.status=CharacterCreateStatus::cancelled;return r;},std::make_shared<CharacterRegistrationState>());if(selectionMode)characters_->selection([](uint32_t id,const std::atomic_bool& cancel){for(int i=0;i<50&&!cancel;++i)Sleep(10);CharacterSelectionReply r;r.status=cancel?CharacterSelectionStatus::cancelled:CharacterSelectionStatus::success;if(!cancel){r.request_may_have_been_sent=true;r.character.id=id;r.character.name=L"Preview MAIN";r.lobbies={{3,5733,12,L"Snake(Combat)",0,8},{4,5734,0,L"Campbell",0,10},{5,5735,45,L"Otacon",0,1},{6,5736,16,L"Meryl",0,4},{7,5737,8,L"Johnny",0,2},{8,5738,32,L"Liquid",0,3},{9,5739,1,L"Zero(Solo)",0,7}};for(unsigned i=0;i<6;++i)r.lobbies.push_back({uint16_t(20+i),uint16_t(5740+i),uint16_t(12+i*7),L"プレビューロビー "+std::to_wstring(i+1),0,2});r.lobbies.push_back({250,5999,0,L"未分類プレビュー",0,0});}return r;},std::make_shared<CharacterSelectionState>());if(selectionMode)characters_->rooms([](uint32_t,const GameLobbyEntry&,const std::atomic_bool& cancel,std::atomic_bool& refresh,const RoomPublish& publish,RoomRequests& requests){for(int i=0;i<20&&!cancel;++i)Sleep(10);if(cancel)return;RoomReply r;r.status=RoomStatus::ready;for(unsigned i=0;i<9;++i)r.rooms.push_back({1000+i,L"テストルーム "+std::to_wstring(i+1),uint8_t(i),16,0,0,i%2!=0});publish(r);while(!cancel){if(auto action=requests.take()){for(unsigned i=0;i<12&&!cancel;++i)Sleep(10);if(cancel)return;RoomReply result;result.status=RoomStatus::ready;result.event=action->event;result.requested_room=action->id;RoomDetail d;d.id=action->id;d.name=L"テストルーム "+std::to_wstring(action->id-999);d.comment=L"元のルーム情報に合わせたコメント欄。\nこれはオフラインの画面確認です。";d.subtype=1;d.capacity=16;d.players=2;d.password=action->id%2!=0;d.roster={{100,L"Preview HOST"},{101,L"テストPC"}};result.detail=d;if(action->event==RoomEvent::join){result.join_status=RoomJoinStatus::permission_checked;if(d.password&&std::wstring_view(action->password.data())!=L"abc"){result.join_status=RoomJoinStatus::rejected;result.status=RoomStatus::rejected;result.error=0xc0ffee03;}}publish(std::move(result));}if(refresh.exchange(false)){r.rooms.clear();publish(r);}Sleep(10);}});std::osyncstream(std::cout)<<"{\"character_offline_preview\":true}"<<std::endl;}
void LoginScreen::show_port_preview(bool external){if(authEnabled_)throw std::runtime_error("Port preview requires offline login mode");externalPorts_=external;ports_=std::make_unique<PortScreen>(store_.parent_path()/L"network.cfg",external,check_stun,input_,graphics_);std::osyncstream(std::cout)<<"{\"port_ui_preview\":true,\"authenticated\":false}"<<std::endl;}
LoginScreen::LoginScreen(std::filesystem::path store,bool authEnabled,std::function<AuthReply(const AuthCredentials&,const std::atomic_bool&)> transport,bool externalPorts,std::shared_ptr<ControllerInput> input,std::shared_ptr<GraphicsSettings> graphics,std::filesystem::path networkKeys):networkKeys_(std::move(networkKeys)),store_(std::move(store)),authEnabled_(authEnabled),transport_(std::move(transport)),externalPorts_(externalPorts),input_(std::move(input)),graphics_(std::move(graphics)){
 if(!graphics_)graphics_=std::make_shared<GraphicsSettings>(store_.parent_path()/L"graphics.cfg");
 if(!input_)input_=std::make_shared<ControllerInput>(store_.parent_path()/L"input.cfg");
 dc_=CreateCompatibleDC(nullptr);if(!dc_)throw std::runtime_error("Login DC failure");
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=1280;info.bmiHeader.biHeight=-720;
 info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
 bitmap_=CreateDIBSection(dc_,&info,DIB_RGB_COLORS,&pixels_,nullptr,0);
 if(!bitmap_){DeleteDC(dc_);throw std::runtime_error("Login surface failure");}old_=SelectObject(dc_,bitmap_);
 for(int size:{26,22,18,16})fonts_.push_back(CreateFontW(-size,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FIXED_PITCH,L"MS Gothic"));
 try{restored_=load_login(store_,form_);if(restored_){persistedMode_=form_.save_mode();form_.notice(persistedMode_==2?L"保存した入力情報でログインします…":L"保存したGAME IDを復元しました。");}}
 catch(const std::exception&){form_.notice(L"保存した入力情報を読み込めませんでした。再入力してください。");}
 if(authEnabled_&&restored_&&persistedMode_==2)autoAt_=GetTickCount64()+1000;
}
bool LoginScreen::save(){
 try{save_login(store_,form_);saved_=true;persistedMode_=form_.save_mode();return true;}
 catch(const std::exception&){form_.notice(L"入力情報を保存できませんでした。");return false;}
}
void LoginScreen::begin_auth(bool automatic){
 autoAt_=0;if(pending_||authenticated_)return;
 if(GetTickCount64()<retryAt_){form_.notice(L"少し待ってからもう一度お試しください。");return;}
 if(!form_.valid()||form_.length(0)>15){form_.notice(L"GAME IDは15文字以内で、パスワードとともに入力してください。");return;}
 if(authThread_.joinable())authThread_.join();
 auto credentials=std::make_shared<AuthCredentials>(form_.credential(0),form_.credential(1));
 if(form_.save_mode()==0)save();
 cancel_=false;done_=false;pending_=true;automatic_=automatic;++requests_;
 form_.notice(L"OpenMGO2にログインしています…");
 std::osyncstream(std::cout)<<"{\"authentication_started\":true,\"automatic\":"<<(automatic?"true":"false")<<"}"<<std::endl;
 try{authThread_=std::thread([this,credentials]{try{reply_=transport_(*credentials,cancel_);}catch(...){reply_=AuthReply{};}done_=true;});}
 catch(...){pending_=false;form_.notice(L"ログインを開始できませんでした。再度お試しください。");}
}
void LoginScreen::update(){
 if(autoAt_&&GetTickCount64()>=autoAt_)begin_auth(true);
 if(!pending_||!done_)return;authThread_.join();pending_=false;retryAt_=GetTickCount64()+2000;
 const char* status="network_error";
 if(leaving_||reply_.status==AuthStatus::cancelled){status="cancelled";form_.key(LoginForm::cancel);}
 else if(reply_.status==AuthStatus::success){status="success";authenticated_=true;*roomJoinUncertain_=false;if(save())form_.notice(L"Enter：ポートチェック・設定へ進む");form_.clear_password();cue(93);ports_=std::make_unique<PortScreen>(store_.parent_path()/L"network.cfg",externalPorts_,check_stun,input_,graphics_);ports_->continue_enabled=!networkKeys_.empty();}
 else if(reply_.status==AuthStatus::denied){status="denied";form_.notice(L"ログインできませんでした。ID・パスワードとアカウントの有効状態を確認してください。");}
 else if(reply_.status==AuthStatus::protocol_error){status="protocol_error";form_.notice(L"ログイン応答を確認できませんでした。時間をおいて再度お試しください。");}
 else form_.notice(L"OpenMGO2に接続できませんでした。通信状態を確認して再度お試しください。");
 std::osyncstream(std::cout)<<"{\"authentication_result\":\""<<status<<"\",\"http_status\":"<<reply_.http<<",\"automatic\":"<<(automatic_?"true":"false")<<"}"<<std::endl;
}
void LoginScreen::key(LoginForm::Key k){
 autoAt_=0;
 if(pending_){if(k==LoginForm::cancel){cancel_=true;leaving_=true;form_.notice(L"ログインを中止しています…");}return;}
 if(authenticated_){if(k==LoginForm::cancel)cue(form_.key(k));else if(k==LoginForm::confirm)ports_=std::make_unique<PortScreen>(store_.parent_path()/L"network.cfg",externalPorts_,check_stun,input_,graphics_);if(ports_)ports_->continue_enabled=!networkKeys_.empty();return;}
 auto before=form_.attempts();cue(form_.key(k));
 if(form_.attempts()!=before&&form_.valid()){
  if(authEnabled_)begin_auth(false);
  else{save();form_.notice(L"保存設定の動作確認中です。");}
 }
}
LoginScreen::~LoginScreen(){cancel_=true;if(authThread_.joinable())authThread_.join();SelectObject(dc_,old_);DeleteObject(bitmap_);DeleteDC(dc_);for(auto f:fonts_)DeleteObject(f);}
bool LoginScreen::message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 if(characters_){if(!characters_->creation_text_entry()&&!(characters_->creation_visible()&&wp>=VK_F1&&wp<=VK_F4)&&msg==WM_KEYDOWN&&wp!=VK_BACK&&!(lp&(1LL<<25))&&wp!=VK_ESCAPE&&wp!=VK_RETURN&&wp!=VK_TAB&&!(wp>=VK_LEFT&&wp<=VK_DOWN)){auto mapped=input_->keyboard_menu(unsigned(wp));if(mapped)wp=mapped;}return characters_->message(hwnd,msg,wp,lp);}
 if(ports_)return ports_->message(hwnd,msg,wp,lp);
 if((pending_||authenticated_)&&(msg==WM_CHAR||msg==WM_LBUTTONUP))return true;
 if(msg==WM_CHAR){autoAt_=0;if(wp>=32&&wp!=127)form_.character(static_cast<wchar_t>(wp));return true;}
 if(msg==WM_KEYDOWN){autoAt_=0;if(pending_&&wp!=VK_ESCAPE)return true;if(authenticated_&&wp!=VK_ESCAPE&&wp!=VK_RETURN)return true;using F=LoginForm;bool ctrl=(GetKeyState(VK_CONTROL)&0x8000)!=0;
  switch(wp){case VK_ESCAPE:key(F::cancel);break;case VK_TAB:key((GetKeyState(VK_SHIFT)&0x8000)?F::previous:F::next);break;
   case VK_UP:key(F::previous);break;case VK_DOWN:key(F::next);break;
   case VK_SPACE:if(form_.focus()>=4)key(F::confirm);break;
   case VK_RETURN:if(!(lp&(1LL<<30)))key(F::confirm);break;
   case VK_LEFT:key(F::left);break;case VK_RIGHT:key(F::right);break;
   case VK_HOME:form_.key(F::home);break;case VK_END:form_.key(F::end);break;
   case VK_BACK:form_.key(F::eraseBack);break;case VK_DELETE:form_.key(F::eraseForward);break;
   case 'A':if(ctrl)form_.key(F::selectAll);break;
  }return true;
 }
 if(msg==WM_LBUTTONUP){autoAt_=0;RECT r{};GetClientRect(hwnd,&r);if(r.right<=0||r.bottom<=0)return true;
  int x=int(static_cast<short>(LOWORD(lp)))*1280/r.right,y=int(static_cast<short>(HIWORD(lp)))*720/r.bottom;
  if(x>=365&&x<=1085&&y>=239&&y<=290)cue(form_.focus(0));
  else if(x>=365&&x<=1085&&y>=325&&y<=376)cue(form_.focus(1));
  else if(y>=479&&y<=529){int i=x>=740&&x<=1000?2:x>=440&&x<=700?3:-1;if(i>=0){cue(form_.focus(i));key(LoginForm::confirm);}}
  else if(y>=596&&y<=637){int i=x>=120&&x<335?4:x>=350&&x<625?5:x>=640&&x<=1160?6:-1;if(i>=0){cue(form_.focus(i));key(LoginForm::confirm);}}
  SetFocus(hwnd);return true;
 }return false;
}
const void* LoginScreen::draw(){
 update();
 if(characters_){for(auto c:characters_->cues())cue(c);if(characters_->back()){characters_->report();characters_.reset();}else return characters_->draw();}
 if(ports_&&ports_->proceed){ports_->proceed=false;open_characters();if(characters_)return characters_->draw();}
 if(ports_){for(auto c:ports_->cues())cue(c);if(ports_->back()){ports_->report();ports_.reset();}else return ports_->draw();}
 std::memset(pixels_,0,1280*720*4);
 auto fill=[&](int x,int y,int w,int h,COLORREF c){menu_fill(dc_,x,y,w,h,c);};
 auto text=[&](const std::wstring&s,int x,int y,int w,int h,int font,COLORREF c,UINT flags=DT_LEFT){RECT r{x,y,x+w,y+h};SelectObject(dc_,fonts_[font]);SetTextColor(dc_,menu_text_color(c));SetBkMode(dc_,TRANSPARENT);DrawTextW(dc_,s.c_str(),static_cast<int>(s.size()),&r,flags|DT_NOPREFIX);};
 text(L"OpenMGO2",840,78,338,35,1,RGB(224,228,213),DT_RIGHT);
 if(authenticated_){
  text(L"ログインしました",120,206,1040,52,0,RGB(229,241,207));
  text(L"OpenMGO2で認証されました。",120,283,1030,42,1,RGB(229,236,218));
  text(form_.notice(),120,341,1030,80,1,RGB(192,206,174),DT_WORDBREAK);
  text(L"Esc：戻る",83,690,1100,25,3,RGB(174,185,165));
 }else{
 text(L"アカウント情報を入力してください",118,164,1000,36,1,RGB(235,239,224));
 for(int i=0;i<2;++i){int y=239+i*86;bool active=form_.focus()==i;
  text(i?L"PASSWORD":L"GAME ID",144,y+14,215,34,1,RGB(232,236,224));
  fill(365,y,720,51,active?RGB(162,178,137):RGB(89,105,83));fill(367,y+2,716,47,RGB(23,33,27));
  auto value=form_.display(i);size_t caret=active?form_.caret():value.size();size_t begin=caret>49?caret-49:0;
  if(active&&form_.selected())fill(377,y+9,std::min(690,int(value.size())*13),31,RGB(81,101,70));
  text(value.substr(begin,52),377,y+12,686,34,0,RGB(235,238,226));
  if(active&&GetFocus()!=nullptr&&(GetTickCount64()/500)%2==0)fill(377+int(caret-begin)*13,y+10,2,30,RGB(219,234,191));
 }
 text(L"GAME ID：15文字まで / パスワード：64文字まで（半角）",367,391,720,28,3,RGB(178,190,164));
 for(int i=2;i<4;++i){int x=i==2?740:440;bool active=form_.focus()==i;
  fill(x,479,260,50,active?RGB(151,168,126):RGB(50,66,52));
  text(i==2?L"ログイン / LOGIN":L"戻る / BACK",x,491,260,36,1,active?RGB(18,28,19):RGB(228,234,215),DT_CENTER);
 }
 text(form_.notice(),120,423,1035,49,2,RGB(241,217,163),DT_WORDBREAK);
 text(L"保存設定",120,558,900,30,2,RGB(217,226,203));
 const wchar_t* modes[]={L"保存しない",L"IDを保存",L"IDとパスワードを保存（オートログイン）"};
 const int xs[]={120,350,640},ws[]={215,275,520};
 for(int i=0;i<3;++i){bool active=form_.focus()==i+4;
  if(active)fill(xs[i]-5,596,ws[i],41,RGB(63,83,58));
  text((form_.save_mode()==unsigned(i)?L"● ":L"○ ")+std::wstring(modes[i]),xs[i],604,ws[i],31,2,RGB(230,236,220));
 }
 text(authEnabled_?L"接続先：OpenMGO2":L"入力・保存の動作確認",120,650,900,27,3,RGB(182,192,169));
 text(L"Tab / ↑ ↓：項目移動    Enter：決定    Esc：戻る",83,690,1100,25,3,RGB(174,185,165));
 }
 finish_menu_surface(pixels_);
 return pixels_;
}
void LoginScreen::report()const{if(characters_)characters_->report();if(ports_)ports_->report();std::osyncstream(std::cout)<<"{\"login_form\":true,\"submit_attempts\":"<<form_.attempts()<<",\"returned_to_agreement\":"<<(form_.back()?"true":"false")<<",\"authentication_requests\":"<<requests_<<",\"authenticated\":"<<(authenticated_?"true":"false")<<",\"save_mode\":"<<persistedMode_<<",\"settings_written\":"<<(saved_?"true":"false")<<",\"settings_restored\":"<<(restored_?"true":"false")<<"}"<<std::endl;}
}

