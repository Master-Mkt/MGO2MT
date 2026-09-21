#include "combat_initial_profile.h"
#include "combat_service.h"
#include "combat_world.h"
#include "combat_spawn.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
using namespace mgo2mt::combat;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
int main(int argc,char**argv){try{
 check(argc==3,"catalog and original stage root required");
 auto profiles=initial_profiles(20,1,0);check(profiles.size()==43&&profiles.front().id==25&&profiles[1].id==3&&!profiles[1].heldOnly&&profiles[2].id==52&&profiles[2].nativeProjectile,"expanded profiles retain AK, sidearm and support default ordering");
 check(initial_profiles(20,1,2).empty()&&initial_profiles(20,2,0).empty()&&initial_profiles(19,1,0).empty()&&initial_profiles(20,1,1).empty(),"unsupported DP/rule/map/flags cannot borrow profile");
 const auto&w=profiles.front();check(w.magazine==30&&w.reserve==90&&w.damage==275&&w.shotCue==original::ak102_native_shot_cue&&!w.impactCue&&w.bodyCue==8168,"bounded native source profile and reviewed normal shot");
 auto catalog=std::make_shared<weapons::Catalog>();std::string error;check(catalog->load(argv[1],error),"original weapon catalog");
 // Actual packaged original world/spawn data, independently of the small hit
 // fixture below. Runtime uses this same complete-scene world/placement path.
 const auto root=std::filesystem::path(argv[2]);auto world=World::load(root);auto registry=stage::load_object_registry(root/"n022a.objects.cfg");
 stage::SceneSnapshot scene{{1,1,0,0,{20,1,0},host::MatchTransition::initial},1,{}};for(auto&e:registry.entries)scene.objects.push_back({e.bindingId,0,0});check(world.apply(scene),"complete original object state");
 std::ifstream spawnFile(root/"n022a.tdm-spawns.cfg");spawn::Selector selector(spawn::Profile::read(spawnFile),{1,16,false},{{1,2,3,4}});
 Authority originalWorld;originalWorld.begin(1,world.collision(),profiles,world.targets());
 for(uint8_t team=0;team<2;++team){auto p=selector.propose(spawn::Kind::initial,team);check(bool(p),"original initial spawn");auto landing=spawn::place(*p,*world.collision(),{});check(bool(landing),"original stage landing");check(originalWorld.join({uint8_t(team+1),1,uint32_t(team+100)},team+1,landing.pose,1000,1000,std::array<uint16_t,1>{25},0),"actual profile deploys on original stage");check(selector.commit(*p),"spawn committed");}
 auto floor=std::make_shared<const stage::Collision>(stage::Collision::make({{-20000,0,-20000},{20000,0,-20000},{20000,0,20000},{-20000,0,20000}},{{{0,1,2}},{{0,2,3}}}));
 Service host(1);host.configure(floor,profiles);std::array<Identity,2> ids{{{1,1,101},{2,1,102}}};
 std::array<Replica,2> clients;std::array<unsigned,2> shots{},deaths{};unsigned grants=0;uint32_t inputSequence=0;
 host.configure_round({1000,1,false,true},catalog,[&](Authority&a,Identity id,uint8_t team,std::span<const uint16_t> inventory,uint64_t now){check(inventory.size()==3&&inventory[0]==25&&inventory[1]==3&&inventory[2]==52,"grant contains fixed primary/secondary/support identities");bool ok=a.join(id,team,{{0,2,team==1?0.f:3000.f}},1000,1000,inventory,now);if(ok){auto held=a.item_held(id,1);check(held&&held->slots[0].contents.magazine==30&&held->slots[0].contents.reserve==90,"AK grant retains finite 120-round pool");for(unsigned slot=1;slot<3;++slot){const auto& c=held->slots[slot].contents;check(c.resource==items::Resource::ammunition&&c.quantity==1&&c.magazine==profiles[slot].magazine&&c.reserve==profiles[slot].reserve&&!c.charges,"sidearm and support keep their own finite ammunition");}}grants+=ok;return ok;});
 auto flush=[&]{for(auto&d:host.deliveries()){auto i=d.recipient.slot-1;auto value=wire::decode(d.payload);if(auto f=std::get_if<wire::Frame>(&value)){check(clients[i].snapshot(f->snapshot),"client accepts authoritative snapshot");for(auto&e:clients[i].events(f->events)){if(e.kind==EventKind::shot)check(e.cue==original::ak102_native_shot_cue,"both replicas receive reviewed normal shot cue");shots[i]+=e.kind==EventKind::shot;deaths[i]+=e.kind==EventKind::death;}check(clients[i].events(f->events).empty(),"repeated frame cannot replay effects");}}};
 auto command=[&](unsigned i,uint32_t seq,wire::CommandKind kind,uint64_t now){wire::Command c;c.epoch=1;c.sequence=seq;c.kind=kind;if(kind==wire::CommandKind::loaded){c.enabled=true;c.generation=1;c.sceneRevision=1;}else if(kind==wire::CommandKind::ready)c.enabled=true;else if(kind==wire::CommandKind::loadout)c.weapons={25,0,0};check(host.receive(ids[i],wire::encode(c),now),"round command");flush();};
 for(unsigned i=0;i<2;++i){check(host.admit(ids[i]),"admit");check(host.receive(ids[i],wire::encode(wire::Accept{1}),0),"explicit accept");}flush();
 for(unsigned i=0;i<2;++i){command(i,1,wire::CommandKind::loaded,1);command(i,2,wire::CommandKind::ready,2);}host.poll(2);flush();check(host.take_round_start(),"all-ready starts selection");
 for(unsigned i=0;i<2;++i)command(i,3,wire::CommandKind::loadout,3);check(grants==2,"one successful grant per participant");
 auto input=[&](uint64_t ns,bool fire,bool reload=false){wire::Input in{1,++inputSequence,{{0,2,0}},25,fire,reload,fire};check(host.receive(ids[0],wire::encode(in),ns/1000000),"participant input");host.poll(ns/1000000,uint32_t(ns%1000000));flush();};
 uint64_t ns=10000000;
 for(unsigned count=0;count<120;++count){
  if(count&&count%30==0){ns+=1000000;input(ns,false,true);const auto start=ns/1000000;auto p=host.authority().snapshot().players[1];check(p->reloadUntil==start+3487,"normal original reload end");
   ns=(start+2168)*1000000;input(ns,false);check(host.authority().snapshot().players[1]->ammo==0,"before refill remains empty");
   ns=(start+2169)*1000000;input(ns,false);p=host.authority().snapshot().players[1];check(p->ammo==30&&p->reserve==90-count,"finite refill at original event");
   ns=(start+3487)*1000000;input(ns,false);check(!host.authority().snapshot().players[1]->reloadUntil,"reload completion separate");
  }
  input(ns,true);check(host.authority().snapshot().players[1]->ammo==29-count%30,"one shot spends one cartridge");ns+=100100000;
 }
 input(ns,true);auto final=host.authority().snapshot();check(final.players[1]->ammo==0&&final.players[1]->reserve==0,"all120 rounds consumed; no unlimited fallback");
 check(!final.players[2]->alive&&final.players[2]->hp==0&&shots==std::array<unsigned,2>{120,120}&&deaths==std::array<unsigned,2>{1,1},"native base damage and effects shared exactly once");
 for(auto&c:clients)check(c.state()==final,"both native replicas agree on HP/ammo/reload");
 input(ns+1000000,false,true);check(!host.authority().snapshot().players[1]->reloadUntil,"empty reserve cannot reload");
 command(0,4,wire::CommandKind::loadout,ns/1000000+2);check(host.preparation(ids[0],ns/1000000+2)->error==wire::CommandError::already_deployed&&grants==2,"new loadout cannot refill");
 host.remove(ids[0]);flush();ids[0].instance=2;check(host.admit(ids[0],ns/1000000+3),"new incarnation admitted");check(host.receive(ids[0],wire::encode(wire::Accept{1}),ns/1000000+3),"reaccept");flush();command(0,1,wire::CommandKind::loaded,ns/1000000+4);command(0,2,wire::CommandKind::loadout,ns/1000000+5);
 check(host.preparation(ids[0],ns/1000000+5)->error==wire::CommandError::already_deployed&&grants==2&&!host.authority().snapshot().players[1],"reconnect does not replenish first-deployment inventory");
 std::cout<<"AK102 native scope: original stage/team spawns, READY/loadout,120 finite rounds, refill/end, base damage, two wire replicas, duplicate/reconnect grant rejection passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
