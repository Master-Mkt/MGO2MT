#include "multi_ui_layer.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt::multi_ui;
static void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(){try{
 const auto root=std::filesystem::temp_directory_path()/("multi-ui-layer-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(root);
 const auto path=root/"layout.json";Layer layer(path);Context title;title.state="idle";
 check(!layer.frame(0,title),"missing optional layout preserves default UI");
 unsigned revision=0;auto write=[&](const char*s){std::ofstream(path)<<s;std::filesystem::last_write_time(path,std::filesystem::file_time_type::clock::now()+std::chrono::seconds(++revision));};
 const char*valid=R"({"format":"MGO2MT.UI_LAYOUT.1","width":1280,"height":720,"elements":[{"id":"panel","kind":"panel","screen":"title","state":"idle","x":10,"y":10,"width":20,"height":20,"color":[255,32,0,255]}]})";
 write(valid);auto frame=layer.frame(1000,title);check(frame&&(*frame)[(10*1280+10)*4]==255,"new local export applied");
 Context hud;hud.screen="hud";hud.state="active";frame=layer.frame(1001,hud);check(frame&&(*frame)[(10*1280+10)*4+3]==0,"title pixels removed on gameplay transition");
 write("{bad json");check(!layer.frame(2000,title)&&!layer.ready(),"invalid reload removes custom layer");
 write(valid);check(layer.frame(3000,title),"valid export recovers after failure");
 std::filesystem::remove(path);check(!layer.frame(4000,title),"removed layout restores default");
 write(valid);check(layer.frame(5000,title),"recreated layout is discovered");
 auto invalid=title;invalid.screen=std::string(200,'x');check(!layer.frame(5001,invalid),"invalid dynamic context cannot crash game");
 std::filesystem::remove_all(root);std::cout<<"PASS optional layer reload, state transition, recovery and fallback\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
