#include "combat_authority.h"
#include "water_gameplay.h"
#include <cmath>
namespace mgo2win::combat {
bool Authority::release_cover(Identity id){auto s=slot(id);if(!s)return false;if(s->state.cover!=cover::State{}){s->state.cover={};++revision_;}return true;}
void Authority::advance_cover(){
 for(auto&s:slots_)if(s){auto& p=s->state;if(!p.cover.attached&&!p.cover.lean)continue;
  const auto previous=p.cover;
  const auto wet=water_gameplay::sample(water_.get(),*movement_,p.pose.feet,p.pose.capsule,{waterRatio_,true});
  if(p.ladderAnchor||p.specialPc.kind!=special_pc::Kind::human||!active_||!p.alive||p.stunned||p.reloadUntil||p.specialPhase!=SpecialPhase::none||p.evadeKind!=EvadeKind::none||wet.foot==stage::WaterFoot::inWater)p.cover={};
  else p.cover=cover::evaluate(*movement_,p.pose.feet,p.pose.capsule,p.pose.yaw,p.cover,p.cover.lean,true);
  if(p.cover!=previous)++revision_;
 }
}
Reject Authority::cover(Identity id,uint64_t epoch,uint32_t sequence,const cover::Intent&i,uint64_t now,uint32_t life){
 if(epoch!=epoch_)return Reject::generation;auto s=slot(id);if(!s)return Reject::identity;if(life!=s->state.life||!life)return Reject::generation;
 if(!cover::valid(i))return Reject::unavailable;if(!s->poseSequenced||sequence!=s->poseSequence||(s->coverSequenced&&sequence==s->coverSequence))return Reject::sequence;
 if(now<s->poseAt)return Reject::clock;s->coverSequence=sequence;s->coverSequenced=true;
 // Only a new edge is executed; repeated already-ACKed edges still update the
 // current held lean. Neither a denied attach nor a detach can replay later.
 bool edge=false;
 if(i.request){const auto previous=s->coverRequest;if(previous&&uint32_t(i.request-previous)>=0x80000000u)return Reject::sequence;
  if(i.request!=previous){edge=true;s->coverRequest=s->sopView.coverRequest=i.request;++revision_;}}
 if(!active_)return Reject::not_active;if(!s->state.alive||s->state.stunned)return Reject::dead;
 if(edge&&i.action==cover::Action::detach){release_cover(id);return Reject::none;}
 auto& p=s->state;
 if(p.ladderAnchor||p.specialPc.kind!=special_pc::Kind::human||p.specialPhase!=SpecialPhase::none||p.evadeKind!=EvadeKind::none||p.reloadUntil){release_cover(id);return Reject::unavailable;}
 const auto wet=water_gameplay::sample(water_.get(),*movement_,p.pose.feet,p.pose.capsule,{waterRatio_,true});
 if(wet.foot==stage::WaterFoot::inWater){release_cover(id);return Reject::unavailable;}
 auto desired=p.cover;
 if(edge&&i.action==cover::Action::attach){auto contact=cover::acquire(*movement_,p.pose.feet,p.pose.capsule,p.pose.yaw);if(!contact)return Reject::obstructed;desired={true,0,std::atan2(contact->normal[0],contact->normal[2])};}
 desired=cover::evaluate(*movement_,p.pose.feet,p.pose.capsule,p.pose.yaw,desired,i.lean,i.firstPerson);
 if(desired!=p.cover){p.cover=desired;++revision_;}return Reject::none;
}
}
