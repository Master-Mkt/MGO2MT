#include "chat_view.h"
#include <algorithm>
namespace mgo2mt::chat {
std::wstring wide(std::string_view s){if(s.empty()||s.size()>8192)return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0);if(!n)return {};std::wstring out(n,0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),out.data(),n);return out;}
std::string utf8(std::wstring_view s){if(s.empty()||s.size()>4096)return {};int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);if(!n)return {};std::string out(n,0);WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),int(s.size()),out.data(),n,nullptr,nullptr);return out;}
void draw_history(HDC dc,HFONT font,const State& state,RECT rect,uint64_t now,uint64_t age){
 if(!state.joined||rect.right<=rect.left||rect.bottom<=rect.top)return;
 const int saved=SaveDC(dc);IntersectClipRect(dc,rect.left,rect.top,rect.right,rect.bottom);SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);
 const size_t rows=size_t((rect.bottom-rect.top)/46);std::vector<const Line*> visible;
 for(auto i=state.lines.rbegin();i!=state.lines.rend()&&visible.size()<rows;++i)if(!age||(now>=i->receivedAt&&now-i->receivedAt<age))visible.push_back(&*i);
 int y=rect.top;for(auto i=visible.rbegin();i!=visible.rend();++i){const auto&line=**i;auto label=wide(line.name)+(line.radio?L" [RADIO]":line.mode==1?L" [TEAM]":L"")+L"  "+wide(line.text);
  RECT r{rect.left,y,rect.right,y+44};SetTextColor(dc,RGB(15,12,8));OffsetRect(&r,1,1);DrawTextW(dc,label.c_str(),int(label.size()),&r,DT_LEFT|DT_WORDBREAK|DT_END_ELLIPSIS|DT_NOPREFIX);
  OffsetRect(&r,-1,-1);SetTextColor(dc,line.mode==1?RGB(151,208,242):RGB(255,206,126));DrawTextW(dc,label.c_str(),int(label.size()),&r,DT_LEFT|DT_WORDBREAK|DT_END_ELLIPSIS|DT_NOPREFIX);y+=46;
 }RestoreDC(dc,saved);
}
}
