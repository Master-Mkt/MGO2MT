#pragma once
#include "stage_navigation.h"
#include <cstdint>
namespace mgo2mt::stage {
// Native procedural presentation. No original texture, sound, or event timing is claimed.
struct WaterEffectLine {Vec3 from{},to{};float alpha=0;bool splash=false;};
class WaterEffects {
 struct Effect {Vec3 position{},velocity{};float level=0,age=0,radius=0;bool splash=false;};
 std::vector<Effect> effects_;
 std::uint64_t epoch_=0,life_=0;Vec3 previous_{};
 bool initialized_=false,wet_=false;float distance_=0,cooldown_=0,previousLevel_=0;
 void emit(Vec3,float);
public:
 static constexpr std::size_t maximumEffects=64,rippleCount=3,splashCount=5,rippleSegments=24;
 static constexpr float lifetime=1.2f,emissionDistance=200.f,minimumInterval=.14f;
 void reset();
 void update(std::uint64_t epoch,std::uint64_t life,Vec3 position,const NavigationWaterState&,bool active,float seconds);
 std::vector<WaterEffectLine> lines()const;
 std::size_t size()const{return effects_.size();}
};
}
