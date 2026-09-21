#pragma once
#include "evade_action.h"
namespace mgo2mt::combat::evade_runtime {
// Native duration/nominal mean for visually reviewed original snake.mtar
// clips56+57 (40+45 frames; +4038+854 Z) and61 (45 frames; -1965.71875 Z).
// Runtime rolling now uses evade_travel_curve.h and stops with recovery.
// Backstep retains the existing constant-speed adapter. Neither is proof of
// the original action dispatcher or collision policy.
inline constexpr EvadeProfile roll{1417,4892.f/(85.f/60.f)};
inline constexpr EvadeProfile backstep{750,1965.71875f/.75f};
inline constexpr EvadeProfile profile(EvadeKind kind){return is_roll(kind)?roll:kind==EvadeKind::backstep?backstep:EvadeProfile{};}
}
