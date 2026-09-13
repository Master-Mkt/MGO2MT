#pragma once
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

// Current MGO2 ELF SHA256 1a55a41ee5afdebd89075bd205e412c8311f569ba9dc022f629bb63fb4bfd13a.
// D3B458 / row1294580; 7B03F8,7AEB78. See ORIGINAL_BULLET_PENETRATION_20260914.md.
// This is surface-budget, force and baseline HP math, not the complete player damage resolver.
namespace original_bullet_penetration {
inline constexpr int ak102_budget = 250;
inline constexpr int initial_force = 1000;
inline constexpr int minimum_force = 550;
inline constexpr float decay_start = 15000.f;
inline constexpr float decay_end = 80000.f;
inline constexpr float ak102_range = 200000.f;
inline constexpr float ak102_speed = 473750.f;
inline constexpr std::uint64_t free_surface_attribute = 0x8000;
inline constexpr int missing_material_resistance = 1000;
struct SurfaceCost { int resistance = 0; int forceCost = 0; };
struct Crossing { int remaining = 0; bool passes = false; };

// ld primitive+0x18 then rlwinm extracts LOW32 bit0x8000 (not the upper word).
// 7B0C10 material+0x28 is signed; missing material defaults1000. Native rejects
// negative authored resistance rather than inventing semantics for a budget gain.
// Caller must supply the original-oriented normal: dot==0 is free, as is backface.
inline std::optional<SurfaceCost> surface(std::uint64_t attribute,
    std::optional<int> resistance, float directionDotNormal) noexcept {
    if (!std::isfinite(directionDotNormal)) return std::nullopt;
    if ((attribute & free_surface_attribute) || directionDotNormal >= 0.f)
        return SurfaceCost{};
    const int r = resistance.value_or(missing_material_resistance);
    if (r < 0) return std::nullopt;
    return SurfaceCost{r, 100};
}

// 7B0D00 cmpw/ble: remaining==resistance stops. A stopped ray never revives.
inline std::optional<Crossing> cross(int budget, SurfaceCost cost) noexcept {
    if (budget < 0 || cost.resistance < 0 || cost.forceCost < 0) return std::nullopt;
    if (budget <= cost.resistance) return Crossing{0, false};
    return Crossing{budget - cost.resistance, true};
}

// ONE ORIGINAL BULLET UPDATE, not one arbitrary surface. 7AEC04..7AECE0.
// Original sums all queued hit costs first, adds partial distance decay, truncates
// that sum toward zero once, then subtracts it. Crossing decay_end or entering with
// force<=550 sets550 and bypasses queued costs. Subtraction can transiently go below
// 550; no additional same-update clamp is inserted here. A hitscan adapter must name
// its own update partition policy; it cannot claim identical frame/HP timing.
inline std::optional<int> advance_force(int current, float travelled, float step,
    int hitCost) noexcept {
    if (current > initial_force || hitCost < 0 ||
        !std::isfinite(travelled) || !std::isfinite(step) || travelled < 0.f || step < 0.f)
        return std::nullopt;
    const float next = travelled + step;
    if (!std::isfinite(next)) return std::nullopt;
    if (current <= minimum_force || next >= decay_end) return minimum_force;
    float cost = static_cast<float>(hitCost);
    if (next > decay_start) {
        const float distance = travelled >= decay_start ? step : next - decay_start;
        const float product = distance * static_cast<float>(initial_force - minimum_force);
        cost = cost + product / (decay_end - decay_start);
    }
    if (!std::isfinite(cost) || static_cast<double>(cost) > std::numeric_limits<int>::max())
        return std::nullopt;
    return current - static_cast<int>(cost);
}

// 7AC890 separately evaluates each target using ONLY costs strictly preceding its
// hit-list node and its distance from this update's start (hit+0x58). It writes the
// resulting low16 to damageinfo+0x0E; 825C98 loads signed16 and calls D3A3B8.
inline std::optional<int> target_force(int current, float travelled,
    float hitDistance, int priorHitCost) noexcept {
    return advance_force(current, travelled, hitDistance, priorHitCost);
}

// Current ID25/variant0 table1292AC8: HP275, stamina0. D3A584..5C8 signed
// multiplication/division, truncation toward zero; D3A60C maps force0 to1000.
// Body-part, armor, skill and rule adjustments AFTER this getter remain separate.
inline std::optional<int> ak102_base_hp(int signedForce16) noexcept {
    if (signedForce16 < -32768 || signedForce16 > 32767) return std::nullopt;
    const int force = signedForce16 == 0 ? initial_force : signedForce16;
    return (275 * force) / 1000;
}
}
