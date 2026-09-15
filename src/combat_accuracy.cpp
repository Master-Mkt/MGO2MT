#include "combat_authority.h"
namespace mgo2win::combat {
uint64_t Authority::accuracy_seed(const Slot&s)const{
 const auto&id=s.state.identity;return weapon_accuracy::mix(epoch_)^weapon_accuracy::mix(uint64_t(id.character)<<32|uint64_t(id.instance)<<8|id.slot)^weapon_accuracy::mix(uint64_t(s.state.life)<<32|s.state.weapon);
}
std::optional<uint16_t> Authority::accuracy(Identity id,uint64_t now)const{
 const auto*s=slot(id);if(!s)return {};if(s->state.specialPc.kind!=special_pc::Kind::human)return uint16_t(0);
 if(!active_||!s->state.alive||s->state.stunned||!weapons_.contains(s->state.weapon)||!weapons_.at(s->state.weapon).nativeAkAccuracy)return uint16_t(0);
 return s->accuracy.milliradians(weapon_accuracy::native_ak,now);
}
void Authority::advance_accuracy(uint64_t now){
 for(auto&s:slots_)if(s){if(!s->state.alive||s->state.stunned)s->accuracy.reset();
  const auto angle=accuracy(s->state.identity,now);if(angle&&s->sopView.spreadMilliRadians!=*angle){s->sopView.spreadMilliRadians=*angle;++revision_;}
 }
}
}
