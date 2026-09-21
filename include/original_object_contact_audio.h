#pragma once
#include "material_audio.h"
#include <cmath>
#include <cstdint>
#include <optional>

namespace mgo2mt::combat::original_object_audio {
// Current ELF 7B7098 projectile response only. This is NOT a generic rigid-body,
// AK102 drop, box, glass, or human collision sound policy.
struct ProjectileContact {
    uint32_t packedWeapon{}; // Actor +0x274, low nine bits are the weapon ID.
    uint32_t previousBounces{}; // Actor +0x264, before this response increments it.
    uint32_t responseIterations{}; // Actor +0x268, after increment (1..9).
    bool responseReached{}; // Caller proved entry into the actual collision response.
};
struct Sound { uint32_t cue{}; float gain{}; };

inline std::optional<Sound> projectile_base(ProjectileContact contact) noexcept {
    if (!contact.responseReached || contact.responseIterations == 0 ||
        contact.responseIterations > 9 || contact.previousBounces > 4) return {};
    const auto id = contact.packedWeapon & 0x1ffu;
    uint32_t cue;
    if ((id >= 54 && id <= 60) || id == 63) cue = 18502;
    else if (id == 52 || id == 53) cue = 18510;
    else if (id == 62) cue = 18522;
    else return {};
    if (contact.previousBounces != 0) ++cue;
    // Original single-precision fmadds, 7B811C; raw floats 1.0 and BE4CCCCD.
    return Sound{cue, std::fma(static_cast<float>(contact.previousBounces), -0.2f, 1.f)};
}

inline std::optional<Sound> projectile_cue(ProjectileContact contact,
        material_audio::Stage stage, std::optional<uint32_t> materialHash) noexcept {
    auto sound = projectile_base(contact);
    if (!sound) return {};
    // Original null material pointer bypasses remap. An existing material whose
    // word0 is zero is different: pass zero to the reviewed resolver, fail closed.
    if (materialHash) {
        auto cue = material_audio::resolve(stage, sound->cue, *materialHash);
        if (!cue) return {};
        sound->cue = *cue; // Zero means explicit silence; caller must not play it.
    }
    return sound;
}
}
