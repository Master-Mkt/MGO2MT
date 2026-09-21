#include "combat_attack_input.h"
#include "combat_action_presentation.h"
#include "installed_weapon_model.h"
#include "native_loadout.h"
#include <iostream>
using namespace mgo2mt::combat;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
bool invalid(const wire::Input&i){try{wire::encode(i);return false;}catch(const wire::Invalid&){return true;}}
int main(){try{
 for(uint16_t id:{1,2,3,4,7,8,15,18,20,23,24,25,26,30,31,35,37,38,39,41,42,43,44,50,52,53,54,55,56,57,58,59,63,64,65,66,67,69,73,128,129,130,131})check(mgo2mt::weapons::native_loadout::attack_supported(id)&&!mgo2mt::weapons::native_loadout::held_only(id),"client no longer suppresses equipped knife or expanded weapon controls");
 mgo2mt::CharacterModel model;model.bounds={-1,5,-2,1,7,2};mgo2mt::ModelVertex bottom{},top{};bottom.y=5;bottom.ny=1;bottom.u=.25f;top=bottom;top.y=7;model.vertices={bottom,top};
 for(auto normal:std::array<std::array<float,3>,5>{{{0,1,0},{1,0,0},{-1,0,0},{0,0,1},{0,0,-1}}}){auto placed=mgo2mt::installed_weapon_model(model,normal,.7f);const auto&a=placed.vertices[0],b=placed.vertices[1];check(std::abs(a.x)+std::abs(a.y)+std::abs(a.z)<.001f,"bottom stays at HOST anchor");check(std::abs(b.x-2*normal[0])+std::abs(b.y-2*normal[1])+std::abs(b.z-2*normal[2])<.001f&&a.u==.25f,"ground/wall placement preserves size and original UV");}
 for(uint16_t weapon:{1,3,25,50,52})for(bool raised:{false,true})for(bool firstPerson:{false,true}){
  wire::Input input;input.epoch=1;input.weapon=weapon;input.fire=input.firePressed=true;
  select_attack(input,raised,firstPerson,false,false);
  const bool punch=weapon!=1&&!raised&&!firstPerson;
  check(input.weapon==weapon&&input.meleePressed==punch&&input.firePressed!=punch&&input.fire!=punch,"knife only attacks while equipped; other unraised weapons punch");
 }
 wire::Input press;press.epoch=7;press.sequence=1;press.weapon=25;press.meleePressed=true;
 check(std::get<wire::Input>(wire::decode(wire::encode(press)))==press,"CQC edge round trip");
 auto next=press;next.sequence=2;next.meleePressed=false;next.pose.feet[0]=100;
 auto pending=wire::coalesce_input(press,next);check(pending.meleePressed&&pending.pose==next.pose&&pending.sequence==2,"pending punch preserves newest pose");
 next.fire=next.firePressed=next.aiming=true;pending=wire::coalesce_input(press,next);check(pending.meleePressed&&!pending.fire&&!pending.firePressed&&!pending.aiming&&!invalid(pending),"punch and shooting mutually exclusive");
 next={};next.epoch=7;next.sequence=2;next.weapon=25;next.reload=true;pending=wire::coalesce_input(press,next);check(pending.reload&&!pending.meleePressed&&!invalid(pending),"reload cancels pending punch");
 next.reload=false;next.suspended=true;check(!wire::coalesce_input(press,next).meleePressed,"focus loss drops pending punch");
 next.suspended=false;next.life=2;check(!wire::coalesce_input(press,next).meleePressed,"respawn drops old punch");
 next.life=1;next.weapon=1;check(!wire::coalesce_input(press,next).meleePressed,"equipping knife drops gun punch");
 next.firePressed=true;check(std::get<wire::Input>(wire::decode(wire::encode(next))).firePressed,"equipped knife retains attack edge");
 for(unsigned k=0;k<7;++k){auto bad=press;if(k==0)bad.fire=true;if(k==1)bad.reload=true;if(k==2)bad.aiming=true;if(k==3)bad.suspended=true;if(k==4)bad.specialHeld=true;if(k==5)bad.specialPressed=true;if(k==6)bad.firePressed=true;check(invalid(bad),"conflicting CQC flags rejected");}
 presentation::Actions actions;Snapshot snapshot;snapshot.epoch=8;Player actor;actor.identity={0,1,100};actor.life=1;actor.weapon=25;actor.alive=true;snapshot.players[0]=actor;
 Event hit;hit.epoch=8;hit.id=1;hit.kind=EventKind::melee;hit.source=actor.identity;hit.sourceLife=1;hit.weapon=25;
 actions.update(snapshot,std::span(&hit,1),100);check(actions.seconds(actor,true,100)==0&&actions.seconds(actor,false,100)<0,"accepted punch starts only CQC motion");
 actions.update(snapshot,std::span(&hit,1),200);check(actions.seconds(actor,true,200)>.09,"duplicate does not restart motion");
 snapshot.players[0]->weapon=1;actions.update(snapshot,{},300);snapshot.players[0]->weapon=25;actions.update(snapshot,std::span(&hit,1),400);check(actions.seconds(actor,true,400)<0,"weapon switch cannot replay old punch");
 snapshot.players[0]->life=2;hit.id=2;actions.update(snapshot,std::span(&hit,1),500);check(actions.seconds(*snapshot.players[0],true,500)<0,"old life motion rejected");
 hit.id=3;hit.sourceLife=2;hit.weapon=1;hit.kind=EventKind::shot;snapshot.players[0]->weapon=1;actions.update(snapshot,std::span(&hit,1),600);check(actions.seconds(*snapshot.players[0],false,600)==0&&actions.seconds(*snapshot.players[0],true,600)<0,"equipped knife starts dedicated attack motion");
 std::cout<<"CQC edge coalescing, scope, exclusive actions and equipped knife input/motion PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
