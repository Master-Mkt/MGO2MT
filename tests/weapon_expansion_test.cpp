#include "combat_initial_profile.h"
#include "combat_wire.h"
#include "combat_service.h"
#include "original_lock_policy.h"
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
using namespace mgo2mt;using namespace mgo2mt::combat;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
const Identity a{0,1,100},b{1,1,101};
auto floor(){return std::make_shared<const stage::Collision>(stage::Collision::make({{-200000,0,-200000},{200000,0,-200000},{200000,0,200000},{-200000,0,200000}},{{{0,1,2}},{{0,2,3}}}));}
auto begin(uint16_t id,float distance=5000){auto h=std::make_unique<Authority>();h->begin(1,floor(),initial_profiles(20,1,0));check(h->join(a,1,{{0,2,0}},10000,1000,std::array{id},0),"join source");check(h->join(b,2,{{0,2,distance}},10000,1000,std::array{id},0),"join target");h->active(true);return h;}
size_t count(const Decision& d,EventKind kind){return std::count_if(d.events.begin(),d.events.end(),[&](const Event&e){return e.kind==kind;});}
void firearms(){
 auto profiles=initial_profiles(20,1,0);std::set<uint16_t> ids;for(const auto&w:profiles)check(ids.insert(w.id).second,"unique profiles");
 check(profiles.front().id==25&&profiles[1].id==3&&profiles[2].id==52,"default ordering");
 for(const auto& p:original_weapon::firearms){
  auto h=begin(p.id);auto shot=h->fire(a,{1,1,p.id,{0,0,1}},0);check(bool(shot)&&count(shot,EventKind::shot)==1,"all firearms fire");
  const auto state=h->snapshot();check(state.players[0]->ammo==p.magazine-1,"original magazine consumed once");
  check(h->fire(a,{1,1,p.id,{0,0,1}},0).reject==Reject::sequence,"fire replay rejected");
  check(bool(h->reload(a,1,1)),"all firearms reload");h->advance(60000);check(h->snapshot().players[0]->ammo==p.magazine,"reload fills own capacity");
  check(bool(original_lock::weapon_parameters(p.id,0,0,0))==(p.lockRange>0),"original lock zero disables");
 }
 auto m4=begin(24);auto hit=m4->fire(a,{1,1,24,{0,0,1}},0);check(bool(hit)&&m4->snapshot().players[1]->hp==9775&&m4->snapshot().players[1]->stamina==1000,"positive HP ignores signed stamina sentinel");
 auto mosin=begin(43);check(bool(mosin->fire(a,{1,1,43,{0,0,1}},0)),"Mosin fire");check(mosin->snapshot().players[1]->hp==10000&&mosin->snapshot().players[1]->stamina==200,"Mosin recovered nonlethal damage");
}
void melee(){
 for(uint16_t id:{1,73}){auto h=begin(id);auto held=h->item_held(a,1);check(held&&items::wire::encode(*held).has_value(),"durable held state encodes");check(h->snapshot().players[0]->ammo==0,"durable equipment has no ammunition");}
 auto h=begin(25,1000);const auto ammo=h->snapshot().players[0]->ammo;
 auto punch=h->melee(a,{1,1,25,{0,0,1}},0);check(bool(punch)&&count(punch,EventKind::melee)==1&&count(punch,EventKind::shot)==0,"punch distinct event");
 check(h->snapshot().players[0]->ammo==ammo&&h->snapshot().players[1]->hp==10000,"punch no bullet/knife HP");
 check(h->fire(a,{1,2,25,{0,0,1}},1).reject==Reject::interval,"punch gates subsequent fire");
 check(h->equip(a,1,1,800)==Reject::none,"equip default knife");check(h->pose(a,1,1,{{0,2,0}},800)==Reject::none,"knife fresh pose");
 auto knife=h->fire(a,{1,3,1,{0,0,1}},800);check(bool(knife)&&count(knife,EventKind::melee)==0&&count(knife,EventKind::shot)==1,"knife equipped attack");
 check(h->snapshot().players[1]->hp==9825,"knife original HP175");
 check(h->melee(a,{1,4,1,{0,0,1}},801).reject==Reject::unavailable,"equipped knife cannot be implicit CQC punch");
 auto far=begin(25,3000);check(bool(far->melee(a,{1,1,25,{0,0,1}},0))&&far->snapshot().players[1]->stamina==1000,"punch range bounded");
}
void support(){
 for(uint16_t id:{52,53,54,55,56,57,58,59,63}){
  auto h=begin(id);auto shot=h->fire(a,{1,1,id,{0,0,1}},0);check(bool(shot)&&count(shot,EventKind::projectile)==1,"grenade released");
  size_t smoke=0,explosions=0;for(uint64_t at=500;at<=3500;at+=500){auto d=h->advance_projectiles(at);smoke+=count(d,EventKind::smoke);explosions+=count(d,EventKind::explosion);}
  check((id>=56&&id<=59)?smoke==1&&explosions==0:smoke==0&&explosions==1,"distinct grenade activation exactly once");
 }
 for(uint16_t id:{64,65,66,67,69}){
  auto h=begin(id,1200);auto placed=h->fire(a,{1,1,id,{0,0,1}},0);check(bool(placed)&&h->item_state().entities.size()==1&&h->snapshot().players[0]->ammo+h->snapshot().players[0]->reserve==3,"deployable commits one unit");
  if(id==66||id==67){check(h->pose(a,1,1,{{0,2,0}},1001)==Reject::none,"detonator fresh pose");auto blast=h->reload(a,1,1001);check(bool(blast)&&count(blast,EventKind::explosion)==1,"C4 Reload detonates without extra ammo");}
  else h->advance_projectiles(1001);
  check(h->item_state().entities.empty(),"trigger removes installed entity");
  Replica r;check(r.snapshot(h->snapshot()),"status snapshot valid");
 }
}
void support_order_and_surface(){
 for(uint16_t id:{66,67}){
  auto h=begin(id);std::vector<uint64_t> placed;
  for(uint32_t i=0;i<3;++i){auto at=uint64_t(i)*1001;check(h->pose(a,1,i+1,{{0,2,0}},at)==Reject::none,"placement fresh pose");check(bool(h->fire(a,{1,i+1,id,{0,0,1}},at)),"multiple support Fire places");placed.push_back(h->item_state().entities.back().key.id);}
  check(h->item_state().entities.size()==3&&h->snapshot().players[0]->ammo+h->snapshot().players[0]->reserve==1,"three placements conserve charges");
  check(h->pose(a,1,4,{{0,2,0}},3003)==Reject::none,"detonation fresh pose");
  check(count(h->reload(a,1,3003),EventKind::explosion)==1,"one Reload one activation");
  auto state=h->item_state();check(state.entities.size()==2&&state.entities.front().key.id==placed[1],"Reload preserves placement order");
  check(h->reload(a,1,3004).reject==Reject::interval,"Reload cannot bypass activation cadence");
  check(h->pose(a,1,5,{{0,2,0}},4004)==Reject::none,"next detonation fresh pose");
  check(count(h->reload(a,1,4004),EventKind::explosion)==1&&h->item_state().entities.front().key.id==placed[2],"next Reload next placement");
 }
 auto wall=std::make_shared<const stage::Collision>(stage::Collision::make({{-20000,0,-20000},{20000,0,-20000},{20000,0,20000},{-20000,0,20000},{-2000,0,1000},{2000,0,1000},{2000,3000,1000},{-2000,3000,1000}},{{{0,1,2}},{{0,2,3}},{{4,5,6}},{{4,6,7}}}));
 auto h=std::make_unique<Authority>();h->begin(1,wall,initial_profiles(20,1,0));check(h->join(a,1,{{0,2,0}},10000,1000,std::array<uint16_t,1>{66},0),"wall source join");h->active(true);
 check(bool(h->fire(a,{1,1,66,{0,0,1}},0)),"support wall placement");auto position=h->item_state().entities.front().position;check(position.y>1000&&position.nz<-.99f&&std::abs(position.ny)<.01f&&position.z<1000,"HOST surface normal retained");
}
void support_service_dispatch(){
 auto service=std::make_unique<Service>(1);service->configure(floor(),initial_profiles(20,1,0));auto& host=service->authority();
 check(host.join(a,1,{{0,2,0}},10000,1000,std::array<uint16_t,1>{66},0),"remote service source");check(host.join(b,2,{{0,2,1200}},1,1000,std::array<uint16_t,1>{25},0),"remote service victim");host.active(true);
 check(service->admit(a)&&service->receive(a,wire::encode(wire::Accept{1}),0),"remote service accepted");service->deliveries();
 size_t deaths=0,bursts=0;service->event_handler([&](std::span<const Event> events,uint64_t){for(const auto&e:events){deaths+=e.kind==EventKind::death&&e.target==b;bursts+=e.kind==EventKind::explosion;}return Decision{};});
 uint32_t sequence=0;auto input=[&](uint64_t at,bool fire,bool reload){wire::Input q;q.epoch=1;q.sequence=++sequence;q.weapon=66;q.pose.feet={0,2,0};q.firePressed=fire;q.reload=reload;check(service->receive(a,wire::encode(q),at),"remote command accepted");service->poll(at);service->deliveries();};
 input(0,true,false);input(1001,true,false);auto state=host.item_state();check(state.entities.size()==2,"service two placements");auto second=state.entities[1].key.id;
 input(2002,false,true);check(deaths==1&&bursts==1&&!host.snapshot().players[b.slot]->alive,"Reload death dispatched once");check(host.item_state().entities.size()==1&&host.item_state().entities.front().key.id==second,"service Reload keeps second placement");
 service->poll(2003);service->deliveries();check(deaths==1&&bursts==1,"poll cannot replay Reload activation");input(3003,false,true);check(deaths==1&&bursts==2&&host.item_state().entities.empty(),"next Reload dispatches next blast without duplicate death");
}
}
int main(){try{firearms();melee();support();support_order_and_surface();support_service_dispatch();std::cout<<"weapon expansion OK\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
