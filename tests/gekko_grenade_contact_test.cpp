#include "combat_initial_profile.h"
#include "projectile_body_contact.h"
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace mgo2mt;using namespace mgo2mt::combat;
namespace {
void check(bool yes,const char*why){if(!yes)throw std::runtime_error(why);}
const Identity source{0,1,100},target{1,1,101};
auto scene(bool wall=false){
 std::vector<Vec3> v{{-100000,0,-100000},{100000,0,-100000},{100000,0,100000},{-100000,0,100000}};
 std::vector<stage::CollisionTriangle> t{{{0,1,2},stage::attribute::native_solid},{{0,2,3},stage::attribute::native_solid}};
 if(wall){v.insert(v.end(),{{-10000,0,1500},{10000,0,1500},{10000,10000,1500},{-10000,10000,1500}});t.push_back({{4,5,6},stage::attribute::native_solid});t.push_back({{4,6,7},stage::attribute::native_solid});}
 return std::make_shared<const stage::Collision>(stage::Collision::make(std::move(v),std::move(t)));
}
auto begin(bool friendly=true,bool wall=false){
 auto h=std::make_unique<Authority>();h->begin(1,scene(wall),initial_profiles(20,1,0));
 check(h->join(source,1,{{0,2,0}},1000,1000,std::array<uint16_t,1>{52},0),"source join");
 check(h->join(target,friendly?1:2,{{400,2,3000}},1000,1000,std::array<uint16_t,1>{25},0),"target join");
 check(h->assign_special(target,special_pc::Kind::gekko,true,0)==Reject::none,"Gekko form");h->active(true);
 check(bool(h->fire(source,{1,1,52,{0,0,1}},0)),"grenade fire");return h;
}
Vec3 flight(Authority&h,uint64_t time){h.advance_projectiles(time);auto flights=h.debug_flights();check(flights.size()==1,"grenade retained before original fuse");return flights[0].position;}
size_t count(const Decision&d,EventKind kind){return std::count_if(d.events.begin(),d.events.end(),[&](const Event&e){return e.kind==kind;});}
void normals(){
 constexpr stage::Capsule c{800,4200,2};const Vec3 feet{0,0,0};
 auto n=projectile::capsule_surface_normal({480,1600,-640},feet,c,{0,0,1});check(std::abs(n[0]-.6f)<1e-6f&&n[1]==0&&std::abs(n[2]+.8f)<1e-6f,"cylinder normal follows side contact");
 n=projectile::capsule_surface_normal({0,4200,0},feet,c,{0,-1,0});check(n==Vec3{0,1,0},"upper hemisphere normal");
 n=projectile::capsule_surface_normal({480,160,0},feet,c,{0,1,0});check(std::abs(n[0]-.6f)<1e-6f&&std::abs(n[1]+.8f)<1e-6f&&n[2]==0,"lower hemisphere normal");
 n=projectile::capsule_surface_normal({0,1500,0},feet,c,{0,0,1});check(n==Vec3{0,0,-1},"axis overlap finite fallback");
}
void bounce_and_fuse(){
 for(bool friendly:{false,true}){
  auto h=begin(friendly);const auto before=flight(*h,180),after=flight(*h,400);
  check(std::abs(before[0])<.01f&&before[2]<2308,"before body contact");
  check(after[0]<-400&&after[2]<2308,"oblique Gekko hit deflects sideways without crossing its body");
  check(h->snapshot().players[1]->hp==1000,"body contact does not detonate grenade");
  check(count(h->advance_projectiles(1000),EventKind::explosion)==0&&count(h->advance_projectiles(1800),EventKind::explosion)==0&&count(h->advance_projectiles(2001),EventKind::explosion)==0,"bounces preserve original fuse deadline");
  check(count(h->advance_projectiles(2002),EventKind::explosion)==1&&h->debug_flights().empty(),"single original-fuse explosion");
  check(count(h->advance_projectiles(2020),EventKind::explosion)==0,"fuse explosion cannot repeat");
 }
 auto h=begin(true,true);auto p=flight(*h,400);check(std::abs(p[0])<.01f&&p[2]<1500,"closer wall wins over Gekko body");
}
void current_state(){
 auto human=begin();flight(*human,100);check(human->assign_special(target,special_pc::Kind::human,true,100)==Reject::none,"current form narrows capsule");auto p=flight(*human,400);check(std::abs(p[0])<.01f&&p[2]>4000,"old Gekko radius cannot block after human transformation");
 auto dead=begin(false);flight(*dead,100);burning::Blast blast{{{1,source.slot,source.instance,source.character,1},52,0,1},999,{400,1600,3000},1800,5000,false};dead->explode(blast,100);check(!dead->snapshot().players[1]->alive,"target death fixture");p=flight(*dead,400);check(std::abs(p[0])<.01f&&p[2]>4000,"dead Gekko no longer blocks flight");
 auto disconnected=begin();flight(*disconnected,100);disconnected->leave(source);check(disconnected->advance_projectiles(400).events.empty()&&disconnected->debug_flights().empty(),"departed owner cannot retain damaging flight");
 auto deadOwner=begin(false);flight(*deadOwner,100);burning::Blast kill{{{1,target.slot,target.instance,target.character,1},128,0,2},998,{0,1200,0},1600,5000,false};deadOwner->explode(kill,100);check(!deadOwner->snapshot().players[0]->alive,"owner death fixture");check(deadOwner->advance_projectiles(400).events.empty()&&deadOwner->debug_flights().empty(),"dead owner life cancels flight");
 auto respawned=begin(false);flight(*respawned,100);respawned->explode(kill,100);check(respawned->respawn(source,2,[&]{return respawned->join(source,1,{{0,2,0}},1000,1000,std::array<uint16_t,1>{52},100);}),"owner life advances on respawn");check(respawned->advance_projectiles(400).events.empty()&&respawned->debug_flights().empty(),"new owner life cannot inherit a former life grenade");
 auto transformedOwner=begin();flight(*transformedOwner,40);check(transformedOwner->assign_special(source,special_pc::Kind::gekko,true,40)==Reject::none,"owner transforms around existing flight");p=flight(*transformedOwner,180);check(std::abs(p[0])<.01f&&p[2]>2100,"exact owner body remains excluded even after form change");
}
}
int main(){try{normals();bounce_and_fuse();current_state();std::cout<<"PASS Gekko grenade geometric normals, friendly blocking, oblique bounce, wall priority, fuse and current incarnation\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
