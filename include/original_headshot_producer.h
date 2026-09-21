#pragma once
#include <cstdint>
#include <optional>

namespace mgo2mt::original_headshot_producer {
// Raw current-ELF state, not native camera-mode labels or room rule flags.
// 2EC650 / 2EC718: matching node +48 must be zero (active).
struct CameraState {
    bool active8e375f;
    bool active796c26;
    bool homingPresent;
    uint64_t homingBits; // CF5135 component +388, numeric bits 16 and 17.
};
inline constexpr uint32_t sourceHeadshot = 0x00200000;
inline constexpr uint32_t packedHeadshot = 0x10000000;
inline constexpr uint32_t baseSourceFlags = 0x381;
inline constexpr uint32_t sharedHeadshotMask = 2;

// 2E8150..2E8190 / 2E8D70..2E8DC0. Both homing bits suppress only
// the 796C26 branch; they do not suppress an active 8E375F camera.
constexpr uint32_t camera_flags(const CameraState& camera) {
    const bool suppressed = camera.homingPresent &&
        (camera.homingBits & 0x30000ULL) == 0x30000ULL;
    return baseSourceFlags | ((camera.active8e375f ||
        (camera.active796c26 && !suppressed)) ? sourceHeadshot : 0);
}

// Current 8CADB0 and 827344 use the SAME global state at 12296D0.
// Keep state explicit: other original RNG consumers can occur between steps.
constexpr uint32_t next_random(uint32_t state) {
    return state * uint32_t{0x5d588b65} + uint32_t{1};
}
constexpr bool class4_attack_draw(uint32_t nextState) {
    return ((uint32_t{5} * (nextState >> 16)) >> 16) == 0;
}
struct ProducerStep {
    uint32_t sourceFlags;
    uint32_t randomState;
    bool randomConsumed;
};
// 8CADB0 (online weapon virtual +104), then OR at 2E8190.
// rawClass is the already extracted 8185E0() nibble. Invalid values are
// rejected rather than masked into a different authenticated player class.
constexpr std::optional<ProducerStep> produce(const CameraState& camera,
                                             uint8_t rawClass, uint32_t state) {
    if (rawClass > 15) return std::nullopt;
    uint32_t flags = camera_flags(camera);
    if (rawClass != 4) return ProducerStep{flags, state, false};
    state = next_random(state); // Also consumed when camera already grants HS.
    if (class4_attack_draw(state)) flags |= sourceHeadshot;
    return ProducerStep{flags, state, true};
}
constexpr uint32_t packed_headshot_bit(uint32_t sourceFlags) {
    // 7B07A8..7B07B8. Returns this single packed bit, not the whole packet.
    return (sourceFlags & sourceHeadshot) ? packedHeadshot : 0;
}
struct ReceiverStep {
    bool allowed;
    uint32_t randomState;
    bool randomConsumed;
};
// 826AC8 / 8272DC / 827344. Run at the original normal-weapon damage
// branch, BEFORE inspecting joint/packed HS bit. Even a body/limb hit can
// consume the class4 draw. Do not call early for a miss or an unrelated weapon.
constexpr std::optional<ReceiverStep> receive_gate(uint32_t sharedFlags,
                                                  uint8_t rawClass, uint32_t state) {
    if (rawClass > 15) return std::nullopt;
    if ((sharedFlags & sharedHeadshotMask) == 0)
        return ReceiverStep{false, state, false};
    if (rawClass != 4) return ReceiverStep{true, state, false};
    state = next_random(state);
    // PPC cmpwi of the 32-bit result, independent of C++ signed conversions.
    return ReceiverStep{(state & 0x80000000U) != 0, state, true};
}
// Deliberate native boundary for missing authoritative raw state: no implicit
// default class, room flag, camera or RNG seed can enable the HS damage branch.
constexpr bool confirmed_headshot(const std::optional<ProducerStep>& shot,
                                  const std::optional<ReceiverStep>& receiver) {
    return shot && receiver && receiver->allowed &&
        (shot->sourceFlags & sourceHeadshot) != 0;
}
}
