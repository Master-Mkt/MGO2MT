#pragma once
#include "combat_authority.h"
#include <map>
#include <set>
namespace mgo2win::items {
// Native contact receipt cue hook. Never infer pickup from initial Held grants.
class PickupFeedback {
 uint64_t epoch_=0,floor_=0;combat::Identity self_;uint32_t life_=0;
 std::map<uint64_t,combat::Event> pending_;std::set<uint64_t> seen_;
public:
 void clear(){epoch_=floor_=0;self_={};life_=0;pending_.clear();seen_.clear();}
 bool update(const combat::Snapshot&s,combat::Identity self,std::span<const combat::Event> events){
  if(!s.epoch||self.slot>=24||!s.players[self.slot]||s.players[self.slot]->identity!=self||!s.players[self.slot]->alive){clear();return false;}
  const auto life=s.players[self.slot]->life;if(!life){clear();return false;}
  if(epoch_!=s.epoch||self_!=self||life_!=life){clear();epoch_=s.epoch;self_=self;life_=life;floor_=s.eventWatermark;}
  for(const auto&e:events)if(e.epoch==epoch_&&e.id>floor_&&e.kind==combat::EventKind::itemPickup&&e.source==self_&&e.target==self_&&e.sourceLife==life_&&e.targetLife==life_&&e.weapon&&(e.object==1||e.object==2)&&!seen_.contains(e.id)&&pending_.size()<64)pending_.try_emplace(e.id,e);
  bool play=false;for(auto it=pending_.begin();it!=pending_.end();){if(it->first>s.eventWatermark){++it;continue;}if(seen_.insert(it->first).second)play=true;it=pending_.erase(it);}
  while(seen_.size()>128){floor_=*seen_.begin();seen_.erase(seen_.begin());}
  return play; // At most one cue per frame for simultaneous contact pickups.
 }
};
}
