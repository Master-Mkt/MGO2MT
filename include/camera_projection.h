#pragma once
#include <cmath>
namespace mgo2mt {
inline constexpr float default_vertical_fov=1.f;
inline bool valid_vertical_fov(float fov){return std::isfinite(fov)&&fov>0.f&&fov<3.14159265358979323846f;}
}
