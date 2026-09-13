#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <optional>

namespace mgo2win::original_mastery {
// Current MGO2.ELF SHA 1a55a41e...bfd13a. See the independent
// outputs/mastery_policy_20260913/REPORT.md and verified.json.
// 32A090: selected ID25 -> class4. 8CB7F0 stores this at weapon+58;
// 8C9DD8 maps that class to skill3. This is not inferred from the weapon name.
inline constexpr unsigned ak102_weapon_id=25;
inline constexpr unsigned ak102_weapon_class=4;
inline constexpr unsigned ak102_mastery_skill=3;
inline constexpr uint32_t reload_rate_table=0xFDCC60;
inline constexpr std::array<float,4> reload_rates{
 1.f,std::bit_cast<float>(0x3f933333u),std::bit_cast<float>(0x3fa66666u),1.5f};

// 385DA8 lbz attack+128, then bit6: this means BYTE mask0x40.
// The equivalent big-endian 64-bit field mask is 0x4000000000000000.
// Local secondary-weapon reload writers 37CD4C/37DED8 set this flag;
// remote playback obtains it from the original state message, not a skill.
inline constexpr uint8_t unscaled_reload_flag=0x40;
constexpr unsigned skill_for_weapon_class(unsigned weaponClass){
 return weaponClass>=2&&weaponClass<=6?weaponClass-1:0;
}

// skillLevel is the explicitly supplied, validated level of the class's skill.
// Original 816610 returns0 for absent. No profile/HUD/UDP value is trusted here.
// Set flag => literal1.0 in385B88; clear => virtual134/837258 table getter.
// Unsupported classes select absent level0 in the original class selector.
inline std::optional<float> reload_rate_for_class(unsigned weaponClass,
 unsigned skillLevel,uint8_t originalAttackFlagsByte){
 if(skillLevel>3)return {};
 if((originalAttackFlagsByte&unscaled_reload_flag)||!skill_for_weapon_class(weaponClass))return 1.f;
 return reload_rates[skillLevel];
}

// This wrapper covers the current AK102 only. It does not decide whether a
// reload can start, reproduce every posture/secondary animation, or grant a
// skill. The caller must identify the primary-magazine/exception context.
// A native primary-only implementation may explicitly adapt its absent
// secondary path to flag0; unknown contexts must use its unchanged baseline.
inline std::optional<float> ak102_reload_rate(unsigned weaponId,
 unsigned rifleMasteryLevel,uint8_t originalAttackFlagsByte){
 if(weaponId!=ak102_weapon_id)return {};
 return reload_rate_for_class(ak102_weapon_class,rifleMasteryLevel,originalAttackFlagsByte);
}
}
