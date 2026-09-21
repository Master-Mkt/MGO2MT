#pragma once
#include "combat_authority.h"
#include "player_ragdoll.h"
#include "motion_blend.h"
#include <cmath>
namespace mgo2mt::combat::presentation {
// Local physics presentation only. HOST HP/life owns death and respawn.
class Death {
 uint64_t epoch_=0,scene_=0;Identity id_{};uint32_t life_=0;bool dead_=false;
 uint64_t blastEvent_=0,observedAt_=0;Vec3 flightVelocity_{},lastFeet_{};bool inFlight_=false;
public:
 bool scope(player::Ragdoll& rag,uint64_t epoch,uint64_t scene,Identity id,uint32_t life){
  if(epoch_==epoch&&scene_==scene&&id_==id&&life_==life)return false;
  epoch_=epoch;scene_=scene;id_=id;life_=life;dead_=false;blastEvent_=observedAt_=0;flightVelocity_={};lastFeet_={};inFlight_=false;rag.stop();return true;
 }
 bool update(player::Ragdoll& rag,const CharacterCatalog& catalog,unsigned gender,
             const MotionPose& lastLive,Vec3 origin,float yaw,const Player& p,const Snapshot& snapshot,
             std::span<const Event> events={},uint64_t now=0){
  if(p.identity!=id_||p.life!=life_||snapshot.epoch!=epoch_||!epoch_||!scene_)return false;
  std::optional<Vec3> impulse;
  for(const auto&e:events)if(e.kind==EventKind::knockback&&e.epoch==epoch_&&e.id>blastEvent_&&e.id<=snapshot.eventWatermark&&e.target==id_&&e.targetLife==life_&&valid_knockback_event(e)){blastEvent_=e.id;impulse=e.normal;}
  if(p.alive){
   if(dead_)rag.stop();dead_=false;
   if(p.blastFlight){
    if(impulse)flightVelocity_=*impulse;
    else if(inFlight_&&now>observedAt_&&now-observedAt_<=250&&lastFeet_!=p.pose.feet){auto v=p.pose.feet;for(unsigned i=0;i<3;++i)v[i]=(v[i]-lastFeet_[i])*1000.f/float(now-observedAt_);if(std::hypot(v[0],v[1],v[2])<=blast_motion::maximum_velocity)flightVelocity_=v;}
   }else flightVelocity_={};
   inFlight_=p.blastFlight;if(lastFeet_!=p.pose.feet||!observedAt_){lastFeet_=p.pose.feet;observedAt_=now;}return false;
  }
  if(dead_){if(impulse)rag.launch(*impulse);return false;}dead_=true;
  if(p.specialPc.kind!=special_pc::Kind::human||!rag.start(catalog,gender,lastLive,origin,yaw))return false;
  // A modest native backwards impulse breaks the upright equilibrium. It is
  // not inferred from surface normals or an unauthenticated damage message.
  auto point=rag.root_position();point[1]+=450;
  rag.impulse({-std::sin(yaw)*12000,3000,-std::cos(yaw)*12000},point);
  if(impulse)rag.launch(*impulse);else if(inFlight_)rag.launch(flightVelocity_);return true;
 }
 bool dead()const{return dead_;}
};
}
