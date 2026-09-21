#pragma once
#include "host_hit_geometry.h"
#include <cmath>
namespace mgo2mt::combat::cover {
// Conservative native exposure proxy: retain the normal body and add displaced
// upper-body BOX regions. Feet/legs never teleport, and leaning grants no safe
// hole where the normal torso stood. Original animated region policy is unknown.
inline std::optional<host_hit::Hit> upper_hit(const host_hit::Vec3&o,const host_hit::Vec3&d,float maximum,const host_hit::Vec3&feet,float yaw,host_hit::Stance stance,const host_hit::Vec3&offset){
 auto best=host_hit::query(o,d,maximum,feet,yaw,0,stance);auto bones=host_hit::pose(0,stance);
 const float c=std::cos(yaw),s=std::sin(yaw);auto rotate=[&](host_hit::Vec3 v){return host_hit::Vec3{v[0]*c+v[2]*s,v[1],-v[0]*s+v[2]*c};};
 for(size_t i=0;i<original_hit_regions::boxes.size();++i){const auto& box=original_hit_regions::boxes[i];if(!box.bone||box.bone>=13||box.bone>=bones.size())continue;
  auto bone=bones[box.bone];for(auto&axis:bone.axes)axis=rotate(axis);bone.origin=rotate(bone.origin);for(unsigned j=0;j<3;++j)bone.origin[j]+=feet[j]+offset[j];
  auto hit=host_hit::intersect(o,d,best?best->distance:maximum,bone,box.offset,box.halfExtent);
  if(hit&&(!best||*hit<best->distance)){host_hit::Vec3 point;for(unsigned j=0;j<3;++j)point[j]=o[j]+d[j]* *hit;best=host_hit::Hit{*hit,box.bone,uint8_t(i),box.nameHash,point};}
 }return best;
}
}
