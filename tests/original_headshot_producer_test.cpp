#include "original_headshot_producer.h"
#include "original_hit_regions.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace hs = mgo2win::original_headshot_producer;
namespace hit = mgo2win::original_hit_regions;
static void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message); // Always active in Release.
}
int main() try {
    for (unsigned bits = 0; bits != 32; ++bits) {
        hs::CameraState c{bool(bits & 1), bool(bits & 2), bool(bits & 4),
            uint64_t(bool(bits & 8)) << 16 | uint64_t(bool(bits & 16)) << 17};
        bool expected = c.active8e375f;
        if (!expected && c.active796c26) {
            expected = true;
            if (c.homingPresent && (bits & 8) && (bits & 16)) expected = false;
        }
        check(hs::camera_flags(c) == (expected ? 0x200381U : 0x381U), "camera PPC branch table");
        auto noisy = c; noisy.homingBits |= ~uint64_t{0x30000};
        check(hs::camera_flags(noisy) == hs::camera_flags(c), "unrelated homing bits do not change predicate");
    }
    check(hs::next_random(0) == 1 && hs::next_random(1) == 0x5d588b66, "LCG known vectors");
    check(hs::next_random(0xffffffff) == 0xa2a7749c, "LCG wrap is modulo 2^32");
    unsigned allowed = 0;
    for (uint32_t hi = 0; hi != 65536; ++hi) {
        const bool expected = hi <= 13107;
        check(hs::class4_attack_draw(hi << 16) == expected, "complete high16 threshold domain");
        check(hs::class4_attack_draw((hi << 16) | 0xffff) == expected, "draw ignores low16");
        allowed += expected;
    }
    check(allowed == 13108, "integer threshold includes 13107");
    const hs::CameraState inactive{false, false, false, 0};
    const hs::CameraState active{true, false, true, 0x30000};
    for (uint8_t klass = 0; klass < 16; ++klass) {
        auto shot = hs::produce(inactive, klass, 0);
        auto lit = hs::produce(active, klass, 0);
        check(shot && lit, "all raw nibble classes are representable");
        check(shot->randomConsumed == (klass == 4), "only attacker class4 consumes RNG");
        check(shot->randomState == (klass == 4 ? 1U : 0U), "attacker state transition");
        check(lit->randomState == shot->randomState && lit->randomConsumed == shot->randomConsumed,
              "active camera must not short-circuit class4 RNG");
        check(hs::packed_headshot_bit(shot->sourceFlags) == (klass == 4 ? 0x10000000U : 0U), "class4 can add HS without camera");
        auto off = hs::receive_gate(0xfffffffdU, klass, 0);
        check(off && !off->allowed && !off->randomConsumed && off->randomState == 0, "shared bit off consumes no RNG");
        auto on = hs::receive_gate(2, klass, 0);
        check(on && on->allowed == (klass != 4) && on->randomConsumed == (klass == 4), "receiver exact class gate");
    }
    for (uint32_t seed : std::array<uint32_t,6>{0,1,2,0x7fffffff,0x80000000,0xffffffff}) {
        const uint32_t expected = uint32_t(uint64_t(seed) * 1566083941ULL + 1);
        auto gate = hs::receive_gate(2, 4, seed);
        check(gate && gate->randomState == expected && gate->allowed == (expected >= 0x80000000U), "signed receiver draw");
        check(gate->allowed == hit::headshot_allowed(2, 4, gate->allowed ? -1 : 0), "existing damage gate agreement");
    }
    check(!hs::produce(active, 16, 1) && !hs::produce(active, 255, 1), "invalid attacker class rejected");
    check(!hs::receive_gate(2, 16, 1) && !hs::receive_gate(0, 255, 1), "invalid receiver class rejected");
    auto shot = hs::produce(inactive, 4, 0); // Draw 1: adds the source bit.
    auto receiver = hs::receive_gate(2, 4, shot->randomState); // Shared draw 2: positive, denies.
    check(receiver && receiver->randomState == 0x5d588b66 && !hs::confirmed_headshot(shot, receiver),
          "producer/receiver must advance one shared stream, not two independently seeded streams");
    auto receiver2 = hs::receive_gate(2, 0, receiver->randomState);
    check(hs::confirmed_headshot(shot, receiver2), "known class0 target permits produced HS");
    check(!hs::confirmed_headshot({}, receiver2) && !hs::confirmed_headshot(shot, {}), "unknown authoritative state stays disabled");
    auto damage = hit::ak102_region_damage(275, 1000, 4, 1000, true, hs::confirmed_headshot(shot, receiver2));
    check(damage && damage->headshot && damage->damage == 1000, "known full-force HS reaches original max HP rule");
    auto body = hit::ak102_region_damage(275, 1000, 2, 1000, true, receiver->allowed);
    auto limb = hit::ak102_region_damage(275, 1000, 6, 1000, true, receiver->allowed);
    check(body && body->damage == 275 && limb && limb->damage == 165 && receiver->randomConsumed,
          "receiver draw precedes body/limb selection despite no HS damage");
    auto penetrated = hit::ak102_region_damage(247, 1000, 3, 900, true, receiver2->allowed);
    check(penetrated && penetrated->damage == 988, "raw force below1000 takes original 4x branch");
    check(hs::packed_headshot_bit(0xffdfffffU) == 0 && hs::packed_headshot_bit(0x200000) == 0x10000000,
          "packing tests exact bit only");
    std::cout << "original_headshot_producer_test PASS\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n'; return 1;
}
