#include "weapon_projectiles.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace mgo2mt::projectile {
namespace {
bool finite(Vec3 v){for(float x:v)if(!std::isfinite(x)||std::abs(x)>1.e8f)return false;return true;}
Vec3 add(Vec3 a,Vec3 b){for(unsigned i=0;i<3;++i)a[i]+=b[i];return a;}
Vec3 mul(Vec3 a,float x){for(auto&v:a)v*=x;return a;}
float dot(Vec3 a,Vec3 b){float v=0;for(unsigned i=0;i<3;++i)v+=a[i]*b[i];return v;}
float length(Vec3 a){return std::sqrt(dot(a,a));}
bool unit(Vec3 a){return finite(a)&&std::abs(length(a)-1)<.001f;}
bool scope_valid(Scope s){return s.epoch&&s.scene;}
}
bool valid(const Profile&p){return (p.weapon==2||p.weapon==50||throwable(p.weapon)||p.weapon==129||p.weapon==103)&&
 std::isfinite(p.speed)&&p.speed>0&&p.speed<=500000&&std::isfinite(p.gravity)&&p.gravity>=0&&p.gravity<=50000&&
 std::isfinite(p.range)&&p.range>0&&p.range<=1000000&&p.ttlMs&&p.ttlMs<=60000&&p.fuseMs<=p.ttlMs&&
 (p.explodeOnContact||p.fuseMs)&&std::isfinite(p.bounceRestitution)&&p.bounceRestitution>=0&&p.bounceRestitution<=1&&
 std::isfinite(p.trailSpacing)&&p.trailSpacing>=0&&(p.trailSpacing==0||p.trailSpacing>=10)&&std::isfinite(p.blastRadius)&&p.blastRadius>=0&&p.blastRadius<=100000;}
Pool::Pool(size_t c):capacity_(c){if(!c||c>4096)throw std::invalid_argument("projectile capacity");entries_.reserve(c);}
void Pool::reset(Scope s){scope_=s;entries_.clear();seen_.fill({});clock_=0;clockArmed_=false;}
void Pool::remove(Owner o){std::erase_if(entries_,[&](const auto&e){return e.shot.owner==o;});if(o.slot<24&&seen_[o.slot]&&seen_[o.slot]->owner==o)seen_[o.slot].reset();}
std::vector<Owner> Pool::owners()const{std::vector<Owner> result;for(const auto&s:seen_)if(s)result.push_back(s->owner);return result;}
std::vector<DebugFlight> Pool::debug_flights(size_t limit)const{std::vector<DebugFlight> out;limit=(std::min)(limit,size_t(16));out.reserve((std::min)(limit,entries_.size()));for(const auto&e:entries_){if(out.size()==limit)break;out.push_back({e.shot.owner,e.shot.acceptedShotId,e.shot.weapon,e.position,e.traceFrom,e.traceTo,e.at});}return out;}
Submit Pool::spawn(const Shot&s,const Profile&p,uint64_t now){
 if(!scope_valid(scope_)||s.scope!=scope_)return Submit::scope;
 if(!valid(s.owner))return Submit::identity;
 if(!valid(p)||p.weapon!=s.weapon)return Submit::profile;
 if(!s.acceptedShotId)return Submit::sequence;
 if(!finite(s.origin)||!unit(s.direction))return Submit::invalid;
 if((clockArmed_&&now<clock_)||now>UINT64_MAX-p.ttlMs)return Submit::clock;
 auto&old=seen_[s.owner.slot];if(old&&(old->owner!=s.owner||s.acceptedShotId<=old->shot))return Submit::sequence;
 if(!available())return Submit::capacity;
 entries_.push_back({s,p,s.origin,mul(s.direction,p.speed),now,now,0,0,s.origin,s.origin});old=Seen{s.owner,s.acceptedShotId};clock_=now;clockArmed_=true;
 return Submit::accepted;
}
Step Pool::advance(uint64_t now,const Trace&trace){
 if(!trace)return advance_typed(now,{});
 return advance_typed(now,[&](Vec3 origin,Vec3 direction,float maximum,Owner owner,uint16_t){return trace(origin,direction,maximum,owner);});
}
Step Pool::advance_typed(uint64_t now,const TypedTrace&trace){
 Step out;if(clockArmed_&&now<clock_){out.discarded=entries_.size();entries_.clear();return out;}clock_=now;clockArmed_=true;
 std::vector<Entry> keep;keep.reserve(entries_.size());
 for(auto e:entries_){bool done=false;
  if(!trace||now<e.at||now-e.at>1000){++out.discarded;continue;}
  const uint64_t expiry=e.born+e.policy.ttlMs,fuse=e.policy.fuseMs?e.born+e.policy.fuseMs:UINT64_MAX;
  const uint64_t until=(std::min)(now,(std::min)(expiry,fuse));
  while(e.at<until&&!done){const auto ms=(std::min)(uint64_t(20),until-e.at);const float dt=float(ms)*.001f;
   Vec3 displacement=mul(e.velocity,dt);displacement[1]-=.5f*e.policy.gravity*dt*dt;
   const float distance=length(displacement),remaining=e.policy.range-e.traveled;
   if(!finite(displacement)||!std::isfinite(distance)||remaining<=0){++out.discarded;done=true;break;}
   const float travel=(std::min)(distance,remaining);const Vec3 direction=distance>0?mul(displacement,1/distance):Vec3{0,0,1};
   e.traceFrom=e.position;e.traceTo=add(e.position,mul(direction,travel));
   std::optional<Contact> hit;try{if(travel>0)hit=trace(e.position,direction,travel,e.shot.owner,e.shot.weapon);}catch(...){++out.discarded;done=true;break;}
   if(hit&&(!std::isfinite(hit->distance)||hit->distance<0||hit->distance>travel||!unit(hit->normal)||(hit->target&&!valid(*hit->target)))){++out.discarded;done=true;break;}
   const float actual=hit?hit->distance:travel;const Vec3 next=add(e.position,mul(direction,actual));
   if(e.policy.trailSpacing>0&&actual>0){float mark=e.policy.trailSpacing-e.trailRemainder;
    for(;mark<=actual&&out.trails.size()<64;mark+=e.policy.trailSpacing)out.trails.push_back({e.shot.scope,e.shot.owner,e.shot.acceptedShotId,e.shot.weapon,add(e.position,mul(direction,mark))});
    e.trailRemainder=std::fmod(e.trailRemainder+actual,e.policy.trailSpacing);
   }
   e.position=next;e.traveled+=actual;e.velocity[1]-=e.policy.gravity*dt;e.at+=ms;
   if(hit){if(e.policy.explodeOnContact){out.impacts.push_back({e.shot.scope,e.shot.owner,e.shot.acceptedShotId,e.shot.weapon,e.position,hit->normal,hit->target,hit->object,false,e.policy.blastRadius});done=true;}
    else{auto normal=hit->normal;float approach=dot(e.velocity,normal);if(approach>0){normal=mul(normal,-1);approach=-approach;}
     e.velocity=mul(add(e.velocity,mul(normal,-2*approach)),e.policy.bounceRestitution);e.position=add(e.position,mul(normal,1));}
   }else if(travel<distance||e.traveled>=e.policy.range){++out.discarded;done=true;}
  }
  if(!done&&now>=fuse){out.impacts.push_back({e.shot.scope,e.shot.owner,e.shot.acceptedShotId,e.shot.weapon,e.position,{}, {},0,true,e.policy.blastRadius});done=true;}
  if(!done&&now>=expiry){++out.discarded;done=true;}
  if(!done)keep.push_back(e);
 }
 entries_.swap(keep);return out;
}
std::optional<Ammo> refill(Ammo a){if(!a.capacity||a.magazine>a.capacity||a.magazine==a.capacity||!a.reserve)return {};
 const auto amount=(std::min)(unsigned(a.capacity-a.magazine),unsigned(a.reserve));a.magazine+=uint16_t(amount);a.reserve-=uint16_t(amount);return a;}
std::optional<Ammo> consume(Ammo a){if(!a.capacity||a.magazine>a.capacity||!a.magazine)return {};--a.magazine;return a;}
}
