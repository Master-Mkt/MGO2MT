#pragma once
#include "motion_blend.h"
#include <array>
#include <cmath>
#include <stdexcept>
namespace mgo2win::motion_blend {
// Render identity only; scene, full actor identity, life and model generation.
using Scope=std::array<uint64_t,5>;
class Lane {
 MotionBlend blend_;Scope scope_{};uint64_t source_=0,ticket_=0;double clipTime_=0;bool initialized_=false;
public:
 void reset(){blend_.reset();scope_={};source_=ticket_=0;clipTime_=0;initialized_=false;}
 const MotionPose* pose()const{return blend_.pose();}
 float progress()const{return blend_.progress();}
 bool active()const{return blend_.active();}
 bool matches(Scope scope)const{return initialized_&&scope_==scope;}
 bool same_source(uint64_t source)const{return initialized_&&source_==source;}
 const MotionPose& sample(Scope scope,uint64_t source,double clipTime,const MotionPose& target,double dt,float rate){
  if(!source||!std::isfinite(clipTime)||clipTime<0){if(blend_.pose())return *blend_.pose();throw std::invalid_argument("Invalid motion presentation clock/source");}
  const bool newScope=!matches(scope);uint64_t next=newScope?1:ticket_;
  if(!newScope&&(source!=source_||clipTime+1e-6<clipTime_))++next;
  // Commit identity and restart clocks only after a valid pose is accepted.
  if(newScope||!next){MotionBlend fresh;try{fresh.update(1,target,dt,rate);}catch(const std::invalid_argument&){if(blend_.pose())return *blend_.pose();throw;}blend_=std::move(fresh);next=1;}
  else{blend_.update(next,target,dt,rate);if(!blend_.last_update_accepted())return *blend_.pose();}
  initialized_=true;scope_=scope;ticket_=next;source_=source;clipTime_=clipTime;
  return *blend_.pose();
 }
 // Change the coordinate frame of an already displayed pose, retaining the
 // picture in world space before a physics/animation handoff starts blending.
 void rebase(std::array<float,3> from,float fromYaw,std::array<float,3> to,float toYaw,std::array<float,3> bindRoot){
  if(!std::isfinite(fromYaw)||!std::isfinite(toYaw))return;
  for(unsigned k=0;k<3;++k)if(!std::isfinite(from[k])||!std::isfinite(to[k])||!std::isfinite(bindRoot[k]))return;
  if(!blend_.pose())return;auto p=*blend_.pose();const float angle=fromYaw-toYaw;
  auto rotate=[](std::array<float,3> v,float a){float s=std::sin(a),c=std::cos(a);return std::array<float,3>{c*v[0]+s*v[2],v[1],-s*v[0]+c*v[2]};};
  std::array<float,3> point{};for(unsigned k=0;k<3;++k)point[k]=bindRoot[k]+p.root[k];point=rotate(point,fromYaw);
  for(unsigned k=0;k<3;++k)point[k]+=from[k]-to[k];point=rotate(point,-toYaw);
  for(unsigned k=0;k<3;++k)p.root[k]=point[k]-bindRoot[k];
  auto& q=p.rotations.at(p.rootBone);float s=std::sin(angle*.5f),c=std::cos(angle*.5f);
  q={c*q[0]+s*q[2],c*q[1]+s*q[3],c*q[2]-s*q[0],c*q[3]-s*q[1]};
  MotionBlend fresh;try{fresh.update(ticket_?ticket_:1,p,0,5);}catch(const std::invalid_argument&){return;}blend_=std::move(fresh);
 }
};
inline MotionPose local_physics_pose(MotionPose pose,float actorYaw){
 auto& q=pose.rotations.at(pose.rootBone);float s=std::sin(-actorYaw*.5f),c=std::cos(-actorYaw*.5f);
 q={c*q[0]+s*q[2],c*q[1]+s*q[3],c*q[2]-s*q[0],c*q[3]-s*q[1]};return pose;
}
}
