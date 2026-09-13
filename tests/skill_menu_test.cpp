#include "skill_menu.h"
#include "menu_audio.h"
#include "controller_input.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
static void check(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char**argv){
 auto file=std::filesystem::temp_directory_path()/("mgo2win-skill-menu-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".tsv");
 struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove(path,ec);}}cleanup{file};
 try{
  {std::ofstream f(file);f<<"MGO2WIN_SKILLS\t1\n";for(unsigned i=0;i<12;++i)for(unsigned level=1;level<=3;++level)f<<"SKILL\t"<<i<<'\t'<<level<<'\t'<<level<<"\tSynthetic "<<i<<"\t確認スキル "<<i<<'\n';}
  auto catalog=std::make_shared<skills::Catalog>();std::string error;check(catalog->load(file,error),"synthetic menu catalog loads");SkillMenu menu;
  auto key=[&](unsigned k,LPARAM lp=0){menu.message(nullptr,WM_KEYDOWN,k,lp);};auto cue=[&](unsigned expected){check(menu.cues()==std::vector<unsigned>{expected},"one semantic cue per action");};
  menu.open(catalog,{});key(VK_UP);check(menu.focus()==0&&menu.cues().empty(),"clamped focus is silent");
  key(menu_key(4));cue(menu_audio::Confirm);check(menu.draft().entries==std::vector<skills::Choice>{{0,1}},"controller confirm selects level one");
  key(menu_key(3));cue(menu_audio::Cursor);check(menu.draft().entries[0].level==2,"right direction increases level");
  key(VK_RIGHT);cue(menu_audio::Cursor);key(VK_DOWN);cue(menu_audio::Cursor);key(VK_RETURN);cue(menu_audio::Confirm);
  check(skills::validate(*catalog,menu.draft()).used==4&&menu.draft().entries.size()==2,"menu uses total cost budget");
  key(VK_RIGHT);check(menu.cues().empty()&&menu.draft().entries.back().level==1,"failed cost increase changes neither draft nor sound");
  key(VK_DOWN,1LL<<30);check(menu.focus()==1&&menu.cues().empty(),"held input does not repeat");
  key(menu_key(5));cue(menu_audio::Cancel);check(!menu.visible()&&menu.draft().entries.empty()&&!menu.take_saved(),"cancel rolls back every change");
  menu.open(catalog,{});key(VK_RETURN);cue(menu_audio::Confirm);key(VK_END);cue(menu_audio::Cursor);key(VK_RETURN);cue(menu_audio::Confirm);
  auto saved=menu.take_saved();check(saved&&saved->entries==std::vector<skills::Choice>{{0,1}}&&!menu.take_saved()&&!menu.visible(),"save returns selection once after closing");
  menu.open(catalog,*saved,4,false);key(VK_RIGHT);key(VK_RETURN);key(VK_DELETE);key(VK_F6);check(menu.draft()==*saved&&menu.cues().empty(),"round-active read-only mode rejects all edit paths");key(VK_ESCAPE);cue(menu_audio::Cancel);
  menu.open(catalog,*saved,8);key(VK_F6);cue(menu_audio::Confirm);check(menu.draft().entries.empty(),"explicit clear removes selection");key(VK_ESCAPE);cue(menu_audio::Cancel);check(menu.draft()==*saved,"cancel also restores clear action");
  menu.open(catalog,{});key(VK_NEXT);cue(menu_audio::Cursor);check(menu.focus()==8,"next-page navigation");
  HWND w=CreateWindowExW(0,L"STATIC",L"skill menu test",WS_POPUP,0,0,1280,720,nullptr,nullptr,nullptr,nullptr);check(w!=nullptr,"offscreen window");
  menu.message(w,WM_LBUTTONUP,0,MAKELPARAM(300,211));cue(menu_audio::Confirm);check(menu.draft().entries==std::vector<skills::Choice>{{8,1}},"mouse selects correct paged skill");
  menu.message(w,WM_LBUTTONUP,0,MAKELPARAM(710,630));cue(menu_audio::Cancel);DestroyWindow(w);check(!menu.take_saved(),"mouse cancel keeps original selection");
  menu.open(catalog,{{{0,1},{2,2},{4,1}}});
  if(argc>1){BITMAPFILEHEADER f{};BITMAPINFOHEADER i{};f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(i);f.bfSize=f.bfOffBits+1280*720*4;i.biSize=sizeof(i);i.biWidth=1280;i.biHeight=-720;i.biPlanes=1;i.biBitCount=32;std::ofstream out(argv[1],std::ios::binary);out.write(reinterpret_cast<char*>(&f),sizeof(f));out.write(reinterpret_cast<char*>(&i),sizeof(i));out.write(static_cast<const char*>(menu.draw()),1280*720*4);check(bool(out),"skill menu review image");}
  menu.close();check(menu.cues().empty(),"context change silently closes stale skills UI");
  menu.open(nullptr,{});key(VK_RETURN);key(VK_F10);check(menu.visible()&&!menu.take_saved()&&menu.cues().empty(),"missing catalog cannot produce selection");key(VK_ESCAPE);cue(menu_audio::Cancel);
  std::cout<<"Skill menu controller, keyboard, mouse, budget, save/cancel and read-only paths passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
