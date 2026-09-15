#include "combat_object_damage.h"
#include "stage_profiles.h"
#include <algorithm>
#include <limits>
namespace mgo2win::combat {
ObjectDamage ObjectDamage::load(const std::filesystem::path& root,uint8_t map){
 return load(root,map,Policy{});
}
ObjectDamage ObjectDamage::load(const std::filesystem::path& root,uint8_t map,Policy policy){
 if(!policy.valid())throw std::invalid_argument("Native breakable-light durability");
 ObjectDamage out;out.map_=map;out.lightHits_=policy.lightHits;if(map!=20&&map!=7)return out;
 auto bindings=stage::read_object_bindings(root,false,map);
 if(map==7){
  // The compiler binds each reviewed/native lamp explicitly. prop_num is not
  // an object index: only an intact->broken light rule identifies a lamp here.
  for(const auto& b:bindings){
   const bool off=std::any_of(b.lights.begin(),b.lights.end(),[](const auto& r){return r.mask==1&&r.value==1&&!r.light.enabled;});
   if(!off)continue;
   if(b.width!=1||b.cboxOrdinal!=-1)throw std::runtime_error("Breakable light state contract");
   size_t count=0;
   for(const auto& p:b.parts)if(p.collision&&p.mask==1&&p.value==0){
    if(p.collision->triangles.empty()||!p.componentId||!out.components_.emplace(p.componentId,Target{b.bindingId,{},Kind::light}).second)throw std::runtime_error("Breakable light hit component");
    ++count;
   }
   if(!count)throw std::runtime_error("Breakable light has no intact hit geometry");
  }
  if(out.components_.empty())throw std::runtime_error("BB breakable light bindings unavailable");
  return out;
 }
 if(bindings.size()!=32)throw std::runtime_error("Reviewed drum binding extent");
 for(size_t i:{size_t(10),size_t(11)}){const auto& b=bindings[i];if(b.width!=1||b.cboxOrdinal!=-1)throw std::runtime_error("Reviewed drum state contract");
  std::vector<stage::CollisionInstance> parts;
  for(const auto& p:b.parts)if(p.collision&&!p.hitOnly&&p.mask==1&&p.value==0)parts.push_back({p.componentId,p.collision,p.placement?p.placement->position:b.position,p.placement?p.placement->degrees:b.degrees});
  auto solid=stage::Collision::combine(stage::Collision::make({},{}),parts);
  if(solid.triangles.size()!=46||solid.vertices.empty())throw std::runtime_error("Reviewed drum GEOM extent");
  auto low=solid.vertices.front(),high=low;for(auto v:solid.vertices)for(unsigned a=0;a<3;++a){low[a]=(std::min)(low[a],v[a]);high[a]=(std::max)(high[a],v[a]);}
  stage::Vec3 center{};for(unsigned a=0;a<3;++a)center[a]=(low[a]+high[a])*.5f;
  for(const auto& p:b.parts)if(p.collision&&p.mask==1&&p.value==0)if(!out.components_.emplace(p.componentId,Target{b.bindingId,center,Kind::drum}).second)throw std::runtime_error("Drum component collision");
 }
 return out;
}
void ObjectDamage::remember(uint64_t id){
 seen_.insert(id);highest_=(std::max)(highest_,id);
 // Bounded exact deduplication plus a non-replayable retired serial window.
 const auto floor=highest_>4096?highest_-4096:0;
 while(!seen_.empty()&&*seen_.begin()<=floor)seen_.erase(seen_.begin());
}
ObjectDamage::Result ObjectDamage::apply(std::span<const Event> events,stage::SceneAuthority& objects,World& world,Authority& combat,uint64_t now){
 Result out;const auto snapshot=combat.snapshot();
 if(!combat.active()||!objects.request()||!world.snapshot()||world.snapshot()->request!=*objects.request()||objects.registry().map!=map_||snapshot.epoch<epoch_)return out;
 if(snapshot.epoch!=epoch_){epoch_=snapshot.epoch;seen_.clear();highest_=0;hits_.clear();request_.reset();}
 if(request_!=objects.request()){request_=objects.request();hits_.clear();}
 for(const auto& e:events){
  const auto found=components_.find(e.object);
  if(e.kind!=EventKind::impact||!e.id||e.epoch!=snapshot.epoch||found==components_.end()||!e.weapon||e.source.slot>=24||!e.sourceLife)continue;
  const auto& source=snapshot.players[e.source.slot];
  if(!source||source->identity!=e.source||source->life!=e.sourceLife)continue;
  if(seen_.contains(e.id)||(highest_>4096&&e.id<=highest_-4096))continue;
  const auto& current=*world.snapshot();
  const auto currentState=std::find_if(current.objects.begin(),current.objects.end(),[&](const auto& x){return x.bindingId==found->second.binding;});
  if(currentState==current.objects.end()||currentState->current!=0)continue;
  const auto entry=std::find_if(objects.registry().entries.begin(),objects.registry().entries.end(),[&](const auto& r){return r.bindingId==found->second.binding;});
  if(entry==objects.registry().entries.end()||entry->width!=1||entry->update!=host::ObjectStates::Update::bits)continue;
  const uint16_t required=found->second.kind==Kind::light?lightHits_:1;
  const auto oldHits=hits_.find(found->second.binding);const uint16_t previous=oldHits==hits_.end()?0:oldHits->second;
  if(previous+1<required){hits_[found->second.binding]=uint16_t(previous+1);remember(e.id);continue;}
  // A complete published scene is copied before update; never publish a delta
  // while leaving HOST collision on the previous revision.
  auto nextObjects=objects;auto record=nextObjects.update(found->second.binding,1);if(!record)continue;
  auto next=*world.snapshot();if(next.revision==UINT64_MAX)continue;++next.revision;
  auto state=std::find_if(next.objects.begin(),next.objects.end(),[&](const auto& x){return x.bindingId==found->second.binding;});
  if(state==next.objects.end()||state->current)continue;state->current=1;
  auto nextWorld=world;if(!nextWorld.apply(next))continue;
  if(!combat.object_world(nextWorld.collision(),nextWorld.targets()))continue;
  objects=std::move(nextObjects);world=std::move(nextWorld);out.records.push_back(std::move(*record));++out.destroyed;
  hits_.erase(found->second.binding);remember(e.id);
  if(found->second.kind==Kind::light)continue;
  burning::Blast blast;blast.source={{snapshot.epoch,e.source.slot,e.source.instance,e.source.character,e.sourceLife},e.weapon,found->second.binding,source->team};
  blast.serial=e.id;blast.position=found->second.center;blast.radius=4500;blast.damage=300;blast.ignite=true;
  auto damage=combat.explode(blast,now);out.combat.events.insert(out.combat.events.end(),damage.events.begin(),damage.events.end());
 }
 return out;
}
}
