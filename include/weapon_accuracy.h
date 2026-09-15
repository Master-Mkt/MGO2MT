#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
namespace mgo2win::weapon_accuracy {
using Vec3=std::array<float,3>;
// Native AK policy, not recovered original recoil/stance/skill constants.
// Cone half angles are integer microradians; recovery is microradians/second.
struct Policy {uint32_t base=3000,maximum=30000,perShot=4000,recovery=12000;};
inline constexpr Policy native_ak{};
constexpr bool valid(Policy p){return p.base<=p.maximum&&p.maximum<=100000&&p.perShot>0&&p.perShot<=100000&&p.recovery>0&&p.recovery<=1000000;}
inline uint64_t mix(uint64_t x){x+=0x9e3779b97f4a7c15ull;x=(x^(x>>30))*0xbf58476d1ce4e5b9ull;x=(x^(x>>27))*0x94d049bb133111ebull;return x^(x>>31);}
class State {
 // excess stores thousandths of a microradian, so integer milliseconds do not
 // round away recovery between polls. Reads never mutate the clock or RNG.
 uint64_t at_=0,shots_=0;uint32_t excess_=0;bool clock_=false;
 uint32_t remaining(Policy p,uint64_t now)const{
  if(!clock_||now<=at_)return excess_;const auto elapsed=now-at_;
  if(elapsed>=uint64_t(excess_)/p.recovery+1)return 0;
  return uint32_t(uint64_t(excess_)-elapsed*p.recovery);
 }
public:
 void reset(){*this={};}
 uint64_t shots()const{return shots_;}
 std::optional<float> radians(Policy p,uint64_t now)const{
  if(!valid(p)||(clock_&&now<at_))return {};return float(double(p.base)*.000001+double(remaining(p,now))*.000000001);
 }
 std::optional<uint16_t> milliradians(Policy p,uint64_t now)const{
  if(!valid(p)||(clock_&&now<at_))return {};return uint16_t((uint64_t(p.base)*1000+remaining(p,now)+999999)/1000000);
 }
 bool accepted(Policy p,uint64_t now){
  if(!valid(p)||(clock_&&now<at_)||shots_==UINT64_MAX)return false;
  excess_=uint32_t((std::min)(uint64_t(p.maximum-p.base)*1000,uint64_t(remaining(p,now))+uint64_t(p.perShot)*1000));at_=now;clock_=true;++shots_;return true;
 }
 // Uniform solid angle within the pre-shot cone. Seed is HOST scope-derived;
 // clients never provide angle, random bits, or accepted shot count.
 std::optional<Vec3> direction(Policy p,uint64_t now,Vec3 aim,uint64_t seed)const{
  auto angle=radians(p,now);if(!angle)return {};double length=0;for(float x:aim){if(!std::isfinite(x))return {};length+=double(x)*x;}if(std::abs(length-1)>0.002)return {};
  length=std::sqrt(length);for(auto&x:aim)x=float(x/length);
  const uint64_t a=mix(seed^mix(shots_+1)),b=mix(a);const double u=double(a>>11)*0x1.0p-53,v=double(b>>11)*0x1.0p-53;
  const double cosine=1-u*(1-std::cos(double(*angle))),sine=std::sqrt((std::max)(0.,1-cosine*cosine)),phi=6.2831853071795864769*v;
  Vec3 tangent=std::abs(aim[1])<.9f?Vec3{aim[2],0,-aim[0]}:Vec3{0,-aim[2],aim[1]};double tl=0;for(auto x:tangent)tl+=double(x)*x;tl=std::sqrt(tl);for(auto&x:tangent)x=float(x/tl);
  const Vec3 bitangent{aim[1]*tangent[2]-aim[2]*tangent[1],aim[2]*tangent[0]-aim[0]*tangent[2],aim[0]*tangent[1]-aim[1]*tangent[0]};Vec3 result{};
  for(unsigned i=0;i<3;++i)result[i]=float(aim[i]*cosine+sine*(tangent[i]*std::cos(phi)+bitangent[i]*std::sin(phi)));return result;
 }
};
}
