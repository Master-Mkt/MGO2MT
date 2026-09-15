#include "combat_falling.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace mgo2win::combat::falling {
namespace {bool valid(Scope s){return s.epoch&&s.slot<24&&s.instance&&s.character&&s.life;}}
bool valid(Policy p)noexcept{return std::isfinite(p.safeHeight)&&std::isfinite(p.severeHeight)&&std::isfinite(p.fatalHeight)&&p.safeHeight>=0&&p.safeHeight<p.severeHeight&&p.severeHeight<p.fatalHeight&&p.fatalHeight<1000000&&p.severePermille>0&&p.severePermille<1000;}
std::optional<uint32_t> damage(float height,uint32_t maxHp,Policy p)noexcept{
 if(!valid(p)||!std::isfinite(height)||height<0||!maxHp||maxHp>1000000)return {};
 if(height<=p.safeHeight)return 0;if(height>=p.fatalHeight)return maxHp;
 const double severe=double(p.severePermille)/1000;
 const double fraction=height<=p.severeHeight?severe*(double(height)-p.safeHeight)/(p.severeHeight-p.safeHeight):severe+(1-severe)*(double(height)-p.severeHeight)/(p.fatalHeight-p.severeHeight);
 // Round damage down, preserving at least one HP below the fatal threshold.
 return (std::min)(maxHp-1,uint32_t(std::floor(fraction*maxHp)));
}
Tracker::Tracker(Policy p):policy_(p){if(!valid(p))throw std::invalid_argument("Native fall policy");}
void Tracker::reset_tracking(){previousY_=peakY_=0;at_=0;clock_=primed_=airborne_=observedGround_=false;}
void Tracker::clear(){scope_={};reset_tracking();}
std::optional<Landing> Tracker::update(const Input&i,uint32_t maxHp){
 if(!valid(i.scope))return {};
 if(scope_==i.scope&&clock_&&i.nowMs<at_)return {}; // Old input cannot erase evidence.
 if(!i.eligible||i.ladder){clear();return {};}
 if(!std::isfinite(i.feetY)||std::abs(i.feetY)>=1000000||!maxHp||maxHp>1000000)return {};
 if(fatalScope_&&*fatalScope_==i.scope)return {};
 if(scope_!=i.scope){scope_=i.scope;fatalScope_.reset();reset_tracking();}
 if(clock_&&i.nowMs==at_&&previousY_==i.feetY&&observedGround_==i.grounded)return {};
 clock_=true;at_=i.nowMs;observedGround_=i.grounded;
 if(!primed_){primed_=true;previousY_=peakY_=i.feetY;airborne_=!i.grounded;return {};}
 const auto emit=[&](float height,bool fatal)->std::optional<Landing>{
  if(serial_==std::numeric_limits<uint64_t>::max()){fatalScope_=i.scope;return {};}
  if(fatal)fatalScope_=i.scope;
  return Landing{height,*damage(height,maxHp,policy_),fatal,++serial_};
 };
 if(!i.grounded){if(!airborne_)peakY_=(std::max)(previousY_,i.feetY);else peakY_=(std::max)(peakY_,i.feetY);airborne_=true;previousY_=i.feetY;
  const float height=(std::max)(0.f,peakY_-i.feetY);if(height>=policy_.fatalHeight)return emit(height,true);return {};
 }
 previousY_=i.feetY;if(!airborne_){peakY_=i.feetY;return {};}
 const float height=(std::max)(0.f,peakY_-i.feetY);airborne_=false;peakY_=i.feetY;
 return emit(height,height>=policy_.fatalHeight);
}
}
