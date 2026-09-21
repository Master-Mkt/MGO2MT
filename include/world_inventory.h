#pragma once
#include "item_drop_policy.h"
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <vector>
namespace mgo2mt::items {
// Preserve existing primary/secondary/support/equipment indices. Knife owns
// an additional slot, so the fixed grant never replaces the support grenade.
inline constexpr uint8_t equipment_slot=3,knife_slot=4,held_slot_count=5;
constexpr bool weapon_slot(uint8_t slot){return slot<3||slot==knife_slot;}
struct Actor {uint8_t slot=255;uint16_t instance=0;uint32_t character=0,life=0;bool operator==(const Actor&)const=default;};
struct Scope {uint64_t epoch=0,generation=0;bool operator==(const Scope&)const=default;};
struct EntityKey {Scope scope;uint64_t id=0;bool operator==(const EntityKey&)const=default;};
enum class Resource {durable,ammunition,charges};
struct Contents {
 uint32_t item=0,quantity=0,magazine=0,reserve=0,charges=0;Resource resource=Resource::durable;Domain domain=Domain::weapon;
 bool operator==(const Contents&)const=default;
 bool empty_resource()const noexcept {return resource==Resource::ammunition?magazine==0&&reserve==0:resource==Resource::charges&&charges==0;}
};
struct HeldSlot {Contents contents;uint64_t revision=1;bool operator==(const HeldSlot&)const=default;};
enum class PlacementKind {dropped,installed,round};
struct Position {float x=0,y=0,z=0,yaw=0,nx=0,ny=1,nz=0;bool operator==(const Position&)const=default;};
struct Entity {EntityKey key;uint64_t revision=1;PlacementKind kind=PlacementKind::dropped;Actor owner;Contents contents;Position position;bool operator==(const Entity&)const=default;};
struct Seed {Contents contents;Position position;};
struct Movement {EntityKey key;uint64_t revision=0;Position position;};
struct Capacity {uint32_t dropped=0,installed=0;};
struct SnapshotState {Scope scope;uint64_t revision=0;Capacity capacity;std::vector<Entity> entities;};
struct Request {Scope scope;Actor actor;uint64_t sequence=0;bool authorized=false;};
enum class ResultCode {ok,scope,identity,replay,unauthorized,invalid,stale,not_found,occupied,capacity,policy,exhausted,resource};
struct Result {ResultCode code=ResultCode::invalid;std::optional<Entity> entity;bool destroyed=false;explicit operator bool()const noexcept{return code==ResultCode::ok;}};
enum class Consume {magazine,reserve,charges};
// HOST-only component. Request.authorized is a caller decision after checking
// current activity/life, distance, ray/world, item compatibility and operation
// rights; never copy this flag or HeldSlot contents from a client request.
// All accesses to the supplied persistent Authority HeldSlot must share the
// Authority's serialization. This module atomically transfers that slot and
// registry; passing a temporary and later performing a fallible commit is wrong.
class WorldInventory {
 struct Peer {Actor actor;uint64_t sequence=0;};
 mutable std::mutex mutex_;Scope scope_;Capacity capacity_;uint64_t nextId_=1,revision_=0;
 std::map<uint64_t,Entity> entities_;std::map<uint32_t,Peer> peers_;
 bool seeded_=false;
 ResultCode begin(const Request&);
public:
 explicit WorldInventory(Capacity capacity):capacity_(capacity){}
 bool reset(Scope); // clears registry/admissions; invalid scope leaves unchanged
 bool configure(Capacity); // rejects shrinking below current counts
 // HOST-only initial batch; no player/held slot or drop permission is used.
 // Called once in an empty generation, before admitting players. Atomic.
 bool seed(Scope,std::span<const Seed>);
 bool admit(Actor); // same full identity may advance life; starts a new life-scoped replay floor
 void remove(Actor); // does not destroy world entities owned by that actor
 Result drop(const Request&,HeldSlot&,uint64_t expectedHeldRevision,Position,const DropPolicy&);
 Result install(const Request&,HeldSlot&,uint64_t expectedHeldRevision,uint32_t quantity,Position,const DropPolicy&);
 // HOST accepted weapon action: transfer exactly one loaded deployable into
 // an installed entity while retaining the owner's remaining ammunition.
 Result deploy(Actor,HeldSlot&,uint64_t expectedHeldRevision,Position);
 bool erase_deployed(EntityKey,uint64_t expectedEntityRevision);
 Result pickup(const Request&,EntityKey,uint64_t expectedEntityRevision,HeldSlot&,uint64_t expectedHeldRevision);
 // Serialized HOST physics/contact operations; do not consume client sequences.
 bool move(Scope,std::span<const Movement>); // all-or-nothing, excludes installed
 Result contact_pickup(Actor,EntityKey,uint64_t,HeldSlot&,uint64_t);
 Result consume(const Request&,EntityKey,uint64_t expectedEntityRevision,Consume,uint32_t amount,const DropPolicy&);
 std::vector<Entity> snapshot()const;
 SnapshotState state()const; // one lock: metadata and entity rows are consistent
 size_t size(PlacementKind)const;
};
}
