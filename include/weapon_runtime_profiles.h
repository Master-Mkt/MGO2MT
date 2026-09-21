#pragma once
#include "combat_authority.h"
#include "original_weapon_reload.h"
#include "weapon_reload_adapter_policy.h"
namespace mgo2mt::combat {
// Original identity, magazine, cadence, damage, range and shot cue. Reserve is
// the native three-magazine grant. Action-driven cadence and reload deadlines
// below remain explicit native adapters until each original action is connected.
inline std::vector<Weapon> firearm_profiles(uint8_t map){
 constexpr std::array<std::pair<uint16_t,uint32_t>,22> cues{{
  {2,10087},{3,10092},{4,10197},{7,10082},{8,10102},{15,10117},{18,10127},{20,10072},{23,10122},{24,10007},{25,10002},
  {26,10027},{30,10022},{31,10017},{35,10037},{37,10042},{38,10047},{39,10142},{41,10062},{42,10052},{43,10067},{44,10187}}};
 std::vector<Weapon> result;result.reserve(cues.size());
 for(const auto& p:original_weapon::firearms){
  Weapon w;w.id=p.id;w.magazine=p.magazine;w.reserve=uint16_t(p.magazine*3);w.damage=uint32_t(p.hp);
  w.staminaDamage=p.hp>0?0:uint32_t(std::max(0,p.staminaRaw));w.range=p.bullet.range;
  // Shotgun uses a separate pellet producer; this bounded native adapter
  // preserves the recovered total damage until that producer is ported.
  if(p.id==37||p.id==38)w.range=30000;
  w.materialMap=map;w.originalFirearm=p.id!=37&&p.id!=38;
  w.automatic=original_weapon::automatic(p.id);if(p.intervalTicks>0)w.fireIntervalTicks=uint32_t(p.intervalTicks);
  else w.intervalMs=p.id==2?600:p.id==37?1000:1500;
  if(auto motion=original_weapon::reload_motion(p.id))w.reloadMotion=motion;
  else {w.reloadRefillMs=3000;w.reloadMs=native_reload_presentation_ms(p.id);} // Native whole-tube M870 adapter.
  // Current M4 event refill680 exceeds the available base selector duration.
  // Preserve the original refill deadline; full action3500ms is native until
  // the matching current MTAR/state is recovered (visual is scaled to it).
  if(p.id==24){w.reloadMotion.reset();w.reloadRefillMs=2269;w.reloadMs=native_reload_presentation_ms(p.id);}
  for(const auto& cue:cues)if(cue.first==p.id)w.shotCue=cue.second;
  w.bodyCue=8168;result.push_back(w);
 }
 return result;
}
}
