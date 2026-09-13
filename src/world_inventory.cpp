#include "world_inventory.h"
#include <cmath>
#include <limits>
#include <new>
namespace mgo2win::items {
namespace {
bool valid(Actor a){return a.slot<24&&a.instance&&a.character&&a.life;}
bool same_identity(Actor a,Actor b){return a.slot==b.slot&&a.instance==b.instance&&a.character==b.character;}
bool valid(Position p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::isfinite(p.yaw);}
bool valid(const Contents& c){
 if(!c.item||!c.quantity||c.domain>Domain::world_item)return false;
 switch(c.resource){case Resource::durable:return !c.magazine&&!c.reserve&&!c.charges;
 case Resource::ammunition:return c.quantity==1&&!c.charges;
 case Resource::charges:return !c.magazine&&!c.reserve;
 default:return false;}
}
bool empty(const HeldSlot& h){return h.contents==Contents{};}
size_t count(const std::map<uint64_t,Entity>& es,PlacementKind k){size_t n=0;for(const auto&[id,e]:es)if(e.kind==k)++n;return n;}
}
bool WorldInventory::reset(Scope scope){std::lock_guard lock(mutex_);if(!scope.epoch||!scope.generation)return false;if(scope==scope_)return true;if(scope_.epoch&&(scope.epoch<scope_.epoch||(scope.epoch==scope_.epoch&&scope.generation<scope_.generation)))return false;scope_=scope;revision_=1;entities_.clear();peers_.clear();nextId_=1;return true;}
bool WorldInventory::configure(Capacity c){std::lock_guard lock(mutex_);if(count(entities_,PlacementKind::dropped)>c.dropped||count(entities_,PlacementKind::installed)>c.installed)return false;if(capacity_.dropped!=c.dropped||capacity_.installed!=c.installed){if(revision_==UINT64_MAX)return false;capacity_=c;++revision_;}return true;}
bool WorldInventory::admit(Actor a){std::lock_guard lock(mutex_);if(!valid(a)||!scope_.epoch)return false;auto it=peers_.find(a.character);if(it!=peers_.end()){if(!same_identity(a,it->second.actor)||a.life<it->second.actor.life)return false;if(a.life>it->second.actor.life)it->second.sequence=0;it->second.actor=a;return true;}for(const auto&[id,p]:peers_)if(p.actor.slot==a.slot)return false;peers_.emplace(a.character,Peer{a});return true;}
void WorldInventory::remove(Actor a){std::lock_guard lock(mutex_);auto i=peers_.find(a.character);if(i!=peers_.end()&&i->second.actor==a)peers_.erase(i);}
ResultCode WorldInventory::begin(const Request& r){
 if(r.scope!=scope_||!scope_.epoch)return ResultCode::scope;
 auto p=peers_.find(r.actor.character);if(!valid(r.actor)||p==peers_.end()||p->second.actor!=r.actor)return ResultCode::identity;
 if(!r.sequence||r.sequence<=p->second.sequence)return ResultCode::replay;
 p->second.sequence=r.sequence; // rejected attempts cannot be replayed later
 return r.authorized?ResultCode::ok:ResultCode::unauthorized;
}
Result WorldInventory::drop(const Request& r,HeldSlot& held,uint64_t revision,Position position,const DropPolicy& policy){
 std::lock_guard lock(mutex_);auto code=begin(r);if(code!=ResultCode::ok)return {code};
 if(!held.revision||held.revision!=revision)return {ResultCode::stale};
 if(!valid(held.contents)||!valid(position))return {ResultCode::invalid};
 if(!policy.allows_drop())return {ResultCode::policy};
 if(held.revision==UINT64_MAX||revision_==UINT64_MAX)return {ResultCode::exhausted};
 if(held.contents.empty_resource()&&policy.discards_empty()){held.contents={};++held.revision;++revision_;return {ResultCode::ok,{},true};}
 if(count(entities_,PlacementKind::dropped)>=capacity_.dropped)return {ResultCode::capacity};
 if(!nextId_||nextId_==UINT64_MAX)return {ResultCode::exhausted};
 Entity e{{scope_,nextId_},1,PlacementKind::dropped,r.actor,held.contents,position};
 try{entities_.emplace(nextId_,e);}catch(const std::bad_alloc&){return {ResultCode::capacity};}
 ++nextId_;held.contents={};++held.revision;++revision_;return {ResultCode::ok,e};
}
Result WorldInventory::install(const Request& r,HeldSlot& held,uint64_t revision,uint32_t quantity,Position position,const DropPolicy&){
 std::lock_guard lock(mutex_);auto code=begin(r);if(code!=ResultCode::ok)return {code};
 if(!held.revision||held.revision!=revision)return {ResultCode::stale};
 if(!valid(held.contents)||!valid(position)||!quantity||quantity>held.contents.quantity)return {ResultCode::invalid};
 // Installing/using an item is distinct from dropping it. The caller's
 // operation permission decides installability, not the drop-only override.
 if(held.contents.empty_resource())return {ResultCode::resource};
 if(quantity!=held.contents.quantity&&held.contents.resource!=Resource::durable)return {ResultCode::invalid};
 if(revision_==UINT64_MAX||held.revision==UINT64_MAX||!nextId_||nextId_==UINT64_MAX)return {ResultCode::exhausted};
 if(count(entities_,PlacementKind::installed)>=capacity_.installed)return {ResultCode::capacity};
 auto contents=held.contents;contents.quantity=quantity;
 Entity e{{scope_,nextId_},1,PlacementKind::installed,r.actor,contents,position};
 try{entities_.emplace(nextId_,e);}catch(const std::bad_alloc&){return {ResultCode::capacity};}
 ++nextId_;held.contents.quantity-=quantity;if(!held.contents.quantity)held.contents={};++held.revision;++revision_;return {ResultCode::ok,e};
}
Result WorldInventory::pickup(const Request& r,EntityKey key,uint64_t revision,HeldSlot& destination,uint64_t heldRevision){
 std::lock_guard lock(mutex_);auto code=begin(r);if(code!=ResultCode::ok)return {code};
 if(key.scope!=scope_)return {ResultCode::scope};
 auto found=entities_.find(key.id);if(found==entities_.end())return {ResultCode::not_found};
 if(found->second.revision!=revision||!destination.revision||destination.revision!=heldRevision)return {ResultCode::stale};
 if(!empty(destination))return {ResultCode::occupied};
 if(revision_==UINT64_MAX||destination.revision==UINT64_MAX)return {ResultCode::exhausted};
 const auto e=found->second;destination.contents=e.contents;++destination.revision;entities_.erase(found);++revision_;return {ResultCode::ok,e};
}
Result WorldInventory::consume(const Request& r,EntityKey key,uint64_t revision,Consume which,uint32_t amount,const DropPolicy& policy){
 std::lock_guard lock(mutex_);auto code=begin(r);if(code!=ResultCode::ok)return {code};
 if(key.scope!=scope_)return {ResultCode::scope};
 auto found=entities_.find(key.id);if(found==entities_.end())return {ResultCode::not_found};
 const auto& previous=found->second;if(previous.revision!=revision)return {ResultCode::stale};
 if(previous.kind!=PlacementKind::installed||!amount)return {ResultCode::invalid};
 if(revision_==UINT64_MAX||previous.revision==UINT64_MAX)return {ResultCode::exhausted};
 auto e=previous;uint32_t* value=nullptr;
 if(e.contents.resource==Resource::ammunition){if(which==Consume::magazine)value=&e.contents.magazine;else if(which==Consume::reserve)value=&e.contents.reserve;}
 else if(e.contents.resource==Resource::charges&&which==Consume::charges)value=&e.contents.charges;
 if(!value||amount>*value)return {ResultCode::resource};
 *value-=amount;++e.revision;
 if(e.contents.empty_resource()&&policy.discards_empty()){entities_.erase(found);++revision_;return {ResultCode::ok,e,true};}
 found->second=e;++revision_;return {ResultCode::ok,e};
}
std::vector<Entity> WorldInventory::snapshot()const{std::lock_guard lock(mutex_);std::vector<Entity> result;result.reserve(entities_.size());for(const auto&[id,e]:entities_)result.push_back(e);return result;}
SnapshotState WorldInventory::state()const{std::lock_guard lock(mutex_);SnapshotState state{scope_,revision_,capacity_,{}};state.entities.reserve(entities_.size());for(const auto&[id,e]:entities_)state.entities.push_back(e);return state;}
size_t WorldInventory::size(PlacementKind kind)const{std::lock_guard lock(mutex_);return count(entities_,kind);}
}
