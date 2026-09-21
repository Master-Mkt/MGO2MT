#include "character_model.h"
#include <cstring>
#include <cmath>
#include <stdexcept>
namespace mgo2mt {
CharacterModel::CharacterModel(std::span<const char> b,ModelExtent extent){
 auto require=[](bool ok){if(!ok)throw std::runtime_error("Invalid GWM1 character model");};
 require(b.size()>=48&&b.size()<=64*1024*1024&&!std::memcmp(b.data(),"GWM1",4));
 size_t at=4;auto word=[&](){require(at+4<=b.size());uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(uint8_t(b[at++]))<<(8*i);return v;};
 // Only sky positions may exceed the ordinary model coordinate envelope.
 auto number=[&](bool position=false){uint32_t u=word();float v;std::memcpy(&v,&u,4);require(std::isfinite(v)&&std::abs(v)<=(position&&extent==ModelExtent::sky?4000000:1000000));return v;};
 auto version=word();require(version>=1&&version<=3);auto nv=word(),ni=word(),np=word(),nt=word();
 require(nv&&nv<=1000000&&ni&&ni<=3000000&&ni%3==0&&np&&np<=4096&&nt&&nt<=1024);
 require(uint64_t(nv)*(version==3?56:version==2?48:32)+uint64_t(ni)*4+uint64_t(np)*(version>=2?32:16)+uint64_t(nt)*16<=b.size()-48);
 for(auto& x:bounds)x=number(true);for(int i=0;i<3;++i)require(bounds[i]<=bounds[i+3]);
 require(bounds[4]-bounds[1]>.001f);
 vertices.reserve(nv);
 for(uint32_t i=0;i<nv;++i){ModelVertex v{number(true),number(true),number(true),number(),number(),number(),number(),number()};
  if(version>=2){v.ar=number();v.ag=number();v.ab=number();v.aa=number();require(v.ar>=0&&v.ar<=1&&v.ag>=0&&v.ag<=1&&v.ab>=0&&v.ab<=1&&v.aa>=0&&v.aa<=1);}
  v.u1=version==3?number():v.u;v.v1=version==3?number():v.v;
  require(v.x>=bounds[0]&&v.x<=bounds[3]&&v.y>=bounds[1]&&v.y<=bounds[4]&&v.z>=bounds[2]&&v.z<=bounds[5]);
  float len=v.nx*v.nx+v.ny*v.ny+v.nz*v.nz;require(len>.5f&&len<1.5f);vertices.push_back(v);}
 indices.reserve(ni);for(uint32_t i=0;i<ni;++i){auto v=word();require(v<nv);indices.push_back(v);}
 uint32_t end=0;for(uint32_t i=0;i<np;++i){ModelPart p{word(),word(),word(),word()};if(version>=2){p.materialShader=word();for(auto&v:p.tint){v=number();require(v>=0&&v<=16);}}require(p.first==end&&p.count&&p.count%3==0&&p.count<=ni-end&&p.texture<nt&&!p.flags);end+=p.count;parts.push_back(std::move(p));}require(end==ni);
 for(uint32_t i=0;i<nt;++i){ModelTexture t{word(),word(),word(),{}};auto size=word();require(t.width&&t.height&&t.width<=8192&&t.height<=8192&&(t.codec==9||t.codec==11));
  require(size==uint64_t((t.width+3)/4)*((t.height+3)/4)*(t.codec==9?8:16)&&size<=b.size()-at);
  t.pixels.assign(reinterpret_cast<const uint8_t*>(b.data()+at),reinterpret_cast<const uint8_t*>(b.data()+at+size));at+=size;textures.push_back(std::move(t));}
 if(version==3){
  require(at+8<=b.size()&&!std::memcmp(b.data()+at,"MAT3",4));at+=4;require(word()==np);
  auto be=[](const auto&raw,size_t offset){uint32_t v=0;for(unsigned i=0;i<4;++i)v=(v<<8)|raw[offset+i];return v;};
  for(auto&part:parts){
   auto size=word();require(size<=1024*1024&&size<=b.size()-at);auto finish=at+size;
   auto raw=[&](auto&dest){require(at<=finish&&dest.size()<=finish-at);std::memcpy(dest.data(),b.data()+at,dest.size());at+=dest.size();};
   auto field=[&](){require(at<=finish&&finish-at>=4);return word();};
   auto string=[&](){auto n=field();require(n<=65536&&n<=finish-at);std::string value(b.data()+at,n);require(value.find('\0')==std::string::npos);at+=n;return value;};
   auto&o=part.original;o.present=true;raw(o.raw);raw(o.vertexDeclaration);
   o.key=be(o.raw,0);o.nameHash=be(o.raw,4);o.parameterCount=be(o.raw,12);
   require(o.key==part.materialShader&&o.parameterCount<=8&&be(o.raw,8)<=8);
   o.sourceIndex=field();o.requestedRules=field();o.normalSlot=field();o.extraNormalSlot=field();o.reflectionSlot=field();o.paletteSlot=field();
   o.mdnPath=string();o.mdnSha256=string();o.packagePath=string();o.packageSha256=string();o.vertexProgramSha256=string();o.fragmentProgramSha256=string();o.ruleId=string();o.fallbackReason=string();
   auto sha=[](const std::string&s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;};
   require(!o.mdnPath.empty()&&sha(o.mdnSha256));
   for(const auto*s:{&o.packageSha256,&o.vertexProgramSha256,&o.fragmentProgramSha256})require(s->empty()||sha(*s));
   for(unsigned i=0;i<8;++i)for(unsigned j=0;j<4;++j){size_t off=48+8*i+2*j;uint32_t h=(uint32_t(o.raw[off])<<8)|o.raw[off+1];o.half[i][j]=uint16_t(h);
    uint32_t bits=((h&0x8000)<<16)|((h&0x03ff)<<13)|(((h&0x7c00)+0x1c000)<<13);std::memcpy(&o.parameters[i][j],&bits,4);
   }
   auto count=field();require(count==be(o.raw,8));
   for(unsigned i=0;i<count;++i){OriginalTexture t;raw(t.raw);t.image=field();require(t.image==noMaterialTexture||t.image<nt);t.provenance=string();o.textures.push_back(std::move(t));}
   for(auto slot:{o.normalSlot,o.extraNormalSlot,o.reflectionSlot,o.paletteSlot})require(slot==noMaterialTexture||slot<count);
   require(at==finish);
  }
 }
 require(at==b.size());
}
}
