#include "stage_normals.h"
#include "stage_lighting.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <cstring>
#include <stdexcept>
using namespace mgo2mt;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
std::vector<char> read(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);check(bool(f),"input");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){try{
 check(stage::rsx_cmp_normal(0x001ff800)==std::array<float,3>{0,1,0},"original MDN floor word decodes UP");
 check(stage::rsx_cmp_normal(1024u<<11)==std::array<float,3>{0,-1,0},"signed Y11 minimum");
 check(stage::rsx_cmp_normal(1023)==std::array<float,3>{1,0,0},"positive X11");check(stage::rsx_cmp_normal(1024)==std::array<float,3>{-1,0,0},"negative X11");
 check(stage::rsx_cmp_normal(511u<<22)==std::array<float,3>{0,0,1},"positive Z10");check(stage::rsx_cmp_normal(512u<<22)==std::array<float,3>{0,0,-1},"negative Z10");
 check(argc==3,"data and sidecars required");size_t total=0;
 for(auto stageName:{"n001a","n004a","n007a","n022a","n023a"}){
  const std::string name=stageName;auto input=std::filesystem::path(argv[1])/(name=="n022a"?"outputs/handedness_floor_20260915/assets/fixtures/n022a.gwm":name=="n007a"?"work/stages/native/n007a.gwm":"outputs/multistage_20260913/native/"+name+".gwm");auto b=read(input),s=read(std::filesystem::path(argv[2])/(std::string(stageName)+".gwn"));CharacterModel model(b);auto before=model.vertices;
  auto count=stage::apply_original_normals(model,b,s);check(count==model.vertices.size(),"every source stage normal restored");total+=count;
  for(size_t i=0;i<before.size();++i){auto v=model.vertices[i];v.nx=before[i].nx;v.ny=before[i].ny;v.nz=before[i].nz;check(!std::memcmp(&v,&before[i],sizeof(v)),"no position/UV/authored color modification");}
  if(std::string(stageName)=="n022a"){
   const auto&v=model.vertices[62413];check(v.x==-45000&&v.y==0&&v.z==31125&&v.ny==1,"raw MDN indexed floor evidence");
   std::ifstream f(std::filesystem::path(argv[1])/"work/stages/native/n022a.lighting.cfg");auto l=stage::Lighting::read(f);auto corrected=l.sample({v.x,v.y,v.z},{v.nx,v.ny,v.nz}),old=l.sample({v.x,v.y,v.z},{before[62413].nx,before[62413].ny,before[62413].nz});check(corrected.color[0]>old.color[0]+.3f,"LT3 upward floor receives directional light");
   std::cout<<"QQ floor red illumination "<<old.color[0]<<" -> "<<corrected.color[0]<<'\n';
  }
  auto good=model.vertices;
  for(unsigned variant=0;variant<4;++variant){auto bad=s;auto gb=b;if(variant==0)bad.pop_back();if(variant==1)bad[12]^=1;if(variant==2)for(int i=0;i<4;++i)bad[44+i]=char(255);if(variant==3)gb.back()^=1;
   bool rejected=false;try{stage::apply_original_normals(model,gb,bad);}catch(const std::exception&){rejected=true;}check(rejected&&!std::memcmp(good.data(),model.vertices.data(),good.size()*sizeof(ModelVertex)),"invalid input rejects atomically");}
 }
 std::cout<<"Original normals PASS "<<total<<" vertices, 5 unchanged GWM files\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
