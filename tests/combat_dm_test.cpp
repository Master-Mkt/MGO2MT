#include "combat_round.h"
#include "combat_standings.h"
#include "combat_spawn_profile.h"
#include <fstream>
#include <iostream>
#include <set>
using namespace mgo2win;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(int argc,char**argv){try{
 check(argc==3,"catalog / spawn root");auto catalog=std::make_shared<weapons::Catalog>();std::string error;check(catalog->load(argv[1],error),"catalog");
 auto floor=std::make_shared<const stage::Collision>(stage::Collision::make({{-20000,0,-20000},{20000,0,-20000},{20000,0,20000},{-20000,0,20000}},{{{0,1,2}},{{0,2,3}}}));
 combat::Policy policy;policy.freeForAll=true;combat::Authority authority(policy);
 combat::Weapon ak{25,1000,0,100,300,30,90,10000,9001,9002,true};std::array profiles{ak};authority.begin(8,floor,profiles);
 combat::RoundCoordinator::Policy rules;rules.freeForAll=true;rules.autoAssign=false;rules.generation=3;rules.countdownMs=1000;rules.roundDurationMs=10000;rules.endOnTimeout=true;
 combat::RoundCoordinator round(8,rules,catalog,profiles,[](combat::Authority&a,combat::Identity id,uint8_t team,std::span<const uint16_t>w,uint64_t now){combat::Pose p;p.feet={0,2,float((id.slot-1)*3000)};return a.join(id,team,p,1000,1000,w,now);});
 combat::Identity a{1,10,100},b{2,11,200};check(round.join(a,0)&&round.join(b,0),"DM joins");
 for(auto id:{a,b}){auto p=round.state(id,0);check(p.freeForAll&&p.players[id.slot]->team==0&&p.autoAssign,"FFA no team selection");combat::wire::Command c;c.epoch=8;c.life=1;c.sequence=1;c.kind=combat::wire::CommandKind::loaded;c.enabled=true;c.generation=3;c.sceneRevision=1;check(round.command(id,c,authority,0),"DM loaded");c.sequence=2;c.kind=combat::wire::CommandKind::ready;c.generation=0;c.sceneRevision=0;check(round.command(id,c,authority,0)&&round.state(id,0).error==combat::wire::CommandError::none,"DM READY with team0");}
 round.poll(authority,0);check(round.take_start(),"DM starts");
 for(auto id:{a,b}){combat::wire::Command c;c.epoch=8;c.sequence=3;c.kind=combat::wire::CommandKind::loadout;c.weapons={25,0,0};check(round.command(id,c,authority,1)&&round.state(id,1).error==combat::wire::CommandError::none,"DM loadout");}
 auto shot=authority.fire(a,{8,1,25,{0,0,1},1},2);check(bool(shot)&&!authority.snapshot().players[2]->alive,"FFA damage despite team0 and friendlyFire off");round.poll(authority,3);
 const auto state=round.state(a,3);auto ranking=combat::standings(state);check(ranking.size()==2&&ranking[0].id==a&&ranking[0].kills==1&&ranking[0].rank==1&&ranking[1].deaths==1,"confirmed kills/deaths ranking");
 auto encoded=combat::wire::encode(state);check(std::get<combat::wire::Preparation>(combat::wire::decode(encoded))==state,"DM preparation versioned roundtrip");
 check(authority.fire(a,{8,1,25,{0,0,1},1},3).reject==combat::Reject::sequence,"duplicate kill request rejected");round.poll(authority,4);check(round.state(a,4).players[1]->kills==1,"no double score");
 auto tie=state;tie.players[2]->kills=1;auto tied=combat::standings(tie);check(tied[0].rank==1&&tied[1].rank==1,"equal kills share rank despite deaths");
 round.poll(authority,10001);check(round.state(a,10001).phase==combat::wire::RoundPhase::ended&&!authority.active(),"DM timeout ends combat");
 for(auto rule:{0,1}){std::ifstream input(std::filesystem::path(argv[2])/(rule?"n022a.tdm-spawns-v2.cfg":"n022a.dm-spawns.cfg"));auto profile=combat::spawn::StageProfile::read(input);for(uint8_t cap:{uint8_t(8),uint8_t(16)}){combat::spawn::StageSelector selector(profile,{1,cap,false,20,uint8_t(rule)},{{1,2,3,4}});std::set<uint8_t> order;unsigned count=rule?16:cap==8?16:32;for(unsigned i=0;i<count;++i){auto p=selector.propose(combat::spawn::Kind::initial,0);check(bool(p)&&p->arrayIndex<count,"permutation index");order.insert(p->arrayIndex);check(selector.commit(*p)&&!selector.commit(*p),"one commit");}check(order.size()==count,"every original initial entry used exactly once");auto resp=selector.propose(combat::spawn::Kind::respawn,0);check(bool(resp)&&resp->creationPose.feet[1]==resp->sourcePosition[1],"respawn original direct Y");}}
 // Exercise the actual per-stage GEOM-derived respawn groups, both teams and capacities.
 for(const auto& entry:std::filesystem::directory_iterator(argv[2]))if(entry.path().filename().string().ends_with(".tdm-spawns-v2.cfg")||entry.path().filename().string().ends_with(".dm-spawns.cfg")){
  std::ifstream input(entry.path());auto profile=combat::spawn::StageProfile::read(input);
  for(uint8_t cap:{uint8_t(8),uint8_t(16)})for(bool swap:{false,true})for(uint8_t team:{uint8_t(0),uint8_t(1)}){
   combat::spawn::StageSelector selector(profile,{1,cap,swap,profile.map(),profile.rule()},{{1,2,3,4}});std::set<unsigned> used;
   for(unsigned n=0;n<256;++n){auto p=selector.propose(combat::spawn::Kind::respawn,team);check(bool(p),"GEOM respawn proposal");
    const uint8_t expected=profile.rule()==0?0:team^uint8_t(swap);const auto& group=profile.group(cap<=8?combat::spawn::Variant::mini:combat::spawn::Variant::normal,combat::spawn::Kind::respawn,expected);
    check(p->normalizedTeam==expected&&p->sourcePosition==group.at(p->arrayIndex).position&&p->hash==group.at(p->arrayIndex).hash&&p->creationPose.feet==p->sourcePosition,"respawn selects original GEOM position from own team only");
    check(selector.propose(combat::spawn::Kind::respawn,team)==p,"failed/uncommitted proposal cannot consume RNG");used.insert(p->arrayIndex);check(selector.commit(*p),"commit random respawn");
   }check(used.size()>1,"repeated respawns vary original GEOM point");
  }
 }
 combat::wire::Input unarmed;unarmed.epoch=1;unarmed.sequence=1;check(std::get<combat::wire::Input>(combat::wire::decode(combat::wire::encode(unarmed)))==unarmed,"unarmed pose valid");bool rejected=false;unarmed.fire=true;try{combat::wire::encode(unarmed);}catch(...){rejected=true;}check(rejected,"unarmed fire invalid");
 std::cout<<"DM team0 lifecycle / confirmed individual scores / time expiry / original spawn groups / unarmed codec PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
