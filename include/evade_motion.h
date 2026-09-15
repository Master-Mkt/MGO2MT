#pragma once
#include "player_motion.h"
#include "evade_runtime_profile.h"
#include <stdexcept>
namespace mgo2win::player {
class EvadeMotionBank {
 PlayerMotionBank bank_;
public:
 struct Sample {MotionPose pose;PlayerMotion phase;};
 explicit EvadeMotionBank(std::span<const char> bytes):bank_(bytes){
  auto valid=[&](PlayerMotion action,uint32_t key,uint32_t index,uint32_t frames){auto* c=bank_.find(action);return c&&c->sourceKey==key&&c->sourceIndex==index&&c->frames==frames&&c->fps==60&&!c->loop;};
  if(bank_.size()!=3||!valid(PlayerMotion::Roll,0x57bb63,56,40)||!valid(PlayerMotion::RollRecover,0x52de74,57,45)||!valid(PlayerMotion::Backstep,0x55b29b,61,45))throw std::runtime_error("Unreviewed evasion motion bank");
 }
 std::optional<Sample> sample(combat::EvadeKind kind,double seconds)const{
  if(!std::isfinite(seconds)||seconds<0)return {};
  if(kind==combat::EvadeKind::none||!combat::valid_evade_kind(kind))return {};
  // Left/right use the same reviewed 56/57 data. The HOST pose already carries
  // travel yaw; never apply a second side offset or rename an unknown clip.
  auto phase=kind==combat::EvadeKind::backstep?PlayerMotion::Backstep:seconds>=40./60.?PlayerMotion::RollRecover:PlayerMotion::Roll;
  if(phase==PlayerMotion::RollRecover)seconds-=40./60.;
  return Sample{*bank_.sample(phase,seconds),phase};
 }
};
}
