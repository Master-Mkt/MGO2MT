#include "combat_authority.h"
#include "water_gameplay.h"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace mgo2mt::combat {
void Authority::configure_ladders(std::vector<ladder::Anchor> anchors){
 if(anchors.size()>128)throw std::invalid_argument("Ladder capacity");std::set<uint16_t> ids;
 for(const auto&a:anchors)if(!ladder::valid(a)||!ids.insert(a.id).second)throw std::invalid_argument("Ladder anchor");
 for(const auto&s:slots_)if(s&&s->state.ladderAnchor)throw std::logic_error("Cannot replace occupied ladder anchors");
 ladders_=std::move(anchors);
}
Decision Authority::ladder_action(Identity id,uint64_t epoch,uint32_t sequence,const ladder::Intent&i,uint64_t now,uint32_t life){
 if(epoch!=epoch_)return {Reject::generation,{}};auto*s=slot(id);if(!s)return {Reject::identity,{}};auto&p=s->state;
 if(!life||life!=p.life)return {Reject::generation,{}};
 if(!ladder::valid(i))return {Reject::unavailable,{}};
 if(!s->poseSequenced||s->poseSequence!=sequence||(s->ladderSequenced&&s->ladderSequence==sequence))return {Reject::sequence,{}};
 if(now<s->poseAt||now<s->ladderAt)return {Reject::clock,{}};
 const auto elapsed=s->ladderSequenced?(std::min)(uint64_t(100),now-s->ladderAt):0;
 s->ladderSequence=sequence;s->ladderSequenced=true;s->ladderAt=now;
 if(!active_)return {Reject::not_active,{}};if(!p.alive||p.stunned)return {Reject::dead,{}};
 if(p.mountedId||p.flightId||p.specialPc.kind!=special_pc::Kind::human||p.reloadUntil||p.cover.attached||p.cover.lean||p.evadeKind!=EvadeKind::none||p.specialPhase!=SpecialPhase::none||p.pose.capsule.height!=1700)return {Reject::unavailable,{}};
 if(!movement_||water_gameplay::sample(water_.get(),*movement_,p.pose.feet,p.pose.capsule,{waterRatio_,true}).foot==stage::WaterFoot::inWater)return {Reject::unavailable,{}};
 std::vector<ladder::Body> peers;for(const auto&other:slots_)if(other&&other->state.alive&&other->state.identity!=id)peers.push_back({other->state.pose.feet,other->state.pose.capsule});
 if(!s->ladderState){
  if(i.action!=ladder::Action::enter)return i.action==ladder::Action::none&&!i.anchorId&&!i.axis?Decision{}:Decision{Reject::unavailable,{}};
  auto a=std::find_if(ladders_.begin(),ladders_.end(),[&](const auto&a){return a.id==i.anchorId;});if(a==ladders_.end())return {Reject::unavailable,{}};
  auto entered=ladder::enter(*a,p.pose.feet,p.pose.capsule,*movement_,peers);if(!entered)return {Reject::obstructed,{}};
  s->ladderState=*entered;p.ladderAnchor=a->id;p.pose.feet=entered->feet;p.pose.yaw=a->facingYaw;s->poseAt=now;++revision_;return {};
 }
 if(i.action==ladder::Action::none&&!i.anchorId&&!i.axis)return {};
 if(i.anchorId!=p.ladderAnchor)return {Reject::identity,{}};
 auto next=*s->ladderState;
 if(i.action==ladder::Action::leave){if(!ladder::leave(next,*movement_,peers))return {Reject::obstructed,{}};p.pose.feet=next.feet;p.ladderAnchor=0;s->ladderState.reset();s->poseAt=now;++revision_;return {};}
 if(i.action==ladder::Action::enter)return {}; // An accepted edge cannot climb again.
 if(!ladder::advance(next,i.axis,double(elapsed)*.001,*movement_,peers))return {Reject::invalid_pose,{}};
 if(next.feet!=p.pose.feet){p.pose.feet=next.feet;s->poseAt=now;++revision_;}s->ladderState=next;return {};
}
}
