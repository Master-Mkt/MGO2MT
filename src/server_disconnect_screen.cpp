#include "menu_font.h"
#include "server_disconnect_screen.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
namespace mgo2mt {
namespace {
void draw(std::span<uint32_t> target,std::wstring_view text,RECT box,int height,COLORREF color,bool panel){
 if(target.size()!=1280*720||text.empty())return;
 HDC dc=CreateCompatibleDC(nullptr);if(!dc)return;BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=1280;bi.bmiHeader.biHeight=-720;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
 void* bits=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);if(!bitmap){DeleteDC(dc);return;}auto old=SelectObject(dc,bitmap);
 std::memset(bits,0,1280*720*4);auto font=create_menu_font(height,FW_NORMAL);auto oldFont=SelectObject(dc,font);
 SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);DrawTextW(dc,text.data(),int(text.size()),&box,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
 GdiFlush();auto pixels=static_cast<uint32_t*>(bits);
 for(int y=box.top-8;y<box.bottom+8;++y)for(int x=box.left-8;x<box.right+8;++x){if(x<0||x>=1280||y<0||y>=720)continue;const auto i=size_t(y)*1280+x;if(pixels[i]&0xffffff)target[i]=pixels[i]|0xff000000;else if(panel)target[i]=0xeb20180d;}
 SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
}
}
void paint_server_disconnect(std::span<uint32_t> pixels,LobbyDisconnectReason reason){draw(pixels,server_disconnect_text(reason),{170,452,1130,550},23,RGB(255,205,132),true);}
void paint_lobby_ping(std::span<uint32_t> pixels,std::optional<uint32_t> ping){
 if(pixels.size()!=1280*720)return;
 // Cache the glyph surface; PING changes at most once per15seconds.
 static thread_local std::vector<uint32_t> cache(1280*720);
 static thread_local std::wstring previous;
 const auto text=ping?L"SERVER PING "+std::to_wstring(*ping)+L" ms":L"SERVER PING --";
 if(text!=previous){std::fill(cache.begin(),cache.end(),0);draw(cache,text,{1040,8,1254,27},15,RGB(238,228,203),false);previous=text;}
 for(int y=0;y<35;++y)for(int x=1032;x<1262;++x){const auto i=y*1280+x;if(cache[i])pixels[i]=cache[i];}
}
}
