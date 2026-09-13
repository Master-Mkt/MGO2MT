#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

namespace mgo2win::original {
// MGO2 SHA 1a55a41ee5afdebd89075bd205e412c8311f569ba9dc022f629bb63fb4bfd13a.
// Full identity and PPC are in
// notes/COMBAT_CLOCK_END_20260913.md. CPU path 175EA8 advances float32
// time by rate * actor time_base; 333A38 crosses (previous, current].
// 17617C / 1774DC set end bit 0 at cycle - baseTick (inclusive).
// Windows schedules a nominal frame of 5 ticks per 1001/60000 seconds.
// This is the documented native clock policy, not measured PS3 wall time.
struct ReloadMotion {
 uint32_t baseTick=0,intervals=0,refillTick=0;
 float rate=1;
};
struct ReloadTiming {
 uint32_t refillFrame=0,endFrame=0,refillMs=0,endMs=0;
 double endSeconds=0;
};
inline std::optional<ReloadTiming> reload_timing(ReloadMotion motion){
 if(!motion.baseTick||motion.baseTick>1000||motion.intervals<2||motion.intervals>3600||
    !motion.refillTick||!std::isfinite(motion.rate)||motion.rate<=0||motion.rate>16)return {};
 const float end=float(motion.baseTick*(motion.intervals-1));
 if(float(motion.refillTick)>end)return {};
 const float delta=5.f*motion.rate;
 float previous=0;ReloadTiming result;
 // Reject profiles longer than the native authority's existing 60 s limit.
 for(uint32_t frame=1;frame<=3596;++frame){
  const float current=previous+delta;
  if(!result.refillFrame&&previous<float(motion.refillTick)&&float(motion.refillTick)<=current)result.refillFrame=frame;
  if(current>=end){result.endFrame=frame;break;}
  previous=current;
 }
 if(!result.refillFrame||!result.endFrame)return {};
 // Ceil only the final deadline: fractional frame time is never accumulated
 // as rounded milliseconds, and the float32 rate is applied at each frame.
 result.refillMs=(result.refillFrame*1001u+59u)/60u;
 result.endMs=(result.endFrame*1001u+59u)/60u;
 result.endSeconds=double(result.endFrame)*1001.0/60000.0;
 return result;
}
inline constexpr ReloadMotion ak102_reload{5,210,650,1.f};
inline constexpr std::array<float,4> rifle_reload_rates{1.f,1.15f,1.3f,1.5f};
inline constexpr double nominal_motion_fps=60000.0/1001.0;
}
