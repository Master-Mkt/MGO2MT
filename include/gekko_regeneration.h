#pragma once
#include <algorithm>
#include <cstdint>
namespace mgo2win::special_pc::regeneration {
// User-requested Windows rule, not a recovered original MGS4/MGO2 ability.
inline constexpr uint32_t maximum_hp=1000;
struct Scope {
 uint64_t epoch=0;uint8_t slot=255;uint16_t instance=0;uint32_t character=0,life=0;
 bool operator==(const Scope&)const=default;
};
struct Policy {uint32_t fullRecoveryMs=30000;bool operator==(const Policy&)const=default;};
inline constexpr bool valid(Scope s){return s.epoch&&s.slot<24&&s.instance&&s.character&&s.life;}
inline constexpr bool valid(Policy p){return p.fullRecoveryMs>=1&&p.fullRecoveryMs<=30000;}
class State {
 Scope scope_;uint64_t at_=0,remainder_=0;uint32_t period_=0;bool armed_=false;
public:
 void reset(){*this={};}
 uint64_t fraction()const{return remainder_;}
 bool armed()const{return armed_;}
 // Settle at the damage timestamp BEFORE subtracting authoritative damage.
 // Do not reset this state on a later hit: fractional HP and the clock survive.
 // First valid sample establishes a baseline, never heals retroactively.
 uint32_t advance(Scope scope,Policy policy,uint64_t now,uint32_t hp,bool eligible){
  if(!eligible||!valid(scope)||!valid(policy)||!hp||hp>maximum_hp){reset();return hp;}
  if(!armed_||scope_!=scope){scope_=scope;at_=now;remainder_=0;period_=policy.fullRecoveryMs;armed_=true;return hp;}
  // A stale timestamp cannot roll back the accumulated clock or double-credit
  // a later interval. A changed policy starts at its actual adoption timestamp.
  if(now<at_)return hp;
  if(period_!=policy.fullRecoveryMs){remainder_=remainder_*policy.fullRecoveryMs/period_;period_=policy.fullRecoveryMs;at_=now;if(hp==maximum_hp)remainder_=0;return hp;}
  const uint64_t elapsed=(std::min)(now-at_,uint64_t(period_));at_=now;
  if(hp==maximum_hp){remainder_=0;return hp;}
  const uint64_t credit=elapsed*maximum_hp+remainder_;
  const uint32_t heal=uint32_t(credit/period_);
  remainder_=credit%period_;
  const auto result=hp+(std::min)(heal,maximum_hp-hp);
  if(result==maximum_hp)remainder_=0;
  return result;
 }
};
}
