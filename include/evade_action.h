#pragma once
#include <cstdint>
#include <cmath>
namespace mgo2mt::combat {
enum class EvadeKind:uint8_t {none=0,roll=1,backstep=2,rollLeft=3,rollRight=4};
// Native directional variants reuse the reviewed forward roll phases.
inline constexpr bool is_roll(EvadeKind kind){return kind==EvadeKind::roll||kind==EvadeKind::rollLeft||kind==EvadeKind::rollRight;}
inline constexpr int evade_side(EvadeKind kind){return kind==EvadeKind::rollLeft?-1:kind==EvadeKind::rollRight?1:0;}
// Host-selected native execution policy. Zero leaves the action unavailable;
// callers must supply reviewed clip timing/speed, never client packet values.
struct EvadeProfile {uint16_t durationMs=0;float speed=0;};
inline bool valid_evade_kind(EvadeKind kind){return unsigned(kind)<=4;}
inline bool valid_evade_profile(EvadeProfile p){return p.durationMs&&p.durationMs<=10000&&std::isfinite(p.speed)&&p.speed>0&&p.speed<=6000;}
}
