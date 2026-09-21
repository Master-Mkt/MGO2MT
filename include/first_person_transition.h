#pragma once
#include "character_renderer.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace mgo2mt {
// Native presentation timing. Gameplay aim and the HOST shot origin remain
// authoritative; only the local view and non-arm visibility travel in time.
class FirstPersonTransition {
 WorldView visible_{};std::array<float,3> offset_{},fromDirection_{};
 uint64_t started_=0,last_=0;float body_=1,fromBody_=1,fromFov_=1;
 bool ready_=false,first_=false,moving_=false;
 static float ease(float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}
public:
 static constexpr uint64_t camera_ms=180,fade_out_ms=70,fade_in_delay_ms=90;
 void reset(){*this={};}
 bool split_body()const{return first_||moving_;}
 float body_opacity()const{return body_;}
 void presented(const WorldView& view){visible_=view;}
 WorldView update(const WorldView& target,bool first,uint64_t now){
  if(!ready_||now<last_){reset();ready_=true;first_=first;body_=first?0.f:1.f;visible_=target;last_=now;return visible_;}
  last_=now;
  if(first!=first_){first_=first;started_=now;moving_=true;fromBody_=body_;fromFov_=visible_.verticalFov;fromDirection_=visible_.direction;for(unsigned i=0;i<3;++i)offset_[i]=visible_.eye[i]-target.eye[i];}
  if(!moving_){visible_=target;body_=first?0.f:1.f;return visible_;}
  const auto age=now-started_;const float t=ease(float(age)/float(camera_ms));visible_=target;
  visible_.verticalFov=fromFov_*(1-t)+target.verticalFov*t;
  for(unsigned i=0;i<3;++i){visible_.eye[i]+=offset_[i]*(1-t);visible_.direction[i]=fromDirection_[i]*(1-t)+target.direction[i]*t;}
  float length=0;for(auto v:visible_.direction)length+=v*v;
  if(length>1e-10f){length=std::sqrt(length);for(auto&v:visible_.direction)v/=length;}else visible_.direction=target.direction;
  body_=first?fromBody_*(1-ease(float(age)/float(fade_out_ms))):fromBody_+(1-fromBody_)*ease(float(age>fade_in_delay_ms?age-fade_in_delay_ms:0)/float(camera_ms-fade_in_delay_ms));
  if(age>=camera_ms){moving_=false;visible_=target;body_=first?0.f:1.f;}
  return visible_;
 }
};
}
