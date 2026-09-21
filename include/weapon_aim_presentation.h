#pragma once
#include "weapon_reticle.h"
#include "enemy_tag_target.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace mgo2mt::reticle {
// Native camera-only kick, separate from the HOST's actual bullet dispersion.
// Does not change the player's input pose or grant a client accuracy advantage.
class Recoil {
 std::optional<Scope> scope_;uint64_t watermark_=0,eventCursor_=0;float pitch_=0,yaw_=0;
public:
 void reset(){scope_.reset();watermark_=eventCursor_=0;pitch_=yaw_=0;}
 void update(Scope scope,bool eligible,uint64_t watermark,std::span<const combat::Event> events,double dt){
  if(!eligible||!scope.epoch||!scope.scene||!scope.life||(!original_weapon::find(scope.weapon)&&scope.weapon!=50&&scope.weapon!=128&&scope.weapon!=129)||
     scope.identity.slot>=24||!scope.identity.instance||!scope.identity.character||!std::isfinite(dt)||dt<0){reset();return;}
  if(scope_!=scope){reset();scope_=scope;watermark_=eventCursor_=watermark;return;}
  if(watermark<watermark_)return;
  const auto decay=float(std::exp(-16.0*(std::min)(dt,1.0)));pitch_*=decay;yaw_*=decay;
  // A snapshot watermark covers later event chunks too. Consume actual events
  // separately so delayed chunks and arrivals after the snapshot read survive.
  std::set<uint64_t> shots;auto consumed=eventCursor_;
  for(const auto&e:events)if(e.epoch==scope.epoch&&e.id>eventCursor_){
   consumed=(std::max)(consumed,e.id);
   if(e.kind==combat::EventKind::shot&&e.source==scope.identity&&e.sourceLife==scope.life&&e.weapon==scope.weapon)shots.insert(e.id);
  }
  for(auto id:shots){
   pitch_=(std::min)(.018f,pitch_+.0045f);
   yaw_=std::clamp(yaw_+((id&1)?-.0015f:.0015f),-.006f,.006f);
  }
  eventCursor_=consumed;watermark_=watermark;
 }
 combat::Vec3 direction(combat::Vec3 base)const{
  auto u=enemy_tag::unit(base);if(!u)return base;
  float pitch=std::clamp(std::asin(std::clamp((*u)[1],-1.f,1.f))+pitch_,-1.45f,1.45f);
  float yaw=std::atan2((*u)[0],(*u)[2])+yaw_,c=std::cos(pitch);
  return {std::sin(yaw)*c,std::sin(pitch),std::cos(yaw)*c};
 }
 float pitch()const{return pitch_;}float yaw()const{return yaw_;}
};
// Project the unspread bullet ray's first visible contact, not the third-person
// camera's center. All living bodies can obscure it; this is a presentation
// capsule proxy, not an authoritative damage/hit-region prediction.
inline std::optional<combat::Vec3> aim_point(combat::Vec3 origin,combat::Vec3 direction,
 const stage::Collision& world,const stage::Collision* objects,const combat::Snapshot& snapshot,combat::Identity self){
 if(!enemy_tag::finite(origin))return {};auto ray=enemy_tag::unit(direction);if(!ray)return {};
 float distance=200000;
 auto query=stage::query::bullet;
 if(self.slot<snapshot.players.size())if(const auto& p=snapshot.players[self.slot];p&&p->identity==self)
  if(auto category=projectile::collision_query(p->weapon))query=*category;
 for(const auto* collision:{&world,objects})if(collision)if(auto hit=collision->ray(origin,*ray,distance,query))distance=hit->distance;
 for(const auto&p:snapshot.players)if(p&&p->alive&&p->identity!=self)
  if(auto hit=enemy_tag::capsule(origin,*ray,p->pose);hit&&*hit<distance)distance=*hit;
 return enemy_tag::add(origin,enemy_tag::mul(*ray,distance));
}
// A finite target plane is closer to the firing eye than a shoulder camera.
// Correct that depth ratio before the FOV projection; this is a local cone
// footprint approximation and does not claim per-pixel original reticle math.
inline std::optional<float> camera_angle(float angle,combat::Vec3 origin,combat::Vec3 target,combat::Vec3 eye,combat::Vec3 direction){
 if(!std::isfinite(angle)||angle<0||angle>.1f||!enemy_tag::finite(origin)||!enemy_tag::finite(target)||!enemy_tag::finite(eye))return {};
 auto ray=enemy_tag::unit(direction);if(!ray)return {};
 auto delta=enemy_tag::sub(target,eye);float depth=enemy_tag::dot(delta,*ray);
 auto shot=enemy_tag::sub(target,origin);float distance=std::sqrt(enemy_tag::dot(shot,shot));
 if(!std::isfinite(depth)||depth<10||depth>=500000||!std::isfinite(distance))return {};
 return std::atan(std::tan(angle)*distance/depth);
}
}
