#pragma once
#include "stage_collision.h"
#include "stage_water.h"
#include <algorithm>
#include <cmath>
namespace mgo2win::water_gameplay {
// User-requested native extension. In particular .65 is NOT original JJ/MGO2
// movement behavior. Oxygen timing/damage is a separate HOST-owned policy.
struct Policy {float horizontalScale=.65f;bool blockProneTranslation=true;};
inline bool valid_policy(const Policy& p){return std::isfinite(p.horizontalScale)&&p.horizontalScale>0&&p.horizontalScale<=1;}
inline bool valid_body(stage::Capsule c){return (c.radius==260||c.radius==350)&&c.skin==2&&(c.height==1700||c.height==1100||c.height==560)&&c.height>=2*c.radius;}
struct Contact {
 // First four fields preserve the original NavigationWaterState aggregate.
 std::optional<float> level;float depthAboveFloor=0,horizontalScale=1;stage::WaterFoot foot=stage::WaterFoot::dry;
 float floorY=0,faceY=0;bool faceSubmerged=false,proneBlocked=false;
};
inline bool finite(stage::Vec3 p){return std::all_of(p.begin(),p.end(),[](float x){return std::isfinite(x)&&std::abs(x)<1000000;});}
// Native proxy matching Navigation::eye, not an animated face-bone query.
// It describes standing/crouching/prone by the accepted capsule height. Prone
// and supine intentionally share a proxy and horizontal movement restriction.
inline Contact evaluate(const stage::Water& water,stage::Vec3 feet,float floorY,stage::Capsule body,Policy policy={}){
 Contact result;if(!valid_policy(policy)||!valid_body(body)||!finite(feet)||!std::isfinite(floorY)||std::abs(floorY)>=1000000||floorY>feet[1]+2*body.skin)return result;
 const auto level=water.control_level(feet,floorY);if(!level)return result;
 result.level=level;result.floorY=floorY;result.faceY=feet[1]+(body.height-150);
 result.depthAboveFloor=(std::max)(0.f,*level-floorY);result.foot=stage::Water::classify(*level,floorY,feet[1]);
 if(result.foot==stage::WaterFoot::inWater){
  const auto bodyLevel=water.level(feet);if(!bodyLevel||*bodyLevel!=*level){result.foot=stage::WaterFoot::dry;return result;}
  result.proneBlocked=policy.blockProneTranslation&&body.height==560;
  result.horizontalScale=result.proneBlocked?0:policy.horizontalScale;
  // Floor, body and face must belong to an unambiguous authored FIELD. A
  // finite AA water surface is not a volume and never enters this function.
  const auto faceLevel=water.level({feet[0],result.faceY,feet[2]});
  result.faceSubmerged=faceLevel&&*faceLevel==*level&&result.faceY<=*level;
 }
 return result;
}
inline Contact sample(const stage::Water* water,const stage::Collision& world,stage::Vec3 feet,stage::Capsule body,Policy policy={}){
 if(!water||!valid_body(body)||!valid_policy(policy)||!finite(feet))return {};
 auto origin=feet;origin[1]+=body.skin*2;
 const auto floor=world.ray(origin,{0,-1,0},body.height+body.skin*4);
 if(!floor||floor->normal[1]<.70710678f)return {};
 return evaluate(*water,feet,floor->position[1],body,policy);
}
}
