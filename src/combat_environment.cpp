#include "combat_authority.h"
#include <algorithm>
namespace mgo2mt::combat {
namespace {
burning::Key key(uint64_t epoch,const Player&p){return {epoch,p.identity.slot,p.identity.instance,p.identity.character,p.life};}
Identity identity(burning::Key k){return {k.slot,k.instance,k.character};}
}
void Authority::environmental_damage(Slot& slot,uint32_t amount,const burning::Source& source,Decision& out,uint64_t now){
 auto& p=slot.state;if(!p.alive||!amount)return;regenerate(slot,now);
 const uint32_t damage=(std::min)(amount,p.hp);p.hp-=damage;p.alive=p.hp!=0;p.stunned=p.alive&&p.stamina==0;++revision_;
 Event e;e.kind=EventKind::damage;e.source=identity(source.actor);e.sourceLife=source.actor.life;e.target=p.identity;e.targetLife=p.life;e.weapon=source.weapon;e.object=source.object;e.position=p.pose.feet;e.hpDamage=damage;e.hp=p.hp;e.stamina=p.stamina;
 // emit() intentionally looks up current life for ordinary immediate events.
 // A delayed blast/burn must retain the original owner life even after respawn.
 emit(out,e);out.events.back().sourceLife=source.actor.life;
 if(p.alive)return;slot.regeneration.reset();
 release_special_pc(p.identity);p.specialPc.action=special_pc::Action::none;p.specialPc.serial=0;p.specialPc.elapsedMs=0;p.cover={};p.evadeKind=EvadeKind::none;p.evadeSerial=0;p.evadeElapsedMs=0;
 p.reloadUntil=0;p.reloadLevel=0;slot.reloadRefillAt=0;slot.accuracy.reset();slot.sopView.spreadMilliRadians=0;slot.burn.clear();p.burning=false;slot.ladderState.reset();p.ladderAnchor=0;
 clear_sop_slot(p.identity.slot);refresh_sop_views();
 if(auto& score=scores_[p.identity.slot];score&&score->id==p.identity&&score->deaths<1000000000)++score->deaths;
 // Delayed owner metadata stays in the event. Credit only the exact surviving
 // owner identity AND life, never a new occupant/respawn in the same slot.
 auto* owner=this->slot(identity(source.actor));
 if(owner&&owner->state.life==source.actor.life&&owner->state.identity!=p.identity&&(policy_.freeForAll||!source.team||source.team!=p.team))
  if(auto& score=scores_[source.actor.slot];score&&score->id==owner->state.identity&&score->kills<1000000000)++score->kills;
 e.kind=EventKind::death;e.hpDamage=0;emit(out,e);out.events.back().sourceLife=source.actor.life;
}
Decision Authority::advance_burning(uint64_t now){
 Decision out;
 for(auto& current:slots_)if(current){auto& s=*current;auto& p=s.state;s.burn.bind(key(epoch_,p));const auto source=s.burn.source();
  auto step=s.burn.advance(key(epoch_,p),now,p.maxHp,p.alive,active_,p.faceSubmerged);
  if(p.burning!=step.burning){p.burning=step.burning;++revision_;}
  environmental_damage(s,step.damage,source,out,now);
 }
 return out;
}
Decision Authority::explode(const burning::Blast& blast,uint64_t now){
 Decision out;
 if(!active_){out.reject=Reject::not_active;return out;}
 if(!burning::valid(blast)){out.reject=Reject::weapon;return out;}
 if(blast.source.actor.epoch!=epoch_){out.reject=Reject::generation;return out;}
 // Trusted HOST producer supplies accepted identity/life/team at launch, even
 // when that actor has since died or left. No socket invokes this API directly.
 auto candidate=explosionReplay_[blast.source.actor.slot];
 if(!candidate.accept(blast.source,blast.serial)){out.reject=Reject::sequence;return out;}
 for(const auto& s:slots_)if(s&&now<s->poseAt){out.reject=Reject::clock;return out;}
 explosionReplay_[blast.source.actor.slot]=candidate;
 out=advance_burning(now);
 Event burst;burst.kind=EventKind::explosion;burst.source=identity(blast.source.actor);burst.weapon=blast.source.weapon;burst.object=blast.source.object;burst.position=blast.position;emit(out,burst);out.events.back().sourceLife=blast.source.actor.life;
 for(auto& current:slots_)if(current){auto& s=*current;auto& p=s.state;
  if(!p.alive)continue;
  const bool self=p.identity==identity(blast.source.actor)&&p.life==blast.source.actor.life;
  if(!self&&!policy_.freeForAll&&!policy_.friendlyFire&&blast.source.team&&p.team==blast.source.team)continue;
  if(!burning::exposed(blast,p.pose.feet,p.pose.capsule,world_.get(),targets_.get()))continue;
  auto motionPolicy=blast_motion::default_for_weapon(blast.source.weapon);
  if(auto weapon=weapons_.find(blast.source.weapon);weapon!=weapons_.end()&&weapon->second.blastMotion)motionPolicy=*weapon->second.blastMotion;
  const auto velocity=blast_velocity(s,blast,motionPolicy);
  auto impactPoint=p.pose.feet;impactPoint[1]+=p.pose.capsule.height*.5f;
  // Establish forced motion before stun/death cleanup: an elevated mounted
  // seat must not recover to its old ground position before the blast.
  const bool launched=velocity&&launch_blast(s,*velocity,motionPolicy,now);
  environmental_damage(s,blast.damage,blast.source,out,now);
  if(blast.staminaDamage&&p.alive){regenerate(s,now);const auto amount=std::min(blast.staminaDamage,p.stamina);p.stamina-=amount;p.stunned=p.stamina==0;
   Event e;e.kind=EventKind::damage;e.source=identity(blast.source.actor);e.weapon=blast.source.weapon;e.target=p.identity;e.position=p.pose.feet;e.staminaDamage=amount;e.hp=p.hp;e.stamina=p.stamina;emit(out,e);out.events.back().sourceLife=blast.source.actor.life;++revision_;
   if(p.stunned){release_special_pc(p.identity);p.specialPhase=SpecialPhase::none;s.specialHeld=false;s.specialAt=now;p.cover={};p.evadeKind=EvadeKind::none;p.evadeSerial=0;p.evadeElapsedMs=0;p.reloadUntil=0;p.reloadLevel=0;s.reloadRefillAt=0;s.ladderState.reset();p.ladderAnchor=0;s.accuracy.reset();s.sopView.spreadMilliRadians=0;}
  }
  if(blast.ignite&&p.alive&&!p.faceSubmerged){s.burn.bind(key(epoch_,p));
   if(s.burn.ignite(key(epoch_,p),blast.source,blast.serial,now,burnPolicy_)&&!p.burning){p.burning=true;++revision_;}
  }
  if(velocity&&(launched||!p.alive)){
   Event impulse;impulse.kind=EventKind::knockback;impulse.source=identity(blast.source.actor);impulse.sourceLife=blast.source.actor.life;
   impulse.target=p.identity;impulse.targetLife=p.life;impulse.weapon=blast.source.weapon;impulse.object=blast.source.object;impulse.position=impactPoint;impulse.normal=*velocity;
   emit(out,impulse);out.events.back().sourceLife=blast.source.actor.life;
  }
 }
 return out;
}
}
