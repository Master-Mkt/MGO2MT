#include "item_drop_physics.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;using namespace mgo2mt::items;
namespace {void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}stage::Collision floor(float y=0){return stage::Collision::make({{-10000,y,-10000},{10000,y,-10000},{10000,y,10000},{-10000,y,10000}},{{{0,1,2}},{{0,2,3}}});}}
int main(){try{
 Scope scope{1,1};Actor owner{0,1,10,1},other{1,1,20,1};WorldInventory inv({4,1});check(inv.reset(scope)&&inv.admit(owner)&&inv.admit(other),"admit");HeldSlot held{{25,1,30,90,0,Resource::ammunition},1};DropPolicy allow;allow.drop=DropOverride::allow;
 auto drop=inv.drop({scope,owner,1,true},held,1,{0,600,0,0},allow);check(bool(drop),"drop");auto world=floor();DropPhysics physics;check(physics.clear({0,300,0,0},drop.entity->contents,&world,nullptr),"finite raised box clear");check(!physics.clear({0,-10,0,0},drop.entity->contents,&world,nullptr),"box floor overlap rejected");
 physics.advance(inv.state(),&world,nullptr,0,true);check(!physics.contact(*drop.entity,owner,{0,0,0},{},0),"owner cannot immediately reclaim");check(!physics.contact(*drop.entity,other,{0,0,0},{},0),"other grace");
 float previous=600;bool fell=false,bounced=false;
 for(uint64_t now=16;now<=4000;now+=16){auto moves=physics.advance(inv.state(),&world,nullptr,now,true);check(inv.move(scope,moves),"atomic motion");float y=inv.state().entities.front().position.y;check(y>=-.1f,"never crosses floor");fell|=y<previous;bounced|=y>previous+.01f;previous=y;}
 auto landed=inv.state().entities.front();check(fell&&bounced&&physics.grounded(landed.key)&&landed.position.y<1,"gravity bounce settle with correct bottom");check(!physics.contact(landed,owner,{0,0,0},{},4000),"owner still touching after grace blocked");check(!physics.contact(landed,owner,{2000,0,0},{},4000),"owner leaves");check(physics.contact(landed,owner,{0,0,0},{},4000),"owner recontact allowed");check(physics.contact(landed,other,{0,0,0},{},4000),"actual other contact");check(!physics.contact(landed,other,{1400,0,0},{},4000),"manual 1500 range is not contact");
 auto before=inv.state();auto wrong=Movement{landed.key,landed.revision+1,{0,10,0,0}};check(!inv.move(scope,std::span<const Movement>(&wrong,1))&&inv.state().entities==before.entities,"stale motion atomic");
 HeldSlot destination;check(bool(inv.contact_pickup(other,landed.key,landed.revision,destination,1)),"HOST contact transfer");check(destination.contents.item==25&&destination.contents.magazine==30&&destination.contents.reserve==90,"all ammunition conserved");
 // Autonomous transfer did not steal the next client sequence.
 check(bool(inv.drop({scope,other,1,true},destination,destination.revision,{0,600,0,0},allow)),"client request sequence untouched");
 physics.advance(inv.state(),&world,nullptr,4000,true);auto paused=inv.state();check(physics.advance(paused,&world,nullptr,8000,false).empty(),"pause does not fall");auto resumed=physics.advance(paused,&world,nullptr,8016,true);check(resumed.size()==1&&resumed.front().position.y>590,"pause has no catchup");check(physics.advance(paused,&world,nullptr,7000,true).empty(),"backward clock no motion");
 auto wall=stage::Collision::make({{100,-1000,-1000},{100,2000,-1000},{100,2000,1000},{100,-1000,1000}},{{{0,1,2}},{{0,2,3}}});check(!physics.clear({0,300,0,0},landed.contents,&world,&wall),"finite box catches wall even center ray misses");
 auto next=inv.state();next.scope={2,1};next.entities.clear();physics.advance(next,&world,nullptr,0,true);check(physics.size()==0&&!physics.contact(landed,owner,{0,0,0},{},9000),"epoch reset rejects old entities");
 WorldInventory seed({1,0});check(seed.reset({3,1}),"seed scope");Seed s{{16,1,0,0,0,Resource::durable,Domain::equipment},{0,2,0,0}};check(seed.seed({3,1},std::span<const Seed>(&s,1)),"neutral nondrop seed");DropPhysics initial;auto seeded=seed.state();initial.advance(seeded,&world,nullptr,0,true);check(initial.contact(seeded.entities[0],other,{0,0,0},{},0),"round seed contact no owner or drop permission requirement");
 std::cout<<"finite-box floor/wall, bounce, settle, grace/recontact, atomic inventory/replay, pause/epoch/seed PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
