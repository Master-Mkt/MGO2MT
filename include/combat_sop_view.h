#pragma once
#include "combat_authority.h"
#include <cmath>
namespace mgo2mt::combat {
inline bool valid_special(const Player& p){
 return unsigned(p.specialPhase)<=3&&(p.specialPhase==SpecialPhase::none||
   (p.alive&&!p.stunned&&!p.reloadUntil&&p.pose.capsule.height>=1700));
}
// A recipient-specific view: a client never chooses a linked target or reports a group.
inline bool valid_sop_view(const SopView& v,const Snapshot& s){
 if(v.recipient==Identity{})return v==SopView{};
 if(v.recipient.slot>=24||!v.recipient.instance||!v.recipient.character||!v.life||v.visibleMask&0xff000000u)return false;
 const auto& self=s.players[v.recipient.slot];
 if(!self||self->identity!=v.recipient||self->life!=v.life||v.visibleMask&(1u<<v.recipient.slot))return false;
 if(v.visibleMask&&(v.jammed||!self->alive||!self->team||!v.activation))return false;
 for(float x:v.origin)if(!std::isfinite(x)||std::abs(x)>=1000000)return false;
 if((!v.activation&&v.origin!=Vec3{})||(!v.inputSequenced&&v.inputSequence))return false;
 if((v.evadeRequest||v.coverRequest||v.specialPcRequest)&&!v.inputSequenced)return false;
 // HOST-approved JSON cone, maximum 0.1 rad * 8 moving multiplier. The
 // replicated angle is rounded outward; no client-supplied accuracy state.
 if(v.spreadMilliRadians>800||(v.spreadMilliRadians&&(!self->alive||self->stunned||!self->weapon)))return false;
 for(unsigned i=0;i<24;++i)if(v.visibleMask&(1u<<i)){
  const auto& target=s.players[i];if(!target||!target->alive||target->team!=self->team)return false;
 }
 return true;
}
}
