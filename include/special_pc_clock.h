#pragma once
#include "combat_authority.h"
#include <algorithm>
namespace mgo2win::special_pc {
class Clock {
 uint64_t epoch_=0,at_=0;combat::Identity id_;uint32_t life_=0,serial_=0;Action action_=Action::none;double seconds_=0;
public:
 void clear(){*this={};}
 double sample(uint64_t epoch,combat::Identity id,uint32_t life,State state,uint64_t now){
  if(!valid(state)||state.kind!=Kind::gekko||state.action==Action::none){clear();return 0;}
  const auto host=double(state.elapsedMs)/1000.;
  if(epoch!=epoch_||id!=id_||life!=life_||state.serial!=serial_||state.action!=action_||now<at_)seconds_=host;
  else seconds_=(std::max)(host,seconds_+double(now-at_)/1000.);
  epoch_=epoch;id_=id;life_=life;serial_=state.serial;action_=state.action;at_=now;
  seconds_=(std::min)(seconds_,double(duration(state.action))/1000.);return seconds_;
 }
};
}
