#include "combat_authority.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;using namespace mgo2mt::combat;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(){try{
 std::vector<stage::Vec3> v{{-5000,0,-5000},{-5000,0,5000},{5000,0,5000},{5000,0,-5000},{500,3000,-5000},{500,3000,5000},{5000,3000,5000},{5000,3000,-5000}};
 auto world=std::make_shared<stage::Collision>(stage::Collision::make(v,{{{0,1,2},stage::attribute::native_solid},{{0,2,3},stage::attribute::native_solid},{{4,5,6},stage::attribute::native_solid},{{4,6,7},stage::attribute::native_solid}}));
 Weapon w;w.id=23;w.damage=100;w.intervalMs=100;w.reloadMs=500;w.magazine=30;w.reserve=60;w.range=10000;
 Authority host;host.begin(7,world,std::array{w});ladder::Anchor a{1,{0,4,0},{0,3004,0},{-400,4,0},{1100,3004,0},1.57079633f};host.configure_ladders({a});Identity id{0,1,100};Pose p{a.bottomExit,0,0,{350,1700,2}};
 check(host.join(id,1,p,1000,1000,std::array<uint16_t,1>{23},0),"join");host.active(true);uint32_t sequence=1;uint64_t now=100;
 auto submit=[&](ladder::Intent i){auto pose=host.snapshot().players[0]->pose;check(host.pose(id,7,sequence,pose,now)==Reject::none,"accepted pose prerequisite");auto out=host.ladder_action(id,7,sequence,i,now);++sequence;now+=100;return out;};
 check(bool(submit({ladder::Action::enter,1,0})),"HOST native anchor entry");check(host.snapshot().players[0]->ladderAnchor==1,"anchor snapshot");
 check(host.ladder_action(id,7,sequence-1,{ladder::Action::none,1,1},now).reject==Reject::sequence,"replay cannot climb");
 p=host.snapshot().players[0]->pose;p.feet[1]+=500;check(host.pose(id,7,sequence,p,now)==Reject::unavailable,"air teleport rejected");
 p=host.snapshot().players[0]->pose;p.capsule.radius=260;check(host.pose(id,7,sequence,p,now)==Reject::unavailable,"attached capsule resize rejected");
 check(host.ladder_action(id,8,sequence,{},now).reject==Reject::generation,"old epoch rejected");check(host.ladder_action(id,7,sequence,{},now,2).reject==Reject::generation,"old life rejected");
 auto before=host.snapshot().players[0]->pose.feet;check(bool(submit({})),"idle safely stops");check(host.snapshot().players[0]->pose.feet==before,"idle no gravity/no climb");
 check(submit({ladder::Action::none,2,1}).reject==Reject::identity,"different anchor rejected");
 for(int n=0;n<60;++n)check(bool(submit({ladder::Action::none,1,1})),"HOST ascent");check(host.snapshot().players[0]->pose.feet==a.top,"top clamps");
 check(bool(submit({ladder::Action::leave,1,0}))&&host.snapshot().players[0]->ladderAnchor==0&&host.snapshot().players[0]->pose.feet==a.topExit,"supported top exit");
 check(bool(submit({ladder::Action::enter,1,0})),"upper reentry");for(int n=0;n<60;++n)check(bool(submit({ladder::Action::none,1,-1})),"descent");check(bool(submit({ladder::Action::leave,1,0}))&&host.snapshot().players[0]->pose.feet==a.bottomExit,"roundtrip");
 check(bool(submit({ladder::Action::enter,1,0})),"reenter before phase reset");host.active(false);check(!host.snapshot().players[0]->ladderAnchor,"phase reset clears attachment");
 std::cout<<"HOST ladder sequence/scope/pose/idle/roundtrip/reset PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
