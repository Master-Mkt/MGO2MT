#include "combat_tracer.h"
#include "weapon_visual_policy.h"
#include <algorithm>
#include <cmath>
namespace mgo2mt::combat::tracers {
namespace {
bool finite(Vec3 p){for(float x:p)if(!std::isfinite(x)||std::abs(x)>1000000)return false;return true;}
bool owner(const Snapshot&s,Identity id,uint32_t life){if(id.slot>=24||!life)return false;const auto&p=s.players[id.slot];return p&&p->identity==id&&p->life==life;}
Vec3 at(const Event&e,float d){auto p=e.position;for(int i=0;i<3;++i)p[i]+=e.normal[i]*d;return p;}
}
void Pool::clear(){entries_.clear();seen_.clear();epoch_=scene_=floor_=now_=0;self_={};life_=0;}
void Pool::synchronize(uint64_t epoch,uint64_t scene,Identity self,uint32_t life,uint64_t watermark,uint64_t now){
 if(!epoch||!scene||!life||self.slot>=24){clear();return;}
 if(epoch!=epoch_||scene!=scene_||self!=self_||life!=life_||now<now_){clear();epoch_=epoch;scene_=scene;self_=self;life_=life;floor_=watermark;}
 now_=now;
}
void Pool::expire(const Snapshot&s,uint64_t now){
 if(s.epoch!=epoch_||!owner(s,self_,life_)){clear();return;}
 if(now<now_){entries_.clear();floor_=s.eventWatermark;seen_.clear();}now_=now;
 std::erase_if(entries_,[&](const Entry&e){return now-e.born>=lifetimeMs||!owner(s,e.shot.source,e.shot.sourceLife);});
}
void Pool::dispatch(std::span<const Event>events,const Snapshot&s,uint64_t now){
 expire(s,now);if(!epoch_)return;
 for(const auto&e:events){if(e.epoch!=epoch_||!e.id||e.id<=floor_||e.id>s.eventWatermark||!seen_.insert(e.id).second)continue;
  if(seen_.size()>4096){floor_=*seen_.begin();seen_.erase(seen_.begin());}
  if(e.kind!=EventKind::shot||!tracer_weapon(e.weapon)||!owner(s,e.source,e.sourceLife)||!finite(e.position)||!finite(e.normal)||!std::isfinite(e.shotDistance)||e.shotDistance<=0||e.shotDistance>1000000)continue;
  float n=0;for(float x:e.normal)n+=x*x;if(std::abs(n-1)>1e-3f||!finite(at(e,e.shotDistance)))continue;
  if(entries_.size()==capacity)entries_.erase(entries_.begin());entries_.push_back({e,now});
 }
}
std::vector<Segment> Pool::sample(const Snapshot&s,uint64_t now){
 expire(s,now);std::vector<Segment> result;result.reserve(entries_.size());
 for(const auto&e:entries_){float t=float(now-e.born)/float(lifetimeMs);float length=std::min(1200.f,e.shot.shotDistance);float head=length+(e.shot.shotDistance-length)*t;
  result.push_back({at(e.shot,std::max(0.f,head-length)),at(e.shot,head),1-t});}
 return result;
}
}
