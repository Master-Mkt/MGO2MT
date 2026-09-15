#pragma once
#include "water_effects.h"
#include <map>
namespace mgo2win::stage {
// Cosmetic finite-surface crossings; no water volume, damage, speed or audio.
// All counts, trajectory and time limits here are explicit native choices.
struct WaterSurfaceScope {uint64_t epoch=0,scene=0;bool operator==(const WaterSurfaceScope&)const=default;};
struct WaterSurfaceActor {uint64_t identity=0,life=0;Vec3 point{};};
constexpr uint64_t water_surface_identity(uint8_t slot,uint16_t instance,uint32_t character){return (uint64_t(slot)<<48)|(uint64_t(instance)<<32)|character;}
class WaterSurfaceEffects {
 struct Track {uint64_t life=0,lastEmission=0;Vec3 point{};bool emitted=false;};
 struct Drop {uint64_t actor=0,life=0,born=0;Vec3 point{},velocity{};};
 WaterSurfaceScope scope_;uint64_t lastNow_=0;bool initialized_=false;
 std::map<uint64_t,Track> tracks_;std::vector<Drop> drops_;
public:
 static constexpr size_t maximumActors=24,maximumDrops=288,dropsPerCrossing=12;
 static constexpr uint64_t lifetimeMs=600,minimumIntervalMs=140;
 void reset();
 void update(WaterSurfaceScope,std::span<const WaterSurfaceActor>,const WaterSurface*,uint64_t now,bool active);
 std::vector<WaterEffectLine> lines(uint64_t now)const;
 size_t size()const{return drops_.size();}
};
}
