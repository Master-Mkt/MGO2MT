#include "combat_authority.h"
#include <algorithm>
#include <cmath>
namespace mgo2mt::combat {
namespace {
float distance(Vec3 a,Vec3 b){return std::sqrt((a[0]-b[0])*(a[0]-b[0])+(a[1]-b[1])*(a[1]-b[1])+(a[2]-b[2])*(a[2]-b[2]));}
bool newer(uint32_t a,uint32_t b){return a!=b&&uint32_t(a-b)<0x80000000u;}

}
Reject Authority::assign_special(Identity id,special_pc::Kind kind,bool showName,uint64_t now){
 auto*s=slot(id);if(!s)return Reject::identity;if(!special_pc::valid(kind))return Reject::unavailable;if(now<s->poseAt)return Reject::clock;
 auto& p=s->state;if(p.mountedId||p.flightId||p.ladderAnchor)return Reject::unavailable;if(p.specialPc.kind==kind){if(kind==special_pc::Kind::gekko&&p.specialPc.nameVisible!=showName){p.specialPc.nameVisible=showName;++revision_;}return Reject::none;}
 if(!p.alive||p.stunned)return Reject::dead;if(p.specialPc.action!=special_pc::Action::none)return Reject::unavailable;
 for(const auto& held:s->inventory)if(held.revision==UINT64_MAX)return Reject::sequence;
 if(kind==special_pc::Kind::gekko)for(uint16_t weapon=128;weapon<=131;++weapon)if(!weapons_.contains(weapon))return Reject::unavailable;
 auto capsule=kind==special_pc::Kind::gekko?special_pc::native_gekko.capsule:s->humanCapsule;
 if(!movement_->clear(p.pose.feet,capsule))return Reject::obstructed;
 for(auto&other:slots_)if(other&&other->state.alive&&other->state.identity!=id){auto& q=other->state;auto d=Vec3{p.pose.feet[0]-q.pose.feet[0],0,p.pose.feet[2]-q.pose.feet[2]};if(std::hypot(d[0],d[2])<capsule.radius+q.pose.capsule.radius&&p.pose.feet[1]<q.pose.feet[1]+q.pose.capsule.height&&q.pose.feet[1]<p.pose.feet[1]+capsule.height)return Reject::obstructed;}
 regenerate(*s,now);
 if(kind==special_pc::Kind::gekko){s->humanHp=p.maxHp;s->humanStamina=p.maxStamina;s->humanCapsule=p.pose.capsule;p.maxHp=special_pc::native_gekko.hp;p.maxStamina=special_pc::native_gekko.stamina;}
 else{p.maxHp=s->humanHp;p.maxStamina=s->humanStamina;}
 // A HOST form change preserves the current health fraction; toggling forms
 // cannot act as a heal/revive or replenish magazine contents.
 const uint32_t oldHp=kind==special_pc::Kind::gekko?s->humanHp:special_pc::native_gekko.hp;
 const uint32_t oldStamina=kind==special_pc::Kind::gekko?s->humanStamina:special_pc::native_gekko.stamina;
 p.hp=std::max(1u,uint32_t(uint64_t(p.hp)*p.maxHp/oldHp));p.stamina=uint32_t(uint64_t(p.stamina)*p.maxStamina/oldStamina);
 if(kind==special_pc::Kind::gekko){
  s->humanWeapon=p.weapon;s->humanEquipment=s->selectedEquipment;
  for(size_t i=0;i<s->inventory.size();++i){s->humanInventory[i]=s->inventory[i].contents;s->inventory[i].contents={};++s->inventory[i].revision;}
  constexpr uint8_t slots[]{0,1,2,items::knife_slot};
  for(unsigned i=0;i<4;++i){const auto&w=weapons_.at(uint16_t(128+i));s->inventory[slots[i]].contents={w.id,1,w.magazine,0,0,items::Resource::ammunition,items::Domain::weapon};}
  p.weapon=128;s->selectedEquipment=255;
 }else{
  for(size_t i=0;i<s->inventory.size();++i){s->inventory[i].contents=s->humanInventory[i];++s->inventory[i].revision;}
  p.weapon=s->humanWeapon;s->selectedEquipment=s->humanEquipment;
 }
 const auto* held=holding(*s,p.weapon);p.ammo=held?uint16_t(held->contents.magazine):0;p.reserve=held?uint16_t(held->contents.reserve):0;
 s->fired=false;s->fireSequenced=false;s->falling.clear();
 p.pose.capsule=capsule;p.specialPc={kind,kind==special_pc::Kind::human||showName};p.reloadUntil=0;p.reloadLevel=0;s->reloadRefillAt=0;p.evadeKind=EvadeKind::none;p.evadeSerial=0;p.evadeElapsedMs=0;
 s->specialJump.reset();s->specialClimb.reset();s->specialRecovery.reset();s->approvedVelocity={};s->previousApprovedVelocity={};s->approvedVelocityAt=s->previousApprovedVelocityAt=now;
 s->regeneration.reset();regenerate(*s,now);
 s->specialPcRequest=0;s->specialPcSequence=0;s->specialPcSequenced=false;s->specialPcAt=0;s->sopView.specialPcRequest=0;
 release_cover(id);sopGroups_.clear(id.slot);clear_sop_slot(id.slot);refresh_sop_views();s->accuracy.reset();s->sopView.spreadMilliRadians=0;s->poseAt=now;++revision_;return Reject::none;
}
Reject Authority::special_action(Identity id,uint64_t epoch,uint32_t sequence,const special_pc::Intent&i,uint64_t now,uint32_t life){
 if(epoch!=epoch_)return Reject::generation;auto*s=slot(id);if(!s)return Reject::identity;if(life!=s->state.life||!life)return Reject::generation;
 if(!special_pc::valid(i)||!i.request)return Reject::unavailable;
 if(!s->poseSequenced||sequence!=s->poseSequence||(s->specialPcSequenced&&sequence==s->specialPcSequence)||!newer(i.request,s->specialPcRequest))return Reject::sequence;
 if(now<s->poseAt||now<s->specialPcAt)return Reject::clock;
 s->specialPcSequenced=true;s->specialPcSequence=sequence;s->specialPcRequest=s->sopView.specialPcRequest=i.request;++revision_;
 auto& p=s->state;if(!active_)return Reject::not_active;if(!p.alive||p.stunned)return Reject::dead;if(p.specialPc.kind!=special_pc::Kind::gekko||p.specialPc.action!=special_pc::Action::none)return Reject::unavailable;
 auto floorOrigin=p.pose.feet;floorOrigin[1]+=10;auto floor=movement_->ray(floorOrigin,{0,-1,0},30,stage::query::player_floor);if(!floor||std::abs(floor->normal[1])<.7f)return Reject::invalid_pose;
 s->specialClimb.reset();s->specialMeleeWeapon=130;
 if(i.action==special_pc::Action::climb){std::vector<special_pc::JumpBody> peers;for(const auto& other:slots_)if(other&&other->state.alive&&other->state.identity!=id)peers.push_back({other->state.pose.feet,other->state.pose.capsule});
  s->specialClimb=special_pc::begin_climb(p.pose.feet,p.pose.yaw,*movement_,peers);if(!s->specialClimb)return Reject::obstructed;
 }
 if(i.action==special_pc::Action::jump){Vec3 velocity=s->approvedVelocity;auto at=s->approvedVelocityAt;if(std::hypot(velocity[0],velocity[2])<.1f){velocity=s->previousApprovedVelocity;at=s->previousApprovedVelocityAt;}if(now<at||now-at>special_pc::jump_velocity_max_age_ms)velocity={};const float speed=std::hypot(velocity[0],velocity[2]);if(speed>special_pc::native_gekko.runSpeed)for(auto&v:velocity)v*=special_pc::native_gekko.runSpeed/speed;s->specialJump=special_pc::begin_jump(p.pose.feet,velocity);if(!s->specialJump)return Reject::invalid_pose;}else s->specialJump.reset();
 if(i.action==special_pc::Action::jump||i.action==special_pc::Action::climb){s->falling.clear();s->specialRecovery.reset();}
 p.specialPc.action=i.action;p.specialPc.serial=i.request;p.specialPc.elapsedMs=0;s->specialPcAt=now;s->specialPcStart=p.pose.feet;s->specialPcHit=false;++revision_;return Reject::none;
}
void Authority::clear_jump_velocity(Identity id){if(auto*s=slot(id)){s->approvedVelocity={};s->previousApprovedVelocity={};}}
void Authority::release_special_pc(Identity id){
 if(auto* body=slot(id);body&&(!active_||!body->state.alive||body->state.stunned)){if(!active_||!body->state.alive||!body->state.blastFlight)cancel_catapult(*body,false);release_mounted(id);}
 auto*s=slot(id);if(!s||s->state.specialPc.action==special_pc::Action::none)return;auto&p=s->state;const bool traversal=p.specialPc.action==special_pc::Action::jump||p.specialPc.action==special_pc::Action::climb;
 if(p.specialPc.action==special_pc::Action::jump&&movement_&&s->specialJump){std::vector<special_pc::JumpBody> peers;for(const auto& other:slots_)if(other&&other->state.alive&&other->state.identity!=id)peers.push_back({other->state.pose.feet,other->state.pose.capsule});s->specialJump->feet=p.pose.feet;special_pc::cancel_jump(*s->specialJump,*movement_,peers);p.pose.feet=s->specialJump->feet;}s->specialJump.reset();s->specialClimb.reset();if(traversal)s->falling.clear();
 p.specialPc.action=special_pc::Action::none;p.specialPc.serial=0;p.specialPc.elapsedMs=0;if(traversal){s->specialRecovery=special_pc::RecoveryFall{p.pose.feet,0,s->poseAt,p.life};sample_fall(*s,s->poseAt);}++revision_;
}
Decision Authority::advance_special_pc(uint64_t now){
 Decision result;for(auto&s:slots_)if(s){auto& p=s->state;auto& state=p.specialPc;
  if(s->specialRecovery){
   if(!active_||!p.alive||state.kind!=special_pc::Kind::gekko||s->specialRecovery->life!=p.life)s->specialRecovery.reset();
   else{std::vector<special_pc::JumpBody> peers;for(const auto& other:slots_)if(other&&other->state.alive&&other->state.identity!=p.identity)peers.push_back({other->state.pose.feet,other->state.pose.capsule});
    if(special_pc::advance_recovery_fall(*s->specialRecovery,now,*movement_,peers)){
     if(p.pose.feet!=s->specialRecovery->feet){p.pose.feet=s->specialRecovery->feet;s->poseAt=now;++revision_;}
     auto fall=sample_fall(*s,now);result.events.insert(result.events.end(),fall.events.begin(),fall.events.end());
     if(!p.alive||s->specialRecovery->grounded)s->specialRecovery.reset();
    }
   }
  }
  if(state.action==special_pc::Action::none)continue;if(now<s->specialPcAt)continue;
  if(!active_||!p.alive||p.stunned){release_special_pc(p.identity);continue;}
  const auto elapsed=std::min<uint64_t>(now-s->specialPcAt,special_pc::duration(state.action));
  if(state.action==special_pc::Action::jump&&s->specialJump){std::vector<special_pc::JumpBody> peers;for(const auto& other:slots_)if(other&&other->state.alive&&other->state.identity!=p.identity)peers.push_back({other->state.pose.feet,other->state.pose.capsule});if(special_pc::advance_jump(*s->specialJump,uint32_t(elapsed),*movement_,peers)&&s->specialJump->feet!=p.pose.feet){p.pose.feet=s->specialJump->feet;s->poseAt=now;++revision_;}}
  if(state.action==special_pc::Action::climb&&s->specialClimb){std::vector<special_pc::JumpBody> peers;for(const auto& other:slots_)if(other&&other->state.alive&&other->state.identity!=p.identity)peers.push_back({other->state.pose.feet,other->state.pose.capsule});
   const bool advanced=special_pc::advance_climb(*s->specialClimb,uint32_t(elapsed),*movement_,peers);
   if(s->specialClimb->feet!=p.pose.feet){p.pose.feet=s->specialClimb->feet;s->poseAt=now;++revision_;}
   if(!advanced&&s->specialClimb->cancelled){release_special_pc(p.identity);continue;}
  }
  if(state.action==special_pc::Action::kick&&!s->specialPcHit&&elapsed>=special_pc::native_gekko.kickHitMs){s->specialPcHit=true;const Vec3 forward{std::sin(p.pose.yaw),0,std::cos(p.pose.yaw)};
   const auto profile=weapons_.find(s->specialMeleeWeapon);if(profile==weapons_.end())continue;const auto& configured=profile->second;
   for(auto& victim:slots_)if(victim&&victim->state.alive&&victim->state.identity!=p.identity){auto& q=victim->state;if(!policy_.freeForAll&&p.team&&p.team==q.team&&!policy_.friendlyFire)continue;
    Vec3 target=q.pose.feet;target[1]+=std::min(900.f,q.pose.capsule.height*.5f);auto origin=p.pose.feet;origin[1]+=900;const auto length=distance(origin,target);if(length>configured.range+q.pose.capsule.radius||length<1)continue;
    Vec3 direction{};for(unsigned i=0;i<3;++i)direction[i]=(target[i]-origin[i])/length;const float along=direction[0]*forward[0]+direction[2]*forward[2];const float lateral=std::abs((target[0]-origin[0])*forward[2]-(target[2]-origin[2])*forward[0]);if((s->specialMeleeWeapon!=131&&(along<.6f||lateral>special_pc::native_gekko.kickRadius+q.pose.capsule.radius))||std::abs(target[1]-origin[1])>1000)continue;
    bool blocked=false;for(auto& collision:{world_,targets_})if(collision)if(auto hit=collision->ray(origin,direction,length);hit&&hit->distance<length-1)blocked=true;if(blocked)continue;
    regenerate(*victim,now);Event damage;damage.kind=EventKind::damage;damage.source=p.identity;damage.target=q.identity;damage.weapon=s->specialMeleeWeapon;damage.position=target;damage.hpDamage=std::min(q.hp,configured.damage);damage.staminaDamage=std::min(q.stamina,configured.staminaDamage);q.hp-=damage.hpDamage;q.stamina-=damage.staminaDamage;q.alive=q.hp!=0;q.stunned=q.alive&&q.stamina==0;damage.hp=q.hp;damage.stamina=q.stamina;++revision_;emit(result,damage);
    if(!q.alive||q.stunned)release_mounted(q.identity);
    if(q.stunned){release_special_pc(q.identity);q.specialPhase=SpecialPhase::none;victim->specialHeld=false;q.cover={};q.evadeKind=EvadeKind::none;q.evadeSerial=0;q.evadeElapsedMs=0;q.reloadUntil=0;q.reloadLevel=0;victim->reloadRefillAt=0;victim->ladderState.reset();q.ladderAnchor=0;victim->accuracy.reset();victim->sopView.spreadMilliRadians=0;}
    if(!q.alive){victim->regeneration.reset();victim->burn.clear();q.burning=false;victim->ladderState.reset();q.ladderAnchor=0;release_special_pc(q.identity);q.cover={};q.evadeKind=EvadeKind::none;q.evadeSerial=0;q.evadeElapsedMs=0;q.specialPc.action=special_pc::Action::none;q.specialPc.serial=0;q.specialPc.elapsedMs=0;q.reloadUntil=0;q.reloadLevel=0;victim->reloadRefillAt=0;victim->accuracy.reset();victim->sopView.spreadMilliRadians=0;clear_sop_slot(q.identity.slot);refresh_sop_views();if(auto& score=scores_[q.identity.slot];score&&score->deaths<1000000000)++score->deaths;if((policy_.freeForAll||!p.team||p.team!=q.team)&&scores_[p.identity.slot]&&scores_[p.identity.slot]->kills<1000000000)++scores_[p.identity.slot]->kills;damage.kind=EventKind::death;damage.hpDamage=damage.staminaDamage=0;emit(result,damage);}
   }
  }
  const auto duration=special_pc::duration(state.action);
  if(elapsed>=duration){const bool traversal=state.action==special_pc::Action::jump||state.action==special_pc::Action::climb;s->specialJump.reset();s->specialClimb.reset();if(traversal)s->falling.clear();s->approvedVelocity={};s->previousApprovedVelocity={};state.action=special_pc::Action::none;state.serial=0;state.elapsedMs=0;if(traversal){s->specialRecovery=special_pc::RecoveryFall{p.pose.feet,0,now,p.life};sample_fall(*s,now);}++revision_;}
  else if(state.elapsedMs!=elapsed){state.elapsedMs=uint16_t(elapsed);++revision_;}
 }return result;
}
}
