#pragma once
#include "combat_authority.h"
namespace mgo2mt::special_pc {
// Observe authoritative transitions only; held buttons/repeated snapshots do not replay.
class SaluteAudio {
 struct Seen {combat::Identity identity{};uint32_t life=0,serial=0;Action action=Action::none;};
 std::array<Seen,24> seen_{};uint64_t epoch_=0,scene_=0;
public:
 std::vector<combat::Vec3> update(const combat::Snapshot& state,uint64_t scene){
  std::vector<combat::Vec3> result;const bool baseline=!state.epoch||!scene||epoch_!=state.epoch||scene_!=scene;
  if(baseline)seen_={};epoch_=state.epoch;scene_=scene;
  for(size_t i=0;i<seen_.size();++i){const auto& p=state.players[i];auto& old=seen_[i];
   if(!p){old={};continue;}const auto& a=p->specialPc;
   if(!baseline&&old.identity==p->identity&&old.life==p->life&&p->alive&&a.kind==Kind::gekko&&a.action==Action::salute&&a.serial&&a.elapsedMs<300&&(old.action!=a.action||old.serial!=a.serial))result.push_back(p->pose.feet);
   old={p->identity,p->life,a.serial,a.action};
  }return result;
 }
 void clear(){seen_={};epoch_=scene_=0;}
};
}
