#pragma once
#include "stage_collision.h"
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>
namespace mgo2mt::projectile {
using Vec3=stage::Vec3;
struct Scope {uint64_t epoch=0,scene=0;bool operator==(const Scope&)const=default;};
struct Owner {uint8_t slot=255;uint16_t instance=0;uint32_t character=0,life=0;bool operator==(const Owner&)const=default;};
struct Profile {
 uint16_t weapon=0;float speed=0,gravity=0,range=0;uint32_t ttlMs=0,fuseMs=0;
 bool explodeOnContact=false;float bounceRestitution=0,trailSpacing=0;
 float blastRadius=0; // HOST launch policy; zero retains the existing weapon adapter radius.
 bool operator==(const Profile&)const=default;
};
// IDs and MK2 range/speed are original table data. Remaining flight/fuse,
// smoke and collision values below are explicit native simulation policies.
inline constexpr Profile native_mk2{2,145000,0,76000,1000,0,true,0,0};
inline constexpr Profile native_rpg7{50,30000,0,150000,6000,0,true,0,600};
inline constexpr Profile native_gekko_missile{129,30000,0,150000,6000,0,true,0,600};
inline constexpr Profile native_white_phosphorus{53,12000,9800,100000,6000,3000,false,.35f,0};
constexpr bool throwable(uint16_t id){return (id>=52&&id<=59)||id==63;}
// Medium throw is an explicit native input policy. ID52's launch magnitude,
// gravity and 600-tick fuse are recovered; other grenade clocks are native.
inline Profile native_throw(uint16_t id){return {id,12000,9800,100000,6000,id==52?2002u:3000u,false,.35f,0};}
bool valid(const Profile&);
inline bool valid(Owner o){return o.slot<24&&o.instance&&o.character&&o.life;}
// Trace must return the nearest admitted world/object/body hit on this finite
// segment, including friendly bodies as blockers. Damage/FF remain HOST-owned.
// Normal must be finite and unit length; target may be absent for static world.
struct Contact {float distance=0;Vec3 normal{};std::optional<Owner> target;uint32_t object=0;};
using Trace=std::function<std::optional<Contact>(Vec3 origin,Vec3 direction,float maximum,Owner source)>;
// Category travels with the accepted flight, not the owner's current weapon.
// The owner can switch equipment while a grenade is still in the air.
using TypedTrace=std::function<std::optional<Contact>(Vec3 origin,Vec3 direction,float maximum,Owner source,uint16_t weapon)>;
inline std::optional<stage::CollisionQuery> collision_query(uint16_t weapon){
 if(weapon==2)return stage::query::bullet;
 if(weapon==50||weapon==129)return stage::query::missile;
 if(throwable(weapon)||weapon==103)return stage::query::bomb;
 return {};
}
struct Shot {Scope scope;Owner owner;uint64_t acceptedShotId=0;uint16_t weapon=0;Vec3 origin{},direction{};};
struct Impact {Scope scope;Owner owner;uint64_t acceptedShotId=0;uint16_t weapon=0;Vec3 position{},normal{};std::optional<Owner> target;uint32_t object=0;bool fuse=false;float blastRadius=0;};
struct Trail {Scope scope;Owner owner;uint64_t acceptedShotId=0;uint16_t weapon=0;Vec3 position{};};
struct Step {std::vector<Impact> impacts;std::vector<Trail> trails;size_t discarded=0;};
// Actual point-sweep diagnostics; no invented sphere radius. Last endpoints
// are the exact most recent finite segment passed to Trace.
struct DebugFlight {Owner owner;uint64_t shot=0;uint16_t weapon=0;Vec3 position{},traceFrom{},traceTo{};uint64_t simulatedAt=0;bool operator==(const DebugFlight&)const=default;};
enum class Submit {accepted,scope,identity,profile,sequence,capacity,clock,invalid};
class Pool {
 struct Entry {Shot shot;Profile policy;Vec3 position{},velocity{};uint64_t born=0,at=0;float traveled=0,trailRemainder=0;Vec3 traceFrom{},traceTo{};};
 struct Seen {Owner owner;uint64_t shot=0;};
 Scope scope_;size_t capacity_;std::vector<Entry> entries_;std::array<std::optional<Seen>,24> seen_{};
 uint64_t clock_=0;bool clockArmed_=false;
public:
 explicit Pool(size_t capacity=128);
 void reset(Scope); // epoch/scene replacement cancels all simulation and replay history
 void remove(Owner); // exact incarnation only; permits a new life in this slot
 Submit spawn(const Shot&,const Profile&,uint64_t now);
 // Native bounded 20ms segments, max 1000ms catch-up. Excessive gap/rollback
 // cancels flight without damage, never teleports a missile through geometry.
 // No ammunition mutation: caller must reserve capacity before committing shot.
 Step advance(uint64_t now,const Trace&);
 Step advance_typed(uint64_t now,const TypedTrace&);
 bool available()const{return entries_.size()<capacity_;}
 size_t size()const{return entries_.size();}
 const Scope& scope()const{return scope_;}
 std::vector<Owner> owners()const; // replay channels, including completed shots
 std::vector<DebugFlight> debug_flights(size_t limit=16)const;
};
struct Ammo {uint16_t capacity=0,magazine=0,reserve=0;bool operator==(const Ammo&)const=default;};
// Native magazine/reserve accounting (reserve excludes loaded rounds). Pure
// checked transactions; empty magazine is reloadable, dry/no-reserve is not.
std::optional<Ammo> refill(Ammo);
std::optional<Ammo> consume(Ammo);
}
