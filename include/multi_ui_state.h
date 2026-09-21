#pragma once
#include "multi_ui.h"
namespace mgo2mt::multi_ui {
// Native presentation names, independent of network opcodes and input routing.
enum class Route {title,loading,agreement,login,settings,characters,creation,
 lobbyGroups,lobbies,rooms,roomDetail,briefing,loadout,gameplay,result};
inline Context presentation(Route route,bool loading=false,bool modal=false){
 Context c;
 switch(route){
 case Route::title:c.screen="title";c.state="idle";break;
 case Route::loading:c.screen="title";c.state="loading";break;
 case Route::agreement:c.screen="menu";c.state="agreement";break;
 case Route::login:c.screen="menu";c.state="login";break;
 case Route::settings:c.screen="menu";c.state="settings";break;
 case Route::characters:c.screen="menu";c.state="characters";break;
 case Route::creation:c.screen="menu";c.state="creation";break;
 case Route::lobbyGroups:c.screen="lobby";c.state="groups";break;
 case Route::lobbies:c.screen="lobby";c.state="list";break;
 case Route::rooms:c.screen="lobby";c.state="rooms";break;
 case Route::roomDetail:c.screen="lobby";c.state="detail";break;
 case Route::briefing:c.screen="briefing";c.state="waiting";break;
 case Route::loadout:c.screen="briefing";c.state="loadout";break;
 case Route::gameplay:c.screen="hud";c.state="active";break;
 case Route::result:c.screen="result";c.state="summary";break;
 }
 if(loading)c.flags.insert("loading");if(modal)c.flags.insert("modal");
 return c;
}
}
