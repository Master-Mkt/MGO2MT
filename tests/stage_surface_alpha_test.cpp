#include "stage_surface_alpha.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
namespace {
void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);}
std::vector<char> read(const char*path){std::ifstream f(path,std::ios::binary|std::ios::ate);check(bool(f),"source asset opens");auto n=f.tellg();check(n>0&&n<64*1024*1024,"source extent");std::vector<char>b(static_cast<size_t>(n));f.seekg(0);check(bool(f.read(b.data(),n)),"source read");return b;}
void word(std::vector<char>&b,size_t at,uint32_t value){for(unsigned k=0;k<4;++k)b.at(at+k)=char(value>>(8*k));}
}
int main(int argc,char**argv){try{
 check(argc==3,"GWM and GSA paths");auto gwm=read(argv[1]),gsa=read(argv[2]);CharacterModel model(gwm);auto beforeVertices=model.vertices.size(),beforeTextures=model.textures.size();
 check(stage::apply_surface_alpha(model,gwm,gsa)==775,"reviewed QQ775 part bindings");check(model.parts[514].surfaceAlpha==1&&model.parts[435].surfaceAlpha==1,"floor strip and bullet-hole alpha enabled");check(!model.parts[403].surfaceAlpha&&!model.parts[406].surfaceAlpha,"opaque two-layer terrain preserved");check(std::count_if(model.parts.begin(),model.parts.end(),[](auto&p){return p.surfaceAlpha==1;})==775&&model.vertices.size()==beforeVertices&&model.textures.size()==beforeTextures,"only selected part policy changes");
 for(auto&p:model.parts)p.surfaceAlpha=0;
 auto reject=[&](const std::vector<char>&bytes,const std::vector<char>&source){bool caught=false;try{stage::apply_surface_alpha(model,source,bytes);}catch(...){caught=true;}check(caught,"invalid sidecar refused");check(std::none_of(model.parts.begin(),model.parts.end(),[](auto&p){return p.surfaceAlpha!=0;}),"failure cannot partially bind earlier parts");};
 auto bad=gsa;bad[16]^=1;reject(bad,gwm);bad=gsa;word(bad,bad.size()-4,2);reject(bad,gwm);bad=gsa;word(bad,48,4096);reject(bad,gwm);bad=gsa;word(bad,48+16,0x130003);reject(bad,gwm);bad=gsa;bad.push_back(0);reject(bad,gwm);bad=gsa;bad.pop_back();reject(bad,gwm);auto changed=gwm;changed.back()^=1;reject(gsa,changed);
 auto texture=model.parts[514].texture;model.parts[514].texture=0;reject(gsa,gwm);model.parts[514].texture=texture;check(stage::apply_surface_alpha(model,gwm,gsa)==775,"valid mapping remains usable after failures");
 std::cout<<"QQ surface alpha775: original SHA, exact part/texture bounds, atomic rejection, floor and wall overlays PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
