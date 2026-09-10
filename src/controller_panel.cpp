#include "controller_panel.h"
#include <iostream>
#include <syncstream>
namespace mgo2win {
ControllerPanel::ControllerPanel(std::filesystem::path p,std::shared_ptr<ControllerInput> input):path_(std::move(p)),input_(std::move(input)),draft_(input_->config){}
void ControllerPanel::select_device(int delta){draft_.device=1-draft_.device;dirty_=true;capture_=-1;notice_=L"機器ごとの割り当てを表示しています。保存すると有効になります。";cues_.push_back(94);}
void ControllerPanel::bind(unsigned code){assign_input(draft_,unsigned(capture_),code);capture_=-1;armed_=false;dirty_=true;notice_=L"割り当てを変更しました。重複する割り当ては入れ替えます。";cues_.push_back(93);}
void ControllerPanel::save(){try{save_input(path_,draft_);input_->config=draft_;input_->reset();dirty_=false;notice_=L"保存して適用しました。";std::osyncstream(std::cout)<<"{\"controller_saved\":true,\"device\":"<<draft_.device<<",\"slot\":"<<draft_.slot<<"}"<<std::endl;}catch(...){notice_=L"設定を保存できませんでした。保存先を確認してください。";}}
void ControllerPanel::activate(){cues_.push_back(93);
 if(focus_==0)select_device(1);
 else if(focus_==1){draft_.slot=(draft_.slot+1)%4;dirty_=true;connected_=false;}
 else if(focus_>=2&&focus_<=9){capture_=page_*8+focus_-2;armed_=false;notice_=draft_.device?L"一度すべて離してから、割り当てるボタン・方向を入力してください。Escで取消。":L"割り当てるキーを押してください。Escで取消。";}
 else if(focus_==10){page_=(page_+2)%3;}
 else if(focus_==11){page_=(page_+1)%3;}
 else if(focus_==12)save();
 else if(focus_==13){InputConfig defaults;if(draft_.device)draft_.gamepad=defaults.gamepad;else draft_.keyboard=defaults.keyboard;dirty_=true;notice_=L"この機器の割り当てを初期値へ戻しました。保存で確定します。";}
 else if(focus_==14){back_=true;draft_=input_->config;dirty_=false;}
}
bool ControllerPanel::sample(const PadSample& s){connected_=s.connected;held_=s.held;if(capture_<0)return false;
 if(!draft_.device)return true;
 if(!s.connected){armed_=false;return true;}if(!s.held){armed_=true;return true;}
 if(armed_){uint32_t bits=s.pressed;if(bits&&(bits&(bits-1))==0){unsigned code=0;while(!(bits&(1u<<code)))++code;bind(code);}}
 return true;
}
bool ControllerPanel::message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_KILLFOCUS){cancel_capture();return false;}
 if(capture_>=0){
  if(msg==WM_KEYDOWN){if(wp==VK_ESCAPE){cancel_capture();notice_=L"割り当ての変更を取り消しました。";cues_.push_back(93);}
   else if(!draft_.device&&!(lp&(1LL<<30))&&!(lp&(1LL<<25))){if(valid_input_key(unsigned(wp)))bind(unsigned(wp));else notice_=L"このキーは設定操作用です。別のキーを選んでください。Escで取消。";}return true;}
  if(msg==WM_CHAR)return true;
  if(msg==WM_LBUTTONUP){cancel_capture();notice_=L"割り当ての変更を取り消しました。";return true;}
 }
 if(msg==WM_CHAR)return true;
 if(msg==WM_KEYDOWN){
  if(wp==VK_ESCAPE){draft_=input_->config;dirty_=false;back_=true;}
  else if(wp==VK_TAB||wp==VK_UP||wp==VK_DOWN){bool prev=wp==VK_UP||(wp==VK_TAB&&(GetKeyState(VK_SHIFT)&0x8000));focus_=(focus_+(prev?14:1))%15;cues_.push_back(94);}
  else if(wp==VK_LEFT||wp==VK_RIGHT){if(focus_==0)select_device(1);else if(focus_==1){draft_.slot=(draft_.slot+(wp==VK_LEFT?3:1))%4;dirty_=true;}else {page_=(page_+(wp==VK_LEFT?2:1))%3;}cues_.push_back(94);}
  else if(wp==VK_PRIOR||wp==VK_NEXT){page_=(page_+(wp==VK_PRIOR?2:1))%3;cues_.push_back(94);}
  else if((wp==VK_RETURN||wp==VK_SPACE)&&!(lp&(1LL<<30)))activate();return true;
 }
 if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(!r.right||!r.bottom)return true;int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;
  if(y>=217&&y<257&&x>=410&&x<760){focus_=0;activate();}
  else if(y>=217&&y<257&&x>=810&&x<1160){focus_=1;activate();}
  else if(y>=298&&y<554&&x>=120&&x<1160){focus_=2+(y-298)/32;activate();}
  else if(y>=263&&y<294&&x>=930&&x<1040){focus_=10;activate();}
  else if(y>=263&&y<294&&x>=1050&&x<1160){focus_=11;activate();}
  else if(y>=599&&y<642){if(x>=120&&x<440)focus_=12;else if(x>=470&&x<800)focus_=13;else if(x>=830&&x<1160)focus_=14;else return true;activate();}
  SetFocus(hwnd);return true;
 }return false;
}
void ControllerPanel::draw(HDC dc,const std::vector<HFONT>& fonts){
 auto fill=[&](int x,int y,int w,int h,COLORREF c){RECT r{x,y,x+w,y+h};auto b=CreateSolidBrush(c);FillRect(dc,&r,b);DeleteObject(b);};
 auto text=[&](std::wstring s,int x,int y,int w,int h,int font,COLORREF color){RECT r{x,y,x+w,y+h};SelectObject(dc,fonts[font]);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);DrawTextW(dc,s.c_str(),int(s.size()),&r,DT_LEFT|DT_NOPREFIX);};
 auto box=[&](int i,std::wstring label,int x,int y,int w,int h){bool a=focus_==i;fill(x,y,w,h,a?RGB(151,168,126):RGB(37,53,42));text(label,x+12,y+8,w-24,h-6,2,a?RGB(18,28,19):RGB(232,237,218));};
 text(L"使用する入力機器",125,227,280,30,1,RGB(224,232,212));box(0,draft_.device?L"XInput  ◀ ▶":L"キーボード  ◀ ▶",410,217,350,40);
 box(1,L"XInput #"+std::to_wstring(draft_.slot+1)+(connected_?L"  接続中":L"  未接続"),810,217,350,40);
 text(L"割り当て："+std::to_wstring(page_+1)+L" / 3  （各8項目）",125,272,800,27,2,RGB(208,220,192));box(10,L"◀ 前",930,263,110,31);box(11,L"次 ▶",1050,263,110,31);
 for(int i=0;i<8;++i){unsigned action=page_*8+i;int y=298+i*32;bool active=focus_==i+2;fill(120,y,1040,31,active?RGB(62,84,55):RGB(23,33,27));
  text(action_name(action),135,y+5,590,28,2,RGB(229,237,216));
  auto code=draft_.device?draft_.gamepad[action]:draft_.keyboard[action];
  text(capture_==int(action)?L"入力待ち…":input_name(code,draft_.device!=0),760,y+5,375,28,2,RGB(240,216,160));}
 text(notice_,120,565,1040,28,3,RGB(244,218,161));
 box(12,L"設定を保存・適用",120,599,320,43);box(13,L"この機器を初期値へ",470,599,330,43);box(14,L"ネットワークへ戻る",830,599,330,43);
 text(dirty_?L"未保存の変更があります":L"保存した設定が有効です",120,649,1040,27,3,RGB(191,206,175));
 text(L"F1/F2：タブ  PgUp/PgDn：ページ  Enter：変更  Esc/クリック：入力待ち取消",83,690,1120,25,3,RGB(174,185,165));
}
void ControllerPanel::report()const{std::osyncstream(std::cout)<<"{\"controller_panel\":true,\"device\":"<<input_->config.device<<",\"slot\":"<<input_->config.slot<<",\"unsaved\":"<<(dirty_?"true":"false")<<",\"capturing\":"<<(capturing()?"true":"false")<<"}"<<std::endl;}
}
