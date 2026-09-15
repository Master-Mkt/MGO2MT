#pragma once
#include "combat_wire.h"
#include <array>
#include <algorithm>
#include <cmath>
namespace mgo2win::special_pc {
// A/Y have separate meaning only for an admitted special PC. Missing focus,
// identity or HOST ACK cannot replay an action after returning to a menu.
class Input {
 uint64_t epoch_=0,at_=0;combat::Identity identity_;uint32_t life_=0,counter_=0;
 Kind kind_=Kind::human;bool armed_=false,a_=false,y_=false,b_=false;Intent pending_;
public:
 bool scope(uint64_t epoch,combat::Identity id,uint32_t life,Kind kind){if(epoch_==epoch&&identity_==id&&life_==life&&kind_==kind)return false;*this={};epoch_=epoch;identity_=id;life_=life;kind_=kind;return true;}
 void cancel(){pending_={};at_=0;armed_=a_=y_=b_=false;}
 bool pending()const{return pending_.request!=0;}
 Intent intent(bool active)const{return active?pending_:Intent{};}
 void acknowledge(const combat::SopView& view,uint64_t now){
  if(!pending())return;if(now<at_||now-at_>=1500){pending_={};at_=0;return;}
  if(view.recipient==identity_&&view.life==life_&&view.specialPcRequest&&uint32_t(view.specialPcRequest-pending_.request)<0x80000000u){pending_={};at_=0;}
 }
 bool step(const std::array<float,24>& values,bool active,Action current,uint64_t now,bool climbAvailable=false){
  if(!active||kind_!=Kind::gekko||!epoch_||!life_||identity_.slot>=24||!identity_.instance||!identity_.character){cancel();return false;}
  for(auto value:values)if(!std::isfinite(value)){cancel();return false;}
  const bool a=values[5]>.12f,y=values[7]>.12f,b=values[4]>.12f;
  if(!armed_){if(std::none_of(values.begin(),values.end(),[](float v){return v>.12f;}))armed_=true;a_=a;y_=y;b_=b;return false;}
  Action request=a&&!a_?(climbAvailable?Action::climb:Action::jump):y&&!y_?Action::kick:b&&!b_?Action::salute:Action::none;a_=a;y_=y;b_=b;
  if(request==Action::none||current!=Action::none||pending())return false;
  if(++counter_==0)++counter_;pending_={request,counter_};at_=now;return true;
 }
};
}
