#pragma once
#include "combat_initial_profile.h"
inline std::vector<mgo2win::combat::Weapon> gekko_test_profiles(mgo2win::combat::Weapon human){
 auto all=mgo2win::combat::initial_profiles(20,1,0);std::vector<mgo2win::combat::Weapon> result{human};
 for(const auto&w:all)if(mgo2win::special_pc::weapon(w.id))result.push_back(w);return result;
}
