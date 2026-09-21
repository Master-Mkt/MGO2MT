#pragma once
#include "gekko_motion.h"
namespace mgo2mt::special_pc {
// Original pose samples, native greeting selection; no original salute label.
class GekkoGreeting {
 PlayerMotionBank bank_;
public:
 explicit GekkoGreeting(std::span<const char> data):bank_(data){auto c=bank_.find(PlayerMotion::Idle);if(bank_.size()!=1||!c||c->sourceIndex!=132||c->sourceKey!=0xAA3B67||c->frames!=140||c->fps!=60||c->loop||c->rootBone!=0xA89233||c->tracks.size()!=56)throw std::runtime_error("Invalid Gekko greeting bank");}
 std::optional<GekkoSample> sample(double seconds)const{if(!std::isfinite(seconds)||seconds<0)return {};auto p=bank_.sample(PlayerMotion::Idle,seconds);if(!p)return {};p->root[1]-=gekko_model_feet_offset;return GekkoSample{std::move(*p),0xAA3B67,132,0,seconds,0};}
};
// Native alternating contacts at half a source walk/run cycle. Not recovered
// MTSQ sound events. Scope changes, pauses and long stalls produce no backlog.
class GekkoFootsteps {
 uint64_t scope_=0,life_=0;uint32_t key_=0;double seconds_=0;bool valid_=false;
public:
 void reset(){*this={};}
 bool update(uint64_t scope,uint64_t life,uint32_t key,double seconds,bool eligible){
  if(!eligible||!scope||!life||!std::isfinite(seconds)||seconds<0||(key!=0xC06DF3&&key!=0xDF5481)){reset();return false;}
  const double period=key==0xC06DF3?102./120:72./120;
  const bool emit=valid_&&scope_==scope&&life_==life&&key_==key&&seconds>=seconds_&&seconds-seconds_<=.25&&std::floor(seconds/period)>std::floor(seconds_/period);
  scope_=scope;life_=life;key_=key;seconds_=seconds;valid_=true;return emit;
 }
};
}
