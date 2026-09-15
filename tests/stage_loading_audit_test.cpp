#include "combat_world.h"
#include "stage_profiles.h"
#include <chrono>
#include <bit>
#include <fstream>
#include <iostream>
#include <thread>

using namespace mgo2win;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
stage::Result wait(stage::Assets& assets){
 const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(60);
 while(assets.result().status==stage::Status::loading&&std::chrono::steady_clock::now()<until)
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
 return assets.result();
}
void same(const stage::Collision& a,const stage::Collision& b){
 check(a.vertices==b.vertices&&a.triangles.size()==b.triangles.size()&&a.materials.size()==b.materials.size(),"HOST/client collision extents and all vertices match");
 for(size_t i=0;i<a.triangles.size();++i){const auto&x=a.triangles[i];const auto&y=b.triangles[i];
  check(x.vertices==y.vertices&&x.attribute==y.attribute&&x.polygonAttribute==y.polygonAttribute&&x.material==y.material&&x.object==y.object,"HOST/client every triangle and material binding match");}
 for(size_t i=0;i<a.materials.size();++i){const auto&x=a.materials[i];const auto&y=b.materials[i];
  check(x.id==y.id&&x.friction==y.friction&&x.restitution==y.restitution&&x.verified==y.verified&&x.resistance==y.resistance&&x.resistanceVerified==y.resistanceVerified,"HOST/client material values match");}
}
void compare(combat::World& world,stage::Assets& assets,const stage::SceneSnapshot& snapshot){
 check(world.apply(snapshot)&&assets.object_states(snapshot),"complete scene accepted at both endpoints");
 const auto r=assets.result();check(world.collision()&&r.collision&&world.targets(),"collision snapshots present");same(*world.collision(),*r.collision);
 if(r.objectHitCollision)same(*world.targets(),*r.objectHitCollision);
 else check(world.targets()->vertices.empty()&&world.targets()->triangles.empty(),"no client hit model is empty HOST target set");
}
void sky(const std::filesystem::path& source,const std::filesystem::path& output){
 // Tiny synthetic terrain: this tests loading/cache ownership,
 // not the original sky actor, visual shader, or original map placement.
 const auto unique=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
 const auto root=output/("sky-fixture-"+unique);std::filesystem::create_directories(root);
 std::vector<char> bytes{'G','W','M','1'};
 auto u=[&](uint32_t v){for(unsigned i=0;i<4;++i)bytes.push_back(char(v>>(8*i)));};
 auto f=[&](float v){u(std::bit_cast<uint32_t>(v));};
 for(uint32_t v:{1,3,3,1,1})u(v);for(float v:{0,0,0,1,1,0})f(v);
 for(auto xyz:{std::array<float,3>{0,0,0},{1,0,0},{0,1,0}}){for(float x:xyz)f(x);for(float x:{0,0,1,0,0})f(x);}
 for(uint32_t v:{0,1,2,0,3,0,0,4,4,9,8})u(v);for(unsigned i=0;i<8;++i)bytes.push_back(0);
 for(const char* stage:{"n001a","n022a"}){std::ofstream out(root/(std::string(stage)+".gwm"),std::ios::binary);out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));}
 stage::Assets assets(root);host::LoadRequest q{1,1,0,0,{20,1,0},host::MatchTransition::initial};
 assets.select(q);auto r=wait(assets);check(r.status==stage::Status::preview_ready&&r.model&&!r.skyModel,"optional absent sky allows terrain");
 assets.select(std::nullopt);check(!assets.result().skyModel&&!assets.result().model,"clear removes optional sky and terrain");
 std::filesystem::copy_file(source,root/"n022a.sky.gwm");assets.select(q);r=wait(assets);
 check(r.status==stage::Status::preview_ready&&r.skyModel&&!r.skyModel->parts.empty(),"valid sky loads");const auto saved=r.skyModel;
 ++q.sequence;++q.generation;q.transition=host::MatchTransition::next_round;assets.select(q);r=wait(assets);
 check(r.status==stage::Status::preview_ready&&r.skyModel==saved,"same map generation retains immutable sky");
 ++q.sequence;q.rotation.map=1;q.transition=host::MatchTransition::map_change;assets.select(q);r=wait(assets);
 check(r.status==stage::Status::preview_ready&&r.model&&!r.skyModel,"other map cannot borrow previous sky");
 {std::ofstream bad(root/"n022a.sky.gwm",std::ios::binary|std::ios::trunc);bad<<"broken";}
 ++q.sequence;q.rotation.map=20;assets.select(q);r=wait(assets);
 check(r.status==stage::Status::invalid&&!r.model&&!r.skyModel,"present corrupt sky rejects load atomically");
 assets.select(std::nullopt);r=assets.result();check(r.status==stage::Status::idle&&!r.request&&!r.model&&!r.skyModel,"clear after failed sky load is empty");
}
}
int main(int argc,char** argv){try{
 check(argc==4,"stage root, valid sky GWM, and audit output directory required");
 const std::filesystem::path root=argv[1],output=argv[3];std::filesystem::create_directories(output);
 std::ofstream report(output/"actual_load.csv");check(bool(report),"audit report writable");
 report<<"map,stage,model_vertices,model_indices,model_parts,model_textures,authored_vertices,authored_triangles,materials,registry_entries,native_static,intact_solid_triangles,intact_hit_triangles,active_components,cboxes\n";
 for(const auto& profile:stage::runtime_profiles){
  std::cout<<"loading "<<profile.stage<<std::endl;
  auto registry=stage::load_combat_object_registry(stage::asset_path(root,profile.map,".objects.cfg"));
  const auto bindings=stage::read_object_bindings(root,false,profile.map);
  check(registry.complete&&bindings.size()==registry.entries.size(),"explicit native registry matches all binding rows");
  for(const auto& b:bindings)for(const auto& p:b.parts)check(!p.model,"HOST binding load excludes model/texture data");
  auto world=combat::World::load(root,profile.map);check(!world.collision()&&!world.snapshot(),"HOST awaits complete initial scene");
  stage::Assets assets(root);host::LoadRequest q{1,7,0,0,{profile.map,1,0},host::MatchTransition::initial};assets.select(q);auto loaded=wait(assets);
  check(loaded.status==stage::Status::preview_ready&&loaded.model&&loaded.authoredCollision&&loaded.objectBindings,"all stage model/collision/bindings load");
  check(!loaded.objectSnapshot&&!loaded.objectHitCollision&&!loaded.objectModel,"no speculative initial dynamic snapshot");
  stage::SceneSnapshot s{q,1,{}};for(const auto&e:registry.entries)s.objects.push_back({e.bindingId,0,0});compare(world,assets,s);auto intact=assets.result();
  report<<unsigned(profile.map)<<','<<profile.stage<<','<<loaded.model->vertices.size()<<','<<loaded.model->indices.size()<<','<<loaded.model->parts.size()<<','<<loaded.model->textures.size()<<','<<loaded.authoredCollision->vertices.size()<<','<<loaded.authoredCollision->triangles.size()<<','<<loaded.authoredCollision->materials.size()<<','<<registry.entries.size()<<','<<registry.nativeStatic<<','<<intact.collision->triangles.size()<<','<<world.targets()->triangles.size()<<','<<intact.activeObjectComponents<<','<<intact.cboxes.size()<<'\n';report.flush();
  auto saved=world.collision();auto bad=s;bad.revision=0;check(!world.apply(bad)&&!assets.object_states(bad)&&world.collision()==saved,"zero revision cannot publish geometry");
  bad=s;bad.request.rotation.map=255;check(!world.apply(bad)&&!assets.object_states(bad),"unknown map cannot borrow geometry");
  bad=s;bad.objects.push_back({0xfffffffeu,0,0});++bad.revision;
  check(!world.apply(bad)&&!assets.object_states(bad)&&world.collision()==saved&&assets.result().objectSnapshot==s,"unknown extra binding cannot publish even a partial matching scene");
  if(profile.map==20){
   check(!registry.nativeStatic&&registry.entries.size()==32&&intact.cboxes.size()==15,"n022a reviewed success32 with 15 CBOX placements");
   check(loaded.authoredCollision->triangles.size()==145875&&world.targets()->triangles.size()==528,"n022a source static/hit extents");
   const auto solid=intact.collision->triangles.size();s.objects[10].current=1;++s.revision;compare(world,assets,s);
   check(world.collision()->triangles.size()+46==solid&&world.targets()->triangles.size()==456,"drum loss removes original solid46/hit72");
   s.objects[12].current=1;++s.revision;compare(world,assets,s);check(world.targets()->triangles.size()==444,"bottle loss removes one hit box12");
   s.objects[17].current=3;++s.revision;compare(world,assets,s);check(world.collision()->triangles.size()+66==solid,"CBOX final loss removes original20");
   s.objects[0].current=1;++s.revision;compare(world,assets,s);check(world.collision()->triangles.size()+66==solid,"car changes draw state while solid persists");
   check(intact.collision->triangles.size()==solid&&intact.objectHitCollision->triangles.size()==528,"published prior snapshot stays immutable");
   saved=world.collision();bad=s;bad.objects.pop_back();++bad.revision;check(!world.apply(bad)&&!assets.object_states(bad)&&world.collision()==saved,"partial registry cannot mutate world");
  }else if(profile.map==7){
   check(registry.nativeLights&&!registry.nativeStatic&&registry.entries.size()==15&&bindings.size()==15,"BB native registry covers fifteen original lamp placements");
   check(intact.objectHitCollision&&world.targets()->triangles.size()==180,"BB fifteen intact original target boxes");
   s.objects[0].current=1;++s.revision;compare(world,assets,s);
   check(world.targets()->triangles.size()==168,"broken BB lamp loses only its own twelve target triangles");
   check(intact.objectHitCollision->triangles.size()==180,"published intact BB hit snapshot immutable");
  }else{
   check(registry.nativeStatic&&registry.entries.empty()&&bindings.empty()&&intact.activeObjectComponents==0&&intact.cboxes.empty(),"other three stages deliberately have no recovered dynamic actor bindings");
   same(*loaded.authoredCollision,*world.collision());bad=s;bad.objects.push_back({1,1,0});++bad.revision;
   check(!world.apply(bad)&&!assets.object_states(bad),"static-only HOST and client reject invented dynamic actor");
  }
  // A new complete generation restores its own initial state, and stale client
  // snapshots cannot put previous-round damage back into the new scene.
  const auto old=s;++q.sequence;++q.generation;++q.round;q.transition=host::MatchTransition::next_round;assets.select(q);loaded=wait(assets);
  check(loaded.status==stage::Status::preview_ready&&!loaded.objectSnapshot&&!loaded.objectHitCollision,"next generation clears dynamic state");
  check(!assets.object_states(old),"old generation cannot revive old object state");s={q,1,{}};for(const auto&e:registry.entries)s.objects.push_back({e.bindingId,0,0});compare(world,assets,s);
  if(profile.map==20)check(world.targets()->triangles.size()==528,"new generation restores intact targets");
  // Normal DM uses the same explicit static/dynamic profile, not arbitrary rules.
  ++q.sequence;++q.generation;q.rotation.rule=0;assets.select(q);check(wait(assets).status==stage::Status::preview_ready,"DM stage loads");s.request=q;compare(world,assets,s);
  bad=s;bad.request.rotation.rule=2;check(!world.apply(bad),"unreviewed rule cannot borrow native combat registry");
  assets.select(std::nullopt);check(!assets.result().model&&!assets.result().skyModel&&!assets.result().collision&&!assets.result().objectSnapshot,"full scene clear retains no map payload");
 }
 sky(argv[2],output);std::cout<<"5 actual stage Assets/World audits and optional sky ownership boundaries PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
