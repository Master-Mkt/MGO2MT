#pragma once
#include "combat_authority.h"
#include "original_ak102_audio.h"
#include "stage_profiles.h"

namespace mgo2win::combat {
// Limited native first-deployment profile, not a complete original combat port.
// Source/range conventions and deliberate omissions are recorded in
// notes/COMBAT_AK102_NATIVE_20260913.md and COMBAT_AMMO_CONDITIONS_20260913.md.
// Each life receives the host grant. Only authenticated, round-frozen skills
// enter the reviewed primary AK reload path. No underbarrel, shared ammo partner,
// GCX override or retained DP inventory exists in this path.
inline std::vector<Weapon> initial_profiles(uint8_t map,uint8_t rule,uint8_t flags){
 if(!stage::runtime_stage_supported(map)||rule>1||flags!=0)return {};
 Weapon ak;
 ak.id=25;ak.damage=275;ak.staminaDamage=0;ak.magazine=30;ak.reserve=90;
 ak.range=200000;ak.automatic=true;ak.fireIntervalTicks=original::ak102_fire_ticks;
 ak.reloadMotion=original::ak102_reload;ak.nativePrimaryMastery=true;ak.nativeAkPenetration=true;ak.nativeAkHitRegions=true;ak.materialMap=map;
 // Original AK102 callback/cues are reviewed in COMBAT_NORMAL_GUNSHOT_20260913.
 // This limited native profile uses the near cue; the original listener/owner
 // mode and SCE mixer are not reproduced. Material impact dispatch is pending.
 // Existing original body-impact cue, variant 0, is a native uniform choice;
 // do not infer the original target actor-kind selector from gender/appearance.
 ak.shotCue=original::ak102_native_shot_cue;ak.impactCue=0;ak.bodyCue=8168;
 return {ak};
}
}
