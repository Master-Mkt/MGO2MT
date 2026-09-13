#pragma once
#include "footstep_timeline.h"
#include "stage_collision.h"
#include "stage_water.h"
#include "material_audio.h"
#include <cmath>
namespace mgo2win::combat::footsteps {
// Native contact adapter, not original CBD438's capsule/extended floor query.
// Original CCFE40 consumes cached control materials, not a ray from the foot bone.
inline std::optional<stage::CollisionMaterial> dry_floor(const stage::Collision& world,
    stage::Vec3 feet,const stage::Water* water=nullptr){
 for(float v:feet)if(!std::isfinite(v)||std::abs(v)>=1e7f)return {};
 auto from=feet;from[1]+=100;
 auto hit=world.ray(from,{0,-1,0},200);
 if(!hit||hit->normal[1]<.7f||hit->position[1]>feet[1]+4)return {};
 const auto& triangle=world.triangles[hit->triangle];
 if(!(triangle.attribute&0x10)||triangle.attribute&0x8000)return {};
 if(water){auto level=water->control_level(feet,hit->position[1]);if(level&&*level>hit->position[1])return {};}
 auto material=world.material(hit->triangle);
 return material.verified&&material.id?std::optional(material):std::nullopt;
}
// Exactly the renderer's model-local -> Y rotation -> actor translation.
inline std::optional<stage::Vec3> world_bone(stage::Vec3 bone,stage::Vec3 origin,float yaw){
 for(float v:bone)if(!std::isfinite(v)||std::abs(v)>=1e7f)return {};
 for(float v:origin)if(!std::isfinite(v)||std::abs(v)>=1e7f)return {};
 if(!std::isfinite(yaw))return {};
 const float c=std::cos(yaw),s=std::sin(yaw);
 return stage::Vec3{origin[0]+bone[0]*c+bone[2]*s,origin[1]+bone[1],origin[2]-bone[0]*s+bone[2]*c};
}
}
