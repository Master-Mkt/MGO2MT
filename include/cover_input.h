#pragma once
#include "combat_wire.h"
namespace mgo2win::player {
// Reliable contextual edges share HOST lifetime/identity, never a client pose flag.
class CoverInput {
 uint64_t epoch_=0,at_=0;combat::Identity identity_;uint32_t life_=0,counter_=0;
 combat::cover::Intent pending_;
public:
 bool scope(uint64_t epoch,combat::Identity identity,uint32_t life){if(epoch==epoch_&&identity==identity_&&life==life_)return false;*this={};epoch_=epoch;identity_=identity;life_=life;return true;}
 void cancel(){pending_={};at_=0;}
 bool pending()const{return pending_.request!=0;}
 combat::cover::Action action()const{return pending_.action;}
 bool press(combat::cover::Action action,uint64_t now){
  if(!epoch_||!life_||identity_.slot>=24||!identity_.instance||!identity_.character||action==combat::cover::Action::none||unsigned(action)>2||pending())return false;
  if(++counter_==0)++counter_;pending_={counter_,action};at_=now;return true;
 }
 void acknowledge(const combat::SopView& view,uint64_t now){
  if(!pending())return;
  if(now<at_||now-at_>=1500){cancel();return;}
  if(view.recipient==identity_&&view.life==life_&&view.coverRequest&&uint32_t(view.coverRequest-pending_.request)<0x80000000u)cancel();
 }
 combat::cover::Intent intent(int lean,bool firstPerson,bool active)const{
  if(!active)return {};auto result=pending_;result.lean=lean>=-1&&lean<=1?int8_t(lean):0;result.firstPerson=firstPerson;return result;
 }
};
}
