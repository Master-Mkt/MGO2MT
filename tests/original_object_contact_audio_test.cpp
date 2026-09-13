#include "original_object_contact_audio.h"
#include <array>
#include <cstdlib>
#include <iostream>
using namespace mgo2win::combat;
using namespace original_object_audio;
static void require(bool v) { if (!v) { std::cerr << "object contact audio contract failed\n"; std::exit(1); } }
int main() {
    for (uint32_t id = 0; id <= 511; ++id) {
        const uint32_t expected = (id >= 54 && id <= 60) || id == 63 ? 18502u :
            id == 52 || id == 53 ? 18510u : id == 62 ? 18522u : 0u;
        for (uint32_t n = 0; n <= 5; ++n) {
            auto sound = projectile_base({id, n, 1, true});
            require(sound.has_value() == (expected != 0 && n < 5));
            if (sound) {
                require(sound->cue == expected + (n != 0 ? 1u : 0u));
                require(sound->gain == std::fma(float(n), -0.2f, 1.f));
                require(sound->gain > 0 && sound->gain <= 1);
            }
        }
    }
    require(!projectile_base({25, 0, 1, true})); // AK must never inherit grenade sound.
    require(!projectile_base({52, 0, 1, false}));
    require(!projectile_base({52, 0, 0, true}));
    require(projectile_base({52, 4, 9, true}).has_value());
    require(!projectile_base({52, 0, 10, true}));
    require(!projectile_base({52, UINT32_MAX, 1, true}));
    require(projectile_base({0xfffffe34u, 0, 1, true})->cue == 18510);
    auto absent = projectile_cue({52, 0, 1, true}, material_audio::Stage::unknown, {});
    require(absent && absent->cue == 18510 && absent->gain == 1);
    require(!projectile_cue({52, 0, 1, true}, material_audio::Stage::n022a, 0));
    require(!projectile_cue({52, 0, 1, true}, material_audio::Stage::unknown, 0x1818acu));
    auto remapped = projectile_cue({52, 0, 1, true}, material_audio::Stage::n022a, 0x1818acu);
    auto expected = material_audio::resolve(material_audio::Stage::n022a, 18510, 0x1818acu);
    require(remapped.has_value() == expected.has_value());
    if (remapped) require(remapped->cue == *expected && remapped->gain == 1);
    // Stage-specific absence must not borrow another stage's conversion.
    require(!projectile_cue({52, 0, 1, true}, material_audio::Stage::n004a, 0x8e8ccu));
    std::cout << "original projectile contact IDs, first/repeat, gain, material and AK exclusion PASS\n";
}
