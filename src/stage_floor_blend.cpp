#include "stage_floor_blend.h"
#include <windows.h>
#include <bcrypt.h>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <algorithm>
namespace mgo2mt::stage {
size_t apply_floor_blend(CharacterModel&m,std::span<const char>gwm,std::span<const char>b){
 auto require=[](bool v){if(!v)throw std::runtime_error("Invalid original floor blend");};
 require(b.size()>=52&&b.size()<=64*1024*1024&&!std::memcmp(b.data(),"GFB1",4)&&gwm.size()<=64*1024*1024);
 size_t at=4;auto word=[&](){require(at+4<=b.size());uint32_t v=0;for(unsigned k=0;k<4;++k)v|=uint32_t(uint8_t(b[at++]))<<(8*k);return v;};
 auto nv=word(),vc=word(),pc=word(),tc=word();require(nv==m.vertices.size()&&vc&&vc<=nv&&pc&&pc<=m.parts.size()&&tc&&tc<=512);
 std::array<unsigned char,32>digest{};require(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<char*>(gwm.data())),ULONG(gwm.size()),digest.data(),32)>=0);
 require(!std::memcmp(digest.data(),b.data()+20,32));at=52;
 struct Vertex{uint32_t index;std::array<float,4>uv;};std::vector<Vertex>vertices;std::vector<bool>present(nv);
 for(uint32_t i=0;i<vc;++i){Vertex v;v.index=word();require(v.index<nv&&(vertices.empty()||v.index>vertices.back().index));for(auto&f:v.uv){f=std::bit_cast<float>(word());require(std::isfinite(f)&&std::abs(f)<65536);}present[v.index]=true;vertices.push_back(v);}
 struct Part{uint32_t index,normal,blend;};std::vector<Part>parts;
 for(uint32_t i=0;i<pc;++i){auto index=word(),base=word(),normal=word(),blend=word();require(index<m.parts.size()&&(parts.empty()||index>parts.back().index)&&normal<tc&&blend<tc);
  const auto&p=m.parts[index];require(p.materialShader==0x130003&&p.texture==base&&!p.flags&&p.floorBlend==noMaterialTexture);
  require(uint64_t(p.first)+p.count<=m.indices.size());for(size_t j=p.first;j<size_t(p.first)+p.count;++j)require(m.indices[j]<nv&&present[m.indices[j]]);
  parts.push_back({index,normal,blend});
 }
 std::vector<ModelTexture>textures;for(uint32_t i=0;i<tc;++i){auto w=word(),h=word(),c=word(),size=word();require(w&&h&&w<=4096&&h<=4096&&(c==9||c==11)&&size==((w+3)/4)*((h+3)/4)*(c==9?8:16)&&at+size<=b.size());ModelTexture t{w,h,c,{}};t.pixels.assign(b.begin()+at,b.begin()+at+size);at+=size;textures.push_back(std::move(t));}require(at==b.size());
 // Everything is validated before mutation, including coverage of each part.
 const auto base=uint32_t(m.textures.size());m.textures.reserve(m.textures.size()+textures.size());
 for(auto&t:textures)m.textures.push_back(std::move(t));
 for(auto v:vertices){auto&dst=m.vertices[v.index];dst.u1=v.uv[0];dst.v1=v.uv[1];dst.u2=v.uv[2];dst.v2=v.uv[3];}
 for(auto p:parts){m.parts[p.index].floorNormal=base+p.normal;m.parts[p.index].floorBlend=base+p.blend;}
 return parts.size();
}
size_t load_floor_blend(CharacterModel&m,std::span<const char>gwm,const std::filesystem::path&p){
 if(!std::filesystem::exists(p))return 0;std::ifstream in(p,std::ios::binary|std::ios::ate);auto n=in.tellg();if(n<52||n>64*1024*1024)throw std::runtime_error("Floor blend extent");std::vector<char>b(static_cast<size_t>(n));in.seekg(0);if(!in.read(b.data(),n))throw std::runtime_error("Floor blend read");return apply_floor_blend(m,gwm,b);
}
}
