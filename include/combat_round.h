#pragma once
#include "combat_wire.h"
#include "weapon_catalog.h"
#include "host_rules.h"
#include <functional>
#include <utility>
#include <map>

namespace mgo2win::combat {
// A native host lifecycle using reviewed content. It does not invent an
// original START opcode, spawn placement, weapon profile or DP balance.
class RoundCoordinator {
public:
 struct Policy {uint64_t countdownMs=120000;uint8_t generation=1;bool dpEnabled=false,autoAssign=true;std::array<uint8_t,16> restrictions{};uint32_t respawnDelayMs=3000;uint32_t roundDurationMs=0;bool endOnTimeout=false;bool freeForAll=false;};
 // Called inside the spawn transaction. Implementations must not advance the
 // selector's counter if Authority::join rejects the selected pose.
 using Spawn=std::function<bool(Authority&,Identity,uint8_t,std::span<const uint16_t>,uint64_t)>;
private:
 struct Participant {wire::RoundPlayer state;uint32_t balance=0,lastCommand=0;bool sequenced=false;wire::CommandError error=wire::CommandError::none;std::array<uint16_t,3> selected{};std::optional<uint64_t> diedAt;bool respawnEligible=false;};
 uint64_t epoch_=0,revision_=1,nextClock_=0;Policy policy_;host::RoundRules rules_;
 std::shared_ptr<const weapons::Catalog> catalog_;std::vector<uint16_t> supported_;uint8_t required_=0;
 std::array<std::optional<Participant>,24> players_{};Spawn spawn_;
 std::map<uint32_t,uint32_t> grantedCharacters_;
 std::optional<uint64_t> roundStarted_;uint32_t roundRemainingMs_=0;
 bool available_=false,startPending_=false,ended_=false;
 Participant* find(Identity);bool grant(Participant&,Authority&,uint64_t);
public:
 RoundCoordinator(uint64_t,Policy,std::shared_ptr<const weapons::Catalog>,std::span<const Weapon>,Spawn);
 bool join(Identity,uint64_t,std::optional<uint8_t> retainedTeam=std::nullopt);void leave(Identity);
 bool command(Identity,const wire::Command&,Authority&,uint64_t);
 void poll(Authority&,uint64_t);
 // Check before any command grant or simulation step; once latched, even a
 // clock regression cannot re-enable combat or mint a respawn entitlement.
 bool expire(Authority&,uint64_t);
 bool ended()const{return ended_;}
 wire::Preparation state(Identity,uint64_t)const;
 bool take_start(){return std::exchange(startPending_,false);}
};
}
