#pragma once
#include "combat_initial_profile.h"
inline std::vector<mgo2mt::combat::Weapon> gekko_test_profiles(mgo2mt::combat::Weapon human){
 auto all=mgo2mt::combat::initial_profiles(20,1,0);std::vector<mgo2mt::combat::Weapon> result{human};
 for(const auto&w:all)if(mgo2mt::special_pc::weapon(w.id))result.push_back(w);return result;
}
