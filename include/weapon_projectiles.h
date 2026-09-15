#pragma once
#include "stage_collision.h"
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>
namespace mgo2win::projectile {
using Vec3=stage::Vec3;
struct Scope {uint64_t epoch=0,scene=0;bool operator==(const Scope&)const=default;};
struct Owner {uint8_t slot=255;uint16_t instance=0;uint32_t character=0,life=0;bool operator==(const Owner&)const=default;};
struct Profile {
 uint16_t weapon=0;float speed=0,gravity=0,range=0;uint32_t ttlMs=0,fuseMs=0;
 bool explodeOnContact=false;float bounceRestitution=0,trailSpacing=0;
 bool operator==(const Profile&)const=default;
};
// IDs and MK2 range/speed are original table data. Remaining flight/fuse,
// smoke and collision values below are explicit native simulation policies.
inline constexpr Profile native_mk2{2,145000,0,76000,1000,0,true,0,0};
inline constexpr Profile native_rpg7{50,30000,0,150000,6000,0,true,0,600};
inline constexpr Profile native_gekko_missile{129,30000,0,150000,6000,0,true,0,600};
inline constexpr Profile native_white_phosphorus{53,12000,9800,100000,6000,3000,false,.35f,0};
bool valid(const Profile&);
bool valid(Owner);
// Trace must return the nearest admitted world/object/body hit on this finite
// segment, including friendly bodies as blockers. Damage/FF remain HOST-owned.
// Normal must be finite and unit length; target may be absent for static world.
struct Contact {float distance=0;Vec3 normal{};std::optional<Owner> target;uint32_t object=0;};
using Trace=std::function<std::optional<Contact>(Vec3 origin,Vec3 direction,float maximum,Owner source)>;
struct Shot {Scope scope;Owner owner;uint64_t acceptedShotId=0;uint16_t weapon=0;Vec3 origin{},direction{};};
struct Impact {Scope scope;Owner owner;uint64_t acceptedShotId=0;uint16_t weapon=0;Vec3 position{},normal{};std::optional<Owner> target;uint32_t object=0;bool fuse=false;};
struct Trail {Scope scope;Owner owner;uint64_t acceptedShotId=0;uint16_t weapon=0;Vec3 position{};};
struct Step {std::vector<Impact> impacts;std::vector<Trail> trails;size_t discarded=0;};
enum class Submit {accepted,scope,identity,profile,sequence,capacity,clock,invalid};
class Pool {
 struct Entry {Shot shot;Profile policy;Vec3 position{},velocity{};uint64_t born=0,at=0;float traveled=0,trailRemainder=0;};
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
 bool available()const{return entries_.size()<capacity_;}
 size_t size()const{return entries_.size();}
 const Scope& scope()const{return scope_;}
 std::vector<Owner> owners()const; // replay channels, including completed shots
};
struct Ammo {uint16_t capacity=0,magazine=0,reserve=0;bool operator==(const Ammo&)const=default;};
// Native magazine/reserve accounting (reserve excludes loaded rounds). Pure
// checked transactions; empty magazine is reloadable, dry/no-reserve is not.
std::optional<Ammo> refill(Ammo);
std::optional<Ammo> consume(Ammo);
}
