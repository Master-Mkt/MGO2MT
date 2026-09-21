#pragma once
#include <windows.h>
#include "stage_music.h"

namespace mgo2mt {
// Native deployment selector. Opening/browsing/cancelling does not change music.
class MusicMenu {
 HDC dc_=nullptr;HBITMAP bitmap_=nullptr;HGDIOBJ old_=nullptr;HFONT font_=nullptr;void* pixels_=nullptr;
 std::vector<stage::Track> tracks_;size_t focus_=0;bool visible_=false;
 std::optional<std::string> choice_;std::vector<unsigned> cues_;
 void cue(unsigned);void move(int);void choose();
public:
 MusicMenu();~MusicMenu();MusicMenu(const MusicMenu&)=delete;MusicMenu& operator=(const MusicMenu&)=delete;
 void open(const stage::MusicLibrary&,const std::string& selected);
 void close(bool feedback=false);
 bool visible()const{return visible_;}
 size_t focus()const{return focus_;}
 bool message(HWND,UINT,WPARAM,LPARAM);
 const void* draw();
 std::optional<std::string> take_choice(){auto result=std::move(choice_);choice_.reset();return result;}
 std::vector<unsigned> cues(){std::vector<unsigned> out;out.swap(cues_);return out;}
};
}
