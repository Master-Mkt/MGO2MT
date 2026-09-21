#pragma once
#include "combat_authority.h"
#include "original_ak102_audio.h"
#include "stage_profiles.h"
#include "weapon_extension_policy.h"
#include "weapon_runtime_profiles.h"

namespace mgo2mt::combat {
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
 // Original identity/magazine/base damage/cue, with explicitly native timing,
 // reserve, flight and blast adapters (WEAPON_EXTENSIONS_20260914.md).
 Weapon mk2;const auto&m=weapon_extensions::mk2;mk2.id=m.weapon;mk2.magazine=m.magazine;mk2.reserve=m.reserve;
 mk2.staminaDamage=245;mk2.intervalMs=m.intervalMs;mk2.reloadRefillMs=m.refillMs;mk2.reloadMs=m.endMs;mk2.range=76000;mk2.shotCue=m.shotCue;mk2.bodyCue=8168;
 Weapon rpg;const auto&r=weapon_extensions::rpg7;rpg.id=r.weapon;rpg.magazine=r.magazine;rpg.reserve=r.reserve;
 rpg.damage=1125;rpg.intervalMs=r.intervalMs;rpg.reloadRefillMs=r.refillMs;rpg.reloadMs=r.endMs;rpg.range=150000;rpg.shotCue=r.shotCue;rpg.nativeProjectile=true;
 Weapon wp;wp.id=53;wp.magazine=1;wp.reserve=3;wp.damage=0;wp.intervalMs=1000;wp.reloadRefillMs=600;wp.reloadMs=1000;wp.range=100000;wp.nativeProjectile=true;
 Weapon knife;knife.id=1;knife.meleeAttack=true;knife.damage=175;knife.magazine=1;knife.range=1250;knife.intervalMs=750;knife.reloadMs=750;
 // Explicit native Gekko prototypes. IDs/names are original; ballistics and
 // attack timing below are gameplay policy, not recovered original numbers.
 Weapon vulcan;vulcan.id=128;vulcan.damage=100;vulcan.magazine=1000;vulcan.intervalMs=80;vulcan.reloadMs=1000;vulcan.range=200000;vulcan.automatic=true;vulcan.shotCue=original::ak102_native_shot_cue;
 Weapon missile;missile.id=129;missile.damage=1125;missile.magazine=1;missile.intervalMs=1200;missile.reloadMs=1000;missile.range=150000;missile.nativeProjectile=true;missile.shotCue=r.shotCue;
 Weapon kick;kick.id=130;kick.damage=400;kick.magazine=1;kick.intervalMs=2084;kick.reloadMs=1000;kick.range=2700;
 Weapon stomp=kick;stomp.id=131;stomp.damage=500;
 auto profiles=firearm_profiles(map);
 for(auto& w:profiles){if(w.id==25)w=ak;else if(w.id==2)w=mk2;}
 profiles.insert(profiles.end(),{rpg,wp,knife,vulcan,missile,kick,stomp});
 for(uint16_t id:{52,54,55,56,57,58,59,63}){Weapon g;g.id=id;g.nativeProjectile=true;g.magazine=1;g.reserve=3;g.range=100000;g.intervalMs=1000;g.reloadRefillMs=600;g.reloadMs=1000;g.damage=id==52?750:0;g.staminaDamage=id==54?750:0;profiles.push_back(g);}
 Weapon shield;shield.id=73;shield.meleeAttack=true;shield.staminaDamage=500;shield.magazine=1;shield.range=1600;shield.intervalMs=1000;shield.reloadMs=1000;profiles.push_back(shield);
 for(uint16_t id:{64,65,66,67,69}){Weapon w;w.id=id;w.nativePlaced=true;w.magazine=1;w.reserve=3;w.range=1500;w.intervalMs=1000;w.reloadRefillMs=600;w.reloadMs=1000;w.damage=id==64?850:id==66?1200:0;w.staminaDamage=id==65||id==67?750:0;profiles.push_back(w);}
 // Retain the established default ordering while extending selectable IDs.
 for(size_t i=0;i<3;++i){const uint16_t id=i==0?25:i==1?3:52;auto at=std::find_if(profiles.begin()+i,profiles.end(),[&](const Weapon&w){return w.id==id;});if(at!=profiles.end())std::iter_swap(profiles.begin()+i,at);}
 return profiles;
}
}

