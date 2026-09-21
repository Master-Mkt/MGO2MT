#pragma once
#include "stage_collision.h"
#include "original_bullet_penetration.h"
#include <algorithm>
namespace mgo2mt::combat {
struct BallisticPath {float distance=0;bool blocked=false;int priorForceCost=0;std::vector<stage::CollisionRayHit> impacts;};
// A host-owned, instantaneous ray adapter. Surface resistance and target force
// math are recovered; projectile travel time, ricochet and body-through are not.
inline BallisticPath trace_ak102(stage::Vec3 origin,stage::Vec3 direction,float maximum,
                                const stage::Collision& world,const stage::Collision* objects=nullptr,int originalBudget=original_bullet_penetration::ak102_budget){
 BallisticPath out;out.distance=maximum;
 auto hits=world.ray_all(origin,direction,maximum,stage::query::bullet);if(objects){auto extra=objects->ray_all(origin,direction,maximum,stage::query::bullet);hits.insert(hits.end(),extra.begin(),extra.end());}
 std::stable_sort(hits.begin(),hits.end(),[](const auto&a,const auto&b){return a.distance<b.distance;});
 int budget=originalBudget;std::optional<stage::CollisionRayHit> previous;unsigned surfaces=0;
 for(const auto& hit:hits){
  // GEOM polygons triangulate to several shared-edge hits. Count one identical
  // oriented plane/material/object at a distance, preserving distinct layers.
  if(previous&&hit.distance==previous->distance&&hit.object==previous->object&&hit.attribute==previous->attribute&&hit.polygonAttribute==previous->polygonAttribute&&hit.material.id==previous->material.id&&hit.material.resistance==previous->material.resistance&&hit.material.resistanceVerified==previous->material.resistanceVerified){float normalDot=0;for(unsigned i=0;i<3;++i)normalDot+=hit.normal[i]*previous->normal[i];if(normalDot>.99999f)continue;}
  previous=hit;
  if(++surfaces>64){out.distance=hit.distance;out.blocked=true;break;} // Native bounded work; never shoot past truncation.
  float dot=0;for(unsigned i=0;i<3;++i)dot+=direction[i]*hit.normal[i];
  auto cost=original_bullet_penetration::surface(hit.attribute,hit.material.resistanceVerified?std::optional<int>(hit.material.resistance):std::nullopt,dot);
  // Skip free query surfaces, but retain both sides of ordinary geometry for
  // entry/exit visual impacts. Damage consumes front-face costs only.
  if(!(hit.attribute&original_bullet_penetration::free_surface_attribute))out.impacts.push_back(hit);
  auto cross=cost?original_bullet_penetration::cross(budget,*cost):std::nullopt;
  if(!cross||!cross->passes){out.distance=hit.distance;out.blocked=true;break;}
  budget=cross->remaining;out.priorForceCost+=cost->forceCost;
 }
 return out;
}
}
