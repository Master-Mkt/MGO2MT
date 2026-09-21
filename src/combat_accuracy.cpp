#include "combat_authority.h"
namespace mgo2mt::combat {
uint64_t Authority::accuracy_seed(const Slot&s)const{
 const auto&id=s.state.identity;return weapon_accuracy::mix(epoch_)^weapon_accuracy::mix(uint64_t(id.character)<<32|uint64_t(id.instance)<<8|id.slot)^weapon_accuracy::mix(uint64_t(s.state.life)<<32|s.state.weapon);
}
float Authority::accuracy_scale(const Slot&s,weapon_accuracy::Policy policy,uint64_t now)const{
 // Only approved HOST displacement supplies movement. Retain evidence for a
 // short packet gap; a stationary packet cannot instantly erase recent motion.
 const bool moving=!s.state.mountedId&&s.movementSpeed>50&&now>=s.movedAt&&now-s.movedAt<=150;
 return weapon_accuracy::posture_scale(policy,s.state.pose.capsule.height,moving);
}
std::optional<uint16_t> Authority::accuracy(Identity id,uint64_t now)const{
 const auto*s=slot(id);if(!s)return {};
 if(!active_||!s->state.alive||s->state.stunned||!weapons_.contains(s->state.weapon))return uint16_t(0);
 const auto policy=weapon_accuracy_policy(weapons_.at(s->state.weapon));if(!policy)return uint16_t(0);
 return s->accuracy.milliradians(*policy,now,accuracy_scale(*s,*policy,now));
}
void Authority::advance_accuracy(uint64_t now){
 for(auto&s:slots_)if(s){if(!s->state.alive||s->state.stunned)s->accuracy.reset();
  const auto angle=accuracy(s->state.identity,now);if(angle&&s->sopView.spreadMilliRadians!=*angle){s->sopView.spreadMilliRadians=*angle;++revision_;}
 }
}
}
