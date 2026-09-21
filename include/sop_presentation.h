#pragma once
#include "combat_sop_view.h"
#include <algorithm>
#include <optional>
namespace mgo2mt::sop {
// Native presentation policy. The original activation shader and duration have
// not been recovered: one expanding shell is explicitly a Windows prototype.
struct Pulse {combat::Vec3 origin;float radius=0,width=700,opacity=.4f;};
class Presentation {
 uint64_t epoch_=0,generation_=0,now_=0,began_=0;combat::SopView view_{};bool ready_=false,pulse_=false;
 std::array<std::optional<std::pair<combat::Identity,uint32_t>>,24> targets_{};
public:
 void clear(){*this={};}
 bool update(uint64_t generation,const combat::Snapshot& snapshot,combat::Identity self,
             const combat::SopView& view,bool active,uint64_t now){
  if(!active||!generation||!snapshot.epoch||view.recipient!=self||!combat::valid_sop_view(view,snapshot)||
     self.slot>=24||!snapshot.players[self.slot]||!snapshot.players[self.slot]->alive){clear();return false;}
  bool fresh=!ready_||generation_!=generation||epoch_!=snapshot.epoch||view_.recipient!=self||view_.life!=view.life||now<now_;
  bool trigger=!fresh&&!view.jammed&&view.visibleMask&&view.activation!=view_.activation&&
      uint32_t(view.activation-view_.activation)<0x80000000u;
  if(fresh||view.jammed||!view.visibleMask)pulse_=false;
  if(trigger){began_=now;pulse_=true;}
  targets_={};for(unsigned i=0;i<24;++i)if(view.visibleMask&(1u<<i))targets_[i]=std::pair{snapshot.players[i]->identity,snapshot.players[i]->life};
  generation_=generation;epoch_=snapshot.epoch;view_=view;ready_=true;now_=now;return trigger;
 }
 bool visible(combat::Identity target,uint32_t life,const combat::Snapshot& snapshot)const{
  if(!ready_||view_.jammed||snapshot.epoch!=epoch_||target.slot>=24||!(view_.visibleMask&(1u<<target.slot)))return false;
  if(!targets_[target.slot]||*targets_[target.slot]!=std::pair{target,life})return false;
  const auto& p=snapshot.players[target.slot];const auto& self=snapshot.players[view_.recipient.slot];
  return p&&p->identity==target&&p->life==life&&p->alive&&self&&self->identity==view_.recipient&&
      self->life==view_.life&&self->alive&&self->team&&self->team==p->team;
 }
 std::optional<Pulse> pulse(uint64_t now)const{
  if(!pulse_||now<began_||now-began_>=1200)return std::nullopt;
  float t=float(now-began_)/1200.f;return Pulse{view_.origin,t*48000.f,700.f,.4f*(1.f-t)};
 }
};
// Pending Y is held until its pose acknowledgement arrives. It cannot leave
// movement frozen indefinitely after a rejected/stale pose or a stalled host.
class SpecialInput {
 bool edge_=false,pending_=false,sent_=false;uint32_t sequence_=0;uint64_t at_=0;
public:
 void clear(){*this={};}
 void press(uint64_t now){edge_=pending_=true;sent_=false;at_=now;}
 bool edge()const{return edge_;}
 bool pending()const{return pending_;}
 void sent(uint32_t seq){if(edge_){sequence_=seq;sent_=true;}edge_=false;}
 void acknowledge(const combat::SopView& view,uint64_t now){
  if(pending_&&((sent_&&view.inputSequenced&&uint32_t(view.inputSequence-sequence_)<0x80000000u)||now<at_||now-at_>=1500))clear();
 }
};
class PhaseClock {
 combat::Identity identity_;uint32_t life_=0;uint64_t epoch_=0,at_=0;combat::SpecialPhase phase_=combat::SpecialPhase::none;
public:
 void clear(){*this={};}
 double update(uint64_t epoch,const combat::Player& p,uint64_t now){
  if(epoch_!=epoch||identity_!=p.identity||life_!=p.life||phase_!=p.specialPhase||now<at_){epoch_=epoch;identity_=p.identity;life_=p.life;phase_=p.specialPhase;at_=now;}
  return double(now-at_)/1000.;
 }
};
}
