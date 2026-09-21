#include "combat_authority.h"
#include "combat_wire.h"
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace mgo2mt;using namespace mgo2mt::combat;
namespace {
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
const Identity id{0,1,100};
auto floor(){return std::make_shared<stage::Collision>(stage::Collision::make({{-10000,0,-10000},{10000,0,-10000},{10000,0,10000},{-10000,0,10000}},{{{0,1,2},stage::attribute::native_solid},{{0,2,3},stage::attribute::native_solid}}));}
std::vector<Weapon> fixture_weapons(){std::vector<Weapon> w;for(uint16_t weaponId:{uint16_t(25),uint16_t(3),uint16_t(1),uint16_t(22)}){Weapon x;x.id=weaponId;x.heldOnly=true;w.push_back(x);}return w;}
auto authority(const weapons::Catalog& catalog,uint16_t item,float x=400,std::shared_ptr<const stage::Collision> obstacle={}){
 auto a=std::make_unique<Authority>();a->begin(1,floor(),fixture_weapons(),obstacle);a->item_box_catalog(catalog);items::Seed seed{*a->item_template(items::Domain::weapon,item),{x,2,0,0}};check(a->seed_items(std::span<const items::Seed>(&seed,1)),"seed without drop permission");return a;
}
// Four units above the floor avoids the existing skin=2 exact-tangent
// triangle-edge sweep roundoff, while remaining inside the support tolerance.
void join(Authority&a,std::span<const uint16_t> loadout){Pose p;p.feet[1]=4;check(a.join(id,1,p,1000,1000,loadout,0),"HOST join");a.active(true);}
}
int main(int argc,char**argv){try{
 check(argc==2,"weapon catalog argument");weapons::Catalog catalog;std::string error;check(catalog.load(argv[1],error),"catalog");const std::array<uint16_t,1> primary{25};
 for(uint16_t item:{uint16_t(3),uint16_t(22)}){auto a=authority(catalog,item);join(*a,primary);auto decision=a->advance_items(0);check(decision.events.size()==1&&decision.events[0].kind==EventKind::itemPickup,"catalog and uncategorized nondrop seeds contact-acquired");auto&e=decision.events[0];check(e.source==id&&e.target==id&&e.sourceLife==1&&e.targetLife==1&&e.weapon==item&&e.object==1&&!e.cue&&!e.hp&&!e.stamina,"explicit receipt metadata");
  auto held=a->item_held(id,123);check(held&&held->slots[1].contents.item==item&&a->item_state().entities.empty(),"empty weapon slot and only one transfer");check(a->advance_items(1).events.empty(),"no duplicate receipt");
  wire::Frame f;f.snapshot=a->snapshot();f.events=decision.events;check(!wire::encode(f).empty(),"receipt encodes with live snapshot");
 }
 {auto a=authority(catalog,1);join(*a,primary);check(a->item_held(id,123)->slots[4].contents.item==1&&a->advance_items(0).events.empty()&&a->item_state().entities.size()==1,"initial knife prevents duplicate contact pickup");}
 {auto a=authority(catalog,3,1400);join(*a,primary);check(a->advance_items(0).events.empty()&&a->item_state().entities.size()==1,"1500 manual pickup range not auto contact");}
 {auto a=authority(catalog,3);std::array<uint16_t,2> held{25,3};join(*a,held);check(a->advance_items(0).events.empty(),"duplicate held item never auto-acquired");}
 {auto wall=std::make_shared<stage::Collision>(stage::Collision::make({{300,0,-2000},{300,2500,-2000},{300,2500,2000},{300,0,2000}},{{{0,1,2},stage::attribute::native_solid},{{0,2,3},stage::attribute::native_solid}}));auto a=authority(catalog,3,400,wall);join(*a,primary);check(a->advance_items(0).events.empty(),"conservative overlap cannot pickup through wall");}
 {auto a=authority(catalog,3);join(*a,primary);a->active(false);check(a->advance_items(0).events.empty(),"inactive no contact acquisition");a->active(true);check(a->advance_items(501).events.empty(),"stale player pose no contact acquisition");}
 // Drop owner must actually exit and return, even after the timer expires.
 {auto a=std::make_unique<Authority>();a->begin(1,floor(),fixture_weapons());a->item_box_catalog(catalog);items::DropPolicy policy;policy.drop=items::DropOverride::allow;check(a->item_policy(3,policy),"explicit native drop policy");std::array<uint16_t,2> inventory{25,3};join(*a,inventory);
  auto held=a->item_held(id,123);items::wire::Command command;command.header=held->header;command.header.sequence=1;command.action=items::wire::Action::drop;command.heldSlot=1;command.heldRevision=held->slots[1].revision;check(bool(a->item_action(id,command,0)),"drop selected slot");check(a->item_state().entities.front().position.y==304,"drop begins in air at feet");
  Pose p;p.feet[1]=4;uint32_t seq=0;for(uint64_t t=0;t<=1000;t+=100){check(a->pose(id,1,++seq,p,t)==Reject::none,"fresh owner");check(a->advance_items(t).events.empty(),"no pickup while remaining in contact");}
  p.feet[0]=1000;check(a->pose(id,1,++seq,p,1250)==Reject::none,"owner steps out");check(a->advance_items(1250).events.empty(),"outside box");p.feet[0]=0;check(a->pose(id,1,++seq,p,1500)==Reject::none,"owner returns");check(a->advance_items(1500).events.size()==1,"recontact returns dropped item once");
 }
 std::cout<<"HOST contact: typed/unknown registered seeds, range vs overlap, occlusion, replay/grace and event wire PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
