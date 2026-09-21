#include "original_throw_policy.h"
#include <algorithm>
#include <bit>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace mgo2mt::original::throwing;
void require(bool v,std::source_location at=std::source_location::current()) {
    if(!v) { std::cerr<<"original throw policy failed at line "<<at.line()<<'\n';throw std::runtime_error("original throw policy"); }
}
void close(float a,float b) { require(std::abs(a-b)<=0.000001f*std::max(1.f,std::abs(b))); }
int main() {
    require(grenade_weapon_id==52&&release_fuse_ticks==600);
    constexpr std::array<std::uint32_t,4> bits{0x3f800000,0x3f933333,0x3fa66666,0x3fc00000};
    for(unsigned level=0;level<4;++level) {
        require(std::bit_cast<std::uint32_t>(launch_multipliers[level])==bits[level]);
        for(float base:observed_base_magnitudes)require(scaled_launch_magnitude(base,level)==base*launch_multipliers[level]);
    }
    const auto v=assemble_projectile_velocity({*scaled_launch_magnitude(12000,3),0,-6000},{300,500,100});
    require(v.has_value());close(v->x,18300);close(v->y,500);close(v->z,-5900);
    // Exposes the actual float32 serialization round trip: algebraic cancellation
    // to attack+owner loses this rounding, even though both scale factors look reciprocal.
    const auto roundtrip=assemble_projectile_velocity({5,0,0},{0,0,0});
    require(roundtrip.has_value());require(std::bit_cast<std::uint32_t>(roundtrip->x)==0x40a00001);
    const auto step=predict_airborne_step({100,200,300},{1000,2000,-3000},0.1f);
    require(step.has_value());close(step->segmentEnd.x,200);close(step->segmentEnd.y,400);close(step->segmentEnd.z,0);
    close(step->velocityAfterGravity.y,1020); // old velocity endpoint, no gravity-first or half-g term
    const auto next=predict_airborne_step(step->segmentEnd,step->velocityAfterGravity,0.1f);
    require(next.has_value());close(next->segmentEnd.y,502);close(next->velocityAfterGravity.y,40);
    const auto zero=predict_airborne_step({1,2,3},{4,5,6},0);require(zero&&zero->segmentEnd.y==2&&zero->velocityAfterGravity.y==5);
    auto fuse=advance_fuse(release_fuse_ticks,5);require(fuse&&fuse->remaining==595&&!fuse->callbackDue); // constructor
    for(unsigned i=0;i<118;++i) { fuse=advance_fuse(fuse->remaining,5);require(fuse&&!fuse->callbackDue); }
    require(fuse->remaining==5);fuse=advance_fuse(fuse->remaining,5);require(fuse&&fuse->remaining==0&&fuse->callbackDue);
    const auto late=advance_fuse(4,5);require(late&&late->remaining==-1&&late->callbackDue);
    require(advance_fuse(0,0)->callbackDue&&!advance_fuse(1,0)->callbackDue);
    const float inf=std::numeric_limits<float>::infinity(),nan=std::numeric_limits<float>::quiet_NaN(),max=std::numeric_limits<float>::max();
    require(!scaled_launch_magnitude(1,4)&&!scaled_launch_magnitude(-1,0)&&!scaled_launch_magnitude(inf,0)&&!scaled_launch_magnitude(max,3));
    require(!assemble_projectile_velocity({nan,0,0},{})&&!assemble_projectile_velocity({max,0,0},{max,0,0}));
    require(!predict_airborne_step({},{},-1)&&!predict_airborne_step({},{},nan)&&!predict_airborne_step({},{inf,0,0},0));
    require(!predict_airborne_step({max,0,0},{max,0,0},2));
    require(!advance_fuse(10,-1)&&!advance_fuse(std::numeric_limits<std::int32_t>::min(),1));
    std::cout<<"PASS original grenade ID, skill bits, launch ownership/order, airborne segment/gravity, constructor fuse/boundaries and invalid/overflow rejection\n";
}
