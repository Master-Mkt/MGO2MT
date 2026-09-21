#pragma once
#include <array>
#include <cstdint>
#include "original_weapon_parameters.h"
namespace mgo2mt::weapons::native_loadout {
inline constexpr std::array<uint16_t,3> initial{25,3,52};
// IDs are original; the user's fixed initial grant is a native policy.
constexpr bool attack_supported(uint16_t id) { return original_weapon::find(id)||id==1||id==50||(id>=52&&id<=59)||(id>=63&&id<=67)||id==69||id==73||(id>=128&&id<=131); }
constexpr bool held_only(uint16_t id) { return id&&!attack_supported(id); }
}
