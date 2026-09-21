#pragma once
#include <cmath>
#include <cstdint>

namespace mgo2mt::combat::blast_motion {
// Native impulse policy, not recovered original reaction constants. Optional
// per-weapon gameplay.json settings replace it; legacy HE grenade 52 defaults
// on, while other weapons require explicit configuration.
struct Policy {
 bool enabled=true;
 float horizontalSpeed=4500,upwardSpeed=2800,minimumScale=.25f,gravity=9800;
 uint32_t maxFlightMs=3000;
 bool operator==(const Policy&)const=default;
};
inline constexpr float maximum_velocity=15000;
inline bool valid(const Policy& p){
 return std::isfinite(p.horizontalSpeed)&&p.horizontalSpeed>=0&&p.horizontalSpeed<=20000&&
        std::isfinite(p.upwardSpeed)&&p.upwardSpeed>=0&&p.upwardSpeed<=15000&&
        (!p.enabled||p.horizontalSpeed+p.upwardSpeed>0)&&
        std::isfinite(p.minimumScale)&&p.minimumScale>=0&&p.minimumScale<=1&&
        std::isfinite(p.gravity)&&p.gravity>=100&&p.gravity<=50000&&
        p.maxFlightMs>=100&&p.maxFlightMs<=30000;
}
inline Policy default_for_weapon(uint16_t weapon){Policy result;result.enabled=weapon==52;return result;}
}
