#include "water_surface_effects.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace mgo2win::stage {
namespace {
float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vec3 sub(Vec3 a,Vec3 b){for(unsigned k=0;k<3;++k)a[k]-=b[k];return a;}
Vec3 at(Vec3 p,Vec3 v,float t){for(unsigned k=0;k<3;++k)p[k]+=v[k]*t;p[1]-=900.f*t*t;return p;}
bool finite(Vec3 p){return std::all_of(p.begin(),p.end(),[](float v){return std::isfinite(v)&&std::abs(v)<1e6f;});}
}
void WaterSurfaceEffects::reset(){scope_={};lastNow_=0;initialized_=false;tracks_.clear();drops_.clear();}
void WaterSurfaceEffects::update(WaterSurfaceScope scope,std::span<const WaterSurfaceActor> actors,const WaterSurface* surface,uint64_t now,bool active){
 if(!active||!scope.epoch||!scope.scene||!surface||actors.size()>maximumActors){reset();return;}
 std::set<uint64_t> present;
 for(const auto& a:actors)if(!a.identity||!a.life||!finite(a.point)||!present.insert(a.identity).second){reset();return;}
 if(!initialized_||scope_!=scope||now<lastNow_||now-lastNow_>250){
  reset();scope_=scope;lastNow_=now;initialized_=true;
  for(const auto& a:actors)tracks_.emplace(a.identity,Track{a.life,0,a.point,false});return;
 }
 // Membership and life changes are authoritative even within the same clock tick.
 std::erase_if(drops_,[&](const Drop& d){return std::none_of(actors.begin(),actors.end(),[&](const auto& a){return a.identity==d.actor&&a.life==d.life;});});
 std::erase_if(tracks_,[&](const auto& entry){return !present.contains(entry.first);});
 if(now==lastNow_){for(const auto& a:actors){auto found=tracks_.find(a.identity);if(found==tracks_.end()||found->second.life!=a.life)tracks_[a.identity]={a.life,0,a.point,false};}return;}
 lastNow_=now;
 std::erase_if(tracks_,[&](const auto& entry){return !present.contains(entry.first);});
 std::erase_if(drops_,[&](const Drop& d){return now-d.born>=lifetimeMs||!present.contains(d.actor);});
 for(const auto& a:actors){
  auto old=tracks_.find(a.identity);
  if(old==tracks_.end()){tracks_.emplace(a.identity,Track{a.life,0,a.point,false});continue;}
  auto& track=old->second;auto delta=sub(a.point,track.point);
  if(track.life!=a.life||dot(delta,delta)>2000.f*2000.f){
   std::erase_if(drops_,[&](const Drop& d){return d.actor==a.identity;});track={a.life,0,a.point,false};continue;
  }
  if(!track.emitted||now-track.lastEmission>=minimumIntervalMs)if(auto hit=surface->crossing(track.point,a.point)){
   auto normal=hit->normal;const float sign=dot(normal,delta)>=0?1.f:-1.f;for(auto& v:normal)v*=sign;
   Vec3 tangent=std::abs(normal[1])<.9f?Vec3{normal[2],0,-normal[0]}:Vec3{1,0,0};
   const float length=std::sqrt(dot(tangent,tangent));for(auto& v:tangent)v/=length;
   Vec3 side{normal[1]*tangent[2]-normal[2]*tangent[1],normal[2]*tangent[0]-normal[0]*tangent[2],normal[0]*tangent[1]-normal[1]*tangent[0]};
   while(drops_.size()+dropsPerCrossing>maximumDrops)drops_.erase(drops_.begin());
   for(unsigned i=0;i<dropsPerCrossing;++i){float angle=float(i)*.523598776f;Vec3 origin=hit->position,velocity{};
    for(unsigned k=0;k<3;++k){origin[k]+=normal[k]*3;velocity[k]=normal[k]*300+(tangent[k]*std::cos(angle)+side[k]*std::sin(angle))*220;}
    velocity[1]+=350;drops_.push_back({a.identity,a.life,now,origin,velocity});
   }
   track.lastEmission=now;track.emitted=true;
  }
  track.point=a.point;
 }
}
std::vector<WaterEffectLine> WaterSurfaceEffects::lines(uint64_t now)const{
 std::vector<WaterEffectLine> result;result.reserve(drops_.size());
 if(!initialized_||now<lastNow_)return result;
 for(const auto& d:drops_)if(now>=d.born&&now-d.born<lifetimeMs){float age=float(now-d.born)*.001f;
  result.push_back({at(d.point,d.velocity,age),at(d.point,d.velocity,std::max(0.f,age-.025f)),1-age/(float(lifetimeMs)*.001f),true});
 }
 return result;
}
}
