#pragma once
#include "combat_authority.h"
#include "player_ragdoll.h"
#include "motion_blend.h"
#include <cmath>
namespace mgo2win::combat::presentation {
// Local physics presentation only. HOST HP/life owns death and respawn.
class Death {
 uint64_t epoch_=0,scene_=0;Identity id_{};uint32_t life_=0;bool dead_=false;
public:
 bool scope(player::Ragdoll& rag,uint64_t epoch,uint64_t scene,Identity id,uint32_t life){
  if(epoch_==epoch&&scene_==scene&&id_==id&&life_==life)return false;
  epoch_=epoch;scene_=scene;id_=id;life_=life;dead_=false;rag.stop();return true;
 }
 bool update(player::Ragdoll& rag,const CharacterCatalog& catalog,unsigned gender,
             const MotionPose& lastLive,Vec3 origin,float yaw,const Player& p,const Snapshot& snapshot){
  if(p.identity!=id_||p.life!=life_||snapshot.epoch!=epoch_||!epoch_||!scene_)return false;
  if(p.alive){if(dead_)rag.stop();dead_=false;return false;}
  if(dead_)return false;dead_=true;
  if(p.specialPc.kind!=special_pc::Kind::human||!rag.start(catalog,gender,lastLive,origin,yaw))return false;
  // A modest native backwards impulse breaks the upright equilibrium. It is
  // not inferred from surface normals or an unauthenticated damage message.
  auto point=rag.root_position();point[1]+=450;
  rag.impulse({-std::sin(yaw)*12000,3000,-std::cos(yaw)*12000},point);return true;
 }
 bool dead()const{return dead_;}
};
}
