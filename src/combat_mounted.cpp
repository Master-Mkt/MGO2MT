#include "combat_authority.h"
#include <algorithm>
#include <cmath>
namespace mgo2mt::combat {
namespace {
float distance(Vec3 a,Vec3 b){return std::sqrt((a[0]-b[0])*(a[0]-b[0])+(a[1]-b[1])*(a[1]-b[1])+(a[2]-b[2])*(a[2]-b[2]));}
bool newer(uint32_t a,uint32_t b){return a!=b&&uint32_t(a-b)<0x80000000u;}
bool sight(const stage::Collision* c,Vec3 a,Vec3 b){if(!c)return true;const float d=distance(a,b);if(d<.01f)return true;Vec3 direction{};for(int n=0;n<3;++n)direction[n]=(b[n]-a[n])/d;auto hit=c->ray(a,direction,d);return !hit||hit->distance>=d-4;}
}
Authority::MountedState* Authority::mounted_state(const Slot&s){auto i=mounted_.find(s.state.mountedId);return i==mounted_.end()?nullptr:&i->second;}
const Authority::MountedState* Authority::mounted_state(const Slot&s)const{auto i=mounted_.find(s.state.mountedId);return i==mounted_.end()?nullptr:&i->second;}
bool Authority::mounted_pose(const Slot&s,Pose& p)const{
 const auto*m=mounted_state(s);if(!m||!movement_)return false;
 mounted::clamp_aim(m->instance,m->type,p.yaw,p.pitch);
 if(m->type.kind==mounted::Kind::catapult){p.feet=s.state.pose.feet;return true;}
 auto previous=s.state.pose.feet;const auto path=mounted::operator_path(m->instance,m->type,s.state.pose.yaw,p.yaw);
 auto blocked=[&](Vec3 next){
  auto probe=next;probe[1]+=12;const auto floor=movement_->ray(probe,{0,-1,0},32,stage::query::floor);if(!floor||std::abs(floor->normal[1])<.7f)return true;
  Vec3 delta{};for(unsigned n=0;n<3;++n)delta[n]=next[n]-previous[n];
  for(const auto& collision:{movement_,targets_})if(collision){if(!collision->clear(next,p.capsule,stage::query::player))return true;if(auto hit=collision->sweep(previous,delta,p.capsule,stage::query::player);hit&&hit->fraction<.9999f)return true;}
  for(const auto& other:slots_)if(other&&other->state.alive&&other->state.identity!=s.state.identity){const auto& q=other->state.pose;
   if(next[1]>=q.feet[1]+q.capsule.height||q.feet[1]>=next[1]+p.capsule.height)continue;
   const float length=delta[0]*delta[0]+delta[2]*delta[2];const float t=length>0?std::clamp(((q.feet[0]-previous[0])*delta[0]+(q.feet[2]-previous[2])*delta[2])/length,0.f,1.f):0;
   if(std::hypot(previous[0]+delta[0]*t-q.feet[0],previous[2]+delta[2]*t-q.feet[2])<p.capsule.radius+q.capsule.radius)return true;
  }
  return false;
 };
 for(const auto next:path){if(blocked(next)){p.yaw=s.state.pose.yaw;p.feet=s.state.pose.feet;return true;}previous=next;}
 p.feet=previous;return true;
}
bool Authority::configure_mounted(const mounted::Registry& registry,uint8_t map){
 if(!epoch_||!map||active_||std::any_of(slots_.begin(),slots_.end(),[](const auto&s){return bool(s);}))return false;
 std::map<uint16_t,MountedState> next;
 for(const auto&i:registry.scene(map)){const auto*t=registry.find(i.type);if(!mounted::valid(i)||!t||!mounted::valid(*t)||next.size()>=128)return false;
  uint16_t ammo=0;
  if(t->kind!=mounted::Kind::catapult){
   const auto profile=weapons_.find(t->weapon);if(profile==weapons_.end())return false;const auto&w=profile->second;
   if(w.heldOnly||w.meleeAttack||w.nativePlaced||special_pc::weapon(w.id))return false;
   if(t->kind==mounted::Kind::mortar){if(w.id!=103||!w.nativeProjectile||!w.mountedOnly)return false;}
   else if(w.nativeProjectile)return false;ammo=w.magazine;
  }else if(t->weapon)return false;
  if(!next.emplace(i.id,MountedState{i,*t,ammo}).second)return false;
 }
 auto geometry=std::make_shared<const stage::Collision>(mounted::with_collision(*world_,registry,map));auto movement=stage::movement_collision(geometry);mounted_=std::move(next);mountedRegistry_=registry;mountedMap_=map;world_=std::move(geometry);movement_=std::move(movement);++revision_;return true;
}
bool Authority::release_mounted(Identity id,bool preserveSeat){auto*s=slot(id);if(!s)return false;if(!s->state.mountedId)return true;
 auto&p=s->state;if(!preserveSeat&&p.alive){const auto*m=mounted_state(*s);if(m&&m->type.kind==mounted::Kind::catapult&&!recover_catapult(*s,s->mountedPreviousFeet)&&active_&&!p.stunned)return false;}p.mountedId=0;p.weapon=s->mountedPreviousWeapon;s->mountedPreviousWeapon=0;const auto*held=holding(*s,p.weapon);p.ammo=held?uint16_t(held->contents.magazine):0;p.reserve=held?uint16_t(held->contents.reserve):0;p.aiming=false;p.reloadUntil=0;p.reloadElapsedMs=0;p.reloadLevel=0;s->reloadRefillAt=0;
 s->fireAt=s->carriedFireAt;s->fireSubMsNs=s->carriedFireSubMs;s->fired=s->carriedFired;s->accuracy.reset();s->sopView.spreadMilliRadians=0;s->falling.clear();++revision_;return true;
}
Reject Authority::mount(Identity id,uint64_t epoch,uint32_t sequence,const mounted::Intent&i,uint64_t now,uint32_t life){
 if(epoch!=epoch_)return Reject::generation;auto*s=slot(id);if(!s)return Reject::identity;if(!life||life!=s->state.life)return Reject::generation;if(!mounted::valid(i)||i.action==mounted::Action::none)return Reject::unavailable;
 if(!s->poseSequenced||s->poseSequence!=sequence||(s->mountedSequenced&&!newer(i.request,s->mountedRequest)))return Reject::sequence;
 if(now<s->poseAt)return Reject::clock;s->mountedSequenced=true;s->mountedRequest=i.request;
 if(!active_)return Reject::not_active;auto&p=s->state;if(!p.alive||p.stunned)return Reject::dead;if(now-s->poseAt>policy_.stalePoseMs)return Reject::invalid_pose;
 if(p.flightId)return Reject::unavailable;
 if(i.action==mounted::Action::dismount)return release_mounted(id)?Reject::none:Reject::obstructed;
 if(p.mountedId||p.flightId||p.specialPc.kind!=special_pc::Kind::human||p.specialPc.action!=special_pc::Action::none||p.ladderAnchor||p.cover.attached||p.cover.lean||p.evadeKind!=EvadeKind::none||p.specialPhase!=SpecialPhase::none||p.faceSubmerged||p.pose.capsule.height!=1700)return Reject::unavailable;
 finish_reload(*s,now);if(p.reloadUntil||now<s->meleeUntil)return Reject::reloading;
 auto found=mounted_.find(i.instance);if(found==mounted_.end())return Reject::unavailable;auto& m=found->second;
 if(std::any_of(slots_.begin(),slots_.end(),[&](const auto&o){return o&&o->state.mountedId==i.instance;}))return Reject::unavailable;
 if(!mounted::within_use_range(m.instance,m.type,p.pose.feet))return Reject::invalid_pose;
 const auto position=mounted::operator_position(m.instance,m.type);
 auto checkGround=[&](Vec3 feet){feet[1]+=12;auto h=movement_->ray(feet,{0,-1,0},32,stage::query::floor);return h&&std::abs(h->normal[1])>=.7f;};
 const bool catapult=m.type.kind==mounted::Kind::catapult;
 if((!catapult&&!checkGround(position))||!checkGround(p.pose.feet)||!movement_->clear(position,p.pose.capsule)||(targets_&&!targets_->clear(position,p.pose.capsule,stage::query::player)))return Reject::obstructed;
 Vec3 move{};for(int n=0;n<3;++n)move[n]=position[n]-p.pose.feet[n];if(!catapult)if(auto h=movement_->sweep(p.pose.feet,move,p.pose.capsule);h&&h->fraction<.9999f)return Reject::obstructed;if(!catapult&&targets_)if(auto h=targets_->sweep(p.pose.feet,move,p.pose.capsule,stage::query::player);h&&h->fraction<.9999f)return Reject::obstructed;
 for(const auto&o:slots_)if(o&&o->state.alive&&o->state.identity!=id){const auto&q=o->state;if(std::hypot(position[0]-q.pose.feet[0],position[2]-q.pose.feet[2])<p.pose.capsule.radius+q.pose.capsule.radius&&position[1]<q.pose.feet[1]+q.pose.capsule.height&&q.pose.feet[1]<position[1]+p.pose.capsule.height)return Reject::obstructed;}
 auto eye=p.pose.feet;eye[1]+=p.pose.capsule.height-150;auto operatorEye=position;operatorEye[1]+=p.pose.capsule.height-150;if(!sight(movement_.get(),eye,operatorEye)||!sight(targets_.get(),eye,operatorEye))return Reject::obstructed;
 s->mountedPreviousFeet=p.pose.feet;s->mountedPreviousWeapon=p.weapon;s->carriedFireAt=s->fireAt;s->carriedFireSubMs=s->fireSubMsNs;s->carriedFired=s->fired;s->fireAt=m.fireAt;s->fireSubMsNs=m.subMs;s->fired=m.fired;
 p.mountedId=i.instance;if(!catapult){p.weapon=m.type.weapon;p.ammo=m.ammo;p.reserve=0;}p.pose.feet=position;p.pose.yaw=m.instance.yaw;if(m.type.kind!=mounted::Kind::gun)p.pose.pitch=m.type.initialPitch;mounted::clamp_aim(m.instance,m.type,p.pose.yaw,p.pose.pitch);p.aiming=false;s->accuracy.reset();s->sopView.spreadMilliRadians=0;s->movementSpeed=0;s->falling.clear();s->approvedVelocity={};s->previousApprovedVelocity={};s->poseAt=now;++revision_;return Reject::none;
}
void Authority::advance_mounted(uint64_t now){for(auto&s:slots_)if(s&&s->state.mountedId&&(!active_||!s->state.alive||s->state.stunned||now<s->poseAt||now-s->poseAt>=policy_.stalePoseMs))release_mounted(s->state.identity);}
}
