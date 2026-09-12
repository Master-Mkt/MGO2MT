#include "stage_cbox.h"
#include <array>
#include <bit>
#include <cmath>
#include <set>
#include <stdexcept>
#include <string>

namespace mgo2win::stage {
namespace {
void require(bool ok){if(!ok)throw std::runtime_error("Invalid CBOX placement layout");}
void validate(const CboxLayout& layout){
 require(layout.anchors.size()<=64&&layout.count<=layout.anchors.size());
 std::set<uint32_t> offsets;
 for(const auto& a:layout.anchors){
  require(a.sourceOffset&&offsets.insert(a.sourceOffset).second&&a.key<=0xffffff);
  for(float v:a.position)require(std::isfinite(v)&&std::abs(v)<1000000);
 }
}
}
CboxLayout CboxLayout::read(std::istream& in){
 CboxLayout layout;std::string magic;unsigned version,n;
 require(bool(in>>magic>>version>>layout.count>>n));
 require(magic=="MGO2WIN.STAGE_CBOX"&&version==1&&n<=64&&layout.count<=n);
 for(unsigned i=0;i<n;++i){CboxAnchor a;require(bool(in>>a.sourceOffset>>a.key>>a.position[0]>>a.position[1]>>a.position[2]));layout.anchors.push_back(a);}
 std::string tail;require(!(in>>tail));validate(layout);return layout;
}
std::vector<CboxPlacement> CboxLayout::select(uint8_t hostGeneration)const{
 validate(*this);std::vector<CboxPlacement> result;result.reserve(count);
 std::array<bool,64> used{};uint32_t state=hostGeneration;
 // PPC mullw/addi => low 32 bits, divwu => unsigned remainder. A second
 // advance supplies extsh's signed low 16 bits; probing uses no extra RNG.
 auto advance=[&](){state=state*0x5d588b65u+1u;return state;};
 constexpr float radiansPerUnit=std::bit_cast<float>(uint32_t(0x38c90fdb));
 for(uint32_t i=0;i<count;++i){
  auto index=advance()%uint32_t(anchors.size());
  while(used[index])index=(index+1)%uint32_t(anchors.size());
  used[index]=true;
  auto low=advance()&0xffffu;
  auto angle=int16_t(low<0x8000?int32_t(low):int32_t(low)-65536);
  result.push_back({anchors[index],index,angle,float(angle)*radiansPerUnit});
 }
 return result;
}
}
