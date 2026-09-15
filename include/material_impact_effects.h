#pragma once
#include "bullet_decals.h"
#include <deque>
namespace mgo2win::combat::material_effects {
using Vec3=decals::Vec3;
using Scope=decals::Scope;
using Impact=decals::Impact;
enum class Kind : uint8_t { unknown, metal, wood, stone, glass };
// Current GCX named option E86D7E -> material word5 -> typed resource registry.
// Only maps 1/4/20/21 and the two verified callback families are classified.
// Classification identifies the effect family, not original camera/effect gates,
// renderer coefficients, shader or particle texture parity. See follow-up note.
Kind verified_kind(uint8_t map,uint32_t currentMaterialHash) noexcept;
// Without a verified stage, a material hash alone cannot enable an effect.
inline Kind verified_kind(uint32_t /*currentMaterialHash*/) noexcept {return Kind::unknown;}
// Explicit Windows procedural presentation. None of these numbers are original
// EFF particle count, velocity, gravity, color or lifetime claims.
struct Policy {size_t capacity=256;uint64_t lifetimeMs=550;unsigned particlesPerHit=8;
 float speed=900,gravity=1800,surfaceOffset=2,trailSeconds=.025f;};
struct Line {Vec3 from{},to{};std::array<float,4> rgba{};uint64_t eventId=0;Kind kind=Kind::unknown;};
class Pool {
 struct Particle {Vec3 position{},velocity{};uint64_t born=0,event=0;Kind kind=Kind::unknown;};
 Policy policy_;Scope scope_;std::deque<Particle> particles_;
 uint64_t played_=0,watermark_=0,lastNow_=0;bool established_=false;
 bool expire(uint64_t);
public:
 static constexpr size_t maximumCapacity=256;
 static bool valid(const Policy&)noexcept;
 explicit Pool(Policy={});
 void clear();
 void synchronize(Scope,uint64_t committedEventWatermark,uint64_t now);
 // Only freshly validated static-solid collision and known original dispatch
 // classification may call this. Rejected unknown/water/body events consume ID.
 bool emit(const Impact&,Kind,uint64_t now);
 std::vector<Line> lines(uint64_t now);
 size_t size()const noexcept{return particles_.size();}
};
}
