#pragma once
#include "combat_authority.h"
#include "original_ak102_audio.h"
#include "stage_profiles.h"
#include "weapon_extension_policy.h"

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
 ak.reloadMotion=original::ak102_reload;ak.nativePrimaryMastery=true;ak.nativeAkPenetration=true;ak.nativeAkHitRegions=true;ak.materialMap=map;ak.nativeAkAccuracy=true;
 // Original AK102 callback/cues are reviewed in COMBAT_NORMAL_GUNSHOT_20260913.
 // This limited native profile uses the near cue; the original listener/owner
 // mode and SCE mixer are not reproduced. Material impact dispatch is pending.
 // Existing original body-impact cue, variant 0, is a native uniform choice;
 // do not infer the original target actor-kind selector from gender/appearance.
 ak.shotCue=original::ak102_native_shot_cue;ak.impactCue=0;ak.bodyCue=8168;
 Weapon sidearm;sidearm.id=3;sidearm.heldOnly=true;
 Weapon grenade;grenade.id=52;grenade.heldOnly=true;
 // Original identity/magazine/base damage/cue, with explicitly native timing,
 // reserve, flight and blast adapters (WEAPON_EXTENSIONS_20260914.md).
 Weapon mk2;const auto&m=weapon_extensions::mk2;mk2.id=m.weapon;mk2.magazine=m.magazine;mk2.reserve=m.reserve;
 mk2.staminaDamage=245;mk2.intervalMs=m.intervalMs;mk2.reloadRefillMs=m.refillMs;mk2.reloadMs=m.endMs;mk2.range=76000;mk2.shotCue=m.shotCue;mk2.bodyCue=8168;
 Weapon rpg;const auto&r=weapon_extensions::rpg7;rpg.id=r.weapon;rpg.magazine=r.magazine;rpg.reserve=r.reserve;
 rpg.damage=1125;rpg.intervalMs=r.intervalMs;rpg.reloadRefillMs=r.refillMs;rpg.reloadMs=r.endMs;rpg.range=150000;rpg.shotCue=r.shotCue;rpg.nativeProjectile=true;
 Weapon wp;wp.id=53;wp.magazine=1;wp.reserve=3;wp.damage=0;wp.intervalMs=1000;wp.reloadRefillMs=600;wp.reloadMs=1000;wp.range=100000;wp.nativeProjectile=true;
 Weapon knife;knife.id=1;knife.heldOnly=true;
 // Explicit native Gekko prototypes. IDs/names are original; ballistics and
 // attack timing below are gameplay policy, not recovered original numbers.
 Weapon vulcan;vulcan.id=128;vulcan.damage=100;vulcan.magazine=1000;vulcan.intervalMs=80;vulcan.reloadMs=1000;vulcan.range=200000;vulcan.automatic=true;vulcan.shotCue=original::ak102_native_shot_cue;
 Weapon missile;missile.id=129;missile.damage=1125;missile.magazine=1;missile.intervalMs=1200;missile.reloadMs=1000;missile.range=150000;missile.nativeProjectile=true;missile.shotCue=r.shotCue;
 Weapon kick;kick.id=130;kick.damage=400;kick.magazine=1;kick.intervalMs=2084;kick.reloadMs=1000;kick.range=2700;
 Weapon stomp=kick;stomp.id=131;stomp.damage=500;
 return {ak,sidearm,grenade,mk2,rpg,wp,knife,vulcan,missile,kick,stomp};
}
}

