#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace mgo2win::original::throwing {
// Current ELF evidence: notes/ORIGINAL_THROW_POLICY_20260913.md.
// These are original coordinate/clock values, not SI or final range multipliers.
inline constexpr std::uint16_t grenade_weapon_id=52;
inline constexpr std::array<float,4> launch_multipliers{1.f,1.15f,1.3f,1.5f};
inline constexpr std::array<float,3> observed_base_magnitudes{18000.f,12000.f,6000.f};
inline constexpr float airborne_gravity_y=-9800.f;
inline constexpr std::int32_t release_fuse_ticks=600;
struct Vec3 { float x=0,y=0,z=0; };
inline bool finite(Vec3 a) { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z); }

// 39CEE8/39CF38/39E0C4 multiply the selected magnitude before direction and
// actor orientation are applied. Selection of those branches is not restored.
inline std::optional<float> scaled_launch_magnitude(float base,unsigned skillLevel) {
    if(skillLevel>=launch_multipliers.size()||!std::isfinite(base)||base<0)return {};
    const float result=base*launch_multipliers[skillLevel];
    if(!std::isfinite(result))return {};
    return result;
}
// Caller supplies attack+180 AFTER original skill/direction/orientation work.
// Owner+2D0 is added AFTER that; never multiply the owner's vector by the skill.
// Preserve float32 add, *0.001, *1000 as separate operations (not cancellation).
inline std::optional<Vec3> assemble_projectile_velocity(Vec3 attack,Vec3 owner) {
    if(!finite(attack)||!finite(owner))return {};
    Vec3 out;
    const auto component=[](float a,float b) {
        const float sum=a+b;
        const float argument=sum*0.001f; // 39ECC8..39ECF4
        return argument*1000.f;         // 7B9C5C/6C/7C -> +240/+244/+248
    };
    out={component(attack.x,owner.x),component(attack.y,owner.y),component(attack.z,owner.z)};
    if(!finite(out))return {};
    return out;
}
struct AirbornePrediction { Vec3 segmentEnd,velocityAfterGravity; };
// 7B7410/742C forms the collision-query endpoint using OLD velocity; only later
// 7B75C4 updates velocity.y. This is a prediction for an ordinary airborne branch,
// not a collision solver or an unconditional projectile actor update.
// dt is the caller's float clock record[0]; integer record[12] is a separate input.
inline std::optional<AirbornePrediction> predict_airborne_step(Vec3 position,Vec3 velocity,float dt) {
    if(!finite(position)||!finite(velocity)||!std::isfinite(dt)||dt<0)return {};
    const Vec3 displacement{velocity.x*dt,velocity.y*dt,velocity.z*dt};
    AirbornePrediction out{{position.x+displacement.x,position.y+displacement.y,position.z+displacement.z},velocity};
    out.velocityAfterGravity.y=std::fma(dt,airborne_gravity_y,velocity.y);
    if(!finite(displacement)||!finite(out.segmentEnd)||!finite(out.velocityAfterGravity))return {};
    return out;
}
struct FuseStep { std::int32_t remaining; bool callbackDue; };
// 7B8EA4/AC and constructor 7B9E2C..3C subtract first, then test <=0.
// Stateless: this does not promise exactly-once delivery, destruction or a fuse
// beginning on input press. The original constructor already consumes one delta.
inline std::optional<FuseStep> advance_fuse(std::int32_t remaining,std::int32_t tickDelta) {
    if(tickDelta<0)return {};
    const std::int64_t next=std::int64_t(remaining)-tickDelta;
    if(next<std::numeric_limits<std::int32_t>::min())return {};
    return FuseStep{static_cast<std::int32_t>(next),next<=0};
}
}
