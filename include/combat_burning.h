#pragma once
#include "stage_collision.h"
#include <array>
#include <cstdint>
#include <vector>

namespace mgo2mt::combat::burning {
using Vec3=stage::Vec3;
struct Key {uint64_t epoch=0;uint8_t slot=255;uint16_t instance=0;uint32_t character=0,life=0;bool operator==(const Key&)const=default;};
bool valid(Key)noexcept;
struct Source {Key actor;uint16_t weapon=0;uint32_t object=0;uint8_t team=0;bool operator==(const Source&)const=default;};
// Explicit native values, not the unresolved original 3000/4500 raw-clock path.
struct Policy {uint32_t durationMs=6000,damagePermillePerSecond=50;};
bool valid(Policy)noexcept;
struct Step {uint32_t damage=0,remainingMs=0;bool burning=false,changed=false;};
class State {
 Key target_;Source source_;Policy policy_;uint64_t until_=0,at_=0,event_=0,remainder_=0;bool clock_=false;
public:
 void bind(Key); // Only HOST identity/life admission may change this scope.
 void clear();
 bool ignite(Key target,Source,uint64_t acceptedEvent,uint64_t now,Policy={});
 Step advance(Key target,uint64_t now,uint32_t maxHp,bool alive,bool active,bool submerged);
 const Source& source()const noexcept{return source_;}
 Key key()const noexcept{return target_;}
 bool active()const noexcept{return until_!=0;}
};
struct Replay {
 struct Stamp {Source source;uint64_t serial=0;};
 std::array<Stamp,64> channels{};uint64_t epoch=0;
 bool accept(Source,uint64_t); // Full owner/life + weapon + object channels.
};
// Trusted HOST producer only, with a nonzero actual source weapon for damage
// events. Object-only sources have no wire identity and are rejected.
// Radius/damage are never accepted from client wire.
struct Blast {Source source;uint64_t serial=0;Vec3 position{};float radius=0;uint32_t damage=0;bool ignite=false;uint32_t staminaDamage=0;};
bool valid(const Blast&)noexcept;
// A capsule-nearest point, bounded sphere radius and actual occlusion ray form
// the native exposure test. It does not claim the original blast hit shape.
bool exposed(const Blast&,Vec3 feet,stage::Capsule,const stage::Collision* world,const stage::Collision* objects=nullptr);
struct Line {Vec3 from{},to{};std::array<float,4> rgba{};};
// Stateless procedural flame presentation follows the current snapshot only:
// no replay timer, no damage authority, and no residual effect after clear/life.
std::vector<Line> flames(Key,bool burning,Vec3 feet,stage::Capsule,uint64_t now);
}
