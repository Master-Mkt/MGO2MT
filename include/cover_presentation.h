#pragma once
#include "cover_motion.h"
#include "cover_policy.h"
namespace mgo2win::cover {
// Presentation clock only. All enter/exit/side/stance boundaries use the
// caller's existing MotionBlend; geometry and shot origin remain HOST-owned.
class Timeline {
 bool attached_=false,crouched_=false;int side_=1,lean_=0;double seconds_=0;
 std::optional<Action> action_;
 static Action peek(bool crouch,int side,unsigned phase){return Action((crouch?(side>0?14:17):(side>0?4:7))+phase);}
public:
 void clear(){*this={};}
 std::optional<Sample> sample(const CoverMotionBank* bank,combat::cover::State state,bool crouch,float movement,double dt){
  if(!bank||!std::isfinite(dt)||dt<0||!combat::cover::valid(state)){clear();return {};}
  dt=(std::min)(dt,.1);std::optional<Action> next;
  if(state.attached){
   if(state.lean){next=peek(crouch,state.lean,0);if(attached_&&crouched_==crouch&&lean_==state.lean&&action_){next=action_;if(*next==peek(crouch,state.lean,0)&&seconds_>=duration(*next))next=peek(crouch,state.lean,1);}}
   else if(attached_&&crouched_==crouch&&lean_){next=peek(crouch,lean_,2);side_=lean_;}
   else if(attached_&&crouched_==crouch&&action_&&*action_==peek(crouch,side_,2)&&seconds_<duration(*action_))next=action_;
   else{if(std::abs(movement)>.01f)side_=movement>0?1:-1;next=Action((crouch?10:0)+(std::abs(movement)>.01f?2:0)+(side_>0?0:1));}
  }
  // FPP's separate original orientation layer is not implied by this bank.
  if(next!=action_)seconds_=0;else seconds_+=dt;
  action_=next;attached_=state.attached;crouched_=crouch;lean_=state.lean;
  return next?bank->sample(*next,seconds_):std::nullopt;
 }
 double seconds()const{return seconds_;}
};
class FreeLean {
 float amount_=0;
public:
 void clear(){amount_=0;}
 std::optional<MotionPose> sample(const MotionPose* aim,int desired,double dt){
  if(!aim||desired< -1||desired>1||!std::isfinite(dt)||dt<0){clear();return {};}
  amount_+=std::clamp(float(desired)-amount_,-float((std::min)(dt,.1)/.3),float((std::min)(dt,.1)/.3));
  if(std::abs(amount_)<.00001f){amount_=0;return {};}
  return native_side_lean(*aim,amount_<0?-1:1,std::abs(amount_));
 }
 int side()const{return amount_<0?-1:amount_>0?1:0;}
};
}
