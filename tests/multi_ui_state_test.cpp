#include "multi_ui_state.h"
#include <array>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt::multi_ui;
int main(){try{
 struct Expected{Route route;const char*screen;const char*state;};
 const Expected all[]={{Route::title,"title","idle"},{Route::loading,"title","loading"},
 {Route::agreement,"menu","agreement"},{Route::login,"menu","login"},{Route::settings,"menu","settings"},
 {Route::characters,"menu","characters"},{Route::creation,"menu","creation"},
 {Route::lobbyGroups,"lobby","groups"},{Route::lobbies,"lobby","list"},{Route::rooms,"lobby","rooms"},
 {Route::roomDetail,"lobby","detail"},{Route::briefing,"briefing","waiting"},{Route::loadout,"briefing","loadout"},
 {Route::gameplay,"hud","active"},{Route::result,"result","summary"}};
 for(const auto&e:all)for(unsigned flags=0;flags<4;++flags){const auto c=presentation(e.route,flags&1,flags&2);
  if(c.screen!=e.screen||c.state!=e.state||c.flags.contains("loading")!=bool(flags&1)||c.flags.contains("modal")!=bool(flags&2))throw std::runtime_error("state mapping");
 }
 std::cout<<"PASS 15 native UI routes / 60 transition flag cases\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
