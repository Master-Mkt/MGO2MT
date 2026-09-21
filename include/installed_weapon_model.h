#pragma once
#include "character_model.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace mgo2mt {
// Native placement adapter. Preserve original size/UV/materials; rotate local
// up onto the HOST collision normal and put the bottom at its surface anchor.
inline CharacterModel installed_weapon_model(const CharacterModel& original,std::array<float,3> up,float yaw){
 auto dot=[](auto a,auto b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
 if(!std::isfinite(yaw)||!std::isfinite(dot(up,up))||std::abs(dot(up,up)-1.f)>.001f)throw std::invalid_argument("Installed weapon orientation");
 const float length=std::sqrt(dot(up,up));for(auto&v:up)v/=length;
 std::array<float,3> forward{std::sin(yaw),0,std::cos(yaw)};
 auto project=[&]{const auto d=dot(forward,up);for(unsigned i=0;i<3;++i)forward[i]-=up[i]*d;};project();
 if(dot(forward,forward)<.0001f){forward={0,1,0};project();}const auto f=std::sqrt(dot(forward,forward));for(auto&v:forward)v/=f;
 const std::array<float,3> right{up[1]*forward[2]-up[2]*forward[1],up[2]*forward[0]-up[0]*forward[2],up[0]*forward[1]-up[1]*forward[0]};
 auto rotate=[&](float x,float y,float z){std::array<float,3> p;for(unsigned i=0;i<3;++i)p[i]=right[i]*x+up[i]*y+forward[i]*z;return p;};
 auto result=original;result.bounds={1e30f,1e30f,1e30f,-1e30f,-1e30f,-1e30f};
 for(auto&v:result.vertices){auto p=rotate(v.x,v.y-original.bounds[1],v.z),n=rotate(v.nx,v.ny,v.nz);v.x=p[0];v.y=p[1];v.z=p[2];v.nx=n[0];v.ny=n[1];v.nz=n[2];for(unsigned i=0;i<3;++i){result.bounds[i]=std::min(result.bounds[i],p[i]);result.bounds[i+3]=std::max(result.bounds[i+3],p[i]);}}
 result.hasOverviewBounds=false;return result;
}
}
