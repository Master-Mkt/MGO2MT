#pragma once
#include "host_protocol.h"
#include <optional>

namespace mgo2mt::host {
// Original global cache 0, descriptor 0. Values 0..7 have verified original
// writers. This byte does not acknowledge scene objects, spawning, or START.
std::vector<uint8_t> phase_update(uint8_t phase);
// Parse a complete validated global-cache update. No phase field -> nullopt.
// Unknown received values are retained; consumers must not call them playable.
std::optional<uint8_t> phase_value(std::span<const uint8_t> payload);
}
