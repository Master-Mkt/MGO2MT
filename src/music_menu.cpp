#include "menu_font.h"
#include "music_menu.h"
#include "menu_audio.h"
#include "menu_theme.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace mgo2win {
namespace {constexpr int left=180,top=192,width=920,rowHeight=40,rows=8;}
MusicMenu::MusicMenu(){
 dc_=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
 info.bmiHeader.biWidth=1280;info.bmiHeader.biHeight=-720;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
 bitmap_=CreateDIBSection(dc_,&info,DIB_RGB_COLORS,&pixels_,nullptr,0);
 if(!dc_||!bitmap_){if(bitmap_)DeleteObject(bitmap_);if(dc_)DeleteDC(dc_);throw std::runtime_error("Music menu surface");}
 old_=SelectObject(dc_,bitmap_);font_=create_menu_font(23,FW_NORMAL);
}
MusicMenu::~MusicMenu(){SelectObject(dc_,old_);DeleteObject(bitmap_);DeleteObject(font_);DeleteDC(dc_);}
void MusicMenu::cue(unsigned s){if(cues_.size()<32)cues_.push_back(s);}
void MusicMenu::open(const stage::MusicLibrary& library,const std::string& selected){
 tracks_=library.tracks;focus_=0;choice_.reset();visible_=true;
 for(size_t i=0;i<tracks_.size();++i)if(tracks_[i].id==selected){focus_=i;break;}
 // Caller plays the confirm sound for the MUSIC entry; no duplicate here.
}
void MusicMenu::close(bool feedback){if(!visible_)return;visible_=false;if(feedback)cue(menu_audio::Cancel);}
void MusicMenu::move(int delta){
 if(tracks_.empty())return;auto next=size_t(std::clamp(int(focus_)+delta,0,int(tracks_.size())-1));
 if(next!=focus_){focus_=next;cue(menu_audio::Cursor);}
}
void MusicMenu::choose(){if(focus_>=tracks_.size())return;choice_=tracks_[focus_].id;visible_=false;cue(menu_audio::Confirm);}
bool MusicMenu::message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 if(!visible_)return false;
 if(msg==WM_KEYDOWN){if(lp&(1LL<<30))return true;
  if(wp==VK_ESCAPE)close(true);
  else if(wp==VK_RETURN||wp==VK_SPACE)choose();
  else if(wp==VK_UP)move(-1);else if(wp==VK_DOWN)move(1);
  else if(wp==VK_LEFT||wp==VK_PRIOR)move(-rows);else if(wp==VK_RIGHT||wp==VK_NEXT)move(rows);
  else if(wp==VK_HOME)move(-int(tracks_.size()));else if(wp==VK_END)move(int(tracks_.size()));
  return true;
 }
 if(msg==WM_MOUSEWHEEL){move(short(HIWORD(wp))>0?-1:1);return true;}
 if(msg==WM_LBUTTONUP){RECT r{};GetClientRect(hwnd,&r);if(!r.right||!r.bottom)return true;
  int x=int(short(LOWORD(lp)))*1280/r.right,y=int(short(HIWORD(lp)))*720/r.bottom;
  if(x>=left&&x<left+width&&y>=top&&y<top+rows*rowHeight){
   size_t index=(focus_/rows)*rows+size_t((y-top)/rowHeight);
   if(index<tracks_.size()){focus_=index;choose();} // Click chooses once; no cursor+confirm overlap.
  }else if(y>=554&&y<598){if(x>=720&&x<1100)close(true);else if(x>=180&&x<640)choose();}
  return true;
 }
 return msg==WM_CHAR||msg==WM_KEYUP||msg==WM_LBUTTONDOWN;
}
const void* MusicMenu::draw(){
 std::memset(pixels_,0,1280*720*4);SetBkMode(dc_,TRANSPARENT);auto old=SelectObject(dc_,font_);
 auto text=[&](int x,int y,int w,int h,const std::wstring&s,COLORREF color=RGB(235,242,240)){
  SetTextColor(dc_,color);RECT r{x,y,x+w,y+h};DrawTextW(dc_,s.c_str(),int(s.size()),&r,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
 };
 menu_heading(dc_,font_,L"MUSIC");menu_fill(dc_,155,133,970,493,RGB(48,59,61));menu_section(dc_,font_,L"MUSIC SELECTION",left,153,width);
 size_t page=focus_/rows;
 if(tracks_.empty())text(left+16,top+18,width-32,40,L"再生できるBGMがありません。戻るを選んでください。");
 for(int i=0;i<rows;++i){size_t at=page*rows+size_t(i);if(at>=tracks_.size())break;
  auto&t=tracks_[at];menu_row(dc_,left,top+i*rowHeight,width,rowHeight-2,at,at==focus_);
  text(left+15,top+i*rowHeight,110,rowHeight,t.additional?L"追加曲":L"原曲",RGB(252,193,108));
  text(left+133,top+i*rowHeight,width-165,rowHeight,t.title);
 }
 text(left,515,700,32,L"同じ曲を選ぶと、続きから再生します。");
 text(935,515,160,32,std::to_wstring(tracks_.empty()?0:page+1)+L" / "+std::to_wstring((tracks_.size()+rows-1)/rows));
 menu_cursor(dc_,180,554,460,44);text(180,554,460,44,L"  OK / B・Enter");
 menu_band(dc_,720,554,380,44,1);text(720,554,380,44,L"  戻る / A・Esc");
 text(left,637,width,38,L"↑↓：曲を選択　←→：ページ　B：決定　A：戻る");
 if(!tracks_.empty())menu_focus_guides(dc_,left,top+int(focus_%rows)*rowHeight);
 SelectObject(dc_,old);finish_menu_surface(pixels_);return pixels_;
}
}
