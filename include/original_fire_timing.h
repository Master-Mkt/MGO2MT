#pragma once
#include <cstdint>
#include <optional>

namespace mgo2mt::original {
// Current MGO2 AK102 row 0x1293840: interval 30 at word 2. Nominal Windows
// policy: five original clock ticks per 1001/60000 seconds (see clock audit).
// Keep sub-millisecond precision; this is not the original attack poll phase.
inline constexpr uint32_t ak102_fire_ticks=30;
inline std::optional<uint64_t> fire_interval_ns(uint32_t ticks){
 if(!ticks||ticks>17982)return std::nullopt; // native 60-second profile limit
 return (uint64_t(ticks)*10010000+2)/3;
}
}
