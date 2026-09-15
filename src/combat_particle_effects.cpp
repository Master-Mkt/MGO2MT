#include "combat_particle_effects.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace mgo2win::combat::particles {
namespace {
bool finite(Vec3 p){for(float v:p)if(!std::isfinite(v)||std::abs(v)>=1000000)return false;return true;}
Vec3 plus(Vec3 a,Vec3 b){for(int i=0;i<3;++i)a[i]+=b[i];return a;}
Vec3 scaled(Vec3 a,float n){for(auto&v:a)v*=n;return a;}
std::optional<Vec3> unit(Vec3 p){if(!finite(p))return {};float d=std::hypot(p[0],p[1],p[2]);if(d<1e-6f)return {};return scaled(p,1/d);}
bool owner(const Snapshot&s,Identity id,uint32_t life){if(id.slot>=s.players.size()||!life)return false;const auto&p=s.players[id.slot];return p&&p->identity==id&&p->life==life&&p->alive;}
}
Pool::Pool(Policy p):policy_(p){if(!p.capacity||p.capacity>4096||!p.smokeMs||p.smokeMs>10000||!p.casingMs||p.casingMs>10000)throw std::invalid_argument("Native particle policy");}
void Pool::clear(){entries_.clear();seen_.clear();epoch_=scene_=floor_=now_=0;}
void Pool::synchronize(uint64_t epoch,uint64_t scene,uint64_t watermark,uint64_t now){if(!epoch||!scene){clear();return;}if(epoch_!=epoch||scene_!=scene){clear();epoch_=epoch;scene_=scene;floor_=watermark;}if(now<now_)entries_.clear();now_=now;}
void Pool::expire(const Snapshot&s,uint64_t now){if(s.epoch!=epoch_){entries_.clear();return;}if(now<now_)entries_.clear();now_=now;std::erase_if(entries_,[&](const Entry&e){return !owner(s,e.owner,e.life)||now-e.born>=(e.kind==Kind::flash?80:e.kind==Kind::smoke?policy_.smokeMs:policy_.casingMs);});}
void Pool::dispatch(std::span<const Event> events,const Snapshot&s,uint64_t now){expire(s,now);if(!epoch_||s.epoch!=epoch_)return;for(const auto&e:events){
 if(e.epoch!=epoch_||!e.id||e.id<=floor_||e.id>s.eventWatermark||!seen_.insert(e.id).second)continue;
 if(seen_.size()>4096){floor_=*seen_.begin();seen_.erase(seen_.begin());}
 if(!finite(e.position)||!owner(s,e.source,e.sourceLife))continue;
 Entry p{Kind::smoke,e.source,e.sourceLife,e.position,{},now,e.id};
 if(e.kind==EventKind::projectileTrail&&(e.weapon==50||e.weapon==129)){p.velocity={float(int(e.id%11)-5)*12,110,0};}
 else if(e.kind==EventKind::shot&&(e.weapon==128||e.weapon==129)){auto forward=unit(e.normal);if(!forward)continue;p.kind=Kind::flash;p.origin=plus(e.position,scaled(*forward,850));}
 else if(e.kind==EventKind::shot&&(e.weapon==25||e.weapon==3||e.weapon==2)){
  auto forward=unit(e.normal);if(!forward)continue;auto right=unit(Vec3{(*forward)[2],0,-(*forward)[0]});if(!right)right=Vec3{1,0,0};
  if(e.weapon==25){
   // Caller resolves original CNP_mzf_def after hand skinning. Native flash
   // geometry and a short smoke puff; accepted events only, never trigger input.
   auto emit=[&](Entry q){if(entries_.size()==policy_.capacity)entries_.erase(entries_.begin());entries_.push_back(q);};
   emit({Kind::flash,e.source,e.sourceLife,e.position,scaled(*forward,1),now,e.id});
   emit({Kind::smoke,e.source,e.sourceLife,e.position,{0,100,0},now,e.id});
  }
  p.kind=Kind::casing;p.origin=plus(e.position,plus(scaled(*right,100),Vec3{0,-100,0}));p.velocity=plus(scaled(*right,1000),Vec3{0,650,0});
 }else continue;
 if(entries_.size()==policy_.capacity)entries_.erase(entries_.begin());entries_.push_back(p);
}}
std::vector<Segment> Pool::sample(const Snapshot&s,uint64_t now){expire(s,now);std::vector<Segment> out;out.reserve(entries_.size());for(const auto&e:entries_){float t=float(now-e.born)/1000.f;float alpha=1-float(now-e.born)/float(e.kind==Kind::flash?80:e.kind==Kind::smoke?policy_.smokeMs:policy_.casingMs);auto p=plus(e.origin,scaled(e.velocity,t));Segment line;line.kind=e.kind;
 if(e.kind==Kind::flash){float radius=std::hypot(e.velocity[0],e.velocity[1],e.velocity[2])>0?65.f:230.f;for(auto axis:std::array<Vec3,3>{{{radius,0,0},{0,radius,0},{0,0,radius*1.4f}}}){line.from=plus(p,scaled(axis,-1));line.to=plus(p,axis);line.rgba={1.f,.82f,.3f,alpha};line.widthPixels=5;out.push_back(line);}continue;}
 if(e.kind==Kind::casing){p[1]-=4900*t*t;float angle=t*24+float(e.id%17);Vec3 axis{std::cos(angle)*18,std::sin(angle)*18,8};line.from=plus(p,scaled(axis,-1));line.to=plus(p,axis);line.rgba={.92f,.70f,.24f,alpha};line.widthPixels=2;}
 else{float width=40+100*t;line.from=plus(p,Vec3{-width,0,0});line.to=plus(p,Vec3{width,40+60*t,0});line.rgba={.65f,.65f,.65f,alpha*.65f};line.widthPixels=3+4*t;}
 if(finite(line.from)&&finite(line.to))out.push_back(line);
 }return out;}
}
