#pragma once
#include <cstdint>
#include <optional>
namespace mgo2win::weapon_extensions {
struct Damage {uint32_t hp=0,stamina=0;bool operator==(const Damage&)const=default;};
// Exact current table rows: MK2/RUGER ID2 variant1 @1292A60 = 0/245;
// RPG7 ID50 variant0 @1292BA0 = 1125/0. D3A3B8 selects these for the
// unmodified IDs and scales integer force /1000. Not hit-region/rule scaling.
// Other packed flags and variants are deliberately not accepted here.
inline std::optional<Damage> original_base_damage(uint32_t packedWeapon,int32_t force){
 if((packedWeapon!=2&&packedWeapon!=50)||force<0||force>100000)return {};
 if(!force)force=1000; // Actual D3A57C/D3A60C default-force branch.
 return packedWeapon==2?Damage{0,uint32_t(int64_t(force)*245/1000)}:Damage{uint32_t(int64_t(force)*1125/1000),0};
}
struct NativeTiming {uint16_t weapon,magazine,reserve;uint32_t intervalMs,refillMs,endMs,shotCue;};
// Magazine and shot cue are original facts. Reserve/interval/refill/end are
// explicit native choices until original action/reload motion clocks close.
inline constexpr NativeTiming mk2{2,10,30,600,1700,2200,10087};
inline constexpr NativeTiming rpg7{50,1,3,1200,2900,3500,10167};
}
