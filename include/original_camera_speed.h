#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <array>
#include <optional>
#include <span>

namespace mgo2mt::original::camera_speed {
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

// Current BBA018 saves the same UI value-1 into both packed options and the
// bank25+200 expanded record. BBA814/818: +8 and packed byte1 (normal);
// BBA940/944: +12 and byte2 (shoulder); BBA8FC/900: +6 and byte3 (first-person).
// BBA8A4/8A8: +4 and byte4 low nibble (view-change interpolation).
// 804E88 maps those offsets to globals 1CC/1CE/1D0/1D2, respectively.
enum class Mode : std::uint8_t { normal, shoulder, firstPerson };
struct ExpandedSpeeds {
    std::uint16_t field1cc; // Normal, expanded+8.
    std::uint16_t field1ce; // Shoulder, expanded+12.
    std::uint16_t field1d0; // First-person, expanded+6.
    std::uint16_t field1d2; // Interpolation, expanded+4.
    bool operator==(const ExpandedSpeeds&) const = default;
};
// Pure speed projection from the first five bytes of the packed options.
// Preserve raw 0..15 here: 804E88 is the separate 0..9 clamp. Inversions and
// other options in the remaining nibbles are not speed values.
constexpr std::optional<ExpandedSpeeds> decode_packed_speeds(std::span<const std::uint8_t> packed) {
    if (packed.size() < 5) return std::nullopt;
    return ExpandedSpeeds{std::uint16_t(packed[1] >> 4), std::uint16_t(packed[2] >> 4),
        std::uint16_t(packed[3] >> 4), std::uint16_t(packed[4] & 15)};
}
// Confirmed setting terms only, not absolute rad/s. Shoulder is the original
// initial actor table; a script/actor override can replace that table.
inline std::optional<float> mode_factor(Mode mode, unsigned display) {
    if (!valid_display(display)) return std::nullopt;
    const auto stored = std::uint16_t(display - 1);
    switch (mode) {
    case Mode::normal: return field_1cc_factor(stored);
    case Mode::shoulder: return initial_field_1ce_factor(stored);
    case Mode::firstPerson: return field_1d0_factor(stored);
    }
    return std::nullopt;
}
// Native integration boundary: preserve its default absolute rates and step
// integrator while using the verified relative original setting curve.
// This does not reproduce original acceleration, FOV terms or clock units.
inline std::optional<float> relative_to_default(Mode mode, unsigned display) {
    const auto value = mode_factor(mode, display);
    const auto baseline = mode_factor(mode, default_display);
    if (!value || !baseline) return std::nullopt;
    return *value / *baseline;
}
}
