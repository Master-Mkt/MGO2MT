#pragma once
#include "stage_collision.h"
#include <algorithm>
#include <cmath>
namespace mgo2mt::projectile {
// Native point-sweep contact on the existing upright gameplay capsule. This
// supplies its geometric outward normal to the same bounce solver as walls;
// it does not introduce authored Gekko hit zones or a projectile radius.
inline stage::Vec3 capsule_surface_normal(stage::Vec3 contact,stage::Vec3 feet,
                                         stage::Capsule capsule,stage::Vec3 incoming){
 stage::Vec3 center{feet[0],std::clamp(contact[1],feet[1]+capsule.radius,
                                    feet[1]+capsule.height-capsule.radius),feet[2]};
 stage::Vec3 normal{contact[0]-center[0],contact[1]-center[1],contact[2]-center[2]};
 const float squared=normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2];
 if(squared>1e-8f){const float scale=1/std::sqrt(squared);for(auto&v:normal)v*=scale;return normal;}
 // A ray beginning exactly on the capsule axis has no unique outward normal.
 // Retain the prior conservative response only for that overlap degeneracy.
 return {-incoming[0],-incoming[1],-incoming[2]};
}
}
