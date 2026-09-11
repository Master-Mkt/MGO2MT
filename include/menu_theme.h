#pragma once
#include <windows.h>
#include <cstddef>
#include <algorithm>
#include <string_view>
namespace mgo2win {
// Native interpretation of the user's 2026-09-10 retail menu references.
// Clipped upper-left selection corner, inset rim and translucent amber glass.
inline void menu_rect(HDC dc,int x,int y,int w,int h,COLORREF c){
 if(w<=0||h<=0)return;RECT r{x,y,x+w,y+h};auto b=CreateSolidBrush(c);FillRect(dc,&r,b);DeleteObject(b);
}
inline COLORREF menu_color(COLORREF previous){
 auto green=GetGValue(previous);
 if(green>=200)return RGB(255,205,140);
 if(green>=130)return RGB(194,143,83);
 if(green>=70)return RGB(177,121,61);
 if(green>=40)return RGB(99,75,47);
 return RGB(38,48,49);
}
inline void menu_cursor(HDC dc,int x,int y,int w,int h,bool emphasis=false){
 if(w<=0||h<=0)return;
 int cut=std::min(12,std::min(w/4,h/3));
 POINT p[]={{x+cut,y},{x+w,y},{x+w,y+h},{x,y+h},{x,y+cut}};
 auto region=CreatePolygonRgn(p,5,WINDING);auto saved=SaveDC(dc);ExtSelectClipRgn(dc,region,RGN_AND);
 menu_rect(dc,x,y,w,h,RGB(125,88,47));
 for(int i=0;i<16;++i){int top=y+3+(h-6)*i/16,bottom=y+3+(h-6)*(i+1)/16;
  int shade=15-i;menu_rect(dc,x+7,top,w-11,bottom-top,emphasis?RGB(190+3*shade,134+2*shade,72+shade):RGB(154+3*shade,105+2*shade,53+shade));}
 menu_rect(dc,x+cut,y,w-cut,1,RGB(219,170,107));
 menu_rect(dc,x+1,y+h-2,w-2,1,RGB(125,88,47));
 RestoreDC(dc,saved);DeleteObject(region);
}
inline void menu_fill(HDC dc,int x,int y,int w,int h,COLORREF previous){
 auto g=GetGValue(previous);
 if(h>=24&&h<=64&&w>=60&&g>=70&&g<200){menu_cursor(dc,x,y,w,h,g>=130);return;}
 // Large modal/roster surfaces use smoky gray with a silver edge.
 if(h>100&&w>200){menu_rect(dc,x,y,w,h,RGB(108,124,125));menu_rect(dc,x+2,y+2,w-4,h-4,RGB(48,59,61));return;}
 menu_rect(dc,x,y,w,h,menu_color(previous));
}
inline COLORREF menu_text_color(COLORREF c){
 return GetRValue(c)<45&&GetGValue(c)<45&&GetBValue(c)<45?RGB(249,245,230):c;
}
struct MenuFonts {
 HFONT heading=CreateFontW(-30,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,VARIABLE_PITCH,L"Arial");
 HFONT section=CreateFontW(-17,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,VARIABLE_PITCH,L"Arial");
 ~MenuFonts(){DeleteObject(heading);DeleteObject(section);}
};
inline const MenuFonts& menu_fonts(){static const MenuFonts fonts;return fonts;}
inline void menu_heading(HDC dc,HFONT font,std::wstring_view title){
 auto old=SelectObject(dc,menu_fonts().heading?menu_fonts().heading:font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(238,245,242));
 RECT r{156,66,750,101};DrawTextW(dc,title.data(),int(title.size()),&r,DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);SelectObject(dc,old);
}
inline void menu_section(HDC dc,HFONT font,std::wstring_view title,int x,int y,int w){
 menu_rect(dc,x,y,w,23,RGB(108,124,125));menu_rect(dc,x+2,y+2,w-4,19,RGB(65,80,83));
 menu_rect(dc,x+3,y+2,w-6,1,RGB(187,208,208));menu_rect(dc,x+4,y+3,2,17,RGB(187,208,208));
 auto old=SelectObject(dc,menu_fonts().section?menu_fonts().section:font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(226,242,241));
 RECT r{x+12,y+1,x+w-8,y+23};DrawTextW(dc,title.data(),int(title.size()),&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);SelectObject(dc,old);
}
inline void menu_description(HDC dc,HFONT font,int x,int y,int w){
 menu_rect(dc,x,y,w,1,RGB(108,124,125));auto old=SelectObject(dc,menu_fonts().section?menu_fonts().section:font);SetTextColor(dc,RGB(226,242,241));SetBkMode(dc,TRANSPARENT);
 RECT r{x,y+6,x+w,y+27};DrawTextW(dc,L"DESCRIPTION",-1,&r,DT_LEFT|DT_SINGLELINE);SelectObject(dc,old);
}
inline void menu_character_frame(HDC dc,HFONT font){
 menu_rect(dc,704,170,7,439,RGB(255,178,88));menu_section(dc,font,L"PLAYER CHARACTER",714,170,446);
}
inline void finish_menu_surface(void* pixels){
 GdiFlush();auto*p=static_cast<unsigned char*>(pixels);
 for(size_t i=0;i<1280*720*4;i+=4){auto c=RGB(p[i+2],p[i+1],p[i]);unsigned alpha=255;
  if(!c)alpha=0;
  else if(c==RGB(38,48,49))alpha=90;
  else if(c==RGB(48,59,61))alpha=230;
  else if(c==RGB(99,75,47))alpha=110;
  else if(c==RGB(125,88,47))alpha=125;
  else if(c==RGB(108,124,125)||c==RGB(65,80,83))alpha=180;
  else if(c==RGB(194,143,83)||c==RGB(177,121,61))alpha=178;
  else if(p[i]>=72&&p[i]<88&&c==RGB(190+3*(p[i]-72),134+2*(p[i]-72),p[i]))alpha=210;
  else {int shade=int(p[i])-53;if(shade>=0&&shade<16&&c==RGB(154+3*shade,105+2*shade,53+shade))alpha=175;}
  p[i+3]=static_cast<unsigned char>(alpha);
 }
}
}
