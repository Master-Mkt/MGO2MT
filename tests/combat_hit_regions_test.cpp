#include "combat_authority.h"
#include "combat_initial_profile.h"
#include "host_hit_geometry.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
using namespace combat;
namespace {
void check(bool value,const char* what){if(!value)throw std::runtime_error(what);}
constexpr Identity shooter{0,11,111},target{1,22,222};
std::shared_ptr<const stage::Collision> floor_wall(int resistance=-1){
 std::vector<Vec3> vertices{{-50000,0,-50000},{50000,0,-50000},{50000,0,50000},{-50000,0,50000}};
 std::vector<stage::CollisionTriangle> triangles{{{0,1,2}},{{0,2,3}}};
 if(resistance>=0){vertices.insert(vertices.end(),{{-10000,0,1500},{10000,0,1500},{10000,5000,1500},{-10000,5000,1500}});triangles.push_back({{4,6,5},4,0,0,0});triangles.push_back({{4,7,6},4,0,0,0});}
 return std::make_shared<const stage::Collision>(stage::Collision::make(vertices,triangles,{{1,.5f,.5f,true,resistance<0?100:resistance,true}}));
}
Vec3 center(unsigned region,host_hit::Stance stance,Vec3 feet){
 const auto&box=original_hit_regions::boxes.at(region);auto bone=host_hit::pose(0,stance)[box.bone];auto p=bone.origin;
 for(unsigned i=0;i<3;++i){p[i]+=feet[i];for(unsigned j=0;j<3;++j)p[i]+=bone.axes[j][i]*box.offset[j];}return p;
}
struct Run {uint32_t damage;uint8_t bone;Decision result;};
Run fire(unsigned region,int resistance=-1,bool enabled=true,bool friendly=false,bool ffa=false,host_hit::Stance stance=host_hit::Stance::standing){
 Authority a({friendly,6000,15000,500,ffa});auto weapons=initial_profiles(20,1,0);weapons[0].nativeAkHitRegions=enabled;a.begin(1,floor_wall(resistance),weapons);
 Pose victim;victim.feet={0,2,5000};victim.capsule.height=stance==host_hit::Stance::prone?560.f:stance==host_hit::Stance::crouching?1100.f:1700.f;
 Pose source;source.feet={0,2,0};auto aim=center(region,stance,victim.feet);Vec3 delta{aim[0],aim[1]-1552,aim[2]};float length=std::sqrt(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]);for(auto&v:delta)v/=length;
 source.yaw=std::atan2(delta[0],delta[2]);source.pitch=std::asin(delta[1]);
 auto selected=host_hit::query({0,1552,0},delta,200000,victim.feet,victim.yaw,0,stance);check(bool(selected),"test ray reaches authored BOX");
 check(a.join(shooter,ffa?0:1,source,1000,1000,std::array<uint16_t,1>{25},0)&&a.join(target,ffa?0:friendly?1:2,victim,1000,1000,std::array<uint16_t,1>{25},0),"HOST admitted life poses");a.active(true);
 auto shot=a.fire(shooter,{1,1,25,delta},0);check(bool(shot),"HOST validates shot direction");
 auto damage=1000-a.snapshot().players[target.slot]->hp;check(a.snapshot().players[shooter.slot]->ammo==29,"one accepted ray consumes one round");
 check(a.fire(shooter,{1,1,25,delta},1).reject==Reject::sequence,"replay cannot apply a region twice");
 return {damage,selected->bone,std::move(shot)};
}
}
int main(){try{
 auto body=fire(2);check(body.bone<=4&&body.damage==275,"unobstructed body uses original baseline");
 auto head=fire(0);check(head.bone==3||head.bone==4,"head/neck original joint selected");check(head.damage==275,"no authenticated aim flag means no automatic HS");
 auto leg=fire(11);check(leg.bone>4&&leg.damage==165,"bone BOX leg applies .6 at Authority boundary");
 auto pierced=fire(11,100);check(pierced.bone>4&&pierced.damage==148,"material force900 -> base247 -> limb148 order");
 check(fire(11,250).damage==0,"stopping material prevents region damage");
 check(fire(11,-1,true,true).damage==82,"friendly permitted damage applies original /2 after limb165");
 check(fire(11,-1,true,true,true).damage==165,"DM has no same-team division");
 check(fire(11,-1,false).damage==275,"nonregional profile retains capsule/body behavior");
 for(auto stance:{host_hit::Stance::crouching,host_hit::Stance::prone}){
  auto posed=fire(11,-1,true,false,false,stance);check(posed.bone>4&&posed.damage==165,"stance selects authored leg BOX rather than height band");
 }
 {Authority a;auto w=initial_profiles(20,1,0);w[0].nativeAkPenetration=false;bool rejected=false;try{a.begin(1,floor_wall(),w);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"regional modifier cannot enable on unrelated damage profile");}
 // A nearer actor wins even when the farther one occupies an earlier slot.
 // A friendly body remains an occluder with friendly fire disabled.
 for(bool friendly:{false,true}){
  Authority a;auto w=initial_profiles(20,1,0);a.begin(2,floor_wall(),w);Pose source,near,far;source.feet={0,2,0};near.feet={0,2,2500};far.feet={0,2,5000};
  constexpr Identity front{5,55,555};auto aim=center(0,host_hit::Stance::standing,near.feet);Vec3 d{aim[0],aim[1]-1552,aim[2]};float length=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);for(auto&v:d)v/=length;source.yaw=std::atan2(d[0],d[2]);source.pitch=std::asin(d[1]);
  check(a.join(shooter,1,source,1000,1000,std::array<uint16_t,1>{25},0)&&a.join(target,2,far,1000,1000,std::array<uint16_t,1>{25},0)&&a.join(front,friendly?1:2,near,1000,1000,std::array<uint16_t,1>{25},0),"near and far actors admitted");a.active(true);
  auto shot=a.fire(shooter,{2,1,25,d},0);check(bool(shot),"nearest actor shot accepted");auto s=a.snapshot();check(s.players[target.slot]->hp==1000,"near actor blocks enemy behind regardless of slot order");check(s.players[front.slot]->hp==(friendly?1000u:725u),"only front actor takes damage when allowed");
 }
 // The current renderer displays a stunned standing player down on the ground.
 // Keep its damage proxy down too; capsule metadata alone cannot leave a ghost.
 {
  Authority a;auto w=initial_profiles(20,1,0);Weapon stun;stun.id=26;stun.staminaDamage=1000;stun.intervalMs=100;stun.reloadMs=1000;stun.magazine=30;stun.range=200000;w.push_back(stun);a.begin(3,floor_wall(),w);
  Pose source,victim;source.feet={0,2,0};victim.feet={0,2,5000};
  auto aim=[&](unsigned index,host_hit::Stance stance){auto p=center(index,stance,victim.feet);Vec3 d{p[0],p[1]-1552,p[2]};float length=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);for(auto&v:d)v/=length;source.yaw=std::atan2(d[0],d[2]);source.pitch=std::asin(d[1]);return d;};
  auto d=aim(0,host_hit::Stance::standing);check(a.join(shooter,1,source,1000,1000,std::array<uint16_t,2>{26,25},0)&&a.join(target,2,victim,1000,1000,std::array<uint16_t,1>{25},0),"stun fixture admitted");a.active(true);check(bool(a.fire(shooter,{3,1,26,d},0)),"stamina shot accepted");check(a.snapshot().players[target.slot]->stunned,"standing life becomes stunned");
  check(a.equip(shooter,3,25,101)==Reject::none,"switch back to AK");check(bool(a.fire(shooter,{3,2,25,d},101)),"old head location shot accepted");check(a.snapshot().players[target.slot]->hp==1000,"stunned standing head leaves no floating damage proxy");
  d=aim(11,host_hit::Stance::prone);check(a.pose(shooter,3,1,source,202)==Reject::none,"aim follows downed pose");check(bool(a.fire(shooter,{3,3,25,d},202)),"grounded limb shot accepted");check(a.snapshot().players[target.slot]->hp==835,"downed original limb remains hittable with .6");
 }
 std::cout<<"Authoritative BOX hit, native HS fallback, limb/penetration/team ordering and replay PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
