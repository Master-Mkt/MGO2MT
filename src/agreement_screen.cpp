#include "menu_theme.h"
#include "agreement_screen.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <syncstream>
#include <stdexcept>
namespace mgo2win {
AgreementScreen::AgreementScreen(std::wstring url):url_(std::move(url)){
 if(!allowed_policy_url(url_))throw std::runtime_error("Invalid OpenMGO2 policy URL");
 dc_=CreateCompatibleDC(nullptr);if(!dc_)throw std::runtime_error("Text DC failure");
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=1280;info.bmiHeader.biHeight=-720;
 info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
 bitmap_=CreateDIBSection(dc_,&info,DIB_RGB_COLORS,&pixels_,nullptr,0);
 if(!bitmap_){DeleteDC(dc_);dc_=nullptr;throw std::runtime_error("Text surface failure");}old_=SelectObject(dc_,bitmap_);
 for(int size:{30,24,26,17})fonts_.push_back(CreateFontW(-size,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FIXED_PITCH,L"MS Gothic"));
}
AgreementScreen::~AgreementScreen(){cancel_=true;if(worker_.joinable())worker_.join();if(dc_){SelectObject(dc_,old_);DeleteObject(bitmap_);DeleteDC(dc_);}for(auto f:fonts_)DeleteObject(f);}
void AgreementScreen::start(){
 if(worker_.joinable())worker_.join();done_=false;reported_=false;response_={};yes_=false;accepted_=false;scroll_=0;
 std::osyncstream(std::cout)<<"{\"policy_get_started\":true,\"method\":\"GET\",\"url\":\""<<[&]{std::string s;for(wchar_t c:url_)s.push_back(static_cast<char>(c));return s;}()<<"\"}"<<std::endl;
 worker_=std::thread([this]{response_=fetch_policy(url_,cancel_);done_=true;});
}
void AgreementScreen::report(){
 if(!ready()||reported_)return;reported_=true;
 std::osyncstream(std::cout)<<"{\"policy_http_status\":"<<response_.status<<",\"policy_bytes\":"<<response_.bytes<<",\"policy_sha256\":\""<<response_.sha256
 <<"\",\"policy_ready\":"<<(ok()?"true":"false")<<",\"policy_error\":\""<<response_.error<<"\"}"<<std::endl;
}
int AgreementScreen::input(unsigned b,bool& close){
 if(!ready())return -1;
 if(b&retry&&!ok()){start();return -1;}
 if(b&up)scroll_=std::max(0,scroll_-23);if(b&down)scroll_=std::min(maximum_,scroll_+23);
 if(b&pageUp)scroll_=std::max(0,scroll_-276);if(b&pageDown)scroll_=std::min(maximum_,scroll_+276);
 if(b&home)scroll_=0;if(b&end)scroll_=maximum_;
 if(accepted_)return -1;
 if(ok()&&(b&(left|right))){bool next=(b&left)!=0;if(next!=yes_){yes_=next;std::osyncstream(std::cout)<<"{\"agreement_focus\":\""<<(yes_?"YES":"NO")<<"\"}"<<std::endl;return 94;}}
 if(b&confirm){
  if(yes_&&ok()){accepted_=true;std::osyncstream(std::cout)<<"{\"agreement_choice\":\"YES\",\"authentication_started\":false}"<<std::endl;}
  else {close=true;std::osyncstream(std::cout)<<"{\"agreement_choice\":\"NO\",\"authentication_started\":false}"<<std::endl;}
  return 93;
 }
 return -1;
}
const void* AgreementScreen::draw(){
 auto fill=[&](int x,int y,int w,int h,COLORREF c){menu_fill(dc_,x,y,w,h,c);};
 auto text=[&](const std::wstring& s,int x,int y,int w,int h,int f,COLORREF c,UINT flags=DT_LEFT|DT_TOP){RECT r{x,y,x+w,y+h};SelectObject(dc_,fonts_[f]);SetTextColor(dc_,menu_text_color(c));SetBkMode(dc_,TRANSPARENT);DrawTextW(dc_,s.c_str(),static_cast<int>(s.size()),&r,flags|DT_NOPREFIX);};
 // Transparent native overlay; the original lobby frame is rendered underneath.
 std::memset(pixels_,0,1280*720*4);
 menu_heading(dc_,fonts_[0],L"AGREEMENT");
 text(L"OpenMGO2",840,78,338,35,1,RGB(224,228,213),DT_RIGHT);
 text(L"お知らせ・同意確認",100,157,660,38,1,RGB(236,240,226));
 std::wstring body;
 if(!ready())body=L"OpenMGO2からテキストを取得しています…";
 else if(!ok()){body=L"テキストを取得できませんでした。\n\n"+std::wstring(response_.error.begin(),response_.error.end())+L"\n\nR：再取得    NO：終了";}
 else body=response_.text;
 // A fixed-width Japanese font retains spaces and ASCII art from the server.
 SelectObject(dc_,fonts_[2]);RECT measure{0,0,1050,0};DrawTextW(dc_,body.c_str(),static_cast<int>(body.size()),&measure,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX|DT_EXPANDTABS);
 maximum_=std::max(0,int(measure.bottom)-318);scroll_=std::min(scroll_,maximum_);
 auto saved=SaveDC(dc_);IntersectClipRect(dc_,100,209,1170,531);
 text(body,100,209-scroll_,1050,std::max(318,int(measure.bottom)),2,RGB(225,229,222),DT_WORDBREAK|DT_EXPANDTABS);RestoreDC(dc_,saved);
 if(maximum_){fill(1179,209,4,318,RGB(49,57,53));int h=std::max(24,318*318/(maximum_+318));int y=209+(318-h)*scroll_/maximum_;fill(1179,y,4,h,RGB(183,194,167));}
 text(L"↑ ↓ / PgUp PgDn：スクロール",91,555,640,28,3,RGB(176,189,170));
 text(accepted_?L"YESを選択しました。この試作はここまでです。":L"同意しますか？",100,574,810,36,1,RGB(248,206,99));
 if(!accepted_){
  for(int i=0;i<2;++i){bool selected=i==0?yes_:!yes_;int x=100+i*550;
   fill(x,618,520,40,selected?RGB(141,157,117):RGB(51,61,54));
   text(i==0?L"はい / YES":L"いいえ / NO",x,623,520,30,1,i==0&&!ok()?RGB(91,104,96):selected?RGB(14,21,15):RGB(225,229,215),DT_CENTER);
  }
 }
 text(L"← →：選択    Enter：決定    Esc：終了",83,690,960,25,3,RGB(174,185,165));

 finish_menu_surface(pixels_);
 return pixels_;
}
}
