#pragma once
#include "combat_particle_effects.h"
#include "stage_collision.h"
#include "stage_weather_surface.h"
#include <string_view>
namespace mgo2mt::stage::weather {
// User-requested native QQ preset, using original sandstorm/dust images and GCX colors.
// No original weather VM, network gameplay modifier, or snow simulation is claimed.
struct Frame {
 bool active=false,outdoors=true;
 float strength=0,nearDistance=6000,farDistance=65000,maximum=.78f,skyAmount=.46f;
 std::array<float,3> color{104.f/255.f,102.f/255.f,47.f/255.f};
 std::vector<combat::particles::Sprite> dust;
 std::shared_ptr<const SurfaceGrid> surface;
};
float storm_strength(double elapsedSeconds) noexcept;
float fog_amount(float viewDistance,const Frame&) noexcept;
float reverse_depth_distance(float depth) noexcept;
class Controller {
public:
 Frame sample(std::string_view stage,double elapsedSeconds,Vec3 eye,const Collision* collision=nullptr) const;
 Frame sample(bool fog,bool sand,double elapsedSeconds,Vec3 eye,const Collision* collision=nullptr) const;
};
}
