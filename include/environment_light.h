#pragma once
#include <array>
#include <cmath>
#include <stdexcept>
namespace mgo2mt {
// Resolved original LT3 volume colors; angular response remains native diffuse.
struct EnvironmentLight {
 std::array<float,3> front{},back{},axis{0,1,0},direct{},direction{0,-1,0},scale{1,1,1};
 float weight=0;unsigned volumes=0;
 void validate()const{for(auto group:{front,back,axis,direct,direction,scale})for(float v:group)if(!std::isfinite(v)||std::abs(v)>1000000)throw std::invalid_argument("Environment light");}
};
}
