#pragma once
#include "combat_object_damage.h"
#include <istream>
namespace mgo2mt::combat {
// Native HOST durability. The original callback threshold remains separate evidence.
ObjectDamage::Policy read_breakable_lights(std::istream&);
ObjectDamage::Policy load_breakable_lights(const std::filesystem::path&);
}
