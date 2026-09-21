#pragma once
#include "item_box_profile.h"
#include <map>
namespace mgo2mt::items {
// Native finite-box physics. Position is bottom center. Rotation is cosmetic;
// collision conservatively contains every yaw using the horizontal diagonal.
class DropPhysics {
 struct Body {Position position;stage::Vec3 velocity{};uint64_t born=0,simulatedMs=0;Actor owner;bool ownerExited=false,grounded=false,stopped=false;};
 Scope scope_{};uint64_t clock_=0;bool clocked_=false;
 const stage::Collision* world_=nullptr;const stage::Collision* objects_=nullptr;
 std::map<uint64_t,Body> bodies_;weapons::Catalog catalog_;
public:
 void catalog(const weapons::Catalog& c){catalog_=c;}
 void reset();
 item_box::Size category(const Contents& c)const{return item_box::profile(c,&catalog_).size;}
 stage::Vec3 extent(const Contents&)const;
 bool clear(Position,const Contents&,const stage::Collision*,const stage::Collision*)const;
 std::vector<Movement> advance(const SnapshotState&,const stage::Collision*,const stage::Collision*,uint64_t now,bool active);
 // Observe owner separation even during grace. Only exact original owner life
 // needs to leave/re-enter; other players still observe the 750 ms grace.
 bool contact(const Entity&,Actor,stage::Vec3 feet,stage::Capsule,uint64_t now);
 bool grounded(EntityKey)const;
 size_t size()const{return bodies_.size();}
};
}
