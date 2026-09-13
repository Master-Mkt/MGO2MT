#include "combat_authority.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
namespace {
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
const combat::Identity id{1,11,101};
std::shared_ptr<const stage::Collision> floor(bool wall=false){std::vector<stage::Vec3> v{{-5000,0,-5000},{5000,0,-5000},{5000,0,5000},{-5000,0,5000}};std::vector<stage::CollisionTriangle> t{{{0,1,2}},{{0,2,3}}};if(wall){v.insert(v.end(),{{-1000,0,300},{1000,0,300},{1000,3000,300},{-1000,3000,300}});t.push_back({{4,5,6}});t.push_back({{4,6,7}});}return std::make_shared<const stage::Collision>(stage::Collision::make(v,t));}
items::wire::Command command(uint64_t sequence,items::wire::Action action,uint64_t heldRevision=0,uint64_t entity=0,uint64_t entityRevision=0){items::wire::Command c;c.header={{1,1},123,{id.slot,id.instance,id.character,1},sequence};c.action=action;c.heldSlot=0;c.heldRevision=heldRevision;c.entity=entity;c.entityRevision=entityRevision;if(action==items::wire::Action::install)c.amount=1;if(action==items::wire::Action::use){c.heldSlot=255;c.amount=1;c.heldRevision=0;}return c;}
}
int main(){try{
 // Synthetic timing/damage fixture; only item ID25 exercises the native adapter.
 combat::Weapon weapon{25,1,0,100,200,3,6,10000,9001,9002,true};combat::Authority host;host.begin(1,floor(),std::span(&weapon,1));combat::Pose pose;pose.feet={0,2,0};check(host.join(id,1,pose,100,100,std::array<uint16_t,1>{25},0),"join");host.active(true);
 auto held=host.item_held(id,123);check(held&&held->slots[0].contents.magazine==3&&held->slots[0].revision==1,"initial authoritative held");
 check(bool(host.fire(id,{1,1,25,{0,0,1}},0)),"shot");held=host.item_held(id,123);check(held->slots[0].contents.magazine==2&&held->slots[0].revision==2,"shot updates persistent slot");
 check(host.item_action(id,command(1,items::wire::Action::drop,1),1).code==items::ResultCode::stale,"shot makes old drop revision stale");
 auto drop=host.item_action(id,command(2,items::wire::Action::drop,2),1);check(bool(drop)&&drop.entity&&host.snapshot().players[id.slot]->weapon==0,"drop becomes unarmed");check(drop.entity->contents.magazine==2&&drop.entity->contents.reserve==6,"drop retains exact ammo");
 check(host.equip(id,1,25,2)==combat::Reject::weapon&&host.equip(id,1,0,2)==combat::Reject::none&&host.reload(id,1,2).reject==combat::Reject::weapon,"unarmed cannot re-equip absent inventory or reload");check(host.fire(id,{1,2,0,{0,0,1}},100).reject==combat::Reject::weapon,"unarmed fire no crash");
 auto pick=host.item_action(id,command(3,items::wire::Action::pickup,3,drop.entity->key.id,1),100);check(bool(pick)&&host.snapshot().players[id.slot]->weapon==25&&host.snapshot().players[id.slot]->ammo==2,"pickup and equip transaction");
 check(bool(host.fire(id,{1,3,25,{0,0,1}},110)),"recovered weapon works");check(bool(host.reload(id,1,120)),"reload");check(host.item_action(id,command(4,items::wire::Action::drop,5),121).code==items::ResultCode::unauthorized,"no drop during reload");host.advance(320);held=host.item_held(id,123);check(held->slots[0].contents.magazine==3&&held->slots[0].contents.reserve==4&&held->slots[0].revision==6,"refill updates one persistent inventory");
 auto install=host.item_action(id,command(5,items::wire::Action::install,6),321);check(bool(install)&&install.entity->kind==items::PlacementKind::installed&&host.snapshot().players[id.slot]->weapon==0,"ground-installed AK distinct kind");
 auto use=host.item_action(id,command(6,items::wire::Action::use,0,install.entity->key.id,1),322);check(bool(use)&&host.snapshot().players[id.slot]->weapon==25&&host.snapshot().players[id.slot]->ammo==3&&host.item_state().entities.empty(),"use recovers/equips, no fake ammo consume");
 items::DropPolicy deny;deny.drop=items::DropOverride::deny;host.item_policy(25,deny);held=host.item_held(id,123);check(host.item_action(id,command(7,items::wire::Action::drop,held->slots[0].revision),323).code==items::ResultCode::policy&&host.snapshot().players[id.slot]->weapon==25,"configured drop denial");
 items::DropPolicy allow;allow.drop=items::DropOverride::allow;host.item_policy(25,allow);auto placed=host.item_action(id,command(8,items::wire::Action::drop,held->slots[0].revision),324);check(bool(placed),"second drop");host.world(floor(true));held=host.item_held(id,123);check(host.item_action(id,command(9,items::wire::Action::pickup,held->slots[0].revision,placed.entity->key.id,1),325).code==items::ResultCode::unauthorized&&host.item_state().entities.size()==1,"wall blocks pickup without deleting entity");host.world(floor());check(host.item_action(id,command(9,items::wire::Action::pickup,held->slots[0].revision,placed.entity->key.id,1),326).code==items::ResultCode::replay,"failed authority permission consumes sequence");
 check(bool(host.item_action(id,command(10,items::wire::Action::pickup,held->slots[0].revision,placed.entity->key.id,1),327)),"new request succeeds after wall removal");
 check(host.configure_items(2,{65,65})&&host.item_state().scope.generation==2&&!host.configure_items(1,{64,64}),"host generation and runtime capacity");check(host.item_action(id,command(11,items::wire::Action::drop,1),328).code==items::ResultCode::scope,"old scene request rejected");
 std::cout<<"combat_items_test PASS: authoritative held ammo, stale revisions, drop/install/recover/use, weapon0 and collision gates\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
