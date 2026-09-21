#pragma once
#include "combat_wire.h"
namespace mgo2mt::combat {
// Optional diagnostics are scoped independently from damage/event replication.
// Invalid, old, disabled and stale data is discarded without affecting gameplay.
class DebugFlightReceiver {
 bool enabled_=false;uint32_t life_=0,floor_=0,sequence_=0;uint64_t scene_=0,hostAt_=0,receivedAt_=0;
 std::optional<wire::DebugFlights> state_;
 static bool forward(uint32_t a,uint32_t b){return a!=b&&uint32_t(a-b)<0x80000000u;}
public:
 void clear(){*this={};}
 void request(bool enabled,uint32_t sequence,uint32_t life){if(!enabled){enabled_=false;state_.reset();return;}if(!enabled_||life_!=life){enabled_=true;life_=life;floor_=sequence;sequence_=0;scene_=hostAt_=receivedAt_=0;state_.reset();}}
 bool receive(const wire::DebugFlights&d,const wire::Offer&o,const Snapshot&s,uint64_t now){
  if(!enabled_||!life_||o.self.slot>=24||!o.self.instance||!o.self.character||d.epoch!=o.epoch||d.epoch!=s.epoch||d.recipient!=o.self||d.life!=life_||!d.scene||!d.sequence||d.flights.size()>16||(!forward(d.inputSequence,floor_)&&d.inputSequence!=floor_)||(sequence_&&!forward(d.sequence,sequence_))||d.scene<scene_||d.at<hostAt_)return false;
  const auto&me=s.players[o.self.slot];if(!me||me->identity!=o.self||me->life!=life_||!me->alive)return false;
  for(const auto&f:d.flights){if(!projectile::valid(f.owner)||!f.shot||f.simulatedAt>d.at)return false;const auto&p=s.players[f.owner.slot];if(!p||p->identity!=Identity{f.owner.slot,f.owner.instance,f.owner.character}||p->life!=f.owner.life)return false;}
  state_=d;sequence_=d.sequence;scene_=d.scene;hostAt_=d.at;receivedAt_=now;return true;
 }
 void poll(const std::optional<Snapshot>&s,wire::Status status,uint64_t now){if(!state_)return;if(!s||status!=wire::Status::active||now<receivedAt_||now-receivedAt_>500||s->epoch!=state_->epoch){state_.reset();return;}const auto&me=s->players[state_->recipient.slot];if(!me||me->identity!=state_->recipient||me->life!=state_->life||!me->alive){state_.reset();return;}for(const auto&f:state_->flights){const auto&p=s->players[f.owner.slot];if(!p||p->identity!=Identity{f.owner.slot,f.owner.instance,f.owner.character}||p->life!=f.owner.life){state_.reset();return;}}}
 const std::optional<wire::DebugFlights>& state()const{return state_;}
};
}
