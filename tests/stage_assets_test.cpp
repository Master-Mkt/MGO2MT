#include "stage_assets.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <cstring>
#include <cmath>
using namespace mgo2win;
void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
host::LoadRequest request(uint8_t map,uint64_t sequence=1){return {sequence,uint8_t(sequence),0,0,{map,1,2},host::MatchTransition::initial};}
stage::Result wait(stage::Assets&a){auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(a.result().status==stage::Status::loading&&std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(std::chrono::milliseconds(1));return a.result();}
int main(int argc,char**argv){try{
 check(stage::name(6)=="n006a"&&stage::name(11)=="n018a"&&stage::name(15)=="n015a"&&stage::name(20)=="n022a"&&stage::name(22)=="n020a","updated routes, not base placeholders");check(stage::name(0).empty()&&stage::name(16).empty()&&stage::name(255).empty(),"unknown IDs do not become paths");
 auto folder=std::filesystem::temp_directory_path()/("mgo2win-stage-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(folder);
 {stage::Assets a(folder);a.select(request(0));check(a.result().status==stage::Status::unknown_map,"empty map");a.select(request(2));check(a.result().status==stage::Status::unavailable,"known but unprepared map");a.select(request(20));check(wait(a).status==stage::Status::unavailable,"missing asset");}
 auto file=folder/"n022a.gwm";{std::ofstream out(file,std::ios::binary);out<<"broken";}
 {stage::Assets a(folder);a.select(request(20));check(wait(a).status==stage::Status::invalid&&!a.result().model,"invalid asset cannot become ready");a.select(std::nullopt);check(a.result().status==stage::Status::idle&&!a.result().request,"clear after error");}
 std::vector<char>bytes{'G','W','M','1'};auto u=[&](uint32_t v){for(int i=0;i<4;++i)bytes.push_back(char(v>>(8*i)));};auto f=[&](float v){uint32_t bits;std::memcpy(&bits,&v,4);u(bits);};for(auto v:{1,3,3,1,1})u(v);for(float v:{0,0,0,1,1,0})f(v);for(auto xyz:{std::array<float,3>{0,0,0},{1,0,0},{0,1,0}}){for(auto x:xyz)f(x);for(float x:{0,0,1,0,0})f(x);}for(auto v:{0,1,2,0,3,0,0,4,4,9,8})u(v);for(int i=0;i<8;++i)bytes.push_back(0);{std::ofstream out(file,std::ios::binary);out.write(bytes.data(),bytes.size());}
 {stage::Assets a(folder);a.select(request(20));auto loaded=wait(a);check(loaded.status==stage::Status::preview_ready&&loaded.model&&loaded.model->vertices.size()==3,"valid geometry loads");a.reset();auto reset=wait(a);check(reset.status==stage::Status::preview_ready&&reset.generation>loaded.generation&&reset.model!=loaded.model,"F5 reloads a new generation");loaded=reset;a.select(request(20,2));check(a.result().model==loaded.model&&a.result().request->sequence==2,"round transition reuses immutable geometry");a.select(request(255,3));check(!a.result().model&&a.result().status==stage::Status::unknown_map,"map change clears stale preview");a.select(request(20,4));a.select(std::nullopt);std::this_thread::sleep_for(std::chrono::milliseconds(10));check(a.result().status==stage::Status::idle&&!a.result().model,"cancelled worker never resurrects geometry");}
 std::filesystem::create_directory(folder/"items");{std::ofstream out(folder/"items/113.gwm",std::ios::binary);out.write(bytes.data(),bytes.size());}
 {stage::Assets a(folder);a.select(request(20));auto original=wait(a);host::PlacementReceiver r;r.begin(1);r.receive(1,std::array<uint8_t,6>{0,0x64,2,1,24,0});r.receive(1,std::array<uint8_t,6>{1,113,0,1,0,0});r.receive(1,std::array<uint8_t,10>{2,10,0,20,0,30,0,0,64,0});r.receive(1,std::array<uint8_t,9>{3,0,0,0,0,0,0,0,0});a.receive(r.result());auto applied=a.result();check(applied.receivedModel&&applied.receivedModel->vertices.size()==6,"received item composed with original stage model");const auto&v=applied.receivedModel->vertices[4];check(std::abs(v.x-100)<.001f&&v.y==20&&std::abs(v.z-299)<.001f,"render vertices reproduce received translation and yaw");a.reset();check(a.result().received==applied.received&&a.result().generation==applied.generation,"participant F5 retains host placement without local randomization");a.select(request(20,2));check(!a.result().receivedModel&&!a.result().received,"round transition clears old composed actors");}
 // Synthetic active LT3 group checks the actual model relighting path.
 {std::ofstream light(folder/"n022a.lighting.cfg");light<<"MGO2WIN.STAGE_LIGHTS 3 0\n0 -1 0\n0 0 0\n0 0 0\n0 0 0\n0 1 0\n1 1 1\n-10 -10 -10 10 10 10\nPOINTS 1\n0 0 2 1 1 1 4 4 0 256 257 7 123 -10 -10 -10 10 10 10\n";}
 {stage::Assets a(folder);a.select(request(20));auto loaded=wait(a);check(loaded.model&&loaded.model->vertices[0].lr==.5f,"active original-style point reaches model lighting");host::ObjectStates states(2,{1});std::vector<stage::ObjectLightBinding>bindings{{0,123,{0,0,0},0}};check(!a.object_lights(request(20),states,bindings),"pending snapshot cannot change scene lighting");states.receive(std::array<uint8_t,3>{0xe0,2,1});check(a.object_lights(request(20),states,bindings)&&a.result().model->vertices[0].lr==0,"received broken snapshot re-bakes stage to darkness");host::ObjectStates restored(2,{1});restored.receive(std::array<uint8_t,3>{0xe0,2,0});check(a.object_lights(request(20),restored,bindings)&&a.result().model->vertices[0].lr==.5f,"authoritative intact snapshot restores original lighting");check(!a.object_lights(request(20,2),states,bindings),"stale request rejected by scene state adapter");}
 {stage::Assets a(folder);a.select(request(20));auto loaded=wait(a);
  host::ObjectStates mixed(2,{1,8,2});mixed.receive(std::array<uint8_t,4>{0xe0,2,3,5});
  check(!a.object_lights(request(20),mixed,{{0,123,{},0},{1,123,{},0}}),"bottle bitset cannot be interpreted as a one-bit light switch");
  check(a.result().model==loaded.model&&a.result().lighting==loaded.lighting,"invalid mixed bindings do not partially darken the scene");
  check(!a.object_lights(request(20),mixed,{{2,123,{},0}}),"CBOX state is not a one-bit light switch");
 }
 // A single authoritative revision updates geometry, contact and illumination.
 {std::ofstream collision(folder/"n022a.collision.cfg");collision<<"MGO2WIN.STAGE_COLLISION 1 3 1\n-100 -100 -100\n100 -100 -100\n0 -100 100\n0 1 2 0 0\n";}
 {stage::Assets a(folder),late(folder);a.select(request(20));late.select(request(20));auto base=wait(a);wait(late);
  auto part=std::make_shared<CharacterModel>(bytes);auto wall=std::make_shared<stage::Collision>(stage::Collision::make({{0,-2,-2},{0,2,-2},{0,0,2}},{{{0,1,2}}}));
  stage::ObjectBinding b;b.bindingId=900;b.width=1;b.position={5,0,0};b.parts.push_back({77,1,0,part,wall});b.lights.push_back({1,0,{123,~0u,true,{},0}});
  stage::SceneSnapshot intact{request(20),1,{{900,0,0}}};check(a.object_states(intact,{&b,1}),"complete intact scene");auto before=a.result();
  check(before.objectModel&&before.objectModel->vertices.size()==6&&before.objectModel->vertices[0].lr==.5f,"intact object and light publish together");
  auto hit=before.collision->ray({0,0,0},{1,0,0},10);check(hit&&before.collision->triangles[hit->triangle].object==77,"intact dynamic contact");
  auto broken=intact;broken.revision=2;broken.objects[0].current=1;check(a.object_states(broken,{&b,1}),"broken scene revision");auto after=a.result();
  check(after.objectModel->vertices.size()==3&&after.objectModel->vertices[0].lr==0&&!after.collision->ray({0,0,0},{1,0,0},10),"broken removes visual/contact and disables light atomically");
  check(before.objectModel->vertices.size()==6&&before.collision->ray({0,0,0},{1,0,0},10).has_value(),"published previous snapshot remains immutable");
  check(!a.object_states(intact,{&b,1}),"old scene revision cannot resurrect object");auto invalid=broken;invalid.revision=3;invalid.objects[0].initial=1;check(!a.object_states(invalid,{&b,1}),"same-round baseline cannot change");
  invalid=broken;invalid.revision=3;invalid.objects[0].current=2;check(!a.object_states(invalid,{&b,1})&&a.result().objectModel==after.objectModel,"invalid width rejected without partial mutation");
  auto join=broken;join.revision=1;join.objects[0].initial=1;check(late.object_states(join,{&b,1}),"late join accepts already broken baseline");check(late.result().objectModel->vertices.size()==after.objectModel->vertices.size()&&late.result().objectModel->vertices[0].lr==0,"late join agrees with existing receiver");
  a.reset();check(a.result().objectSnapshot==after.objectSnapshot&&a.result().collision==after.collision,"F5 preserves authoritative object state");
  a.select(request(20,2));auto next=wait(a);check(!next.objectSnapshot&&!next.objectModel&&next.collision->triangles.size()==1,"new round clears old actors atomically");
 }
 std::filesystem::remove(folder/"n022a.collision.cfg");
 std::filesystem::remove(folder/"n022a.lighting.cfg");
 {std::ofstream out(folder/"n022a.cbox.cfg");out<<"MGO2WIN.STAGE_CBOX 1 2 3\n100 5 0 0 0\n132 5 100 0 0\n164 5 200 0 0\n";}
 {stage::Assets a(folder),late(folder);auto first=request(20,100);first.generation=0;a.select(first);auto loaded=wait(a);check(loaded.cboxes.size()==2,"host generation zero selects native CBOX layout");auto arrival=request(20,999);arrival.generation=0;late.select(arrival);check(wait(late).cboxes==loaded.cboxes,"different client load sequence retains host selection");a.reset();auto reloaded=wait(a);check(reloaded.generation!=loaded.generation&&reloaded.cboxes==loaded.cboxes,"F5 reload never substitutes local generation for shared seed");auto next=request(20,101);next.generation=255;a.select(next);check(a.result().cboxes==loaded.cboxLayout->select(255),"cached stage reselects on host generation");a.select(std::nullopt);check(!a.result().cboxLayout&&a.result().cboxes.empty(),"exit clears selected CBOX identities");}
 {std::ofstream out(folder/"n022a.cbox.cfg");out<<"MGO2WIN.STAGE_CBOX 1 4 0\n";}
 {stage::Assets a(folder);a.select(request(20));auto invalid=wait(a);check(invalid.status==stage::Status::invalid&&!invalid.model&&!invalid.cboxLayout&&invalid.cboxes.empty(),"invalid selection never publishes partial scene");}
 std::filesystem::remove(folder/"n022a.cbox.cfg");
 std::filesystem::remove(folder/"items/113.gwm");std::filesystem::remove(folder/"items");
 std::filesystem::remove(file);std::filesystem::remove(folder);
 if(argc>1){stage::Assets a(argv[1]);a.select(request(20));auto loaded=wait(a);const bool restored=argc>2;check(loaded.status==stage::Status::preview_ready&&loaded.model&&loaded.model->vertices.size()==(restored?334183:100060)&&loaded.model->indices.size()==(restored?646803:192558),"original architecture conversion loads without original MDN runtime");if(restored){check(loaded.model->hasOverviewBounds&&loaded.model->vertices[0].lit==1&&loaded.collision&&loaded.collision->triangles.size()==145875,"restored lighting and collision are loaded with the textured model");a.select(request(20,2));check(a.result().collision==loaded.collision,"round transition reuses collision");a.select(std::nullopt);check(!a.result().collision,"exit clears collision");}}
 std::cout<<"stage routes, async geometry, missing/invalid data and cancellation passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
