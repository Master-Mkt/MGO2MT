#include "character_screen.h"
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <syncstream>
namespace mgo2win {
CharacterScreen::CharacterScreen(std::function<CharacterReply(const std::atomic_bool&)>transport,std::function<uint64_t()> clock):transport_(std::move(transport)),clock_(std::move(clock)){
 dc_=CreateCompatibleDC(nullptr);if(!dc_)throw std::runtime_error("Character DC failure");BITMAPINFO i{};i.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);i.bmiHeader.biWidth=1280;i.bmiHeader.biHeight=-720;i.bmiHeader.biPlanes=1;i.bmiHeader.biBitCount=32;
 bitmap_=CreateDIBSection(dc_,&i,DIB_RGB_COLORS,&pixels_,nullptr,0);if(!bitmap_){DeleteDC(dc_);throw std::runtime_error("Character surface failure");}old_=SelectObject(dc_,bitmap_);
 for(int size:{30,23,20,17})fonts_.push_back(CreateFontW(-size,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FIXED_PITCH,L"MS Gothic"));start();
}
void CharacterScreen::stop(){cancel_=true;if(worker_.joinable())worker_.join();pending_=false;}
CharacterScreen::~CharacterScreen(){stop();SelectObject(dc_,old_);DeleteObject(bitmap_);DeleteDC(dc_);for(auto f:fonts_)DeleteObject(f);}
void CharacterScreen::start(){if(pending_||GetTickCount64()<retryAt_)return;stop();slots_.load({});notice_.clear();reply_={};cancel_=false;done_=false;pending_=true;focus_=0;++requests_;cues_.push_back(93);
 try{worker_=std::thread([this]{try{reply_=transport_(cancel_);}catch(...){reply_={};}done_=true;});}catch(...){pending_=false;reply_.status=CharacterStatus::network_error;}
 std::osyncstream(std::cout)<<"{\"character_request_started\":true}"<<std::endl;
}
void CharacterScreen::begin_registration(CharacterCreateRequest request){
 if(pending_||!createTransport_||registrationState_->unresolved){creation_->registration_failed(L"前回の登録結果を一覧で確認してください。");return;}
 stop();registrationState_->unresolved=true;registrationState_->expected_id=0;registrationState_->expected_name=request.name;
 cancel_=false;done_=false;pending_=registering_=true;createReply_={};
 try{worker_=std::thread([this,request=std::move(request)]{try{createReply_=createTransport_(request,cancel_);}catch(...){createReply_.status=CharacterCreateStatus::outcome_unknown;createReply_.request_may_have_been_sent=true;}done_=true;});}
 catch(...){pending_=registering_=false;*registrationState_={};creation_->registration_failed(L"登録処理を開始できませんでした。もう一度お試しください。");}
}
void CharacterScreen::update(){
 if(!pending_||!done_)return;worker_.join();pending_=false;
 if(registering_){registering_=false;cues_.push_back(93);
  auto status=createReply_.status;
  // A successful response without an ID or an inconsistent transport result is ambiguous.
  if((status==CharacterCreateStatus::success&&!createReply_.created_id)||(createReply_.request_may_have_been_sent&&status!=CharacterCreateStatus::success&&status!=CharacterCreateStatus::rejected))status=CharacterCreateStatus::outcome_unknown;
  std::osyncstream(std::cout)<<"{\"character_registration_result\":true,\"status\":"<<int(status)<<",\"error\":"<<createReply_.error<<",\"may_have_been_sent\":"<<(createReply_.request_may_have_been_sent?"true":"false")<<"}"<<std::endl;
  if(status==CharacterCreateStatus::success||status==CharacterCreateStatus::outcome_unknown){
   registrationState_->expected_id=status==CharacterCreateStatus::success?createReply_.created_id:0;
   registrationNotice_=status==CharacterCreateStatus::success?L"登録が完了しました。一覧を更新しています。":L"登録結果を確認できませんでした。再送せず一覧を確認します。";
   creation_.reset();retryAt_=0;start();return;
  }
  *registrationState_={};
  auto message=status==CharacterCreateStatus::rejected?L"登録が受け付けられませんでした。名前や選択内容を確認してください。":status==CharacterCreateStatus::full?L"空きスロットがありません。一覧を再取得してください。":status==CharacterCreateStatus::cancelled?L"登録前に処理を中止しました。":L"登録前に接続できませんでした。通信設定を確認してください。";
  if(creation_)creation_->registration_failed(std::wstring(message)+(createReply_.error?L" ("+std::to_wstring(createReply_.error)+L")":L""));
  return;
 }
 retryAt_=GetTickCount64()+2000;slots_.load(reply_.status==CharacterStatus::success?reply_.list:CharacterList{});focus_=slots_.count()?0:1;
 if(registrationState_->unresolved){
  bool confirmed=false;if(reply_.status==CharacterStatus::success)for(unsigned i=0;i<reply_.list.entries.size();++i){const auto&e=reply_.list.entries[i];if(registrationState_->expected_id?e.id==registrationState_->expected_id:e.name==registrationState_->expected_name){slots_.select(i);focus_=int(i);confirmed=true;break;}}
  if(confirmed){*registrationState_={};registrationNotice_=L"登録されたPCを一覧で確認しました。";}
  else registrationNotice_=L"前回の登録を一覧で確認できていません。再取得して確認してください。新規登録は一時停止中です。";
 }
 notice_=registrationNotice_;cues_.push_back(93);report();
}
void CharacterScreen::tick_hold(){if(!pending_&&slots_.tick(clock_())){++deleteDialogs_;cues_.push_back(93);}}
void CharacterScreen::confirm_delete(){if(slots_.confirm()){++deleteYes_;notice_=L"PC削除の通信は準備中です。キャラクターは削除していません。";}cues_.push_back(93);}
void CharacterScreen::focus(int i){if(i!=focus_){slots_.reset_hold();focus_=i;if(i<int(slots_.count())){slots_.select(unsigned(i));notice_.clear();}if(cues_.size()<32)cues_.push_back(94);}}
void CharacterScreen::activate(){int n=int(slots_.count());if(focus_<n||focus_==n){if(n){if(slots_.occupied())notice_=L"PC選択はサーバー側の並び順対応後に有効になります。";else if(slots_.purchase_required())notice_=L"追加のPCスロット購入は準備中です。購入方法・価格は後日ご案内します。";else if(slots_.can_create()){slots_.reset_hold();creation_=std::make_unique<CharacterCreation>(catalog_,bool(createTransport_)&&!registrationState_->unresolved);notice_.clear();}cues_.push_back(93);}}else if(focus_==n+1)start();else if(focus_==n+2){stop();back_=true;cues_.push_back(93);}}
bool CharacterScreen::message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){update();
 if(creation_){if(creation_->registration_busy())return true;if(msg==WM_KEYDOWN&&(wp==VK_PRIOR||wp==VK_NEXT)&&!creation_->confirming()&&!creation_->discarding()){modelYaw_+=(wp==VK_PRIOR?-.12f:.12f);if(modelYaw_>3.141593f)modelYaw_-=6.283186f;if(modelYaw_< -3.141593f)modelYaw_+=6.283186f;return true;}bool handled=creation_->message(hwnd,msg,wp,lp);if(auto request=creation_->take_registration())begin_registration(std::move(*request));for(auto c:creation_->cues())if(cues_.size()<32)cues_.push_back(c);if(creation_->closed()){creation_->report();creation_.reset();}return handled;}
 if(msg==WM_KILLFOCUS||(msg==WM_ACTIVATEAPP&&!wp)){slots_.reset_hold();slots_.cancel_dialog();return false;}
 if(msg==WM_KEYUP&&wp==VK_BACK){slots_.release();return true;}
 if(msg==WM_CHAR)return true;
 if(msg==WM_KEYDOWN){
  if(wp==VK_BACK){if(!pending_&&focus_<=int(slots_.count())&&!(lp&(1LL<<25)))slots_.press(clock_(),(lp&(1LL<<30))!=0);return true;}
  // Navigation/confirmation cancels a partial hold; key repeats cannot rearm it.
  slots_.reset_hold();
  if(slots_.dialog()){if(wp==VK_ESCAPE){slots_.cancel_dialog();cues_.push_back(93);}else if(wp==VK_LEFT||wp==VK_RIGHT||wp==VK_TAB){slots_.choose(wp==VK_TAB?!slots_.yes():wp==VK_LEFT);cues_.push_back(94);}else if((wp==VK_RETURN||wp==VK_SPACE)&&!(lp&(1LL<<30)))confirm_delete();return true;}
  if(modelAvailable_&&slots_.occupied()&&(wp==VK_LEFT||wp==VK_RIGHT)){modelYaw_+=(wp==VK_LEFT?-.12f:.12f);if(modelYaw_>3.141593f)modelYaw_-=6.283186f;if(modelYaw_< -3.141593f)modelYaw_+=6.283186f;return true;}
  if(wp==VK_ESCAPE){stop();back_=true;cues_.push_back(93);}else if(!pending_){int first=slots_.count()?0:1,last=int(slots_.count())+2,n=last-first+1;if(wp==VK_DOWN||wp==VK_TAB){bool prev=wp==VK_TAB&&(GetKeyState(VK_SHIFT)&0x8000);focus(first+(focus_-first+(prev?n-1:1))%n);}else if(wp==VK_UP)focus(first+(focus_-first+n-1)%n);else if(wp==VK_HOME)focus(first);else if(wp==VK_END)focus(last);else if((wp==VK_RETURN||wp==VK_SPACE)&&!(lp&(1LL<<30)))activate();}return true;
 }
 if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(!r.right||!r.bottom)return true;int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;slots_.reset_hold();
  if(slots_.dialog()){if(y>=395&&y<449){if(x>=390&&x<610){slots_.choose(true);confirm_delete();}else if(x>=670&&x<890){slots_.choose(false);confirm_delete();}}return true;}
  if(y>=617&&y<662&&x>=850&&x<1160){stop();back_=true;cues_.push_back(93);}else if(!pending_){int n=int(slots_.count());if(y>=225&&y<225+n*39&&x>=120&&x<690)focus((y-225)/39);else if(y>=617&&y<662){if(x>=120&&x<480){focus(n);activate();}else if(x>=510&&x<820){focus(n+1);activate();}}}SetFocus(hwnd);return true;
 }return false;
}
const void* CharacterScreen::draw(){update();tick_hold();memset(pixels_,0,1280*720*4);
 if(creation_){creation_->draw(dc_,fonts_);GdiFlush();auto*p=static_cast<unsigned char*>(pixels_);for(size_t i=3;i<1280*720*4;i+=4)p[i]=(p[i-3]||p[i-2]||p[i-1])?255:0;return pixels_;}
 auto fill=[&](int x,int y,int w,int h,COLORREF c){RECT r{x,y,x+w,y+h};auto b=CreateSolidBrush(c);FillRect(dc_,&r,b);DeleteObject(b);};
 auto text=[&](std::wstring_view s,int x,int y,int w,int h,int font,COLORREF c,UINT flags=DT_LEFT){RECT r{x,y,x+w,y+h};SelectObject(dc_,fonts_[font]);SetTextColor(dc_,c);SetBkMode(dc_,TRANSPARENT);DrawTextW(dc_,s.data(),int(s.size()),&r,flags|DT_NOPREFIX);};
 text(L"OpenMGO2",850,82,310,30,1,RGB(215,227,200),DT_RIGHT);text(L"プレイヤースロット",120,160,1000,42,0,RGB(229,238,214));
 text(L"4キャラまで無料 ／ 5キャラ目以降はPCスロットを購入",120,202,1040,22,3,RGB(192,204,178));
 // Worker owns reply_ until done/join. Never read it while pending.
 int n=pending_?0:int(slots_.count());
 if(!pending_&&reply_.status==CharacterStatus::success){for(int i=0;i<n;++i){int y=225+39*i;bool occupied=i<int(reply_.list.entries.size());if(i==int(slots_.selected()))fill(120,y,570,37,RGB(73,96,60));text(std::to_wstring(i+1),130,y+8,35,28,2,RGB(192,211,173));text(occupied?reply_.list.entries[i].name:i>=int(slots_.capacity())?L"PCスロットを購入":i<4?L"未登録（無料）":L"未登録（追加枠）",180,y+7,420,31,1,RGB(233,240,220));if(occupied&&reply_.list.entries[i].main)text(L"MAIN",604,y+9,76,27,3,RGB(224,213,164),DT_RIGHT);}if(!n)text(L"利用可能なプレイヤースロットはありません。",120,245,1040,42,1,RGB(228,235,213));
  // Occupancy gates the selected account appearance.
  if(slots_.occupied())text(modelAvailable_?(modelPartial_?L"外見表示（一部の装備・色は未対応）":L"キャラクター表示"):L"3Dモデル未読込",720,174,440,28,3,RGB(186,200,173),DT_CENTER);
  text(notice_,120,563,1040,44,2,RGB(237,221,181),DT_WORDBREAK);
  if(n){bool active=focus_==n;fill(120,617,360,45,active?RGB(151,168,126):RGB(50,66,52));text(slots_.action(),120,627,360,32,1,active?RGB(18,28,19):RGB(228,234,215),DT_CENTER);}
 }
 else{std::wstring message=L"OpenMGO2からキャラクター一覧を取得しています…";if(!pending_){message=reply_.status==CharacterStatus::server_error?L"サーバーに受け付けられませんでした。ログインからやり直してください。":reply_.status==CharacterStatus::protocol_error?L"サーバーの応答を確認できませんでした。":L"OpenMGO2に接続できませんでした。通信設定を確認してください。";const wchar_t*stages[]={L"接続データ",L"入口への接続",L"接続先の取得",L"アカウントへの接続",L"セッション確認",L"一覧取得"};text(std::wstring(L"確認箇所：")+stages[unsigned(reply_.stage)]+L"  /  エラー "+std::to_wstring(reply_.error),120,350,1040,55,2,RGB(192,204,178));}text(message,120,249,1040,85,1,RGB(237,221,181),DT_WORDBREAK);}
 for(int i=0;i<2;++i){int x=510+i*340;bool active=!pending_&&focus_==n+1+i;fill(x,617,310,45,active?RGB(151,168,126):RGB(50,66,52));text(i?L"設定へ戻る":pending_?L"取得中…":L"再取得",x,627,310,32,1,active?RGB(18,28,19):RGB(228,234,215),DT_CENTER);}
 if(!pending_&&slots_.occupied()){text(L"Backspace 3秒長押し：PC削除の確認",720,582,440,32,3,RGB(212,220,195),DT_CENTER);auto elapsed=slots_.held_ms(clock_());if(elapsed){fill(750,611,380,5,RGB(52,67,47));fill(750,611,int(380*elapsed/3000),5,RGB(222,231,202));}}
 if(slots_.dialog()){fill(300,252,680,235,RGB(135,156,113));fill(303,255,674,229,RGB(25,37,28));text(L"PCを削除しますか？",330,278,620,43,0,RGB(237,241,226),DT_CENTER);text(reply_.list.entries[slots_.selected()].name,330,339,620,36,1,RGB(237,221,181),DT_CENTER);for(int i=0;i<2;++i){int x=i?670:390;bool active=slots_.yes()==!i;fill(x,395,220,54,active?RGB(151,168,126):RGB(50,66,52));text(i?L"NO":L"YES",x,409,220,34,1,active?RGB(18,28,19):RGB(228,234,215),DT_CENTER);}}
 text(slots_.dialog()?L"← →：YES / NO    Enter：決定    Esc：取り消し":L"↑ ↓：項目移動    ← →：モデル回転    Enter：決定    Esc：設定へ戻る",83,690,1120,25,3,RGB(174,185,165));GdiFlush();auto*p=static_cast<unsigned char*>(pixels_);for(size_t i=3;i<1280*720*4;i+=4)p[i]=(p[i-3]||p[i-2]||p[i-1])?255:0;return pixels_;
}
void CharacterScreen::report()const{if(pending_)return;std::osyncstream(std::cout)<<"{\"character_list_result\":true,\"status\":"<<int(reply_.status)<<",\"stage\":"<<int(reply_.stage)<<",\"error\":"<<reply_.error<<",\"count\":"<<reply_.list.entries.size()<<",\"slots\":"<<slots_.capacity()<<",\"display_rows\":"<<slots_.count()<<",\"server_slots\":"<<reply_.list.slots<<",\"purchase_required\":"<<(slots_.purchase_required()?"true":"false")<<",\"occupied_slot\":"<<(slots_.occupied()?"true":"false")<<",\"delete_dialogs\":"<<deleteDialogs_<<",\"delete_yes\":"<<deleteYes_<<",\"requests\":"<<requests_<<",\"selection_sent\":false,\"deletion_sent\":false,\"model_rendered\":"<<(modelRendered_?"true":"false")<<"}"<<std::endl;}
}

