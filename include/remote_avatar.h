#pragma once
#include "combat_authority.h"
#include "host_roster.h"
#include "player_motion.h"
namespace mgo2mt::remote {
// Native presentation from host-authorized GWCB pose/capsule/HP/life.
// Motion labels, speed thresholds and 100ms interpolation are Windows policy.
// Aim/supine/weapon attachments are not represented by this wire contract.
struct Avatar {
 combat::Identity identity;uint32_t life=0;std::array<uint8_t,28> appearance{};
 combat::Vec3 origin{};float yaw=0;PlayerMotion motion=PlayerMotion::Idle;
 double seconds=0;bool alive=true,stunned=false;
 combat::SpecialPhase specialPhase=combat::SpecialPhase::none;double specialSeconds=0;
 combat::EvadeKind evadeKind=combat::EvadeKind::none;uint32_t evadeSerial=0;double evadeSeconds=0;combat::cover::State cover;int8_t coverMove=0;special_pc::State specialPc;double specialPcSeconds=0;
};
class Scene {
 struct Track {
  Avatar avatar;combat::Player player;combat::Vec3 from{},target{};float fromYaw=0,targetYaw=0;
  uint64_t at=0,motionAt=0,movedAt=0,specialAt=0,evadeAt=0,specialPcAt=0;bool moving=false;float speed=0;
 };
 combat::Replica replica_;uint64_t revision_=0,epoch_=0,lastNow_=0;combat::Identity self_;
 std::array<std::optional<Track>,24> tracks_;
public:
 void clear();
 // Inactive/missing context must call clear; incomplete appearance stays absent.
 bool update(const combat::Snapshot&,const host::Roster&,combat::Identity self,uint64_t now);
 std::vector<Avatar> sample(uint64_t now)const;
};
}
