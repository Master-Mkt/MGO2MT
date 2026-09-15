#pragma once
#include "ladder_action.h"
#include <algorithm>
namespace mgo2win::ladder {
class Input {
 uint64_t epoch_=0,actor_=0,life_=0,at_=0;bool armed_=false,held_=false,sent_=false;uint32_t sequence_=0;Intent pending_;
public:
 void cancel(){pending_={};armed_=held_=sent_=false;at_=0;}
 void scope(uint64_t epoch,uint64_t actor,uint64_t life){if(epoch_!=epoch||actor_!=actor||life_!=life){cancel();epoch_=epoch;actor_=actor;life_=life;}}
 bool pending()const{return pending_.action!=Action::none;}
 bool step(bool y,bool active,uint16_t attached,uint16_t nearAnchor,uint64_t now){
  if(!active||!epoch_||!actor_||!life_){cancel();return false;}if(pending()&&(now<at_||now-at_>=1500)){pending_={};sent_=false;}
  if(!armed_){if(!y)armed_=true;held_=y;return false;}bool edge=y&&!held_;held_=y;
  if(!edge||pending()||(!attached&&!nearAnchor))return false;pending_={attached?Action::leave:Action::enter,attached?attached:nearAnchor,0};sent_=false;at_=now;return true;
 }
 void sent(uint32_t sequence){if(pending()&&!sent_){sequence_=sequence;sent_=true;}}
 void acknowledge(uint32_t sequence,bool sequenced){if(pending()&&sent_&&sequenced&&uint32_t(sequence-sequence_)<0x80000000u){pending_={};sent_=false;}}
 Intent intent(uint16_t attached,float axis,bool active)const{if(!active)return {};if(pending())return pending_;return attached?Intent{Action::none,attached,std::clamp(axis,-1.f,1.f)}:Intent{};}
};
}
