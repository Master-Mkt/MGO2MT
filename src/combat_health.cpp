#include "combat_authority.h"
namespace mgo2win::combat {
bool Authority::configure_health(const HealthRules& rules){
 if(active_||!rules.valid())return false;healthRules_=rules;
 for(auto& s:slots_)if(s){s->falling=falling::Tracker(rules.falling);s->regeneration.reset();}return true;
}
void Authority::regenerate(Slot& s,uint64_t now){
 auto& p=s.state;const special_pc::regeneration::Scope scope{epoch_,p.identity.slot,p.identity.instance,p.identity.character,p.life};
 const auto hp=s.regeneration.advance(scope,healthRules_.regeneration,now,p.hp,active_&&p.alive&&p.specialPc.kind==special_pc::Kind::gekko&&p.maxHp==special_pc::regeneration::maximum_hp);
 if(hp!=p.hp){p.hp=hp;++revision_;}
}
Decision Authority::sample_fall(Slot& s,uint64_t now){
 Decision out;auto& p=s.state;if(now<s.poseAt)return out;
 // User policy: Gekko is immune to falling damage, including cancelled jumps,
 // unpowered descent and fatal-height thresholds. Physics continues normally.
 if(p.specialPc.kind==special_pc::Kind::gekko){
  s.falling.clear();return out;
 }
 bool supported=false;if(movement_){auto support=movement_->sweep(p.pose.feet,{0,-30,0},p.pose.capsule);supported=support&&support->normal[1]>=.70710678f;}
 const falling::Scope scope{epoch_,p.identity.slot,p.identity.instance,p.identity.character,p.life};
 // Water is not an exemption: the current JJ implementation is walking on
 // shallow flooded ground, not restored deep-water swimming.
 if(auto landing=s.falling.update({scope,p.pose.feet[1],now,supported,active_&&p.alive,bool(p.ladderAnchor)},p.maxHp);landing&&landing->damage){
  burning::Source self{{epoch_,p.identity.slot,p.identity.instance,p.identity.character,p.life},fall_event_weapon,0,p.team};
  environmental_damage(s,landing->damage,self,out,now);
 }
 return out;
}
Decision Authority::advance_falling(uint64_t now){
 Decision result;for(auto& s:slots_)if(s){auto one=sample_fall(*s,now);result.events.insert(result.events.end(),one.events.begin(),one.events.end());}return result;
}
}
