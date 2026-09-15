#pragma once
#include "gekko_jump.h"
namespace mgo2win::special_pc {
// Native mantle only: up outside the wall, traverse above its top, then settle.
// Never an original animation/physics claim. State stays in the HOST Slot.
struct Climb {stage::Vec3 start{},raised{},across{},landing{},feet{};uint32_t elapsedMs=0;bool cancelled=false,finished=false;};
std::optional<Climb> begin_climb(stage::Vec3 feet,float yaw,const stage::Collision&,std::span<const JumpBody> peers={});
bool advance_climb(Climb&,uint32_t elapsedMs,const stage::Collision&,std::span<const JumpBody> peers={});
void cancel_climb(Climb&);
}
