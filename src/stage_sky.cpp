#include "product_identity.h"
#include "stage_sky.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace mgo2mt::stage {
namespace {
void require(bool value){if(!value)throw std::runtime_error("Invalid original sky binding");}
int16_t wrap(int value){const auto bits=uint16_t(value);return bits<32768?int16_t(bits):int16_t(int(bits)-65536);}
}
SkySettings SkySettings::read(std::istream&in){
 SkySettings s;std::string magic;unsigned version=0;
 require(bool(in>>magic>>version)&&magic==mgo2mt::brand::Format{"MGO2MT.SKY"}&&version==1);
 require(bool(in>>s.modelHash>>s.procedure>>s.mdnSha256)&&s.modelHash&&s.modelHash<=0xffffff&&s.procedure&&s.procedure<=4096);
 require(s.mdnSha256.size()==64&&std::all_of(s.mdnSha256.begin(),s.mdnSha256.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}));
 for(float&v:s.position)require(bool(in>>v)&&std::isfinite(v)&&std::abs(v)<=4000000);
 for(auto&v:s.rotationUnits){int x=0;require(bool(in>>x)&&x>=-32768&&x<=32767);v=int16_t(x);}
 require(bool(in>>s.periodSeconds>>s.direction)&&std::isfinite(s.periodSeconds)&&s.periodSeconds>=0&&s.periodSeconds<=1000000&&(s.direction==1||s.direction==-1));
 for(float&v:s.color)require(bool(in>>v)&&std::isfinite(v)&&v>=0&&v<=16);
 require(bool(in>>s.fog)&&std::isfinite(s.fog)&&s.fog>=0&&s.fog<=16);
 for(float&v:s.fogColor)require(bool(in>>v)&&std::isfinite(v)&&v>=0&&v<=1);
 std::string tag;unsigned count=0;require(bool(in>>tag>>count)&&tag=="UV2"&&count<=65536);
 s.uv2.resize(count);for(auto&uv:s.uv2)for(float&v:uv)require(bool(in>>v)&&std::isfinite(v)&&std::abs(v)<=65536);
 std::string extra;require(!(in>>extra));return s;
}
void SkySettings::validate(const CharacterModel&model)const{
 require(!model.vertices.empty()&&!model.parts.empty());
 bool needsUv2=false;
 for(const auto&p:model.parts){const auto&m=p.original;require(m.present&&m.mdnSha256==mdnSha256&&(m.key==0x100064||m.key==0x100067)&&m.textures.size()==(m.key==0x100067?3:2));needsUv2|=m.key==0x100067;
  for(const auto&t:m.textures)require(t.image<model.textures.size());
 }
 require(needsUv2?uv2.size()==model.vertices.size():uv2.empty());
}
void SkySettings::prepare(CharacterModel&model)const{
 validate(model);for(size_t i=0;i<uv2.size();++i){model.vertices[i].u2=uv2[i][0];model.vertices[i].v2=uv2[i][1];}
}
SkyPose SkySettings::sample(double seconds)const{
 SkyPose result;result.position=position;auto units=rotationUnits;
 if(periodSeconds>0&&std::isfinite(seconds)&&seconds>0){
  // NewSky update 0x604114..0x604150: single-precision 65535 / (period*1000),
  // signed-angle normalization, truncation, then signed16 yaw + direction.
  const float elapsedMs=float(std::fmod(seconds,double(periodSeconds))*1000.0);
  float angle=(elapsedMs*65535.f)/(periodSeconds*1000.f);
  if(angle>=32768.f)angle-=65536.f;if(angle< -32768.f)angle+=65536.f;
  units[1]=wrap(int(rotationUnits[1])+int(angle)*direction);
 }
 for(unsigned i=0;i<3;++i)result.degrees[i]=float(units[i])*(360.f/65536.f);
 result.cloudU=float(units[1])*(-1.f/65535.f);
 return result;
}
}
