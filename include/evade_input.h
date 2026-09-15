#pragma once
#include "combat_wire.h"
namespace mgo2win::player {
// One A press survives coalescing/packet loss until the recipient-specific ACK.
// Cancelling pending work never reuses a request serial within the same life.
class EvadeInput {
 uint64_t epoch_=0,at_=0;combat::Identity identity_;uint32_t life_=0,counter_=0,request_=0;
 combat::EvadeKind kind_=combat::EvadeKind::none;
public:
 enum class Ack {none,accepted,rejected,expired};
 bool scope(uint64_t epoch,combat::Identity id,uint32_t life){if(epoch_==epoch&&identity_==id&&life_==life)return false;*this={};epoch_=epoch;identity_=id;life_=life;return true;}
 void cancel(){request_=0;kind_=combat::EvadeKind::none;at_=0;}
 bool pending()const{return request_!=0;}
 combat::EvadeKind kind()const{return kind_;}
 uint32_t request()const{return request_;}
 void apply(combat::wire::Input& input)const{
  input.evadeKind=kind_;input.evadeRequest=request_;
  if(input.suspended){input.evadeKind=combat::EvadeKind::none;input.evadeRequest=0;input.fire=input.firePressed=input.reload=input.specialPressed=input.specialHeld=false;return;}
  // A short local animation can finish before its network ACK arrives.
  // Retried requests must still exclude all competing action flags.
  if(request_)input.fire=input.firePressed=input.reload=input.specialPressed=input.specialHeld=false;
 }
 bool press(combat::EvadeKind kind,uint64_t now){
  if(!epoch_||!life_||identity_.slot>=24||!identity_.instance||!identity_.character||pending()||kind==combat::EvadeKind::none||!combat::valid_evade_kind(kind))return false;
  if(++counter_==0)++counter_;request_=counter_;kind_=kind;at_=now;return true;
 }
 Ack acknowledge(const combat::SopView& view,const combat::Player& player,uint64_t epoch,uint64_t now){
  if(!pending())return Ack::none;
  if(now<at_||now-at_>=1500){cancel();return Ack::expired;}
  if(epoch!=epoch_||view.recipient!=identity_||view.life!=life_||player.identity!=identity_||player.life!=life_)return Ack::none;
  if(!view.evadeRequest||uint32_t(view.evadeRequest-request_)>=0x80000000u)return Ack::none;
  const bool accepted=player.evadeSerial==request_&&player.evadeKind==kind_;
  cancel();return accepted?Ack::accepted:Ack::rejected;
 }
};
}
