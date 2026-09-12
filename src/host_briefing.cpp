#include "host_briefing.h"
#include "host_match.h"

namespace mgo2win::host {
std::vector<uint8_t> phase_update(uint8_t phase) {
    if (phase > 7) throw Invalid(Error::message);
    // 27E8B0: opcode, cache ID, ceil(55/8) mask bytes, typed byte value.
    return {11, 0, 1, 0, 0, 0, 0, 0, 0, phase};
}
std::optional<uint8_t> phase_value(std::span<const uint8_t> payload) {
    if (payload.size() < 2 || payload[0] != 11 || payload[1] != 0)
        throw Invalid(Error::message);
    MatchState scratch;
    update_match(scratch, payload);
    return scratch.phase;
}
}
