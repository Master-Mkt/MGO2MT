#include "original_ak102_audio.h"
#include <array>
#include <stdexcept>

void check(bool value) { if (!value) throw std::runtime_error("AK102 original cue branch mismatch"); }

int main() {
    using mgo2mt::original::ak102_original_shot_cue;
    // Reviewed EC5390 constants; D3B8E8's model flag branches pass equal AK IDs.
    for (uint32_t flags = 0; flags != 8; ++flags) {
        const uint32_t mode2 = (flags & 4) ? 10000 : 10001;
        for (int tier : std::array{-1, 0, 1, 2, 99}) {
            const uint32_t expected = tier == 0 ? 10002 : tier == 1 ? 10003 : 10004;
            check(ak102_original_shot_cue(0, flags, tier) == expected);
            check(ak102_original_shot_cue(0x10000, flags, tier) ==
                   ((flags & 2) ? mode2 : expected));
            check(ak102_original_shot_cue(0x80010000, flags | 0x20000, tier) ==
                   ((flags & 2) ? mode2 : expected));
        }
    }
    static_assert(mgo2mt::original::ak102_native_shot_cue == 10002);
    static_assert(ak102_original_shot_cue(0x10000, 6, 0) == 10000);
    static_assert(ak102_original_shot_cue(0x10000, 2, 2) == 10001);
}
