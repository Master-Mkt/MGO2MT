#pragma once
#include "item_drop_policy.h"
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <vector>
namespace mgo2win::items {
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
enum class PlacementKind {dropped,installed};
struct Position {float x=0,y=0,z=0,yaw=0;bool operator==(const Position&)const=default;};
struct Entity {EntityKey key;uint64_t revision=1;PlacementKind kind=PlacementKind::dropped;Actor owner;Contents contents;Position position;bool operator==(const Entity&)const=default;};
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
 ResultCode begin(const Request&);
public:
 explicit WorldInventory(Capacity capacity):capacity_(capacity){}
 bool reset(Scope); // clears registry/admissions; invalid scope leaves unchanged
 bool configure(Capacity); // rejects shrinking below current counts
 bool admit(Actor); // same full identity may advance life; starts a new life-scoped replay floor
 void remove(Actor); // does not destroy world entities owned by that actor
 Result drop(const Request&,HeldSlot&,uint64_t expectedHeldRevision,Position,const DropPolicy&);
 Result install(const Request&,HeldSlot&,uint64_t expectedHeldRevision,uint32_t quantity,Position,const DropPolicy&);
 Result pickup(const Request&,EntityKey,uint64_t expectedEntityRevision,HeldSlot&,uint64_t expectedHeldRevision);
 Result consume(const Request&,EntityKey,uint64_t expectedEntityRevision,Consume,uint32_t amount,const DropPolicy&);
 std::vector<Entity> snapshot()const;
 SnapshotState state()const; // one lock: metadata and entity rows are consistent
 size_t size(PlacementKind)const;
};
}
