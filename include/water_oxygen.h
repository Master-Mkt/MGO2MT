#pragma once
#include <algorithm>
#include <cstdint>
namespace mgo2win::water_gameplay {
// User-requested native extension. These are tunable native defaults, not
// recovered MGO2/MGS4 constants. Values are host-owned.
struct OxygenPolicy {
 uint32_t capacityMs=60000,recoveryMs=15000,damagePermillePerSecond=50;
};
constexpr bool valid_oxygen_policy(OxygenPolicy p) {
 return p.capacityMs>=1000&&p.capacityMs<=600000&&p.recoveryMs>=1000&&p.recoveryMs<=600000&&p.damagePermillePerSecond<=1000;
}
struct Oxygen {
 static constexpr uint16_t full=10000;
 uint64_t credit=600000000,recoveryRemainder=0,damageRemainder=0;
 void reset(OxygenPolicy p={}) {credit=uint64_t(p.capacityMs)*full;recoveryRemainder=damageRemainder=0;}
 uint16_t amount(OxygenPolicy p={})const {return uint16_t(std::min<uint64_t>(full,(credit+p.capacityMs-1)/p.capacityMs));}
 // Caller supplies monotonic bounded host elapsed time. Return HP damage,
 // carrying fractions so frame-rate/poll frequency does not alter the rate.
 uint32_t advance(OxygenPolicy p,uint64_t ms,bool submerged,uint32_t maxHp) {
  if(!valid_oxygen_policy(p)||ms>1000||maxHp>1000000)return 0;
  const uint64_t maximum=uint64_t(p.capacityMs)*full;
  credit=(std::min)(credit,maximum);
  if(!submerged){
   uint64_t total=ms*maximum+recoveryRemainder;
   credit=(std::min)(maximum,credit+total/p.recoveryMs);
   recoveryRemainder=credit==maximum?0:total%p.recoveryMs;
   damageRemainder=0;return 0;
  }
  recoveryRemainder=0;
  const uint64_t cost=ms*full,used=(std::min)(cost,credit);
  credit-=used;
  // Numerator uses a sub-ms reservoir remainder, so the crossing from the last
  // breath into damage is exact for every integral host millisecond.
  const uint64_t underwaterCost=cost-used;
  const uint64_t total=underwaterCost*maxHp*p.damagePermillePerSecond+damageRemainder;
  constexpr uint64_t divisor=uint64_t(full)*1000000;
  damageRemainder=total%divisor;
  return uint32_t(total/divisor);
 }
};
}
