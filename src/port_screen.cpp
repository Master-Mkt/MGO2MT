#include "menu_theme.h"
#include "port_screen.h"
#include <cstring>
#include <iostream>
#include <syncstream>
#include <stdexcept>
namespace mgo2win {
PortScreen::PortScreen(std::filesystem::path path,bool external,std::function<StunResult(uintptr_t,const std::atomic_bool&)> probe,std::shared_ptr<ControllerInput> input,std::shared_ptr<GraphicsSettings> graphics):store_(std::move(path)),external_(external),probe_(std::move(probe)),input_(std::move(input)),graphics_(std::move(graphics)){
 if(!graphics_)graphics_=std::make_shared<GraphicsSettings>(store_.parent_path()/L"graphics.cfg");
 auto inputPath=store_.parent_path()/L"input.cfg";if(!input_)input_=std::make_shared<ControllerInput>(inputPath);controls_=std::make_unique<ControllerPanel>(inputPath,input_);
 try{restored_=load_ports(store_,settings_);notice_=restored_?L"保存したポート設定を復元しました。":L"使用するポートを選び、チェックしてください。";}
 catch(...){notice_=L"保存した設定を読めませんでした。初期値を表示します。";}
 number_=std::to_wstring(settings_.port);
 dc_=CreateCompatibleDC(nullptr);if(!dc_)throw std::runtime_error("Port screen DC failure");
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=1280;info.bmiHeader.biHeight=-720;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
 bitmap_=CreateDIBSection(dc_,&info,DIB_RGB_COLORS,&pixels_,nullptr,0);if(!bitmap_){DeleteDC(dc_);throw std::runtime_error("Port screen surface failure");}old_=SelectObject(dc_,bitmap_);
 for(int size:{30,23,20,17})fonts_.push_back(CreateFontW(-size,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FIXED_PITCH,L"MS Gothic"));
 cues_.push_back(93);
 std::osyncstream(std::cout)<<"{\"port_settings_visible\":true,\"settings_restored\":"<<(restored_?"true":"false")<<"}"<<std::endl;
}
void PortScreen::stop_probe(){cancel_=true;if(worker_.joinable())worker_.join();pending_=false;}
PortScreen::~PortScreen(){stop_probe();reservation_.reset();SelectObject(dc_,old_);DeleteObject(bitmap_);DeleteDC(dc_);for(auto f:fonts_)DeleteObject(f);}
void PortScreen::focus(int n){if(n==focus_)return;focus_=n;selected_=false;if(cues_.size()<32)cues_.push_back(94);}
void PortScreen::open_speed(){speed_choice_=settings_.bandwidth_kbps/256-1;speed_open_=true;}
void PortScreen::accept_speed(){
 auto speed=static_cast<uint16_t>((speed_choice_+1)*256);
 if(settings_.bandwidth_kbps!=speed){settings_.bandwidth_kbps=speed;saved_=false;notice_=L"通信速度を変更しました。「設定を保存」で確定してください。";}
 speed_open_=false;cues_.push_back(93);
}
void PortScreen::invalidate(){stop_probe();reservation_.reset();result_={};stun_={};saved_=false;notice_=L"設定を変更しました。チェックしてから保存してください。";}
void PortScreen::update_probe(){
 if(!pending_||!done_)return;worker_.join();pending_=false;
 const char* status="network_error";
 switch(stun_.status){
 case StunStatus::success:status="success";notice_=L"OpenMGO2とのUDP往復を確認しました。対戦相手からの接続可否は別の確認が必要です。";break;
 case StunStatus::timeout:status="timeout";notice_=L"応答がありません。通信経路・ファイアウォール・検査サーバーの状態を確認してください。";break;
 case StunStatus::protocol_error:status="protocol_error";notice_=L"検査応答を確認できませんでした。時間をおいて再度お試しください。";break;
 case StunStatus::server_error:status="server_error";notice_=L"検査サーバーがエラーを返しました。時間をおいて再度お試しください。";break;
 case StunStatus::cancelled:status="cancelled";notice_=L"確認を中止しました。";break;
 default:notice_=L"UDP検査を送信できませんでした。Windowsの通信許可を確認してください。";break;
 }
 cues_.push_back(93);
 std::osyncstream(std::cout)<<"{\"stun_result\":\""<<status<<"\",\"mapped_port\":"<<stun_.mapped_port<<",\"attempts\":"<<stun_.attempts<<",\"error\":"<<stun_.error<<",\"peer_inbound_tested\":false}"<<std::endl;
}
void PortScreen::check(){
 if(pending_)return;stop_probe();stun_={};
 uint16_t n;if(!parse_port(number_,n)){notice_=L"ポート番号を1024～65535の範囲で入力してください。";focus(1);return;}
 settings_.port=n;result_=reservation_.check(settings_);++checks_;
 switch(result_.status){
 case PortStatus::available:notice_=L"このPCでUDPポートを使用できます。外部からの到達は未確認です。";break;
 case PortStatus::in_use:notice_=L"このポートは使用中です。別の番号か自動選択をお試しください。";break;
 case PortStatus::denied:notice_=L"ポートを確保できません。使用中またはWindowsの制限の可能性があります。";break;
 default:notice_=L"ポートを確認できませんでした。番号を変更して再度お試しください。";break;
 }
 std::osyncstream(std::cout)<<"{\"local_port_check\":true,\"available\":"<<(result_.status==PortStatus::available?"true":"false")<<",\"port\":"<<result_.port<<",\"error\":"<<result_.error<<",\"external_reachability_tested\":false}"<<std::endl;
 if(external_&&result_.status==PortStatus::available){
  cancel_=false;done_=false;pending_=true;notice_=L"OpenMGO2とのUDP通信を確認しています…（Escで中止）";
  try{worker_=std::thread([this]{try{stun_=probe_(reservation_.native_socket(),cancel_);}catch(...){stun_={};stun_.status=StunStatus::network_error;}done_=true;});}
  catch(...){pending_=false;notice_=L"検査を開始できませんでした。再度お試しください。";}
 }
}
void PortScreen::save(){
 uint16_t n;if(!parse_port(number_,n)){notice_=L"有効なポート番号を入力してください。";focus(1);return;}
 if(result_.status!=PortStatus::available){notice_=L"先にポートチェックを実行してください。";return;}
 settings_.port=n;
 try{save_ports(store_,settings_);saved_=true;notice_=L"設定を保存しました。対戦相手からの接続可否は未確認です。";}
 catch(...){saved_=false;notice_=L"設定を保存できませんでした。保存先を確認してください。";}
}
void PortScreen::activate(){
 cues_.push_back(93);
 switch(focus_){case 0:settings_.automatic=!settings_.automatic;invalidate();break;case 1:focus(2);break;case 2:check();break;case 3:save();break;case 4:settings_={};number_=L"5730";invalidate();break;case 5:back_=true;reservation_.reset();break;case 6:open_speed();break;case 7:if(continue_enabled)proceed=true;break;}
}
bool PortScreen::message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 update_probe();
 if(msg==WM_KILLFOCUS)controls_->cancel_capture();
 if(graphics_->pending())return graphics_->message(hwnd,msg,wp,lp);
 if(!controls_->capturing()&&!pending_&&msg==WM_KEYDOWN&&!(lp&(1LL<<25))&&wp!=VK_ESCAPE&&wp!=VK_RETURN&&wp!=VK_TAB&&wp!=VK_F3&&!(wp>=VK_LEFT&&wp<=VK_DOWN)&&(controls_tab_||focus_!=1)){unsigned mapped=input_->keyboard_menu(unsigned(wp));if(mapped)wp=mapped;}
 if(!controls_->capturing()&&!pending_){
  int tab=-1;if(msg==WM_KEYDOWN){if(wp==VK_F1)tab=0;if(wp==VK_F2)tab=1;if(wp==VK_F3)tab=2;}
  if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(r.right&&r.bottom){int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;if(y>=153&&y<197){if(x>=120&&x<450)tab=0;else if(x>=470&&x<810)tab=1;else if(x>=830&&x<1160)tab=2;}}}
  if(tab>=0){controls_tab_=tab==1;graphics_tab_=tab==2;speed_open_=false;controls_->clear_back();cues_.push_back(93);return true;}
 }
 if(graphics_tab_){bool used=graphics_->message(hwnd,msg,wp,lp);if(graphics_->back())graphics_tab_=false;return used;}
 if(controls_tab_){bool used=controls_->message(hwnd,msg,wp,lp);if(controls_->back()){controls_->clear_back();controls_tab_=false;}return used;}
 if(pending_&&(msg==WM_KEYDOWN||msg==WM_CHAR||msg==WM_LBUTTONUP)){
  if(msg==WM_KEYDOWN&&wp==VK_ESCAPE){stop_probe();stun_={};stun_.status=StunStatus::cancelled;notice_=L"確認を中止しました。再チェックできます。";std::osyncstream(std::cout)<<"{\"stun_cancelled\":true}"<<std::endl;}return true;
 }
 if(continue_enabled&&!speed_open_&&msg==WM_KEYDOWN&&wp==VK_F4){proceed=true;cues_.push_back(93);return true;}
 if(speed_open_){
  if(msg==WM_CHAR)return true;
  if(msg==WM_KEYDOWN){
   if(wp==VK_ESCAPE){speed_open_=false;cues_.push_back(93);}
   else if((wp==VK_RETURN||wp==VK_SPACE)&&!(lp&(1LL<<30)))accept_speed();
   else if(wp==VK_TAB){accept_speed();focus((GetKeyState(VK_SHIFT)&0x8000)?1:2);}
   else {int previous=speed_choice_;
    if(wp==VK_UP||wp==VK_LEFT)speed_choice_=speed_choice_>0?speed_choice_-1:0;
    else if(wp==VK_DOWN||wp==VK_RIGHT)speed_choice_=speed_choice_<7?speed_choice_+1:7;
    else if(wp==VK_HOME)speed_choice_=0;else if(wp==VK_END)speed_choice_=7;
    if(previous!=speed_choice_&&cues_.size()<32)cues_.push_back(94);
   }return true;
  }
  if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(!r.right||!r.bottom)return true;
   int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;
   if(x>=410&&x<700&&y>=383&&y<623){speed_choice_=(y-383)/30;accept_speed();}
   else speed_open_=false;
   SetFocus(hwnd);return true;
  }
 }
 if(msg==WM_CHAR){if(focus_==1&&wp>=L'0'&&wp<=L'9'){if(selected_){number_.clear();selected_=false;}if(number_.size()<5){number_+=wchar_t(wp);invalidate();}}return true;}
 if(msg==WM_KEYDOWN){
  if(wp==VK_ESCAPE){back_=true;reservation_.reset();cues_.push_back(93);}
  else if(wp==VK_TAB||wp==VK_DOWN||wp==VK_UP){bool prev=wp==VK_UP||(wp==VK_TAB&&(GetKeyState(VK_SHIFT)&0x8000));const int next[]={1,6,3,4,5,0,2},prior[]={5,0,6,2,3,4,1};focus(focus_==7?(prev?5:0):continue_enabled&&((prev&&focus_==0)||(!prev&&focus_==5))?7:prev?prior[focus_]:next[focus_]);}
  else if((wp==VK_RETURN||wp==VK_SPACE)&&!(lp&(1LL<<30))){if(wp==VK_RETURN||focus_!=1)activate();}
  else if((wp==VK_LEFT||wp==VK_RIGHT)&&focus_==0){settings_.automatic=!settings_.automatic;invalidate();cues_.push_back(94);}
  else if(focus_==1){if(wp=='A'&&(GetKeyState(VK_CONTROL)&0x8000))selected_=true;
   else if(wp==VK_BACK||wp==VK_DELETE){if(selected_||wp==VK_DELETE)number_.clear();else if(!number_.empty())number_.pop_back();selected_=false;invalidate();}}
  return true;
 }
 if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(!r.right||!r.bottom)return true;
  int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;
  if(continue_enabled&&y>=625&&y<667&&x>=825&&x<1165){focus(7);activate();}
  else if(y>=219&&y<=264&&x>=410&&x<=1130){focus(0);activate();}
  else if(y>=282&&y<=332&&x>=410&&x<=710){focus(1);selected_=true;}
  else if(y>=341&&y<383&&x>=410&&x<700){focus(6);activate();}
  else if(y>=558&&y<=610){for(int i=0;i<4;++i)if(x>=120+i*270&&x<=365+i*270){focus(i+2);activate();break;}}
  SetFocus(hwnd);return true;
 }return false;
}
const void* PortScreen::draw(){
 update_probe();
 std::memset(pixels_,0,1280*720*4);
 auto fill=[&](int x,int y,int w,int h,COLORREF c){menu_fill(dc_,x,y,w,h,c);};
 auto text=[&](std::wstring_view s,int x,int y,int w,int h,int font,COLORREF c,UINT flags=DT_LEFT){RECT r{x,y,x+w,y+h};SelectObject(dc_,fonts_[font]);SetTextColor(dc_,menu_text_color(c));SetBkMode(dc_,TRANSPARENT);DrawTextW(dc_,s.data(),int(s.size()),&r,flags|DT_NOPREFIX);};
 menu_heading(dc_,fonts_[0],L"OPTION");
 POINT guide{120,219};
 if(graphics_tab_){guide=graphics_->draw(dc_,fonts_);for(auto c:graphics_->cues())if(cues_.size()<32)cues_.push_back(c);}
 else if(controls_tab_){guide=controls_->draw(dc_,fonts_);for(auto c:controls_->cues())if(cues_.size()<32)cues_.push_back(c);}
 else {
 text(L"OpenMGO2",850,82,310,30,1,RGB(215,227,200),DT_RIGHT);
 menu_row(dc_,120,219,1040,50,0,focus_==0);
 menu_row(dc_,120,282,1040,50,1,focus_==1);
 menu_row(dc_,120,341,1040,42,2,focus_==6);
 for(int i=0;i<3;++i)menu_band(dc_,120,392+i*31,1040,31,i+3);
 text(L"ポートの選択",125,232,280,34,1,RGB(224,232,212));

 text(settings_.automatic?L"● 自動（使用中なら空き番号を選択）":L"● 手動（指定した番号を使用）",422,232,700,32,1,RGB(229,238,214));
 text(L"UDPポート番号",125,296,280,34,1,RGB(224,232,212));
 fill(410,282,290,50,focus_==1?RGB(161,180,133):RGB(89,105,83));fill(412,284,286,46,selected_?RGB(66,91,55):RGB(23,33,27));
 text(number_,426,294,255,34,1,RGB(235,240,227));
 text(L"1024～65535 / 初期値 5730",730,299,420,30,3,RGB(188,201,172));
 text(L"通信速度",125,350,280,30,1,RGB(224,232,212));
 fill(410,341,290,42,focus_==6?RGB(161,180,133):RGB(89,105,83));fill(412,343,286,38,RGB(23,33,27));
 text(std::to_wstring(settings_.bandwidth_kbps)+L" kbps",426,350,220,30,1,RGB(235,240,227));
 text(L"▼",660,350,30,30,2,RGB(235,240,227));
 text(L"256～2048 kbps",730,354,420,25,3,RGB(188,201,172));
 std::wstring local=L"未チェック";if(result_.status==PortStatus::available)local=L"使用可能（UDP "+std::to_wstring(result_.port)+L"）";else if(result_.status!=PortStatus::unchecked)local=L"確保できませんでした";
 text(L"このPCでの使用",125,398,310,30,2,RGB(202,216,185));text(local,455,398,680,30,2,RGB(230,235,217));
 std::wstring external=L"未チェック";
 if(!external_)external=L"未実施（ローカル確認モード）";
 else if(pending_)external=L"確認中…";
 else if(stun_.status==StunStatus::success)external=L"往復OK（外部UDP "+std::to_wstring(stun_.mapped_port)+L"）";
 else if(stun_.status==StunStatus::cancelled)external=L"中止";
 else if(stun_.status==StunStatus::timeout)external=L"応答なし（ポート閉鎖とは未確定）";
 else if(stun_.status!=StunStatus::unchecked)external=L"確認できませんでした";
 text(L"OpenMGO2との通信",125,429,310,30,2,RGB(202,216,185));text(external,455,429,680,30,2,RGB(234,211,156));
 text(L"ルーター自動設定",125,460,310,30,2,RGB(202,216,185));text(L"未実装",455,460,680,30,2,RGB(188,201,172));
 text(notice_,120,492,1040,52,2,RGB(244,218,161),DT_WORDBREAK);
 const wchar_t* labels[]={L"チェック",L"設定を保存",L"初期値に戻す",L"戻る"};
 for(int i=0;i<4;++i){int x=120+270*i;bool active=focus_==i+2;fill(x,558,245,50,active?RGB(151,168,126):RGB(50,66,52));text(labels[i],x,571,245,35,1,active?RGB(18,28,19):RGB(228,234,215),DT_CENTER);}
 text(saved_?L"保存済み":L"変更は「設定を保存」で確定します",120,639,700,28,3,RGB(181,195,166));
 if(continue_enabled){fill(825,625,340,42,focus_==7?RGB(151,168,126):RGB(50,66,52));text(L"キャラクター一覧へ [F4]",825,635,340,30,2,focus_==7?RGB(18,28,19):RGB(228,234,215),DT_CENTER);}
 text(L"Tab / ↑ ↓：項目移動    ← →：選択方式    Enter：決定    Esc：戻る",83,690,1120,25,3,RGB(174,185,165));
 if(speed_open_){
  fill(408,381,294,244,RGB(161,180,133));fill(410,383,290,240,RGB(23,33,27));
  for(int i=0;i<8;++i){bool active=i==speed_choice_;menu_row(dc_,411,383+i*30,288,30,i,active);
   text(std::to_wstring((i+1)*256)+L" kbps",426,387+i*30,260,26,2,active?RGB(18,28,19):RGB(228,234,215));}
 }
 if(speed_open_)guide={411,383+speed_choice_*30};
 else if(focus_==0)guide={120,219};
 else if(focus_==1)guide={120,282};
 else if(focus_==6)guide={120,341};
 else if(focus_==7)guide={825,625};
 else guide={120+(focus_-2)*270,558};
 }
 for(int i=0;i<3;++i){bool selected=i==2?graphics_tab_:i==1?controls_tab_:!graphics_tab_&&!controls_tab_;int x=120+i*355,w=330;menu_tab(dc_,x,153,w,44,selected);const wchar_t* labels[]={L"ネットワーク [F1]",L"コントローラー [F2]",L"画質 [F3]"};text(labels[i],x+12,162,w-24,34,1,RGB(233,239,222));}
 menu_focus_guides(dc_,guide.x,guide.y);
 finish_menu_surface(pixels_);return pixels_;
}
void PortScreen::report()const{controls_->report();std::osyncstream(std::cout)<<"{\"port_settings_report\":true,\"checks\":"<<checks_<<",\"saved\":"<<(saved_?"true":"false")<<",\"restored\":"<<(restored_?"true":"false")<<",\"returned\":"<<(back_?"true":"false")<<",\"bandwidth_kbps\":"<<settings_.bandwidth_kbps<<",\"external_reachability_tested\":false}"<<std::endl;}
}
