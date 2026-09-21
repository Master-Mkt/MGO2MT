#include "stage_sky.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
using namespace mgo2mt;
namespace {
void check(bool v,const char*why){if(!v)throw std::runtime_error(why);}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::runtime_error&){caught=true;}check(caught,"invalid sky rejected");}
std::vector<char> bytes(const std::filesystem::path&p){std::ifstream in(p,std::ios::binary);check(bool(in),"original sky asset readable");return {std::istreambuf_iterator<char>(in),{}};}
}
int main(int argc,char**argv){try{
 check(argc==2,"sky data directory required");auto root=std::filesystem::path(argv[1]);
 for(const char*name:{"n022a","n001a","n023a","n007a"}){
  auto raw=bytes(root/(std::string(name)+".sky.gwm"));CharacterModel model(raw,ModelExtent::sky);auto cfg=bytes(root/(std::string(name)+".sky.cfg"));std::string text(cfg.begin(),cfg.end());std::istringstream in(text);auto settings=stage::SkySettings::read(in);settings.validate(model);
  auto old=model.vertices;const auto bounds=model.bounds;settings.prepare(model);check(model.bounds==bounds&&model.vertices.size()==old.size(),"original sky positions remain unchanged");
  bool alpha0=false,alpha1=false;for(size_t i=0;i<old.size();++i){const auto&v=model.vertices[i];check(v.x==old[i].x&&v.y==old[i].y&&v.z==old[i].z&&v.u==old[i].u&&v.v==old[i].v&&v.aa==old[i].aa,"authored vertices and alpha preserved");alpha0|=v.aa<1;alpha1|=v.aa==1;if(!settings.uv2.empty())check(v.u2==settings.uv2[i][0]&&v.v2==settings.uv2[i][1],"third cloud layer gets authored UV2");}check(alpha0&&alpha1,"sky has authored crossfade weights and opaque second-layer weights");
  const auto start=settings.sample(0),quarter=settings.sample(settings.periodSeconds*.25),loop=settings.sample(settings.periodSeconds),huge=settings.sample(1e15),negative=settings.sample(-1),nan=settings.sample(std::numeric_limits<double>::quiet_NaN());
  check(start.position==settings.position&&start.degrees==loop.degrees&&negative.degrees==start.degrees&&nan.degrees==start.degrees,"clock reset/wrap and nonfinite input safe");check(std::abs(quarter.degrees[1]-float(settings.direction)*90)<.02f,"original period and direction");check(std::isfinite(huge.degrees[1])&&std::abs(huge.degrees[1])<=180,"long-running clock remains bounded");check(std::abs(quarter.cloudU+quarter.degrees[1]/360.f)<.0001f,"third layer scroll tracks original final yaw");
  auto bad=settings;bad.mdnSha256[0]=bad.mdnSha256[0]=='0'?'1':'0';rejects([&]{bad.validate(model);});bad=settings;if(!bad.uv2.empty()){bad.uv2.pop_back();rejects([&]{bad.validate(model);});}else{bad.uv2.push_back({0,0});rejects([&]{bad.validate(model);});}
  auto broken=model;broken.parts[0].original.textures[0].image=noMaterialTexture;rejects([&]{settings.validate(broken);});std::istringstream junk(text+"EXTRA");rejects([&]{stage::SkySettings::read(junk);});
 }
 check(!std::filesystem::exists(root/"n004a.sky.cfg"),"unproved stage sky binding is not fabricated");
 std::cout<<"PASS 4 GCX sky settings, source SHA/texture binding, original UV2/alpha preservation and rotation/cloud clock boundaries\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
