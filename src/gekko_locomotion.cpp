#include "gekko_locomotion.h"
#include <algorithm>
#include <cmath>
namespace mgo2mt::gekko_locomotion {
namespace {
constexpr float pi=3.14159265359f;
float wrap(float a){return std::remainder(a,2*pi);}
float turn(float a,float b,float limit){return wrap(a+std::clamp(wrap(b-a),-limit,limit));}
bool scope_valid(Scope s){return s.epoch&&s.scene&&s.character&&s.life&&s.instance&&s.slot<24;}
}
bool valid(const Policy&p){return std::isfinite(p.maximumSpeed)&&p.maximumSpeed>0&&p.maximumSpeed<=10000&&std::isfinite(p.acceleration)&&p.acceleration>0&&p.acceleration<=100000&&std::isfinite(p.deceleration)&&p.deceleration>0&&p.deceleration<=100000&&std::isfinite(p.yawRate)&&p.yawRate>0&&p.yawRate<=20&&std::isfinite(p.reverseCosine)&&p.reverseCosine<0&&p.reverseCosine>=-1&&p.reverseWaitMs<=1000&&p.maximumGapMs&&p.maximumGapMs<=1000;}
void State::reset(){*this=State{};}
Step State::update(Scope scope,Intent intent,uint64_t now,const Policy&p){
 if(!scope_valid(scope)||!valid(p)||!std::isfinite(intent.yaw)||std::abs(intent.yaw)>1000000||!std::isfinite(intent.speed)||intent.speed<0||intent.speed>p.maximumSpeed)return {};
 intent.yaw=wrap(intent.yaw);Step out;out.accepted=true;
 if(!ready_||scope!=scope_||now<at_||now-at_>p.maximumGapMs){reset();ready_=true;scope_=scope;at_=now;travelYaw_=yaw_=intent.yaw;out.yaw=yaw_;out.rebaselined=true;return out;}
 const auto elapsed=now-at_;at_=now;
 for(uint64_t ms=0;ms<elapsed;++ms){
  const bool release=intent.speed==0;const bool opposite=std::cos(wrap(intent.yaw-travelYaw_))<=p.reverseCosine;
  if(release){phase_=speed_>0?Phase::braking:Phase::stopped;waitMs_=0;}
  else if(phase_==Phase::moving&&speed_>0&&opposite){phase_=Phase::braking;waitMs_=0;}
  else if((phase_==Phase::braking||phase_==Phase::waiting)&&!opposite){phase_=Phase::moving;waitMs_=0;}
  else if(phase_==Phase::stopped){phase_=Phase::moving;}
  yaw_=turn(yaw_,intent.yaw,p.yawRate*.001f);
  const float oldSpeed=speed_,oldTravel=travelYaw_;
  if(phase_==Phase::braking){speed_=(std::max)(0.f,speed_-p.deceleration*.001f);if(!speed_){phase_=release?Phase::stopped:Phase::waiting;waitMs_=p.reverseWaitMs;}}
  else if(phase_==Phase::waiting){if(waitMs_)--waitMs_;if(!waitMs_){phase_=Phase::moving;travelYaw_=intent.yaw;}}
  else if(phase_==Phase::moving){travelYaw_=turn(travelYaw_,intent.yaw,p.yawRate*.001f);const float rate=intent.speed>=speed_?p.acceleration:p.deceleration;speed_+=std::clamp(intent.speed-speed_,-rate*.001f,rate*.001f);}
  const float midYaw=wrap(oldTravel+wrap(travelYaw_-oldTravel)*.5f),distance=(oldSpeed+speed_)*.0005f;
  out.displacement[0]+=std::sin(midYaw)*distance;out.displacement[1]+=std::cos(midYaw)*distance;
 }
 out.yaw=yaw_;out.phase=phase_;out.velocity={std::sin(travelYaw_)*speed_,std::cos(travelYaw_)*speed_};return out;
}
}
