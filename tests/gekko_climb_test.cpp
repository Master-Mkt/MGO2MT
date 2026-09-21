#include "gekko_test_profiles.h"
#include "gekko_climb.h"
#include "combat_authority.h"
#include <iostream>
#include <limits>
using namespace mgo2mt;using namespace special_pc;
namespace {
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
stage::Collision scene(float height=7000,float topDepth=5000,bool roof=false){
 std::vector<stage::Vec3> v;std::vector<stage::CollisionTriangle> t;
 auto quad=[&](stage::Vec3 a,stage::Vec3 b,stage::Vec3 c,stage::Vec3 d){auto n=unsigned(v.size());v.insert(v.end(),{a,b,c,d});t.push_back({{n,n+1,n+2},stage::attribute::native_solid});t.push_back({{n,n+2,n+3},stage::attribute::native_solid});};
 quad({-20000,0,-20000},{-20000,0,20000},{20000,0,20000},{20000,0,-20000});
 quad({-5000,0,1500},{5000,0,1500},{5000,height,1500},{-5000,height,1500});
 quad({-5000,height,1500},{-5000,height,1500+topDepth},{5000,height,1500+topDepth},{5000,height,1500});
 if(roof)quad({-10000,height+3000,-10000},{10000,height+3000,-10000},{10000,height+3000,10000},{-10000,height+3000,10000});
 return stage::Collision::make(v,t);
}
void host(){
 using namespace combat;const Identity id{0,1,100};Weapon weapon;weapon.id=23;weapon.damage=100;weapon.intervalMs=100;weapon.reloadMs=500;weapon.magazine=30;weapon.reserve=60;weapon.range=20000;
 const auto make=[&](){auto a=std::make_unique<Authority>();a->begin(9,std::make_shared<stage::Collision>(scene()),gekko_test_profiles(weapon));check(a->join(id,1,{{0,2,0}},1000,1000,std::array<uint16_t,1>{23},0),"HOST admit");a->active(true);check(a->assign_special(id,Kind::gekko,true,0)==Reject::none,"HOST Gekko assignment");return a;};
 auto a=make();auto pose=a->snapshot().players[0]->pose;check(a->pose(id,9,1,pose,100)==Reject::none&&a->special_action(id,9,1,{Action::climb,1},100)==Reject::none,"HOST validates mantle intent");
 check(a->special_action(id,9,1,{Action::climb,1},100)==Reject::sequence&&a->special_action(id,10,1,{Action::climb,2},100)==Reject::generation&&a->special_action(id,9,1,{Action::climb,2},100,2)==Reject::generation,"duplicate epoch and life rejected");
 a->advance_special_pc(1300);auto current=a->snapshot().players[0]->pose;auto forged=current;forged.feet[0]+=1000;check(a->pose(id,9,2,forged,1400)==Reject::unavailable,"client cannot alter HOST mantle path");check(a->assign_special(id,Kind::human,true,1400)==Reject::unavailable,"form change waits for traversal");
 check(a->advance_falling(1300).events.empty(),"powered climb not fall damage");a->advance_special_pc(2700);check(a->snapshot().players[0]->pose.feet[1]==7002&&a->snapshot().players[0]->specialPc.action==Action::none&&a->advance_falling(2700).events.empty(),"HOST mantle completes on support without fall damage");
 auto cancelled=make();pose=cancelled->snapshot().players[0]->pose;cancelled->pose(id,9,1,pose,100);check(cancelled->special_action(id,9,1,{Action::climb,1},100)==Reject::none,"cancel fixture start");cancelled->advance_special_pc(1300);current=cancelled->snapshot().players[0]->pose;cancelled->release_special_pc(id);check(cancelled->snapshot().players[0]->pose==current&&cancelled->snapshot().players[0]->specialPc.action==Action::none,"HOST cancellation preserves approved position");cancelled->advance_special_pc(1800);check(cancelled->snapshot().players[0]->pose.feet[1]<current.feet[1]&&cancelled->snapshot().players[0]->specialPc.action==Action::none,"cancelled actor descends without client input");
 current=cancelled->snapshot().players[0]->pose;check(cancelled->pose(id,9,2,current,1800)==Reject::none,"first accepted active pose hands recovery to client");cancelled->advance_special_pc(2000);check(cancelled->snapshot().players[0]->pose==current,"handoff does not double integrate");
 cancelled->leave(id);check(cancelled->advance_special_pc(3000).events.empty()&&!cancelled->snapshot().players[0],"leave removes traversal state");
 auto jump=make();pose=jump->snapshot().players[0]->pose;pose.feet[0]=-10000; // Start directly on a separately admitted clear floor, outside the ledge.
 jump->begin(10,std::make_shared<stage::Collision>(scene()),gekko_test_profiles(weapon));check(jump->join(id,1,{{-10000,2,0}},1000,1000,std::array<uint16_t,1>{23},0),"new epoch admits baseline");jump->active(true);jump->assign_special(id,Kind::gekko,true,0);pose=jump->snapshot().players[0]->pose;jump->pose(id,10,1,pose,100);check(jump->special_action(id,10,1,{Action::jump,1},100)==Reject::none,"10m HOST jump start");
 for(uint32_t t=0;t<native_gekko.jumpMs;t+=20){jump->advance_special_pc(100+t);check(jump->advance_falling(100+t).events.empty()&&jump->snapshot().players[0]->hp==1000,"controlled10m descent does not self-kill");}jump->advance_special_pc(100+native_gekko.jumpMs);check(jump->advance_falling(100+native_gekko.jumpMs).events.empty()&&jump->snapshot().players[0]->hp==1000,"normal jump landing preserves HP");
}

}
int main(int argc,char**argv){try{
 auto w=scene();auto c=begin_climb({0,2,0},0,w);check(bool(c),"real wall/top/path accepted");check(c->landing[1]==7002&&c->landing[2]>2300,"HOST destination rests beyond edge");
 auto once=*c;check(advance_climb(once,2600,w)&&once.finished&&once.feet==once.landing,"delayed whole mantle sweeps all segments");
 for(uint32_t t=0;t<=2600;t+=20){check(advance_climb(*c,t,w)&&w.clear(c->feet,native_gekko.capsule),"every frame capsule clear");}check(c->feet==once.feet,"poll partition invariant final pose");
 check(!begin_climb({0,2,0},0,scene(10001)),"above 10m rejected");check(bool(begin_climb({0,2,0},0,scene(10000))),"exact 10m accepted");
 check(!begin_climb({0,2,0},0,scene(7000,400)),"narrow landing rejected");check(!begin_climb({0,2,0},0,scene(7000,5000,true)),"full4200 head clearance required");check(!begin_climb({0,2,0},3.14159265f,w),"no forward wall rejected");
 check(!begin_climb({0,2,0},std::numeric_limits<float>::quiet_NaN(),w),"invalid direction rejected");
 std::array peers{JumpBody{{0,7002,2450},native_gekko.capsule}};check(!begin_climb({0,2,0},0,w,peers),"occupied landing rejected");
 auto blocked=begin_climb({0,2,0},0,w).value();advance_climb(blocked,1000,w);auto before=blocked.feet;check(!advance_climb(blocked,900,w)&&blocked.feet==before&&!blocked.cancelled,"clock rollback leaves climb intact");check(!advance_climb(blocked,2000,w,peers)&&blocked.cancelled&&blocked.feet==before,"new peer blocks destination without teleport");
 auto cancelled=begin_climb({0,2,0},0,w).value();advance_climb(cancelled,1200,w);before=cancelled.feet;cancel_climb(cancelled);check(!advance_climb(cancelled,2600,w)&&cancelled.feet==before,"cancel keeps last approved airborne position");
 auto j=begin_jump({-10000,2,0},{}).value();check(advance_jump(j,2029,w)&&std::abs(j.feet[1]-10002)<1,"native apex rises exactly10m");check(gekko_jump_height_ms(800)<1100&&jump_height(2029)==10000,"original pose compensation stays separate");check(advance_jump(j,native_gekko.jumpMs,w)&&j.feet[1]<3,"normal10m jump lands safely");
 auto low=scene(7000,5000,true);auto head=begin_jump({-10000,2,0},{}).value();advance_jump(head,2029,low);check(head.upwardStopped&&head.feet[1]<5900,"head collision stops ascent");
 RecoveryFall fall{{-10000,10002,0},0,0,1};check(advance_recovery_fall(fall,500,w)&&fall.feet[1]<9000&&!fall.grounded,"cancelled10m body falls with no input");auto fallBefore=fall;check(!advance_recovery_fall(fall,499,w)&&fall.feet==fallBefore.feet&&fall.speed==fallBefore.speed,"recovery rollback retains evidence");
 for(uint64_t now=1000;now<=3000;now+=500)check(advance_recovery_fall(fall,now,w),"bounded falling steps");check(fall.grounded&&fall.feet[1]>=1.9f&&fall.feet[1]<3,"recovery sweeps and stops at real ground");
 RecoveryFall delayed{{-10000,10002,0},0,0,1};check(advance_recovery_fall(delayed,100000,w)&&delayed.feet[1]>5000,"long gap processes bounded physics instead of tunnelling");
 if(!(argc>1&&std::string(argv[1])=="--pure-only"))host();
 std::cout<<"Gekko native10m jump/mantle: full capsule path, shelf, ceiling, peers, monotonic time, cancel PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
