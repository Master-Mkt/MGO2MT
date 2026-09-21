#pragma once
#include <array>
#include <vector>
#include <istream>
#include <cstdint>
#include "stage_lighting.h"
namespace mgo2mt::stage {
struct Placement {uint32_t key=0,model=0,group=0;std::array<float,3> position{};float yaw=0;};
struct Round {
 uint64_t seed=0;std::vector<Placement> objects;TransientLights transientLights;
 static Round read(std::istream&);
 Round reset(uint64_t newSeed)const;
};
}
