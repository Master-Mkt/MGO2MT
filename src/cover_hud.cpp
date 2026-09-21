#include "menu_font.h"
#include "cover_hud.h"
#include <string>
#include <algorithm>
namespace mgo2mt::cover_hud {
Renderer::Renderer(){
 dc_=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=1280;info.bmiHeader.biHeight=-720;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
 bitmap_=CreateDIBSection(dc_,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels_),nullptr,0);if(bitmap_)old_=SelectObject(dc_,bitmap_);
 font_=create_menu_font(16,FW_MEDIUM);
}
Renderer::~Renderer(){if(old_)SelectObject(dc_,old_);if(bitmap_)DeleteObject(bitmap_);if(font_)DeleteObject(font_);if(dc_)DeleteDC(dc_);}
void Renderer::paint(std::span<uint32_t> destination,const Model& model){
 if(!pixels_||!dc_||destination.size()!=1280*720)return;std::fill_n(pixels_,1280*720,0u);draw(dc_,std::span(&font_,1),model);GdiFlush();
 for(size_t n=472*1280;n<564*1280;++n)if(pixels_[n])destination[n]=0xff000000u|pixels_[n];
}
void draw(HDC dc,std::span<const HFONT> fonts,const Model& m){
 if(!dc||(!m.available&&!m.attached&&!m.pending&&!(m.firstPerson&&m.lean)))return;
 const int saved=SaveDC(dc);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,196,89));if(!fonts.empty())SelectObject(dc,fonts[0]);
 HBRUSH panel=CreateSolidBrush(RGB(39,28,15));RECT box{978,472,1260,564};FillRect(dc,&box,panel);DeleteObject(panel);
 HPEN pen=CreatePen(PS_SOLID,3,RGB(241,177,65));auto old=SelectObject(dc,pen);
 MoveToEx(dc,997,482,nullptr);LineTo(dc,997,525);MoveToEx(dc,1009,501,nullptr);LineTo(dc,1024,501);LineTo(dc,1033,513);
 Ellipse(dc,1017,483,1029,495);MoveToEx(dc,1023,495,nullptr);LineTo(dc,1023,514);LineTo(dc,1014,527);MoveToEx(dc,1023,514,nullptr);LineTo(dc,1033,527);
 if(m.attached){MoveToEx(dc,999,536,nullptr);LineTo(dc,1035,536);LineTo(dc,1028,530);MoveToEx(dc,999,536,nullptr);LineTo(dc,1006,530);}
 SelectObject(dc,old);DeleteObject(pen);
 const std::wstring title=m.pending?L"HOST確認中…":m.attached?std::wstring(m.action)+L"  壁から離れる":m.available?std::wstring(m.action)+L"  壁に張り付く":L"主観リーン";
 TextOutW(dc,1050,484,title.c_str(),int(title.size()));
 const std::wstring detail=m.attached?(m.lean?L"覗き込み / 構えて射撃":L"左右：壁沿い移動   後ろ：離れる"):L"";
 if(!detail.empty())TextOutW(dc,991,540,detail.c_str(),int(detail.size()));
 if(m.left||m.right){const std::wstring edge=(m.left?L"◀ ":L"")+(std::wstring(L"方向キー：覗く"))+(m.right?L" ▶":L"");TextOutW(dc,1050,509,edge.c_str(),int(edge.size()));}
 RestoreDC(dc,saved);
}
}
