#pragma once
#include "stage_weather_surface.h"
#include "combat_tracer.h"
namespace mgo2mt::stage::weather {
// Native bounded geometric rain/snow, not recovered original particle assets.
std::vector<combat::tracers::Segment> precipitation(const Settings&,double seconds,Vec3 eye,const Collision*);
}
