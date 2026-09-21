#include "music_menu.h"
#include "menu_audio.h"
#include "controller_input.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
static void check(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char**argv){try{
 check(menu_audio::asset(92)==L"92.gwa"&&menu_audio::asset(93)==L"93.gwa"&&menu_audio::asset(94)==L"94.gwa","three distinct reviewed assets");
 check(menu_audio::asset(0).empty()&&menu_audio::asset(95).empty()&&menu_audio::asset(18999).empty(),"unknown cue must not play cursor audio");
 stage::MusicLibrary library;for(int i=0;i<20;++i)library.tracks.push_back({"original:track"+std::to_string(i),L"確認用の表示名 "+std::to_wstring(i),{},i>=16});
 MusicMenu menu;auto key=[&](unsigned k,LPARAM lp=0){menu.message(nullptr,WM_KEYDOWN,k,lp);};
 auto cue=[&](unsigned expected){check(menu.cues()==std::vector<unsigned>{expected},"one semantic sound per action");};
 menu.open(library,library.tracks[0].id);key(VK_UP);check(menu.cues().empty(),"clamped cursor is silent");
 key(menu_key(1));cue(menu_audio::Cursor);check(menu.focus()==1,"D-pad moves cursor");
 key(VK_DOWN,1LL<<30);check(menu.focus()==1&&menu.cues().empty(),"held press does not retrigger");
 key(menu_key(5));cue(menu_audio::Cancel);check(!menu.visible()&&!menu.take_choice(),"cancel preserves current song");
 menu.open(library,library.tracks[0].id);key(VK_RIGHT);cue(menu_audio::Cursor);check(menu.focus()==8,"page navigation");
 key(menu_key(4));cue(menu_audio::Confirm);check(!menu.visible()&&menu.take_choice()==library.tracks[8].id&&!menu.take_choice(),"confirmed selection is consumed once after close");
 stage::MusicPlayback playback;check(playback.select(&library.tracks[8]),"first song starts");
 menu.open(library,library.tracks[8].id);key(menu_key(4));cue(menu_audio::Confirm);check(menu.take_choice()==library.tracks[8].id&&!playback.select(&library.tracks[8]),"same song at deployment preserves running voice");
 menu.open(library,library.tracks[8].id);key(VK_DOWN);cue(menu_audio::Cursor);check(!menu.take_choice(),"browsing never changes playback");
 menu.close();check(menu.cues().empty(),"room transition silently closes stale selector");
 menu.open({},"");key(VK_RETURN);key(VK_DOWN);check(menu.visible()&&!menu.take_choice()&&menu.cues().empty(),"empty library cannot confirm an absent song");key(VK_ESCAPE);cue(menu_audio::Cancel);
 menu.open(library,library.tracks[17].id);
 if(argc>1){BITMAPFILEHEADER f{};BITMAPINFOHEADER i{};f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(i);f.bfSize=f.bfOffBits+1280*720*4;i.biSize=sizeof(i);i.biWidth=1280;i.biHeight=-720;i.biPlanes=1;i.biBitCount=32;std::ofstream out(argv[1],std::ios::binary);out.write(reinterpret_cast<char*>(&f),sizeof(f));out.write(reinterpret_cast<char*>(&i),sizeof(i));out.write(static_cast<const char*>(menu.draw()),1280*720*4);check(bool(out),"music menu image");}
 HWND w=CreateWindowExW(0,L"STATIC",L"music menu test",WS_POPUP,0,0,1280,720,nullptr,nullptr,nullptr,nullptr);check(w!=nullptr,"offscreen test window");
 menu.message(w,WM_LBUTTONUP,0,MAKELPARAM(350,200));DestroyWindow(w);cue(menu_audio::Confirm);check(menu.take_choice()==library.tracks[16].id,"mouse selects one item without cursor+confirm overlap");
 std::cout<<"Menu sound routing and deployment music selection passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
