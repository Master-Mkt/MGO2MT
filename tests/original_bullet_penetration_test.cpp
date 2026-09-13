#ifdef NDEBUG
#undef NDEBUG
#endif
#include "original_bullet_penetration.h"
#include <cassert>
#include <iostream>
using namespace original_bullet_penetration;
int main() {
    const auto ordinary = surface(0x40, 100, -1.f);
    assert(ordinary && ordinary->resistance == 100 && ordinary->forceCost == 100);
    assert(surface(0x8000, 1000, -1.f)->resistance == 0);
    assert(surface(0x800000000000ULL, 1000, -1.f)->resistance == 1000);
    assert(surface(0x800000, 100, -1.f)->forceCost == 100); // mark flag not free
    assert(surface(0x40, std::nullopt, -1.f)->resistance == 1000);
    assert(surface(0x40, std::nullopt, 0.f)->forceCost == 0);
    assert(surface(0x40, 250, 1.f)->resistance == 0);
    assert(!surface(0, -1, -1.f));
    assert(!surface(0, 100, std::numeric_limits<float>::quiet_NaN()));
    auto first = cross(ak102_budget, *ordinary); assert(first->passes && first->remaining == 150);
    auto second = cross(first->remaining, *ordinary); assert(second->passes && second->remaining == 50);
    assert(!cross(second->remaining, *ordinary)->passes);
    assert(!cross(250, {250,100})->passes);
    assert(cross(250, {249,100})->remaining == 1);
    assert(!cross(0, {0,0})->passes);
    assert(!cross(-1, {0,0}));
    assert(advance_force(1000,0,1000,0) == 1000);
    assert(advance_force(1000,0,1000,200) == 800);
    assert(advance_force(1000,14900,100,0) == 1000);
    assert(advance_force(1000,14900,200,0) == 1000); // 100*450/65000 truncates0
    assert(advance_force(1000,14900,300,100) == 899);
    assert(advance_force(1000,15000,6500,100) == 855);
    assert(advance_force(600,15000,100,100) == 500); // original transient undershoot
    assert(advance_force(500,15100,100,100) == 550); // following update clamp
    assert(advance_force(-100,15100,100,100) == 550);
    assert(advance_force(1000,79900,100,200) == 550); // end branch bypasses cost
    assert(!advance_force(1000,0,-1,0));
    assert(!advance_force(1001,0,1,0));
    assert(!advance_force(1000,0,1,std::numeric_limits<int>::max()));
    assert(!advance_force(1000,std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),0));
    assert(target_force(1000,0,10000,0) == 1000);
    assert(target_force(1000,0,10000,100) == 900);
    assert(target_force(1000,0,10000,200) == 800);
    assert(ak102_base_hp(1000) == 275);
    assert(ak102_base_hp(900) == 247);
    assert(ak102_base_hp(800) == 220);
    assert(ak102_base_hp(550) == 151);
    assert(ak102_base_hp(0) == 275); // raw getter sentinel, not immunity
    assert(ak102_base_hp(-1) == 0); // signed truncation rather than floor
    assert(ak102_base_hp(-900) == -247);
    assert(!ak102_base_hp(32768));
    std::cout << "original AK102 surface budget and per-update force boundaries PASS\n";
}
