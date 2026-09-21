#pragma once
#include <cmath>
namespace mgo2mt {
// Source XYZ/bones/UV/collision stay untouched. The source camera's horizontal
// basis is opposite to D3D's LH camera. Apply once at the view boundary only.
inline constexpr float source_screen_x = -1.f;
inline float source_movement_angle(float right,float forward){return std::atan2(source_screen_x*right,forward);}
}
