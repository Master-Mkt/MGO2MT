#include "combat_cycle.h"
#include "gcx_round_items.h"
#include "weapon_restrictions.h"
#include "world_inventory_wire.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace mgo2mt;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F&& operation,const char* message){bool failed=false;try{operation();}catch(const std::exception&){failed=true;}check(failed,message);}
std::string bytes(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);check(bool(in),"fixture input opens");return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};}
void write(const std::filesystem::path& path,const std::string& text){std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<text;check(bool(out),"fixture write");}
void copy_fixture(const std::filesystem::path& source,const std::filesystem::path& destination){
 check(source!=destination&&!destination.empty(),"separate writable fixture");
 std::filesystem::create_directories(destination/"objects");
 for(const auto& file:std::filesystem::directory_iterator(source))if(file.is_regular_file()&&file.path().filename().string().starts_with("n022a")&&file.path().extension()==".cfg")
  std::filesystem::copy_file(file.path(),destination/file.path().filename(),std::filesystem::copy_options::overwrite_existing);
 for(const auto& file:std::filesystem::directory_iterator(source/"objects"))if(file.is_regular_file()&&file.path().extension()==".cfg")
  std::filesystem::copy_file(file.path(),destination/"objects"/file.path().filename(),std::filesystem::copy_options::overwrite_existing);
}
stage::SceneSnapshot scene(const combat::Cycle& cycle,uint8_t slot){
 check(cycle.objects()&&cycle.objects()->request(),"published original scene");
 stage::SceneReceiver receiver(cycle.objects()->registry(),slot);receiver.begin(cycle.request());
 auto packet=cycle.objects()->snapshot(slot);check(packet&&receiver.receive(cycle.request(),*packet)&&receiver.snapshot().has_value(),"late initial scene accepted");
 return *receiver.snapshot();
}
void snapshot_items(combat::Cycle& cycle,uint8_t slot){
 const auto original=cycle.service().authority().item_state();
 items::wire::Header recipient{original.scope,1000u+slot,{slot,uint16_t(100+slot),uint32_t(200+slot),1},0};
 auto pages=items::wire::pages(original,recipient);check(pages.has_value(),"concrete seeded item pages");
 items::wire::Receiver receiver;receiver.bind(recipient,original.capacity);
 for(auto it=pages->rbegin();it!=pages->rend();++it){auto packet=items::wire::encode(*it);check(packet&&receiver.receive(*packet),"reordered late item page accepted");}
 check(receiver.state()&&receiver.state()->entities==original.entities,"late join receives same IDs, contents and positions");
 auto old=items::wire::encode(pages->front());++recipient.scope.epoch;++recipient.scope.generation;
 receiver.bind(recipient,original.capacity);check(!receiver.receive(*old),"old item generation rejected after rebinding");
}
void start(combat::Cycle& cycle,combat::Identity id){
 check(cycle.admit(id,0),"initial participant admitted");
 check(cycle.service().receive(id,combat::wire::encode(combat::wire::Accept{cycle.epoch()}),0),"native offer accepted");
 combat::wire::Command command;command.epoch=cycle.epoch();command.sequence=1;command.kind=combat::wire::CommandKind::loaded;command.enabled=true;command.generation=cycle.request().generation;command.sceneRevision=cycle.world()->snapshot()->revision;
 check(cycle.service().receive(id,combat::wire::encode(command),0),"loaded scene command accepted");
 command={};command.epoch=cycle.epoch();command.sequence=2;command.kind=combat::wire::CommandKind::ready;command.enabled=true;
 check(cycle.service().receive(id,combat::wire::encode(command),1),"ready accepted");cycle.poll(1);
 check(cycle.service().take_round_start(),"one native round start");cycle.service().deliveries();
}
}

int main(int argc,char** argv){const char* phase="fixture";try{
 check(argc==6,"weapon catalog, item policies, real stage root, GCX sidecar and output arguments");
 auto catalog=std::make_shared<weapons::Catalog>();std::string error;check(catalog->load(argv[1],error),"reviewed weapon catalog");
 const auto source=std::filesystem::absolute(argv[3]);const auto fixture=std::filesystem::absolute(argv[5])/"gcx_cycle_fixture";
 copy_fixture(source,fixture);const auto sidecar=fixture/"n022a.gcx-items.cfg";const auto original=bytes(argv[4]);write(sidecar,original);
 struct Restore{std::filesystem::path path;std::string text;~Restore(){std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<text;}} restore{sidecar,original};
 combat::Cycle::Options options;options.stageRoot=fixture;options.rotations={{20,1,0}};options.capacity=3;options.round.roundDurationMs=1000;options.endedDisplayMs=0;
 check(options.itemFacts.load(argv[2],error),"reviewed item namespace policies");
 const auto random=[](){return combat::spawn::Random{{1,2,3,4}};};
 auto legacyOptions=options;legacyOptions.roundItems.useGcx=false;
 combat::Cycle legacy(legacyOptions,catalog,random);check(legacy.world()&&legacy.selector(),"original world/spawns available");
 const auto legacyScene=scene(legacy,0);const auto legacySolid=legacy.world()->collision()->triangles.size();
 for(uint8_t rule:{0,1}){
  phase=rule?"TDM default original pickups":"DM default original pickups";
  auto defaults=options;defaults.rotations[0].rule=rule;combat::Cycle cycle(defaults,catalog,random);
  auto state=cycle.service().authority().item_state();check(state.entities.size()==2,"DM/TDM default has two original pickup groups");
  check(std::count_if(state.entities.begin(),state.entities.end(),[](const auto&e){return e.contents.domain==items::Domain::equipment&&e.contents.item==22;})==1,"one original ENVG");
  check(std::count_if(state.entities.begin(),state.entities.end(),[](const auto&e){return e.contents.domain==items::Domain::equipment&&e.contents.item==10;})==1,"one original pickup item 10");
  check(scene(cycle,0).objects==legacyScene.objects,"defaults retain every original CBOX");snapshot_items(cycle,1);
  phase="combined GCX/manual overlap rejection";
  auto overlap=defaults;overlap.roundItems.enabled=true;
  overlap.roundItems.rules={{20,items::Domain::equipment,16,1,state.entities.front().position}};
  bool rejected=false;
  try{combat::Cycle bad(overlap,catalog,random);}catch(const std::runtime_error& failure){
   if(std::string(failure.what()).find("Combined GCX/manual round item placements overlap")==std::string::npos)throw;
   rejected=true;
  }
  check(rejected,"manual item at original pickup rejects whole candidate before seeding");
 }
 phase="HOST restrictions preserve namespaces and explicit edits";
 {
  auto locked=options;auto& bits=locked.round.restrictions;
  restrictions::set_locked(bits,*restrictions::find("drum"),true);
  restrictions::set_locked(bits,*restrictions::find("envg"),true);
  {combat::Cycle off(locked,catalog,random);check(off.service().authority().item_state().entities.size()==2,"disabled restriction master preserves original pickups");}
  restrictions::set_enabled(bits,true);
  {combat::Cycle none(locked,catalog,random);check(none.service().authority().item_state().entities.empty()&&scene(none,0).objects==legacyScene.objects,"prohibited defaults omitted without removing CBOX");}
  restrictions::set_locked(bits,*restrictions::find("envg"),false);
  {combat::Cycle one(locked,catalog,random);auto state=one.service().authority().item_state();check(state.entities.size()==1&&state.entities[0].contents.domain==items::Domain::equipment&&state.entities[0].contents.item==22,"DRUM bit106 does not incorrectly restrict equipment22");}
  locked.roundItems.replacements={{20,items::GcxSource::cbox,0,items::Domain::equipment,10}};
  rejects([&]{combat::Cycle bad(locked,catalog,random);},"explicit prohibited CBOX replacement rejects full candidate");
  locked.roundItems.replacements.clear();locked.roundItems.enabled=true;
  locked.roundItems.rules={{20,items::Domain::weapon,22,1,{}}};
  restrictions::set_locked(bits,*restrictions::find("patriot"),true);
  rejects([&]{combat::Cycle bad(locked,catalog,random);},"explicit prohibited manual weapon rejects full candidate");
  locked.roundItems.enabled=false;
  {combat::Cycle one(locked,catalog,random);check(one.service().authority().item_state().entities.size()==1,"weapon22 restriction is not equipment22 restriction");}
 }
 // Combine source defaults, all selected CBOX replacements and a non-drop
 // manual item in the one initial HOST batch. No drop permission is granted.
 options.roundItems.enabled=true;options.roundItems.rules={{20,items::Domain::equipment,16,1,{}}};
 options.roundItems.replacements={{20,items::GcxSource::cbox,0,items::Domain::weapon,1}};
 phase="all selected CBOX replacements plus manual item";
 combat::Cycle cycle(options,catalog,random);check(cycle.world()&&cycle.selector(),"replacement candidate published ready");
 const auto replaced=scene(cycle,0);auto itemState=cycle.service().authority().item_state();
 check(itemState.entities.size()==18,"2 pickups + selected 15 CBOX + one manual item in single batch");
 check(std::count_if(itemState.entities.begin(),itemState.entities.end(),[](const auto&e){return e.contents.domain==items::Domain::weapon&&e.contents.item==1;})==15,"non-drop knife replaces only selected CBOX");
 for(size_t i=0;i<replaced.objects.size();++i)check(i<17?replaced.objects[i]==legacyScene.objects[i]:replaced.objects[i].current==3&&replaced.objects[i].initial==3,"CBOX begins removed without old destruction effect");
 check(cycle.world()->collision()->triangles.size()<legacySolid,"original CBOX collision removed before item floor resolution");
 auto clientWorld=combat::World::load(fixture);check(clientWorld.apply(scene(cycle,2)),"late receiver applies same initial scene");
 check(clientWorld.collision()->triangles.size()==cycle.world()->collision()->triangles.size(),"HOST and late-client collision match");
 check(!cycle.service().authority().seed_items({}),"combined initial batch cannot be seeded twice");snapshot_items(cycle,2);
 check(cycle.object_deliveries().empty(),"initial removal is not replayed as later destruction deltas");
 for(const auto& entity:itemState.entities)check(entity.kind==items::PlacementKind::round&&entity.owner==items::Actor{},"neutral original/manual round ownership");
 // Publication is transactional across a next-round build failure, including
 // scene state, item identity, combat epoch and retained participant offers.
 const combat::Identity first{1,257,200},late{2,258,300};start(cycle,first);
 phase="late join and next-generation transaction";
 check(cycle.admit(late,2),"participant joins during current round");snapshot_items(cycle,2);
 check(cycle.service().preparation(late,2)&&!cycle.service().preparation(late,2)->players[2]->loaded,"late join must separately load the same scene");
 cycle.poll(1001);check(cycle.service().ended(),"native clock expires");cycle.service().deliveries();
 const auto beforeRequest=cycle.request();const auto beforeScene=scene(cycle,0);const auto beforeItems=cycle.service().authority().item_state();auto beforeWorld=cycle.world();
 write(sidecar,"MGO2MT.GCX_ROUND_ITEMS 1 20 verified\ngroups 1\ngroup 1 1434838 140 equipment 22 1\nanchor 5561776 6339210 900000 0 900000 0\n");
 rejects([&]{cycle.advance(1002);},"unsupported floor rejects the complete next candidate");
 check(cycle.epoch()==1&&cycle.request()==beforeRequest&&cycle.world()==beforeWorld&&scene(cycle,0)==beforeScene,"failed candidate never publishes removed boxes/new world");
 check(cycle.service().authority().item_state().entities==beforeItems.entities,"failed candidate preserves previous item IDs and positions");
 write(sidecar,original);check(cycle.advance(1002),"fixed source creates one fresh next generation");
 check(cycle.epoch()==2&&cycle.request().generation==2&&cycle.service().authority().item_state().scope==items::Scope{2,2},"scene/combat/items advance together");
 check(cycle.service().authority().item_state().entities.size()==18,"fresh generation regenerates one combined batch");
 check(cycle.service().preparation(first,1002)->players[1]&&cycle.service().preparation(late,1002)->players[2],"participants retained across successful round commit");
 check(!cycle.service().preparation(first,1002)->players[1]->loaded&&!cycle.service().preparation(late,1002)->players[2]->loaded,"new generation requires both clients to load again");snapshot_items(cycle,2);
 stage::SceneReceiver oldReceiver(cycle.objects()->registry(),1);oldReceiver.begin(beforeRequest);check(!oldReceiver.receive(cycle.request(),*cycle.objects()->snapshot(1)),"old scene identity cannot accept new generation");
 // Missing research stays distinct from an authored empty layout. Explicit
 // edits are never silently discarded while the source is unavailable.
 write(sidecar,"MGO2MT.GCX_ROUND_ITEMS 1 20 unavailable\ngroups 0\n");
 phase="unavailable and invalid candidate rejection";
 rejects([&]{combat::Cycle bad(options,catalog,random);},"unavailable source rejects explicit replacements");
 auto noEdits=options;noEdits.roundItems.replacements.clear();noEdits.roundItems.enabled=false;
 {combat::Cycle unavailable(noEdits,catalog,random);check(unavailable.world()&&unavailable.service().authority().item_state().entities.empty()&&scene(unavailable,0).objects==legacyScene.objects,"unavailable source retains original scene, not a fake verified layout");}
 write(sidecar,original);
 auto unknown=options;unknown.roundItems.replacements[0].sourceOffset=1;rejects([&]{combat::Cycle bad(unknown,catalog,random);},"unknown source offset rejects unpublished candidate");
 unknown=options;unknown.roundItems.replacements[0].item=65535;rejects([&]{combat::Cycle bad(unknown,catalog,random);},"unknown target metadata rejects unpublished candidate");
 auto overflow=options;overflow.roundItems.rules[0].count=4096;rejects([&]{combat::Cycle bad(overflow,catalog,random);},"combined 4096 limit includes both GCX and manual seeds");
 auto disabled=options;disabled.roundItems.useGcx=false;disabled.roundItems.enabled=false;combat::Cycle restored(disabled,catalog,random);
 check(scene(restored,0).objects==legacyScene.objects&&restored.world()->collision()->triangles.size()==legacySolid,"fresh round without GCX restores original CBOX visuals/collision");
 std::cout<<"GCX Cycle: DM/TDM original pickups, selected CBOX removal, combined non-drop seeds, late snapshots, new generation and atomic rejection PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<phase<<": "<<error.what()<<'\n';return 1;}}
