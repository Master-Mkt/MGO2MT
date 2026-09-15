#include "combat_initial_profile.h"
#include "combat_particle_effects.h"
#include "gekko_salute_audio.h"
#include "gcx_round_items.h"
#include "stage_profiles.h"
#include "stage_water.h"
#include "weapon_visual_policy.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;using namespace combat;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
auto floor_world(){return std::make_shared<const stage::Collision>(stage::Collision::make({{-100000,0,-100000},{-100000,0,100000},{100000,0,100000},{100000,0,-100000}},{{{0,1,2}},{{0,2,3}}}));}
int main(int argc,char**argv){try{
 check(argc==2,"stage root");const Identity id{0,1,100};auto profiles=initial_profiles(20,1,0);
 auto host=std::make_unique<Authority>();host->begin(1,floor_world(),profiles);check(host->join(id,1,{{0,2,0}},1000,1000,std::array<uint16_t,3>{25,3,52},0),"human spawn");host->active(true);
 auto held=*host->item_held(id,1);check(held.slots[4].contents.item==1&&held.slots[3].contents.item==0,"knife is independent fifth holding");check(items::wire::encode(held).has_value(),"five holdings encode");
 auto fired=host->fire(id,{1,1,25,{0,0,1}},0);check(bool(fired)&&host->snapshot().players[0]->ammo==29,"human shot consumes round");for(auto&e:fired.events)if(e.kind==EventKind::shot)check(e.shotDistance==0,"early magazine has no tracer");
 check(host->assign_special(id,special_pc::Kind::gekko,true,1)==Reject::none,"Gekko assignment");held=*host->item_held(id,1);
 for(auto slot:{0,1,2,4})check(held.slots[slot].contents.item==unsigned(slot==4?131:128+slot),"only original Gekko IDs");check(held.slots[3].contents.item==0&&items::wire::encode(held).has_value(),"empty equipment and Gekko holdings wire");
 check(host->equip(id,1,25,2)==Reject::weapon,"Gekko cannot equip human rifle");
 for(uint16_t weapon:{128,129}){const uint64_t now=weapon==128?10:1210;check(host->pose(id,1,weapon,host->snapshot().players[0]->pose,now)==Reject::none,"gun pose");check(host->equip(id,1,weapon,now)==Reject::none,"Gekko selects gun");auto ammo=host->snapshot().players[0]->ammo;check(bool(host->fire(id,{1,uint32_t(weapon),weapon,{0,0,1}},now)),"Gekko fires");check(host->snapshot().players[0]->ammo==ammo,"Gekko ammunition infinite");}
 check(host->assign_special(id,special_pc::Kind::human,true,1300)==Reject::none,"return human");check(host->snapshot().players[0]->weapon==25&&host->snapshot().players[0]->ammo==29&&host->item_held(id,1)->slots[4].contents.item==1,"human inventory and spent ammo restored");
 check(host->equip(id,1,128,1301)==Reject::weapon,"human cannot select Gekko gun");
 auto banned=profiles;std::erase_if(banned,[](auto&w){return w.id==1;});host->begin(2,floor_world(),banned);check(host->join(id,1,{{0,2,0}},1000,1000,std::array<uint16_t,1>{25},0)&&!host->item_held(id,1)->slots[4].contents.item&&!host->item_template(items::Domain::weapon,1),"HOST knife ban excludes initial and pickup paths");
 // Grounded fire drives the same authority action used by remote replication.
 for(uint16_t weapon:{130,131}){auto h=std::make_unique<Authority>();h->begin(1,floor_world(),profiles);check(h->join(id,1,{{0,2,0}},1000,1000,std::array<uint16_t,1>{25},0),"melee source");Identity enemy{1,2,101};check(h->join(enemy,2,{{0,2,-1800}},1000,1000,std::array<uint16_t,1>{25},0),"melee rear target");h->active(true);check(h->assign_special(id,special_pc::Kind::gekko,true,0)==Reject::none&&h->equip(id,1,weapon,0)==Reject::none,"melee selected");check(bool(h->fire(id,{1,1,weapon,{0,0,1}},0)),"melee trigger");h->advance_special_pc(900);check(h->snapshot().players[1]->hp==(weapon==130?1000u:500u),"kick forward only versus radial stomp");h->advance_special_pc(901);check(h->snapshot().players[1]->hp==(weapon==130?1000u:500u),"one hit per action");}
 // An ordinary fall, outside a jump/climb action, also preserves Gekko HP.
 host->begin(3,floor_world(),profiles);check(host->join(id,1,{{0,30000,0}},1000,1000,std::array<uint16_t,1>{25},0),"high spawn");host->active(true);check(host->assign_special(id,special_pc::Kind::gekko,true,0)==Reject::none,"fall form");host->advance_falling(1);auto p=host->snapshot().players[0]->pose;
 for(unsigned n=1;n<=120;++n){p.feet[1]=std::max(2.f,30000.f-n*250);check(host->pose(id,3,n,p,n*50)==Reject::none,"accepted falling movement");check(host->advance_falling(n*50).events.empty(),"Gekko fall has no damage events");}check(host->snapshot().players[0]->hp==1000,"Gekko high landing HP");
 special_pc::SaluteAudio audio;Snapshot s;s.epoch=9;Player a;a.identity=id;a.life=1;a.alive=true;a.specialPc.kind=special_pc::Kind::gekko;s.players[0]=a;check(audio.update(s,1).empty(),"audio baseline");s.players[0]->specialPc={special_pc::Kind::gekko,true,special_pc::Action::salute,1,0};check(audio.update(s,1).size()==1&&audio.update(s,1).empty(),"salute sounds once");check(audio.update(s,2).empty(),"scene history not replayed");
 particles::Pool pool;s.eventWatermark=0;pool.synchronize(9,1,0,0);Event e;e.epoch=9;e.id=1;e.kind=EventKind::shot;e.weapon=128;e.source=id;e.sourceLife=1;e.normal={0,0,1};e.position={0,2600,0};s.eventWatermark=1;pool.dispatch({&e,1},s,0);check(pool.sample(s,0).size()==3,"muzzle flash rays");pool.dispatch({&e,1},s,1);check(pool.size()==1&&pool.sample(s,80).empty(),"flash duplicate and expiry");e.id=2;e.weapon=129;e.kind=EventKind::projectileTrail;s.eventWatermark=2;pool.dispatch({&e,1},s,100);check(pool.sample(s,100).front().kind==particles::Kind::smoke,"missile smoke");
 for(uint16_t weapon:{2,3,22,24,25,26,30,31,35,50,128,129})for(uint16_t count:{0,1,5,6,30})check(tracer_shot(weapon,count)==((weapon==22||weapon==24||weapon==25||weapon==26||weapon==30||weapon==31||weapon==35)&&count>=1&&count<=5),"AR MG last-five tracer boundary");
 for(const auto& profile:stage::runtime_profiles){auto layout=items::load_gcx_item_layout(argv[1],profile.map);check(layout&&layout->verified&&layout->groups.size()==4,"own stage GCX verified");items::RoundItems settings;items::replace_pickup_item(settings,*layout,22,items::Domain::weapon,2);items::replace_pickup_item(settings,*layout,10,items::Domain::equipment,16);auto unchanged=settings.serialize();bool refused=false;try{items::replace_pickup_item(settings,*layout,99,items::Domain::weapon,2);}catch(...){refused=true;}check(refused&&settings.serialize()==unchanged,"invalid replacement atomic");
  std::ifstream f(std::filesystem::path(argv[1])/(std::string(profile.stage)+".collision.cfg"));auto collision=stage::movement_collision(std::make_shared<const stage::Collision>(stage::Collision::read(f)));
  for(uint8_t rule:{0,1})for(uint8_t generation=1;generation<=16;++generation){auto plan=items::plan_gcx_round_items(settings,&*layout,{},profile.map,rule,generation);check(plan.verified&&plan.items.size()==2,"two own-script placements");for(auto&item:plan.items)check(item.item==2||item.item==16,"replacement preserves selected source");auto seeds=items::resolve_gcx_round_items(plan,*collision,[](auto d,auto id){return std::optional<items::Contents>{{id,1,0,0,0,items::Resource::durable,d}};});check(seeds.size()==2,"both original positions supported by own stage floor");}
  items::replace_pickup_item(settings,*layout,22,items::Domain::equipment,22);items::replace_pickup_item(settings,*layout,10,items::Domain::equipment,10);check(settings.replacements.empty(),"reset restores GCX defaults");
 }
 std::cout<<"Knife, Gekko weapons/ammo/fall/melee, salute audio, flash/smoke, tracer bounds, five-stage GCX replacements PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
