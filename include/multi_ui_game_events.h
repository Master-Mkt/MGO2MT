#pragma once
#include "multi_ui.h"
namespace mgo2mt::multi_ui {
enum class GameAction {none,start,confirm,back};
// Explicit local routes. Arbitrary named events remain available to embedders;
// a layout never calls network, shell or filesystem operations directly.
inline GameAction game_action(const ActionEvent&e,const Context&c,bool focused){
 if(!focused||e.kind!="event"||c.flags.contains("loading")||c.flags.contains("modal"))return GameAction::none;
 if(e.name=="title.start"&&c.screen=="title"&&c.state=="idle")return GameAction::start;
 if(c.screen=="menu"||c.screen=="lobby"||c.screen=="briefing"||c.screen=="result"){
  if(e.name=="menu.confirm")return GameAction::confirm;
  if(e.name=="menu.back")return GameAction::back;
 }
 return GameAction::none;
}
}
