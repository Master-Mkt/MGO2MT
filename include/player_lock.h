#pragma once
#include "enemy_tag_target.h"
#include "original_lock_policy.h"
#include <tuple>

namespace mgo2mt::player_lock {
using combat::Identity;
using combat::Vec3;

// A synthetic cone policy or recovered current-AK geometry. The latter still
// uses this native candidate's camera frame, torso point, roster and collision
// adapters; it is not a restoration of the original aim matrix/bone pipeline.
struct Policy {
 float range, coneCos, aimHeightFraction;
 std::optional<original_lock::Parameters> original;
 Policy()=delete;
 Policy(float distance,float cosine,float height):range(distance),coneCos(cosine),aimHeightFraction(height){}
 Policy(original_lock::Parameters parameters,float height):range(parameters.acquire.range),coneCos(0),aimHeightFraction(height),original(parameters){}
};
struct Input {
 bool enabled=false,active=false;
 Identity self;
 uint64_t expectedEpoch=0,sceneToken=0;
 unsigned rule=~0u;
 Vec3 eye{},look{};
};
struct Target {
 Identity identity;
 uint32_t life=0;
 Vec3 aimPoint{};
 float distance=0;
};

// acquire() explicitly chooses a target. update() only maintains that identity
// and life; losing it never silently switches to another player. No camera,
// weapon, clock, network, room option or personal name-tag state is modified.
class Lock {
 Policy policy_;
 struct Context {uint64_t epoch,scene;Identity self;uint32_t life;unsigned rule;};
 std::optional<Context> context_;
 std::optional<Target> target_;
 uint64_t revision_=0;
 static bool valid_identity(Identity id){return id.slot<24&&id.instance&&id.character;}
 static bool roster_matches(const host::Roster&r,Identity id){
  if(!valid_identity(id))return false;
  const auto&p=r.slots[id.slot];
  return p&&p->slot==id.slot&&p->instance==id.instance&&p->character==id.character;
 }
 const combat::Player* observer(const combat::Snapshot&s,const host::Roster&r,const Input&i)const{
  if(!valid_policy()||!i.enabled||!i.active||!i.expectedEpoch||s.epoch!=i.expectedEpoch||!s.revision||
     !i.sceneToken||!r.complete||!roster_matches(r,i.self)||!enemy_tag::finite(i.eye)||!enemy_tag::unit(i.look)||
     (i.rule!=0&&i.rule!=1))return nullptr;
  const auto&p=s.players[i.self.slot];
  if(!p||p->identity!=i.self||!p->life||!p->alive||p->stunned)return nullptr;
  if(i.rule==1&&(p->team<1||p->team>2))return nullptr;
  return &*p;
 }
 std::optional<Target> candidate(const combat::Snapshot&s,const host::Roster&r,const Input&i,
   const combat::Player&me,unsigned slot,const stage::Collision&world,const stage::Collision*objects,bool retaining=false)const{
  const auto&p=s.players[slot];
  if(!p||p->identity.slot!=slot||p->identity==i.self||p->identity.character==i.self.character||!p->life||
     !p->alive||p->stunned||!roster_matches(r,p->identity))return {};
  if(i.rule==1&&(p->team<1||p->team>2||p->team==me.team))return {};
  const auto&pose=p->pose;
  if(!enemy_tag::finite(pose.feet)||!std::isfinite(pose.capsule.height)||!std::isfinite(pose.capsule.radius)||
     pose.capsule.radius<=0||pose.capsule.height<2*pose.capsule.radius)return {};
  auto aim=pose.feet;aim[1]+=pose.capsule.height*policy_.aimHeightFraction;
  auto delta=enemy_tag::sub(aim,i.eye);auto direction=enemy_tag::unit(delta),look=enemy_tag::unit(i.look);
  if(!direction||!look||!enemy_tag::finite(aim))return {};
  float distance=std::sqrt(enemy_tag::dot(delta,delta));
  if(policy_.original){
   auto right=enemy_tag::unit(Vec3{(*look)[2],0,-(*look)[0]});if(!right)return {};
   Vec3 up{(*look)[1]*(*right)[2],(*look)[2]*(*right)[0]-(*look)[0]*(*right)[2],-(*look)[1]*(*right)[0]};
   auto local=Vec3{enemy_tag::dot(delta,*right),enemy_tag::dot(delta,up),enemy_tag::dot(delta,*look)};
   if(!original_lock::evaluate_local(retaining?policy_.original->retain:policy_.original->acquire,local).accepted)return {};
  }else if(distance>policy_.range||enemy_tag::dot(*direction,*look)<policy_.coneCos)return {};
  if(!enemy_tag::visible(i.eye,aim,world,objects))return {};
  // A living body between the observer and selected capsule blocks capture,
  // including friends and bodies whose roster entry is not yet available.
  auto entry=enemy_tag::capsule(i.eye,*direction,pose);if(!entry)return {};
  for(unsigned n=0;n<s.players.size();++n)if(n!=slot){const auto&body=s.players[n];
   if(body&&body->identity!=i.self&&body->alive)
    if(auto hit=enemy_tag::capsule(i.eye,*direction,body->pose);hit&&*hit<*entry)return {};
  }
  return Target{p->identity,p->life,aim,distance};
 }
public:
 explicit Lock(Policy p):policy_(p){}
 bool valid_policy()const{return std::isfinite(policy_.range)&&policy_.range>0&&policy_.range<1e6f&&
  std::isfinite(policy_.coneCos)&&policy_.coneCos>=0&&policy_.coneCos<=1&&
  std::isfinite(policy_.aimHeightFraction)&&policy_.aimHeightFraction>=0&&policy_.aimHeightFraction<=1&&
  (!policy_.original||(original_lock::valid(policy_.original->acquire)&&original_lock::valid(policy_.original->retain)&&policy_.original->retain.range<1e6f));}
 void clear(){target_.reset();context_.reset();revision_=0;}
 const std::optional<Target>& current()const{return target_;}
 std::optional<Target> acquire(const combat::Snapshot&s,const host::Roster&r,const Input&i,
   const stage::Collision&world,const stage::Collision*objects=nullptr){
  clear();const auto*me=observer(s,r,i);if(!me)return {};
  const auto look=*enemy_tag::unit(i.look);float bestCos=-1;
  for(unsigned slot=0;slot<s.players.size();++slot)if(auto p=candidate(s,r,i,*me,slot,world,objects)){
   auto direction=*enemy_tag::unit(enemy_tag::sub(p->aimPoint,i.eye));float cosine=enemy_tag::dot(direction,look);
   auto key=[](Identity id){return std::tuple{id.slot,id.instance,id.character};};
   const bool prefer=policy_.original?original_lock::prefer_mode0(p->distance,target_?std::optional(target_->distance):std::nullopt):
    !target_||cosine>bestCos||(cosine==bestCos&&(p->distance<target_->distance||
      (p->distance==target_->distance&&key(p->identity)<key(target_->identity))));
   if(prefer){target_=p;bestCos=cosine;}
  }
  if(target_){context_=Context{s.epoch,i.sceneToken,i.self,me->life,i.rule};revision_=s.revision;}
  return target_;
 }
 std::optional<Target> update(const combat::Snapshot&s,const host::Roster&r,const Input&i,
   const stage::Collision&world,const stage::Collision*objects=nullptr){
  if(!target_||!context_){clear();return {};}
  const auto*me=observer(s,r,i);
  if(!me||s.epoch!=context_->epoch||i.sceneToken!=context_->scene||i.self!=context_->self||
     me->life!=context_->life||i.rule!=context_->rule||s.revision<revision_){clear();return {};}
  auto next=candidate(s,r,i,*me,target_->identity.slot,world,objects,true);
  if(!next||next->identity!=target_->identity||next->life!=target_->life){clear();return {};}
  target_=next;revision_=s.revision;return target_;
 }
};
}
