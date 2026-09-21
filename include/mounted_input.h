#pragma once
#include "mounted_weapons.h"
namespace mgo2mt::mounted {
// The remappable gameplay action also used for salute and contextual actions.
inline constexpr unsigned action_button=7;
// One physical edge per request. Acknowledgement is the HOST's admitted input
// sequence, including a refused request; a held button cannot mount twice.
class Input {
 uint64_t epoch_=0,actor_=0,life_=0,at_=0;
 uint32_t request_=0,sequence_=0;bool armed_=false,held_=false,sent_=false,consumed_=false;
 Intent pending_;
public:
 void cancel(){pending_={};armed_=held_=sent_=consumed_=false;at_=0;}
 void scope(uint64_t epoch,uint64_t actor,uint64_t life){if(epoch_!=epoch||actor_!=actor||life_!=life){cancel();request_=0;epoch_=epoch;actor_=actor;life_=life;}}
 bool pending()const{return pending_.action!=Action::none;}
 bool consumes_action()const{return consumed_||pending();}
 bool step(bool button,bool active,uint16_t attached,uint16_t nearby,uint64_t now){
  if(!active||!epoch_||!actor_||!life_){cancel();return false;}
  if(!button)consumed_=false;
  else if(attached||nearby||pending())consumed_=true;
  if(pending()&&(now<at_||now-at_>=1500)){pending_={};sent_=false;}
  if(!armed_){if(!button)armed_=true;held_=button;return false;}
  const bool edge=button&&!held_;held_=button;
  if(!edge||pending()||(!attached&&!nearby))return false;
  if(++request_==0)++request_;
  pending_={attached?Action::dismount:Action::mount,attached?uint16_t(0):nearby,request_};at_=now;sent_=false;return true;
 }
 void sent(uint32_t sequence){if(pending()&&!sent_){sequence_=sequence;sent_=true;}}
 void acknowledge(uint32_t sequence,bool sequenced){if(pending()&&sent_&&sequenced&&uint32_t(sequence-sequence_)<0x80000000u){pending_={};sent_=false;}}
 Intent intent(bool active)const{return active?pending_:Intent{};}
};
}
