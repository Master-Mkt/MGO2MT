#pragma once
#include "combat_falling.h"
#include "gekko_regeneration.h"
#include <filesystem>
#include <istream>
namespace mgo2win::combat {
struct HealthRules {
 falling::Policy falling;
 special_pc::regeneration::Policy regeneration;
 bool valid()const noexcept{return falling::valid(falling)&&special_pc::regeneration::valid(regeneration);}
 static HealthRules read(std::istream&);
 static HealthRules load(const std::filesystem::path&);
};
inline constexpr uint16_t fall_event_weapon=65533; // Native environment event, never a selectable weapon.
}
