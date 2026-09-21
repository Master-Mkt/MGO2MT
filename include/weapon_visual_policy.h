#pragma once
#include <cstdint>
#include "original_weapon_parameters.h"
namespace mgo2mt::combat {
// Original inventory identities: AR M4/AK102/G3A3/MK17/XM8/PATRIOT,
// MG M60E4. A weapon's PRIMARY menu category alone does not imply AR/MG.
constexpr bool tracer_weapon(uint16_t id){return id==22||id==24||id==25||id==26||id==30||id==31||id==35;}
// Last five rounds: evaluated against the HOST magazine before consumption.
constexpr bool tracer_shot(uint16_t id,uint16_t magazine){return tracer_weapon(id)&&magazine>0&&magazine<=5;}
constexpr bool automatic_aim_weapon(uint16_t id){return original_weapon::autoaim(id)||id==128||id==129;}
}
