#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mgo2win::original::camera_speed {
// Current ELF 804E88 clamps expanded camera options; BBF880 displays packed+1.
inline constexpr unsigned minimum_display = 1, maximum_display = 10, default_display = 5;
constexpr bool valid_display(unsigned value) { return value >= minimum_display && value <= maximum_display; }
constexpr unsigned clamp_stored(std::uint16_t value) { return std::min<unsigned>(value, 9); }
// Names identify the observed global-camera fields, not unverified UI modes.
// 70DAA8/70DB20: float32 fused multiply-add, constants 11C27BC/11C27C4.
inline float field_1cc_factor(std::uint16_t value) { return std::fma(float(clamp_stored(value)), .125f, .5f); }
// 2F00D8/2F01F4: setting+1, before acceleration/deceleration and input terms.
constexpr float field_1d0_factor(std::uint16_t value) { return float(clamp_stored(value) + 1); }
// 2F23B8 initializes actor+120..144 to 1..10; 2F4D28 indexes this table.
// This is the initial table only: actor-specific/script overrides are not covered.
constexpr float initial_field_1ce_factor(std::uint16_t value) { return float(clamp_stored(value) + 1); }
// 2ED458/2ED514..528: transition countdown, original clock units, not milliseconds.
constexpr unsigned field_1d2_transition_ticks(std::uint16_t value) { return 10 * (9 - clamp_stored(value)); }
}
