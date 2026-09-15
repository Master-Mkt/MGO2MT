#include "combat_authority.h"
#include "water_gameplay.h"
#include <algorithm>
#include <cmath>
namespace mgo2win::combat {
bool Authority::water(std::shared_ptr<const stage::Water> water,float ratio){
 if(!water_gameplay::valid_policy({ratio,true}))return false;
 water_=std::move(water);waterRatio_=ratio;
 for(auto& s:slots_)if(s){s->oxygen.reset(oxygenPolicy_);s->waterClockArmed=false;s->state.oxygen=water_gameplay::Oxygen::full;s->state.faceSubmerged=false;}
 ++revision_;return true;
}
bool Authority::oxygen_policy(water_gameplay::OxygenPolicy p){
 if(active_||!water_gameplay::valid_oxygen_policy(p))return false;
 oxygenPolicy_=p;for(auto&s:slots_)if(s){s->oxygen.reset(p);s->waterClockArmed=false;s->state.oxygen=water_gameplay::Oxygen::full;s->state.faceSubmerged=false;}++revision_;return true;
}
float Authority::water_scale(const Pose&p)const{
 return movement_?water_gameplay::sample(water_.get(),*movement_,p.feet,p.capsule,{waterRatio_,true}).horizontalScale:1.f;
}
bool Authority::water_blocks(const Pose&p)const{
 return movement_&&water_gameplay::sample(water_.get(),*movement_,p.feet,p.capsule,{waterRatio_,true}).proneBlocked;
}
void Authority::environment(uint64_t now){for(auto&s:slots_)if(s)advance_water(*s,now);}
void Authority::advance_water(Slot&s,uint64_t now){
 auto& p=s.state;
 regenerate(s,now);
 if(s.waterClockArmed&&now<s.waterAt)return;
 const uint64_t elapsed=s.waterClockArmed?std::min<uint64_t>(now-s.waterAt,1000):0;
 s.waterAt=now;s.waterClockArmed=true;
 if(!active_||!p.alive)return;
 auto wet=movement_?water_gameplay::sample(water_.get(),*movement_,p.pose.feet,p.pose.capsule,{waterRatio_,true}):water_gameplay::Contact{};
 const auto previousOxygen=p.oxygen;const auto previousFace=p.faceSubmerged;
 p.faceSubmerged=wet.faceSubmerged;
 auto damage=std::min(p.hp,s.oxygen.advance(oxygenPolicy_,elapsed,p.faceSubmerged,p.maxHp));
 p.oxygen=s.oxygen.amount(oxygenPolicy_);
 if(damage){p.hp-=damage;p.alive=p.hp!=0;p.stunned=p.alive&&p.stamina==0;}
 if(previousOxygen!=p.oxygen||previousFace!=p.faceSubmerged||damage)++revision_;
 if(damage&&!p.alive){s.regeneration.reset();
  s.burn.clear();p.burning=false;s.ladderState.reset();p.ladderAnchor=0;
  release_special_pc(p.identity);
  p.specialPc.action=special_pc::Action::none;p.specialPc.serial=0;p.specialPc.elapsedMs=0;p.cover={};p.evadeKind=EvadeKind::none;p.evadeSerial=0;p.evadeElapsedMs=0;
  p.reloadUntil=0;p.reloadLevel=0;s.reloadRefillAt=0;
  clear_sop_slot(p.identity.slot);refresh_sop_views();
  if(auto&score=scores_[p.identity.slot];score&&score->deaths<1000000000)++score->deaths;
  // Environmental damage has no attacker/weapon. Snapshot+score replication
  // drives HP/death/respawn; do not invent gunshot events or award a kill.
 }
}
}
