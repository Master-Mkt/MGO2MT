#pragma once
#include "special_pc.h"
#include <optional>
#include <span>
namespace mgo2mt::special_pc {
struct JumpBody {stage::Vec3 feet{};stage::Capsule capsule{};};
struct RecoveryFall {stage::Vec3 feet{};float speed=0;uint64_t at=0;uint32_t life=0;bool grounded=false;};
bool advance_recovery_fall(RecoveryFall&,uint64_t now,const stage::Collision&,std::span<const JumpBody> peers={});
struct Jump {stage::Vec3 feet{},velocity{};uint32_t elapsedMs=0;bool horizontalStopped=false,upwardStopped=false,landed=false;};
// Native 10m trajectory, with a separately stretched airborne presentation. The original
// target-driven jump producer is known; its gameplay trajectory is not restored.
inline constexpr uint32_t jump_move_begin_ms=600,jump_move_end_ms=3458;
inline constexpr uint32_t jump_velocity_max_age_ms=250;
std::optional<Jump> begin_jump(stage::Vec3 feet,stage::Vec3 approvedVelocity);
// Absolute monotonic elapsed time, internally split into <= 16ms sweeps.
// Peers contain only live other participants. Invalid inputs leave state intact.
bool advance_jump(Jump&,uint32_t elapsedMs,const stage::Collision&,std::span<const JumpBody> peers={});
// Shared checked capsule/peer sweep for HOST-owned mantle paths.
float traversal_fraction(stage::Vec3 from,stage::Vec3 delta,const stage::Collision&,std::span<const JumpBody> peers={});
bool traversal_clear(stage::Vec3 feet,const stage::Collision&,std::span<const JumpBody> peers={});
// Cancellation leaves current feet unchanged; caller hands airborne feet to fall tracking.
bool cancel_jump(Jump&,const stage::Collision&,std::span<const JumpBody> peers={});
}
