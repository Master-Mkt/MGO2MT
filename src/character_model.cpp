#include "character_model.h"
#include <cstring>
#include <cmath>
#include <stdexcept>
namespace mgo2win {
CharacterModel::CharacterModel(std::span<const char> b){
 auto require=[](bool ok){if(!ok)throw std::runtime_error("Invalid GWM1 character model");};
 require(b.size()>=48&&b.size()<=64*1024*1024&&!std::memcmp(b.data(),"GWM1",4));
 size_t at=4;auto word=[&](){require(at+4<=b.size());uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(uint8_t(b[at++]))<<(8*i);return v;};
 auto number=[&](){uint32_t u=word();float v;std::memcpy(&v,&u,4);require(std::isfinite(v)&&std::abs(v)<=1000000);return v;};
 require(word()==1);auto nv=word(),ni=word(),np=word(),nt=word();
 require(nv&&nv<=1000000&&ni&&ni<=3000000&&ni%3==0&&np&&np<=4096&&nt&&nt<=1024);
 require(uint64_t(nv)*32+uint64_t(ni)*4+uint64_t(np)*16+uint64_t(nt)*16<=b.size()-48);
 for(auto& x:bounds)x=number();for(int i=0;i<3;++i)require(bounds[i]<=bounds[i+3]);
 require(bounds[4]-bounds[1]>.001f);
 vertices.reserve(nv);
 for(uint32_t i=0;i<nv;++i){ModelVertex v{number(),number(),number(),number(),number(),number(),number(),number()};
  require(v.x>=bounds[0]&&v.x<=bounds[3]&&v.y>=bounds[1]&&v.y<=bounds[4]&&v.z>=bounds[2]&&v.z<=bounds[5]);
  float len=v.nx*v.nx+v.ny*v.ny+v.nz*v.nz;require(len>.5f&&len<1.5f);v.u1=v.u;v.v1=v.v;vertices.push_back(v);}
 indices.reserve(ni);for(uint32_t i=0;i<ni;++i){auto v=word();require(v<nv);indices.push_back(v);}
 uint32_t end=0;for(uint32_t i=0;i<np;++i){ModelPart p{word(),word(),word(),word()};require(p.first==end&&p.count&&p.count%3==0&&p.count<=ni-end&&p.texture<nt&&!p.flags);end+=p.count;parts.push_back(p);}require(end==ni);
 for(uint32_t i=0;i<nt;++i){ModelTexture t{word(),word(),word(),{}};auto size=word();require(t.width&&t.height&&t.width<=8192&&t.height<=8192&&(t.codec==9||t.codec==11));
  require(size==uint64_t((t.width+3)/4)*((t.height+3)/4)*(t.codec==9?8:16)&&size<=b.size()-at);
  t.pixels.assign(reinterpret_cast<const uint8_t*>(b.data()+at),reinterpret_cast<const uint8_t*>(b.data()+at+size));at+=size;textures.push_back(std::move(t));}
 require(at==b.size());
}
}
