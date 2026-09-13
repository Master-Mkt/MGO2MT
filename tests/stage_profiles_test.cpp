#include "stage_profiles.h"
#include "combat_world.h"
#undef NDEBUG
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
using namespace mgo2win;
int main(int argc,char**argv){
 for(unsigned i=0;i<256;++i)assert(stage::runtime_stage_supported(uint8_t(i))==(i==1||i==4||i==20||i==21));
 assert(stage::asset_path("data/stages",1,".gwm")==std::filesystem::path("data/stages/n001a.gwm"));
 assert(stage::asset_path("data/stages",20,".objects.cfg")==std::filesystem::path("data/stages/n022a.objects.cfg"));
 for(auto suffix:{"", "../other", ".gwm/other", ".GWM"}){bool caught=false;try{stage::asset_path("x",20,suffix);}catch(const std::invalid_argument&){caught=true;}assert(caught);}
 bool caught=false;try{stage::asset_path("x",255,".gwm");}catch(const std::invalid_argument&){caught=true;}assert(caught);
 if(argc>1){
  const auto check=[](bool v,const char*why){if(!v)throw std::runtime_error(why);};
  stage::Assets assets(argv[1]);
  for(uint8_t map:{1,4,21}){
   auto registry=stage::load_object_registry(stage::asset_path(argv[1],map,".objects.cfg"));
   check(registry.complete&&registry.nativeStatic&&registry.entries.empty()&&registry.map==map,"Native static registry distinct from original actors");
   auto world=combat::World::load(argv[1],map);stage::SceneAuthority authority(registry);
   for(uint8_t rule:{0,1}){
    host::LoadRequest request{uint64_t(map)*2+rule,uint8_t(2+rule),0,0,{map,rule,0},host::MatchTransition::initial};
    authority.begin(request);check(authority.request()==request,"DM/TDM native scene admitted");
    auto wire=authority.snapshot(0);check(wire&&wire->size()==2,"Explicit empty scene snapshot");
    assets.select(request);auto until=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    while(assets.result().status==stage::Status::loading&&std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds(5));
    auto loaded=assets.result();check(loaded.status==stage::Status::preview_ready&&loaded.model&&loaded.collision&&loaded.lighting,"Original model terrain lighting loaded");
    stage::SceneSnapshot snapshot{request,1,{}};check(world.apply(snapshot)&&assets.object_states(snapshot),"Host and client use same native scene");
    auto current=assets.result();check(!world.collision()->triangles.empty()&&world.collision()->vertices==current.collision->vertices&&world.collision()->triangles.size()==current.collision->triangles.size(),"Empty actors preserve world terrain");
    auto bad=snapshot;bad.request.rotation.rule=3;check(!world.apply(bad),"Unreviewed rule rejected");authority.begin(bad.request);check(!authority.request(),"Unreviewed authority rule rejected");
    bad=snapshot;bad.request.rotation.map=20;check(!world.apply(bad),"Wrong stage rejected");bad=snapshot;bad.objects.push_back({1,0,0});check(!world.apply(bad),"Invented actor rejected");
    std::cout<<"map="<<unsigned(map)<<" rule="<<unsigned(rule)<<" vertices="<<loaded.model->vertices.size()<<" collision="<<world.collision()->triangles.size()<<" hemispheres="<<loaded.lighting->hemispheres.size()<<" points="<<loaded.lighting->points.size()<<'\n';
   }
  }
 }
}
