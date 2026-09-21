#pragma once
#include "weapon_extension_policy.h"
namespace mgo2mt::combat {
// Explicit native full-action durations where an original selector/state is
// not yet connected. Shared by HOST profile and presentation time scaling.
constexpr uint32_t native_reload_presentation_ms(uint16_t id){
 switch(id){case 2:return weapon_extensions::mk2.endMs;case 24:case 37:return 3500;case 50:return weapon_extensions::rpg7.endMs;default:return 0;}
}
}
