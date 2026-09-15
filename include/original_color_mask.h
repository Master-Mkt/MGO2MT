#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace mgo2win {
// Original shared block 0x1770F80. These FP logical parameters are NOT
// serialized MDN P0..7, nor VP transform constants bearing the same numbers.
// Evidence: color-uniform-followup-20260915/analysis_findings.json.
struct OriginalColorMaskState {
 using Vector=std::array<float,4>;
 static constexpr float bits(uint32_t value){return std::bit_cast<float>(value);}
 static constexpr Vector initial36(){return {bits(0x3f0978d5),bits(0x3e926e98),bits(0x3e828f5c),1};}
 static constexpr Vector initial37(){return {bits(0x3ece5604),bits(0x3ece5604),bits(0x3ece5604),1};}
 static constexpr Vector initial38(){return {bits(0x3eb2b021),bits(0x3eb2b021),bits(0x3eb2b021),1};}
 Vector p36=initial36(),p37=initial37(),p38=initial38(),p39{};
 // 0x161478 preserves P39; 0x161428 additionally clears it.
 void reset_colors(){p36=initial36();p37=initial37();p38=initial38();}
 void initialize(){reset_colors();p39={};}
 // Pure, explicit handler operation; no speculative GCX registration or UI
 // event is wired to this method. Original frsp -> fmuls, without a clamp.
 void gcx_second_color(const std::array<int32_t,3>& values){
  for(unsigned i=0;i<3;++i)p37[i]=static_cast<float>(values[i])*bits(0x3a83126f);
 }
};

struct OriginalColorMaskDraw {
 // A caller may copy the current common state and override this draw only.
 OriginalColorMaskState colors;
 // Unset means the effective VP c467.x is unknown: keep legacy rendering.
 // The audited embedded default is 1, but runtime overrides are unresolved.
 std::optional<float> c467x;
 void validate()const{
  for(const auto* p:{&colors.p36,&colors.p37,&colors.p38})
   for(float v:*p)if(!std::isfinite(v))throw std::invalid_argument("Non-finite original color-mask coefficient");
  if(c467x&&!std::isfinite(*c467x))throw std::invalid_argument("Non-finite original c467.x");
 }
};
}
