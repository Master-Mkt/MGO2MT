#pragma once
#include <cstdint>
namespace mgo2win {
// Separate placement/pickup menu entry. Observing a held key while unavailable
// never turns it into a fresh press when a modal closes or focus returns.
class ItemMenuShortcut {
 bool keyboard_=false,chord_=false;uint64_t epoch_=0,actor_=0,life_=0;
public:
 bool step(uint64_t epoch,uint64_t actor,uint64_t life,bool keyboard,bool chord,bool eligible){
  const bool same=epoch_==epoch&&actor_==actor&&life_==life;
  const bool open=same&&epoch&&actor&&life&&eligible&&((keyboard&&!keyboard_)||(chord&&!chord_));
  epoch_=epoch;actor_=actor;life_=life;keyboard_=keyboard;chord_=chord;return open;
 }
};
}
