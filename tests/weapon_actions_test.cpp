#include "combat_initial_profile.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;using namespace mgo2win::combat;
namespace {
void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);}
constexpr Identity a{1,10,101},b{2,11,202};
Pose pose(float z){Pose p;p.feet={0,2,z};return p;}
std::shared_ptr<const stage::Collision> world(){return std::make_shared<const stage::Collision>(stage::Collision::make(
 {{-200000,0,-200000},{200000,0,-200000},{200000,0,200000},{-200000,0,200000}},{{{0,1,2}},{{0,2,3}}}));}
void start(Authority&h,uint16_t first=2){h.begin(1,world(),initial_profiles(20,1,0));
 const uint16_t gear[]{first,uint16_t(first==2?50:2),uint16_t(first==53?50:53)};
 check(h.join(a,1,pose(0),10000,1000,gear,0)&&h.join(b,2,pose(10000),10000,1000,gear,0),"admitted loadouts");h.active(true);}
size_t count(const Decision&d,EventKind k){return std::count_if(d.events.begin(),d.events.end(),[&](const auto&e){return e.kind==k;});}
void mk2_and_reload(){Authority h;start(h);
 for(uint32_t i=0;i<10;++i){const uint64_t now=i*600;check(h.pose(a,1,i+1,pose(0),now)==Reject::none,"fresh aim");
  auto r=h.fire(a,{1,i+1,2,{0,0,1}},now);check(bool(r)&&count(r,EventKind::shot)==1,"MK2 accepted");
  check(r.events.front().normal==Vec3{0,0,1}&&r.events.front().cue==10087,"accepted direction/audio");
  if(i==0)check(h.snapshot().players[b.slot]->stamina==755,"original base ST245");
 }
 auto s=h.snapshot();check(s.players[b.slot]->hp==10000&&s.players[b.slot]->stunned,"nonlethal stun");
 check(s.players[a.slot]->ammo==0&&s.players[a.slot]->reserve==30,"ten shots finite magazine");
 check(h.fire(a,{1,11,2,{0,0,1}},5401).reject==Reject::interval,"no early dry shot effect");
 check(bool(h.reload(a,1,5401)),"empty magazine reload begins");h.advance(7100);check(h.snapshot().players[a.slot]->ammo==0,"not early refill");
 h.advance(7101);s=h.snapshot();check(s.players[a.slot]->ammo==10&&s.players[a.slot]->reserve==20&&s.players[a.slot]->reloadUntil==7601,"fill before completion");
 h.advance(7601);check(!h.snapshot().players[a.slot]->reloadUntil,"finish deadline");
}
void rocket_and_scope(){Authority h;start(h,50);auto r=h.fire(a,{1,1,50,{0,0,1}},0);
 check(bool(r)&&count(r,EventKind::projectile)==1&&count(r,EventKind::damage)==0,"rocket not hitscan");
 check(h.snapshot().players[a.slot]->ammo==0,"one round consumed");
 auto effects=h.advance_projectiles(100);check(count(effects,EventKind::projectileTrail)>0&&count(effects,EventKind::damage)==0,"smoke before impact");
 effects=h.advance_projectiles(400);check(count(effects,EventKind::impact)==1&&count(effects,EventKind::damage)>0,"physical contact then blast");
 check(h.snapshot().players[b.slot]->hp<10000,"blast changes target HP");auto health=h.snapshot().players[b.slot]->hp;
 check(h.advance_projectiles(500).events.empty()&&h.snapshot().players[b.slot]->hp==health,"no repeated blast");
 Authority left;start(left,50);check(bool(left.fire(a,{1,1,50,{0,0,1}},0)),"second rocket");check(left.leave(a),"owner leaves");
 check(left.advance_projectiles(400).events.empty()&&left.snapshot().players[b.slot]->hp==10000,"old owner rocket cancelled");
 Authority changed;start(changed,50);check(bool(changed.fire(a,{1,1,50,{0,0,1}},0)),"scene rocket");changed.world(world());
 check(changed.advance_projectiles(400).events.empty(),"scene replacement cancels old flight");
 Authority stopped;start(stopped,50);stopped.fire(a,{1,1,50,{0,0,1}},0);stopped.active(false);
 check(stopped.advance_projectiles(400).events.empty(),"round stop cancels flight");
 Authority wp;start(wp,53);auto launch=wp.fire(a,{1,1,53,{0,0,1}},0);
 check(bool(launch)&&count(launch,EventKind::projectile)==1&&count(launch,EventKind::damage)==0,"WP launch no fake direct damage");
 size_t impacts=0;for(uint64_t now=100;now<=3000;now+=100)impacts+=count(wp.advance_projectiles(now),EventKind::impact);
 check(impacts==1,"WP fuse detonates once after bounces");
}
}
int main(){try{mk2_and_reload();rocket_and_scope();std::cout<<"PASS actual HOST MK2 stun/empty reload and RPG travel/smoke/blast/lifecycle\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
