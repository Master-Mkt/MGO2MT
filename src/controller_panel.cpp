#include "system_ui_icons.h"
#include "menu_theme.h"
#include "controller_panel.h"
#include "menu_audio.h"
#include <iostream>
#include <syncstream>
#include <cmath>
namespace mgo2mt {
ControllerPanel::ControllerPanel(std::filesystem::path p,std::shared_ptr<ControllerInput> input):path_(std::move(p)),input_(std::move(input)),draft_(input_->config){}
void ControllerPanel::discard_changes(){draft_=input_->config;cancel_capture();dirty_=back_=connected_=preview_running_=false;held_=0;left_magnitude_=right_magnitude_=0;cues_.clear();notice_=L"保存した設定を表示しています。変更後は保存して適用してください。";}
void ControllerPanel::select_device(int delta){draft_.device=1-draft_.device;dirty_=true;capture_=-1;notice_=L"機器ごとの割り当てを表示しています。保存すると有効になります。";cue(menu_audio::Cursor);}
void ControllerPanel::bind(unsigned code){assign_input(draft_,unsigned(capture_),code);capture_=-1;armed_=false;dirty_=true;notice_=L"割り当てを変更しました。重複する割り当ては入れ替えます。";cue(menu_audio::Confirm);}
void ControllerPanel::save(){try{save_input(path_,draft_);input_->config=draft_;input_->reset();dirty_=false;notice_=L"保存して適用しました。";std::osyncstream(std::cout)<<"{\"controller_saved\":true,\"device\":"<<draft_.device<<",\"slot\":"<<draft_.slot<<"}"<<std::endl;}catch(...){notice_=L"設定を保存できませんでした。保存先を確認してください。";}}
void ControllerPanel::change_analog(int delta){
 unsigned* values[]={&draft_.left_deadzone,&draft_.right_deadzone,&draft_.run_threshold,&draft_.run_hysteresis};
 if(focus_<2||focus_>5)return;auto index=unsigned(focus_-2);int low=index==2?10:0,high=index<2?90:index==2?100:std::min(30,int(draft_.run_threshold)-1);
 unsigned previous=*values[index];*values[index]=unsigned(std::clamp(int(*values[index])+delta,low,high));if(previous==*values[index])return;
 draft_.run_hysteresis=std::min(draft_.run_hysteresis,draft_.run_threshold-1);dirty_=true;preview_running_=false;
 notice_=L"左右で1%ずつ調整。左・右の遊びと歩行→走行の境界を保存して適用します。";cue(menu_audio::Cursor);
}
void ControllerPanel::select_page(int delta){int next=(page_+delta+4)%4;if(next==page_)return;page_=next;if(page_==3&&focus_>=6&&focus_<=9)focus_=5;preview_running_=false;cue(menu_audio::Cursor);}
void ControllerPanel::move_focus(bool prev){do{focus_=(focus_+(prev?14:1))%15;}while(page_==3&&focus_>=6&&focus_<=9);}
void ControllerPanel::activate(){
 if(focus_==0)select_device(1);
 else if(focus_==1){draft_.slot=(draft_.slot+1)%4;dirty_=true;connected_=false;cue(menu_audio::Cursor);}
 else if(focus_>=2&&focus_<=9){if(page_==3)change_analog(1);else{capture_=page_*8+focus_-2;armed_=false;notice_=draft_.device?L"一度すべて離してから、割り当てるボタン・方向を入力してください。Escで取消。":L"割り当てるキーを押してください。Escで取消。";cue(menu_audio::Confirm);}}
 else if(focus_==10)select_page(-1);
 else if(focus_==11)select_page(1);
 else if(focus_==12){save();cue(menu_audio::Confirm);}
 else if(focus_==13){InputConfig defaults;if(page_==3){draft_.left_deadzone=defaults.left_deadzone;draft_.right_deadzone=defaults.right_deadzone;draft_.run_threshold=defaults.run_threshold;draft_.run_hysteresis=defaults.run_hysteresis;}else if(draft_.device)draft_.gamepad=defaults.gamepad;else draft_.keyboard=defaults.keyboard;dirty_=true;notice_=L"表示中の設定を初期値へ戻しました。保存で確定します。";cue(menu_audio::Confirm);}
 else if(focus_==14){back_=true;draft_=input_->config;dirty_=false;cue(menu_audio::Cancel);}
}
bool ControllerPanel::sample(const PadSample& s){connected_=s.connected;held_=s.raw_held?s.raw_held:s.held;
 left_magnitude_=s.connected&&std::isfinite(s.raw_left_magnitude)?std::clamp(s.raw_left_magnitude,0.f,1.f):0.f;
 right_magnitude_=s.connected&&std::isfinite(s.raw_right_magnitude)?std::clamp(s.raw_right_magnitude,0.f,1.f):0.f;
 float adjusted=std::max(0.f,(left_magnitude_-float(draft_.left_deadzone)*.01f)/(1.f-float(draft_.left_deadzone)*.01f));
 preview_running_=input_running(adjusted,preview_running_,draft_);if(capture_<0)return false;
 if(!draft_.device)return true;
 if(!s.connected){armed_=false;return true;}if(!held_){armed_=true;return true;}
 if(armed_){uint32_t bits=s.pressed;if(bits&&(bits&(bits-1))==0){unsigned code=0;while(!(bits&(1u<<code)))++code;bind(code);}}
 return true;
}
bool ControllerPanel::message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 if(msg==WM_KILLFOCUS){cancel_capture();return false;}
 if(capture_>=0){
  if(msg==WM_KEYDOWN){if(wp==VK_ESCAPE){cancel_capture();notice_=L"割り当ての変更を取り消しました。";cue(menu_audio::Cancel);}
   else if(!draft_.device&&!(lp&(1LL<<30))&&!(lp&(1LL<<25))){if(valid_input_key(unsigned(wp)))bind(unsigned(wp));else notice_=L"このキーは設定操作用です。別のキーを選んでください。Escで取消。";}return true;}
  if(msg==WM_CHAR)return true;
  if(msg==WM_LBUTTONUP){cancel_capture();notice_=L"割り当ての変更を取り消しました。";cue(menu_audio::Cancel);return true;}
 }
 if(msg==WM_CHAR)return true;
 if(msg==WM_KEYDOWN){
  if(wp==VK_ESCAPE){draft_=input_->config;dirty_=false;back_=true;cue(menu_audio::Cancel);}
  else if(wp==VK_TAB||wp==VK_UP||wp==VK_DOWN){bool prev=wp==VK_UP||(wp==VK_TAB&&(GetKeyState(VK_SHIFT)&0x8000));move_focus(prev);cue(menu_audio::Cursor);}
  else if(wp==VK_LEFT||wp==VK_RIGHT){if(focus_==0)select_device(1);else if(focus_==1){draft_.slot=(draft_.slot+(wp==VK_LEFT?3:1))%4;dirty_=true;cue(menu_audio::Cursor);}else if(page_==3&&focus_>=2&&focus_<=5)change_analog(wp==VK_LEFT?-1:1);else select_page(wp==VK_LEFT?-1:1);}
  else if(wp==VK_PRIOR||wp==VK_NEXT)select_page(wp==VK_PRIOR?-1:1);
  else if((wp==VK_RETURN||wp==VK_SPACE)&&!(lp&(1LL<<30)))activate();return true;
 }
 if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(!r.right||!r.bottom)return true;int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;int oldFocus=focus_;auto queued=cues_.size();
  if(y>=217&&y<257&&x>=410&&x<760){focus_=0;activate();}
  else if(y>=217&&y<257&&x>=810&&x<1160){focus_=1;activate();}
  else if(y>=298&&y<554&&x>=120&&x<1160){int row=(y-298)/32;if(page_!=3||row<4){focus_=2+row;if(page_==3&&x<930)change_analog(-1);else activate();}}
  else if(y>=263&&y<294&&x>=120&&x<916){int selected=(x-120)/199;select_page(selected-page_);}
  else if(y>=263&&y<294&&x>=930&&x<1040){focus_=10;activate();}
  else if(y>=263&&y<294&&x>=1050&&x<1160){focus_=11;activate();}
  else if(y>=599&&y<642){if(x>=120&&x<440)focus_=12;else if(x>=470&&x<800)focus_=13;else if(x>=830&&x<1160)focus_=14;else return true;activate();}
  if(focus_!=oldFocus&&cues_.size()==queued)cue(menu_audio::Cursor);SetFocus(hwnd);return true;
 }return false;
}
POINT ControllerPanel::draw(HDC dc,const std::vector<HFONT>& fonts){
 auto fill=[&](int x,int y,int w,int h,COLORREF c){menu_fill(dc,x,y,w,h,c);};
 auto text=[&](std::wstring s,int x,int y,int w,int h,int font,COLORREF color){RECT r{x,y,x+w,y+h};SelectObject(dc,fonts[font]);SetTextColor(dc,menu_text_color(color));SetBkMode(dc,TRANSPARENT);DrawTextW(dc,s.c_str(),int(s.size()),&r,DT_LEFT|DT_NOPREFIX);};
 auto box=[&](int i,std::wstring label,int x,int y,int w,int h){bool a=focus_==i;fill(x,y,w,h,a?RGB(151,168,126):RGB(37,53,42));text(label,x+12,y+8,w-24,h-6,2,a?RGB(18,28,19):RGB(232,237,218));};
 menu_band(dc,120,217,1040,40,0);
 text(L"使用する入力機器",125,227,280,30,1,RGB(224,232,212));box(0,draft_.device?L"XInput  ◀ ▶":L"キーボード  ◀ ▶",410,217,350,40);
 box(1,L"XInput #"+std::to_wstring(draft_.slot+1)+(connected_?L"  接続中":L"  未接続"),810,217,350,40);
 const wchar_t* pages[]={L"ボタン 1",L"ボタン 2",L"移動・視線",L"スティック調整"};
 for(int i=0;i<4;++i){menu_tab(dc,120+i*199,263,191,31,page_==i);text(pages[i],132+i*199,270,170,26,2,RGB(231,224,208));}
 box(10,L"◀ 前",930,263,110,31);box(11,L"次 ▶",1050,263,110,31);
 if(page_<3)for(int i=0;i<8;++i){unsigned action=page_*8+i;int y=298+i*32;bool active=focus_==i+2;menu_row(dc,120,y,1040,31,i,active);
  text(action_name(action),135,y+5,590,28,2,RGB(229,237,216));
  auto code=draft_.device?draft_.gamepad[action]:draft_.keyboard[action];
  text(capture_==int(action)?L"入力待ち…":input_name(code,draft_.device!=0),760,y+5,375,28,2,RGB(240,216,160));}
 else{
  const wchar_t* labels[]={L"左スティックの遊び（デッドゾーン）",L"右スティックの遊び（デッドゾーン）",L"歩行 → 走行の傾き",L"走行 → 歩行の境界の余裕"};
  const unsigned values[]={draft_.left_deadzone,draft_.right_deadzone,draft_.run_threshold,draft_.run_hysteresis};
  for(int i=0;i<4;++i){int y=298+i*32;menu_row(dc,120,y,1040,31,i,focus_==i+2);text(labels[i],135,y+5,610,28,2,RGB(229,237,216));text(L"◀    "+std::to_wstring(values[i])+L" %    ▶",760,y+5,375,28,2,RGB(240,216,160));}
  auto adjusted=[](float raw,unsigned dz){return std::max(0.f,(raw-float(dz)*.01f)/(1.f-float(dz)*.01f));};
  float left=adjusted(left_magnitude_,draft_.left_deadzone),right=adjusted(right_magnitude_,draft_.right_deadzone);
  std::wstring preview=!connected_?L"未接続":left<=0?L"停止":preview_running_?L"走行":L"歩行";
  const std::wstring readouts[]={L"入力確認（設定中の値でプレビュー）",L"左："+std::to_wstring(int(left_magnitude_*100))+L" % → "+std::to_wstring(int(left*100))+L" %     "+preview,L"右："+std::to_wstring(int(right_magnitude_*100))+L" % → "+std::to_wstring(int(right*100))+L" %",L"走行開始 "+std::to_wstring(draft_.run_threshold)+L" % / 歩行へ戻る "+std::to_wstring(draft_.run_threshold-draft_.run_hysteresis)+L" % 未満"};
  for(int i=0;i<4;++i){int y=426+i*32;menu_band(dc,120,y,1040,31,i);text(readouts[i],135,y+5,system_ui_icons().find(0)?760:1000,28,2,RGB(209,226,225));}
 }
 text(notice_,120,565,1040,28,3,RGB(244,218,161));
 box(12,L"設定を保存・適用",120,599,320,43);box(13,page_==3?L"アナログを初期値へ":L"この機器を初期値へ",470,599,330,43);box(14,back_label_,830,599,330,43);
 text(dirty_?L"未保存の変更があります":L"保存した設定が有効です",120,649,1040,27,3,RGB(191,206,175));
 text(L"F1/F2：タブ  PgUp/PgDn：ページ  Enter：変更  Esc/クリック：入力待ち取消",83,690,1120,25,3,RGB(174,185,165));
 if(focus_<2)return {focus_==0?410:810,217};
 if(focus_<10)return {120,298+(focus_-2)*32};
 if(focus_<12)return {focus_==10?930:1050,263};
 return {focus_==12?120:focus_==13?470:830,599};
}
void ControllerPanel::paint_original(void* pixels)const{if(page_==3)if(const auto* icon=system_ui_icons().find(0))weapons::paint_icon(*icon,std::span(static_cast<uint32_t*>(pixels),1280*720),1280,720,910,426,225,128);}
void ControllerPanel::report()const{std::osyncstream(std::cout)<<"{\"controller_panel\":true,\"device\":"<<input_->config.device<<",\"slot\":"<<input_->config.slot<<",\"unsaved\":"<<(dirty_?"true":"false")<<",\"capturing\":"<<(capturing()?"true":"false")<<"}"<<std::endl;}
}
