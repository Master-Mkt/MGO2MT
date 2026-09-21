#pragma once
#include "character_model.h"
#include <array>
#include <istream>
#include <string>
namespace mgo2mt::stage {
struct SkyPose {
 std::array<float,3> position{},degrees{};
 float cloudU=0;
};
// Reviewed GCX NewSky settings, independently bound to the original MDN SHA.
// Runtime scene seconds are a native clock adapter for the original ms counter.
struct SkySettings {
 uint32_t modelHash=0,procedure=0;
 std::string mdnSha256;
 std::array<float,3> position{},color{},fogColor{};
 std::array<int16_t,3> rotationUnits{};
 float periodSeconds=0,fog=0;
 int direction=1;
 std::vector<std::array<float,2>> uv2;
 static SkySettings read(std::istream&);
 void validate(const CharacterModel&)const;
 void prepare(CharacterModel&)const;
 SkyPose sample(double sceneSeconds)const;
};
}
