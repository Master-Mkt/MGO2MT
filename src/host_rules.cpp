#include "host_rules.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace mgo2win::host {
RoundRules::RoundRules(RoundPolicy policy):policy_(policy){
 if(!policy.minimum_players||policy.minimum_players>participants_.size())throw std::invalid_argument("minimum players");
}
ReadyParticipant* RoundRules::find(const ParticipantToken&t){
 if(t.slot>=participants_.size())return nullptr;
 auto&p=participants_[t.slot];return p&&p->token==t?&*p:nullptr;
}
std::size_t RoundRules::player_count()const{
 return std::count_if(participants_.begin(),participants_.end(),[](const auto&p){return p&&p->role==ParticipantRole::player;});
}
void RoundRules::arm(uint64_t now){
 if(phase_==RoundPhase::waiting&&!deadline_&&player_count()){
  const auto max=std::numeric_limits<uint64_t>::max();
  deadline_=policy_.countdown_ms>max-now?max:now+policy_.countdown_ms;
 }
}
bool RoundRules::join(const ParticipantToken&t,ParticipantRole role,uint64_t now){
 if(t.slot>=participants_.size()||!t.character||
    (role!=ParticipantRole::player&&role!=ParticipantRole::spectator&&role!=ParticipantRole::dedicated_host))return false;
 auto&p=participants_[t.slot];if(p)return p->token==t&&p->role==role;
 if(std::any_of(participants_.begin(),participants_.end(),[&](const auto&v){return v&&v->token.character==t.character;}))return false;
 p=ReadyParticipant{t,role};arm(now);return true;
}
bool RoundRules::leave(const ParticipantToken&t){
 if(!find(t))return false;participants_[t.slot].reset();
 if(phase_==RoundPhase::waiting&&!player_count())deadline_.reset();return true;
}
void RoundRules::reset(uint64_t now){
 if(generation_==std::numeric_limits<uint64_t>::max())throw std::overflow_error("round generation");
 ++generation_;phase_=RoundPhase::waiting;deadline_.reset();
 for(auto&p:participants_)if(p){p->prepared=false;p->ready=false;}
 arm(now);
}
bool RoundRules::set_prepared(const ParticipantToken&t,uint64_t generation,bool prepared){
 auto*p=find(t);if(!p||generation!=generation_||p->role!=ParticipantRole::player||phase_!=RoundPhase::waiting)return false;
 p->prepared=prepared;if(!prepared)p->ready=false;return true;
}
bool RoundRules::set_ready(const ParticipantToken&t,uint64_t generation,bool ready){
 auto*p=find(t);if(!p||generation!=generation_||p->role!=ParticipantRole::player||phase_!=RoundPhase::waiting||(!p->prepared&&ready))return false;
 p->ready=ready;return true;
}
StartReason RoundRules::advance(uint64_t now){
 if(phase_!=RoundPhase::waiting||player_count()<policy_.minimum_players)return StartReason::none;
 bool all_ready=true;
 for(const auto&p:participants_)if(p&&p->role==ParticipantRole::player){
  // The caller defines the prerequisite for this preparation boundary. Receipt
  // of room metadata must never also be treated as a scene/object-ready ACK.
  if(!p->prepared)return StartReason::none;
  all_ready=all_ready&&p->ready;
 }
 auto reason=all_ready?StartReason::all_ready:deadline_&&now>=*deadline_?StartReason::countdown:StartReason::none;
 if(reason!=StartReason::none){phase_=RoundPhase::preparing;deadline_.reset();}
 return reason;
}
WeaponAccess weapon_access(const WeaponOption&w,bool enabled,uint32_t balance){
 if(!w.allowed)return WeaponAccess::restricted;
 if(!enabled)return w.available_without_dp?WeaponAccess::allowed:WeaponAccess::dp_disabled;
 return w.dp_cost<=balance?WeaponAccess::allowed:WeaponAccess::insufficient_dp;
}
LoadoutQuote quote_loadout(std::span<const WeaponOption>weapons,bool enabled,uint32_t balance){
 LoadoutQuote out;
 for(const auto&w:weapons){
  auto access=weapon_access(w,enabled,balance);
  if(access!=WeaponAccess::allowed)return {access,out.cost};
  if(enabled){out.cost+=w.dp_cost;if(out.cost>balance)return {WeaponAccess::insufficient_dp,out.cost};}
 }
 return out;
}
SpecialPolicy::SpecialPolicy(std::span<const uint16_t>catalog):catalog_(catalog.begin(),catalog.end()){
 std::sort(catalog_.begin(),catalog_.end());catalog_.erase(std::unique(catalog_.begin(),catalog_.end()),catalog_.end());
}
bool SpecialPolicy::allows(uint16_t id)const{return std::binary_search(catalog_.begin(),catalog_.end(),id);}
}
