#pragma once
#include <cstdint>

namespace mgo2win::original {
// Current MGO2 ELF SHA256:
// 1a55a41ee5afdebd89075bd205e412c8311f569ba9dc022f629bb63fb4bfd13a
// EC4F10 -> D3CB10/+17A8 -> D3C698 -> EC5390 -> D3BB38 -> 616C0.
// These arguments are observed raw flags/results, not inferred owner/view names.
// D3BFF0: 36398 tests global +10/+14 bit 0x10000; actor+1790 supplies bits 0/1.
// EC5390: result 2 uses D3B8E8; otherwise D3BEA8 uses 61F58's distance/angle tier.
constexpr uint32_t ak102_original_shot_cue(uint32_t globalFlags,
                                         uint32_t actorFlags,
                                         int listenerTier) {
    if ((globalFlags & 0x10000) && (actorFlags & 2))
        return (actorFlags & 4) ? 10000 : 10001;
    if (listenerTier == 0) return 10002;
    if (listenerTier == 1) return 10003;
    return 10004; // Original includes missing listener (-1) and other results.
}

// Windows limited AK102 policy: the recovered nearest-tier, ordinary cue.
// Native runtime has no original global/actor audio flags, angle-weighted tier
// computation, alternate view mix, reverb or conditional secondary cue12308.
// It must not reconstruct original flags by assigning meaning to unnamed bits.
inline constexpr uint32_t ak102_native_shot_cue = 10002;
}
