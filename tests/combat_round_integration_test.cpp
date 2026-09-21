#include "combat_service.h"
#include "combat_spawn.h"
#include "combat_world.h"
#include "dedicated_peer.h"
#include "host_session.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <numbers>
#include <set>
#include <stdexcept>
using namespace mgo2mt;
namespace {
namespace cw=combat::wire;
namespace spawn=combat::spawn;
void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);}
std::vector<uint8_t> player_profile(){std::vector<uint8_t> bytes(45);bytes[0]=2;bytes.insert(bytes.end(),{'T','e','s','t',0});return bytes;}
std::shared_ptr<const stage::Collision> small_world(){
 // Test-only geometric fixture: one floor and the surveyed B-side ceiling.
 // Spawn XYZ/yaw still come from the separately reviewed source profile.
 return std::make_shared<const stage::Collision>(stage::Collision::make(
  {{-200000,125,-200000},{-200000,125,200000},{200000,125,200000},{200000,125,-200000},
   {-14000,3375,39000},{-14000,3375,43750},{-7569.11f,3375,43750},{-7569.11f,3375,39000}},
  {{{0,1,2}},{{0,2,3}},{{4,5,6}},{{4,6,7}}}));
}
struct Network {
 combat::Service&service;
 host::Hello owner{100,0x12345678,2,1,{{{192,0,2,1},5732}}};
 std::array<host::Hello,2> hello{{{200,0xabcdef01,2,2,{{{192,0,2,2},5730}}},{300,0x11223344,2,2,{{{192,0,2,3},5730}}}}};
 std::array<host::Machine,2> client;
 std::array<host::DedicatedPeer,2> server;
 std::array<host::Player,2> roster{{{1,0x101,200,"TestA",{}},{2,0x102,300,"TestB",{}}}};
 std::array<combat::Identity,2> ids{{{1,0x101,200},{2,0x102,300}}};
 std::array<bool,2> admitted{},synced{};
 std::array<unsigned,2> acceptedCommands{},rejectedCommands{},shotEvents{};
 std::array<std::set<uint64_t>,2> eventIds;
 uint64_t now=0;unsigned sentClient=0,sentServer=0,dropped=0,starts=0,offers=0,accepts=0;
 explicit Network(combat::Service&s):service(s),client{{{hello[0],100,player_profile(),0},{hello[1],100,player_profile(),0}}},server{{{owner,hello[0],0},{owner,hello[1],0}}}{}
 void queue(unsigned i,std::vector<uint8_t> payload){check(server[i].queue(std::move(payload),now)!=0,"bounded dedicated reliable queue accepts fixture message");}
 void step(){
  for(unsigned i=0;i<2;++i){
   auto packets=client[i].poll(now);
   for(auto p=packets.rbegin();p!=packets.rend();++p){if(++sentClient%13==0){++dropped;continue;}server[i].receive(*p,now);server[i].receive(*p,now);}
   for(auto&e:server[i].events()){
    check(!e.empty(),"nonempty session event");
    if(e[0]==2&&!admitted[i]){
     admitted[i]=true;queue(i,host::roster_record({0,0x100,100,"Host",{}},owner));
     for(unsigned j=0;j<2;++j)if(admitted[j])queue(i,host::roster_record(roster[j],hello[j]));
     queue(i,{7,0,0,0,0,0,3});
     for(unsigned j=0;j<2;++j)if(i!=j&&admitted[j])queue(j,host::roster_record(roster[i],hello[i]));
    }else if(e[0]==10&&!synced[i]){
     synced[i]=true;queue(i,host::room_snapshot(std::array<host::Rotation,1>{{{20,1,0}}},7));check(service.admit(ids[i],now),"session admission precedes native offer");
    }else if(cw::recognized(e)){
     auto record=cw::decode(e);bool accepted=service.receive(ids[i],e,now);
     if(std::holds_alternative<cw::Accept>(record)){check(accepted,"v5 offer is explicitly accepted");++accepts;}
     if(std::holds_alternative<cw::Command>(record))(accepted?acceptedCommands[i]:rejectedCommands[i])++;
    }
   }
  }
  service.poll(now);if(service.take_round_start())++starts;
  for(auto&d:service.deliveries()){
   auto it=std::find(ids.begin(),ids.end(),d.recipient);check(it!=ids.end(),"delivery tied to admitted full identity");
   if(std::holds_alternative<cw::Offer>(cw::decode(d.payload))){check(d.payload[5]==cw::version,"wire offer version is GWCBv6");++offers;}
   queue(unsigned(it-ids.begin()),std::move(d.payload));
  }
  for(unsigned i=0;i<2;++i){
   auto packets=server[i].poll(now);
   for(auto p=packets.rbegin();p!=packets.rend();++p){if(++sentServer%11==0){++dropped;continue;}client[i].receive(*p,now);client[i].receive(*p,now);}
   for(auto&e:client[i].combat_events()){check(eventIds[i].insert(e.id).second,"duplicate datagrams never replay combat effects");if(e.kind==combat::EventKind::shot)++shotEvents[i];}
   check(!server[i].closed(),"fixture session remains open under bounded loss");
  }
  now+=10;
 }
 void until(const std::function<bool()>&predicate,const char*why,uint64_t timeout=5000){auto end=now+timeout;while(!predicate()&&now<end)step();check(predicate(),why);}
 void drain(uint64_t duration){auto end=now+duration;while(now<end)step();}
 cw::Preparation preparation(unsigned i)const{auto p=client[i].result().preparation;check(bool(p),"preparation received");return *p;}
 void send(unsigned i,const cw::Command&command){check(client[i].combat_command(command,now),"participant queues a native preparation command");}
 void ack(unsigned i,uint32_t sequence){until([&]{auto p=client[i].result().preparation;return p&&p->lastCommand==sequence;},"participant receives host command result");}
};
cw::Command command(uint32_t sequence,cw::CommandKind kind){cw::Command c;c.epoch=19;c.sequence=sequence;c.kind=kind;return c;}
}
int main(int argc,char**argv){try{
 check(argc==3||argc==4,"weapon catalog, original spawn profile, optional native stage root required");
 auto catalog=std::make_shared<weapons::Catalog>();std::string error;check(catalog->load(argv[1],error),"reviewed weapon price catalog loaded");
 const auto*pricedAk=catalog->find(weapons::Category::primary,25);
 check(catalog->initial_dp()==1000&&pricedAk&&pricedAk->dp_cost==1000,"DP ledger uses reviewed source price/floor");
 std::ifstream spawnFile(argv[2]);auto spawnProfile=spawn::Profile::read(spawnFile);
 auto geometry=small_world();std::shared_ptr<const stage::Collision> targets;
 if(argc==4){auto root=std::filesystem::path(argv[3]);auto real=combat::World::load(root);auto registry=stage::load_object_registry(root/"n022a.objects.cfg");stage::SceneSnapshot scene{{1,7,0,0,{20,1,0},host::MatchTransition::initial},1,{}};for(auto&e:registry.entries)scene.objects.push_back({e.bindingId,0,0});check(real.apply(scene),"optional original stage has complete 32-object host snapshot");geometry=real.collision();targets=real.targets();}
 // Synthetic damage/timing/ammo/SE values exist ONLY in this test. They are
 // deliberately not a shipped or recovered AK102 gameplay profile.
 combat::Weapon ak{25,35,0,100,500,7,11,500,9001,9002,true},m4=ak;m4.id=24;
 combat::Service service(19);service.configure(geometry,std::array{ak,m4},targets);
 // Explicit test RNG. This does not assign a meaning to original cache byte161.
 spawn::Selector selector(spawnProfile,{19,16,false},{{1,2,3,4}});
 bool temporarilyOccupied=true;unsigned attempts=0,grants=0;
 std::array<spawn::Proposal,2> grantedProposals{};std::array<combat::Pose,2> grantedPoses{};
 auto grant=[&](combat::Authority&a,combat::Identity id,uint8_t team,std::span<const uint16_t> inventory,uint64_t now){
  ++attempts;auto raw=spawn::raw_team(team);if(!raw)return false;auto p=selector.propose(spawn::Kind::initial,*raw);if(!p)return false;
  std::vector<combat::Pose> occupied;for(auto&player:a.snapshot().players)if(player&&player->alive)occupied.push_back(player->pose);
  if(temporarilyOccupied){auto landing=spawn::place(*p,*geometry,{});if(!landing)return false;occupied.push_back(landing.pose);}
  auto landing=spawn::place(*p,*geometry,occupied);if(!landing||!a.join(id,team,landing.pose,1000,1000,inventory,now))return false;
  check(selector.commit(*p),"host grant commits the same inspected proposal exactly once");grantedProposals[*raw]=*p;grantedPoses[*raw]=landing.pose;++grants;return true;
 };
 service.configure_round({60000,7,true,true},catalog,grant);Network net(service);
 net.until([&]{return net.client[0].result().preparation&&net.client[1].result().preparation;},"both encrypted clients finish offer/accept and receive initial preparation");
 check(net.offers==2&&net.accepts==2&&net.starts==0&&!service.authority().active(),"capability acceptance does not grant a spawn or start the round");
 for(unsigned i=0;i<2;++i){auto s=net.preparation(i);check(s.dpBalance==1000&&s.runtimeReady&&!s.players[net.ids[i].slot]->loaded,"host offers supported content and fresh ledger without claiming loaded");auto c=command(1,cw::CommandKind::loaded);c.enabled=true;c.generation=7;c.sceneRevision=1;net.send(i,c);}
 net.ack(0,1);net.ack(1,1);
 auto ready=command(2,cw::CommandKind::ready);ready.enabled=true;net.send(0,ready);net.ack(0,2);
 check(net.preparation(0).players[1]->ready&&net.preparation(0).phase==cw::RoundPhase::waiting,"one READY is not all-ready");
 auto cancel=command(3,cw::CommandKind::ready);cancel.enabled=false;net.send(0,cancel);net.ack(0,3);
 net.send(1,ready);net.ack(1,2);net.drain(300);
 check(!net.preparation(1).players[1]->ready&&net.preparation(1).players[2]->ready&&net.starts==0,"READY cancellation is visible to the other client and prevents early start");
 ready.sequence=4;net.send(0,ready);net.ack(0,4);
 net.until([&]{return net.preparation(0).phase==cw::RoundPhase::selecting&&net.preparation(1).phase==cw::RoundPhase::selecting;},"all-ready opens weapon selection for both peers");
 check(net.starts==1&&net.now<60000&&!service.authority().active(),"one round-start transition precedes weapon grant, not countdown expiry");
 auto choose=command(5,cw::CommandKind::loadout);choose.weapons={24,0,0};net.send(0,choose);net.ack(0,5);
 check(net.preparation(0).error==cw::CommandError::insufficient_dp&&attempts==0&&net.preparation(0).dpBalance==1000,"host rejects unaffordable weapon before spawning or charging");
 choose.sequence=6;choose.weapons={25,0,0};net.send(0,choose);net.ack(0,6);
 check(net.preparation(0).error==cw::CommandError::spawn&&net.preparation(0).dpBalance==1000&&grants==0&&selector.counters()==std::array<uint32_t,2>{0,0},"occupied original location rolls back counter and DP/ammunition grant");
 temporarilyOccupied=false;choose.sequence=7;net.send(0,choose);net.ack(0,7);
 auto second=command(3,cw::CommandKind::loadout);second.weapons={25,0,0};net.send(1,second);net.ack(1,3);
 net.until([&]{return net.client[0].result().combat_state==service.authority().snapshot()&&net.client[1].result().combat_state==service.authority().snapshot();},"both clients receive the same two-player host spawn snapshot");
 check(grants==2&&attempts==3&&selector.counters()==std::array<uint32_t,2>{1,1},"one successful original initial selection per team");
 auto before=service.authority().snapshot();
 for(unsigned i=0;i<2;++i){auto p=before.players[net.ids[i].slot];auto expected=spawnProfile.at(spawn::Variant::normal,spawn::Kind::initial,uint8_t(i),0);
  check(p&&p->team==i+1&&p->pose==grantedPoses[i]&&p->pose.feet[0]==expected.position[0]&&p->pose.feet[2]==expected.position[2]&&grantedProposals[i].hash==expected.hash,"snapshot carries original team spawn XZ with inspected landing");
  check(grantedProposals[i].variant==spawn::Variant::normal&&grantedProposals[i].arrayIndex==0&&std::abs(p->pose.yaw-float(expected.yaw)*2*std::numbers::pi_v<float>/65536)<0.00001f,"host configured capacity and first per-team counter select the original yaw and initial array");
  check(std::abs(p->pose.feet[1]-127)<0.1f&&p->pose.capsule.radius==260&&p->pose.capsule.height==1700,"original locations land on the inspected floor with native standard capsule");
  check(p->ammo==7&&p->reserve==11&&net.preparation(i).dpBalance==0&&net.preparation(i).players[net.ids[i].slot]->deployed,"host-approved loadout charges once and grants synthetic test inventory");
 }
 auto input=cw::Input{19,100,before.players[1]->pose,25,false,false,true};check(net.client[0].combat_input(input,net.now),"deployed participant sends trigger through negotiated protocol");
 net.until([&]{auto p=net.client[0].result().combat_state;return p&&p->players[1]&&p->players[1]->ammo==6&&net.shotEvents[1]==1;},"test shot spends one round and is replicated exactly once");
 auto immutable=service.authority().snapshot();auto rng=selector.random();auto attemptCount=attempts;auto rejected=net.rejectedCommands;
 net.send(0,choose);net.send(1,second);
 net.until([&]{return net.rejectedCommands[0]>rejected[0]&&net.rejectedCommands[1]>rejected[1];},"same-sequence loadouts are rejected after reliable application delivery");
 choose.sequence=8;net.send(0,choose);net.ack(0,8);
 check(net.preparation(0).error==cw::CommandError::already_deployed,"new-sequence repeated loadout cannot re-equip/refill a deployed player");
 net.drain(500);
 check(service.authority().snapshot()==immutable&&selector.counters()==std::array<uint32_t,2>{1,1}&&selector.random()==rng&&attempts==attemptCount&&grants==2,"duplicate loadout cannot change HP, ammunition, selector or RNG");
 for(unsigned i=0;i<2;++i)check(net.preparation(i).dpBalance==0&&net.client[i].result().combat_state==immutable&&net.shotEvents[i]==1,"both clients converge with no duplicate wallet charge/ammo reset/effect replay");
 check(net.dropped>0&&net.starts==1,"loss exercised and start remains one-time");
 std::cout<<"GWCBv6 round integration: 2 encrypted in-memory sessions; loaded, READY cancellation/re-ready, host DP approval, occupied-spawn rollback, 2 original team locations, snapshot convergence, post-shot duplicate loadout with unchanged DP/ammo/selector; dropped="<<net.dropped<<" world="<<(argc==4?"original":"synthetic")<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
