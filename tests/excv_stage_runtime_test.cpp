#include "stage_assets.h"
#include "stage_normals.h"
#include "stage_floor_blend.h"
#include "stage_surface_alpha.h"
#include "stage_object_sync.h"
#include "stage_water.h"
#include "stage_sky.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
namespace fs=std::filesystem;
namespace {
void check(bool yes,const char*why){if(!yes)throw std::runtime_error(why);}
std::vector<char> bytes(const fs::path&p){std::ifstream in(p,std::ios::binary);check(bool(in),"runtime resource readable");return {std::istreambuf_iterator<char>(in),{}};}
}
int main(int argc,char**argv){try{
 check(argc==4,"runtime root and converted group A/B stage roots required");
 size_t models=0,normals=0,alpha=0,blend=0,objects=0,skies=0;
 for(auto [name,id]:{std::pair{"n022a",20}, {"n001a",1}, {"n004a",4}, {"n007a",7}, {"n023a",21}}){
  const auto runtime=fs::path(argv[1])/name,converted=fs::path(argv[id==20||id==21?3:2])/name;
  const auto registry=stage::load_combat_object_registry(runtime/(std::string(name)+".objects.cfg"));
  check(registry.complete&&registry.map==id,"complete reviewed/native registry");
  auto bindings=stage::read_object_bindings(runtime,true,uint8_t(id));check(bindings.size()==registry.entries.size(),"original model/collision bindings load");objects+=bindings.size();
  std::ifstream water(runtime/(std::string(name)+".gww"),std::ios::binary);stage::Water::read(water);
  std::ifstream cbox(runtime/(std::string(name)+".cbox.cfg"));auto layout=stage::CboxLayout::read(cbox);check(layout.count==(id==20?15:0),"reviewed CBOX count");
  if(fs::exists(runtime/(std::string(name)+".sky.gwm"))){auto raw=bytes(runtime/(std::string(name)+".sky.gwm"));CharacterModel sky(raw,ModelExtent::sky);std::ifstream config(runtime/(std::string(name)+".sky.cfg"));auto settings=stage::SkySettings::read(config);settings.validate(sky);settings.prepare(sky);++skies;}
  for(const auto&e:fs::directory_iterator(runtime))if(e.path().extension()==".gws"){std::ifstream surface(e.path(),std::ios::binary);stage::WaterSurface::read(surface);}
  for(const auto&e:fs::directory_iterator(converted))if(e.path().extension()==".gwm"){
   auto raw=bytes(e.path());CharacterModel model(raw);auto vertices=model.vertices;auto indices=model.indices;
   auto apply=[&](const char*ext,auto function){auto path=runtime/e.path().filename();path.replace_extension(ext);if(!fs::exists(path))return size_t(0);auto companion=bytes(path);auto count=function(model,raw,companion);check(count>0,"nonempty companion applied");auto changed=raw;changed.back()^=1;bool rejected=false;try{function(model,changed,companion);}catch(const std::exception&){rejected=true;}check(rejected,"new GWM digest binding rejects changed resource");return count;};
   normals+=apply(".gwn",stage::apply_original_normals);alpha+=apply(".gsa",stage::apply_surface_alpha);blend+=apply(".gfb",stage::apply_floor_blend);
   check(model.vertices.size()==vertices.size()&&model.indices==indices,"companions preserve source topology");for(size_t i=0;i<vertices.size();++i)check(model.vertices[i].x==vertices[i].x&&model.vertices[i].y==vertices[i].y&&model.vertices[i].z==vertices[i].z,"companions preserve source position");++models;
  }
 }
 check(models>=5&&normals>0&&alpha>0&&blend>0&&objects==47&&skies==4,"five stage runtime coverage");
 std::cout<<"PASS EXCV 5 stages: "<<models<<" models, "<<normals<<" normals, "<<alpha<<" alpha parts, "<<blend<<" blend parts, 47 objects, 4 skies, water and CBOX\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
