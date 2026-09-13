#include "invitation_overlay.h"
#include "menu_theme.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace mgo2win::invitation_ui {
namespace {
constexpr int canvasW=1280,canvasH=720,left=48,right=1232,top=28,stripHeight=32;
bool text_valid(const std::wstring& s){
 if(s.empty()||s.size()>512)return false;
 for(size_t i=0;i<s.size();++i){auto c=uint32_t(s[i]);
  if(c<32||(c>=127&&c<=159)||c==0x2028||c==0x2029)return false;
  if(c>=0xd800&&c<=0xdbff){if(++i==s.size()||s[i]<0xdc00||s[i]>0xdfff)return false;}
  else if(c>=0xdc00&&c<=0xdfff)return false;
 }return true;
}
void over(uint32_t& dst,uint32_t rgb,unsigned alpha){
 if(!alpha)return;const unsigned da=dst>>24,oa=alpha+(da*(255-alpha)+127)/255;
 uint32_t result=oa<<24;
 for(unsigned shift=0;shift<24;shift+=8){const unsigned s=(rgb>>shift)&255,d=(dst>>shift)&255;
  const unsigned prem=s*alpha+(d*da*(255-alpha)+127)/255;
  result|=((prem+oa/2)/oa)<<shift;
 }dst=result;
}
}
struct Overlay::Impl {
 HDC dc=nullptr;HBITMAP bitmap=nullptr;HFONT font=nullptr;HGDIOBJ oldBitmap=nullptr,oldFont=nullptr;uint32_t*pixels=nullptr;
 Impl(){dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=40;info.bmiHeader.biWidth=canvasW;info.bmiHeader.biHeight=-canvasH;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
  if(dc)bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
  font=CreateFontW(-23,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FIXED_PITCH,L"MS Gothic");
  if(!dc||!bitmap||!font||!pixels){release();throw std::runtime_error("Invitation GDI allocation");}
  oldBitmap=SelectObject(dc,bitmap);oldFont=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);
 }
 void release(){if(dc&&oldFont)SelectObject(dc,oldFont);if(dc&&oldBitmap)SelectObject(dc,oldBitmap);if(font)DeleteObject(font);if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);}
 ~Impl(){release();}
 void text(std::wstring_view value,RECT r,COLORREF color,UINT flags=DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX){SetTextColor(dc,color);DrawTextW(dc,value.data(),int(value.size()),&r,flags);}
};
Overlay::Overlay()=default;Overlay::~Overlay()=default;
bool Overlay::live(uint64_t now)const{return view_.available&&view_.scope&&view_.id&&view_.receivedAt<=now&&now<view_.expiresAt&&!confirmed_;}
bool Overlay::available()const{return live(now_);}
bool Overlay::visible()const{return open_&&available();}
void Overlay::clear(){view_={};now_=began_=0;open_=accept_=confirmed_=menuHint_=false;}
std::optional<bool> Overlay::hit_test(int x,int y)const{
 if(!visible()||!view_.respondable||y<325||y>=369)return {};
 if(x>=150&&x<530)return true;if(x>=750&&x<1130)return false;return {};
}
void Overlay::sync(View next,uint64_t now){
 const bool changed=next.scope!=view_.scope||next.id!=view_.id;
 if(changed){open_=false;accept_=false;confirmed_=false;began_=next.receivedAt;}
 if(now<now_){open_=false;confirmed_=true;}
 if(!text_valid(next.marquee)||!next.scope||!next.id||next.expiresAt<=next.receivedAt)next.available=false;
 view_=std::move(next);now_=now;if(!live(now)||!view_.respondable)open_=false;
}
bool Overlay::open(uint64_t now){if(now<now_){clear();return false;}now_=now;if(!live(now)||!view_.respondable)return false;open_=true;accept_=false;return true;}
void Overlay::move(int direction){if(visible()&&direction)accept_=!accept_;}
void Overlay::choose(bool accept){if(visible())accept_=accept;}
std::optional<Response> Overlay::confirm(uint64_t now){if(now<now_){clear();return {};}now_=now;if(!visible()||!view_.respondable)return {};Response result{view_.scope,view_.id,accept_};confirmed_=true;open_=false;return result;}
bool Overlay::paint(std::span<uint32_t> dst,int width,int height,uint64_t now){
 if(width<=0||height<=0||size_t(width)>std::numeric_limits<size_t>::max()/size_t(height)||dst.size()<size_t(width)*size_t(height))return false;
 if(now<now_){clear();return false;}now_=now;if(!live(now)){open_=false;return false;}
 try{if(!impl_)impl_=std::make_unique<Impl>();auto&p=*impl_;const int w=std::min(width,canvasW),h=std::min(height,canvasH);bool painted=false;
  auto composite=[&](bool glyphs,int y0,int y1){for(int y=y0;y<std::min(y1,h);++y)for(int x=0;x<w;++x){const auto pixel=p.pixels[y*canvasW+x];const unsigned a=glyphs?std::max({pixel&255,(pixel>>8)&255,(pixel>>16)&255}):pixel>>24;if(a){over(dst[size_t(y)*width+x],glyphs?0xffffff:pixel,a);painted=true;}}};
  if(menuHint_&&!visible()&&view_.respondable){
   std::fill_n(p.pixels,canvasW*canvasH,0u);menu_rect(p.dc,320,652,640,36,RGB(58,36,18));
   p.text(L"招待への返答：Y / F6",{330,652,950,688},RGB(255,205,140));
   finish_menu_surface(p.pixels);composite(false,652,688);
  }
  if(visible()){
   std::fill_n(p.pixels,canvasW*canvasH,0u);
   menu_rect(p.dc,100,245,1080,175,RGB(194,143,83));menu_rect(p.dc,102,247,1076,171,RGB(58,36,18));menu_rect(p.dc,116,261,1048,45,RGB(76,50,27));
   p.text(L"招待への返答を選んでください。",{120,264,1160,303},RGB(255,205,140));
   for(int column=0;column<2;++column){const int x=150+column*600;menu_row(p.dc,x,325,380,44,column,accept_==(column==0));p.text(column==0?L"承諾する":L"辞退する",{x+20,325,x+360,369},RGB(249,245,230));}
   p.text(L"決定／取消は操作設定に従います（Enter／Esc）",{120,380,1160,412},RGB(194,143,83));
   finish_menu_surface(p.pixels);composite(false,245,420);
  }
  // No ellipsis: measure the entire bounded string and move its final glyph
  // through the clipping rectangle. One pass per scope/id; never loop on sync.
  SIZE extent{};if(!GetTextExtentPoint32W(p.dc,view_.marquee.data(),int(view_.marquee.size()),&extent))return painted;
  const uint64_t elapsed=now>=began_?now-began_:0;const double travel=double(elapsed)*.096;
  if(travel<double(right-left)+extent.cx+12){
   const int x=right-int(travel);std::fill_n(p.pixels,canvasW*canvasH,0u);auto saved=SaveDC(p.dc);IntersectClipRect(p.dc,left,top,right,top+stripHeight);
   p.text(view_.marquee,{x,top,x+extent.cx+8,top+stripHeight},RGB(255,255,255),DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
   RestoreDC(p.dc,saved);GdiFlush();composite(true,top,top+stripHeight);
  }return painted;
 }catch(const std::exception&){return false;}
}
}
