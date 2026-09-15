#include "combat_authority.h"
#include <algorithm>
#include <cmath>
namespace mgo2win::combat {
namespace {uint64_t key(items::Domain d,uint32_t id){return (uint64_t(d)<<32)|id;}float distance2(Vec3 a,Vec3 b){float v=0;for(int i=0;i<3;++i)v+=(a[i]-b[i])*(a[i]-b[i]);return v;}}
bool Authority::configure_items(uint64_t generation,items::Capacity capacity,float range){
 if(!epoch_||!generation||!std::isfinite(range)||range<=0||range>10000||generation<itemGeneration_)return false;
 if(!placedItems_.configure(capacity))return false;
 if(generation!=itemGeneration_){if(!placedItems_.reset({epoch_,generation}))return false;itemGeneration_=generation;for(const auto& s:slots_)if(s)placedItems_.admit({s->state.identity.slot,s->state.identity.instance,s->state.identity.character,s->state.life});}
 itemRange_=range;return true;
}
void Authority::item_policies(const items::DropPolicies& source){std::map<uint64_t,items::DropPolicy> candidate;for(const auto&[k,e]:source.entries())if(e.id&&e.domain<=items::Domain::equipment)candidate.emplace(key(e.domain,e.id),e.policy);itemPolicies_.swap(candidate);}
bool Authority::item_policy(uint32_t id,const items::DropPolicy& policy,items::Domain domain){if(!id||id>65535||domain>items::Domain::equipment||policy.drop>items::DropOverride::allow)return false;itemPolicies_.insert_or_assign(key(domain,id),policy);return true;}
std::optional<items::Contents> Authority::item_template(items::Domain domain,uint32_t id)const{
 if(!id||id>65535||domain>items::Domain::equipment)return {};
 if(domain==items::Domain::weapon&&id==1&&!weapons_.contains(1))return {};
 auto weapon=weapons_.find(uint16_t(id));if(domain==items::Domain::weapon&&weapon!=weapons_.end()){
  const auto&w=weapon->second;return items::Contents{id,1,w.magazine,w.reserve,0,w.heldOnly?items::Resource::durable:items::Resource::ammunition,domain};
 }
 if(!itemPolicies_.contains(key(domain,id)))return {};
 return items::Contents{id,1,0,0,0,items::Resource::durable,domain};
}
bool Authority::seed_items(std::span<const items::Seed> seeds){
 if(active_||std::any_of(slots_.begin(),slots_.end(),[](const auto&s){return s.has_value();}))return false;
 for(const auto&s:seeds){auto t=item_template(s.contents.domain,s.contents.item);if(!t||*t!=s.contents)return false;}
 return placedItems_.seed({epoch_,itemGeneration_},seeds);
}
std::optional<items::wire::Held> Authority::item_held(Identity id,uint64_t token)const{
 const auto* s=slot(id);if(!s||!token)return {};
 items::wire::Held out;out.header={{epoch_,itemGeneration_},token,{id.slot,id.instance,id.character,s->state.life},0};out.slots=s->inventory;out.selectedEquipment=s->selectedEquipment;
 for(uint8_t i=0;i<out.slots.size();++i)if(items::weapon_slot(i)&&s->state.weapon&&out.slots[i].contents.domain==items::Domain::weapon&&out.slots[i].contents.item==s->state.weapon)out.selectedSlot=i;return out;
}
items::Result Authority::item_action(Identity admitted,const items::wire::Command& command,uint64_t now){
 using items::ResultCode;using items::wire::Action;using items::Domain;
 const auto& h=command.header;auto* s=slot(admitted);
 if(!s||h.actor.slot!=admitted.slot||h.actor.instance!=admitted.instance||h.actor.character!=admitted.character||h.actor.life!=s->state.life)return {ResultCode::identity};
 if(h.scope!=items::Scope{epoch_,itemGeneration_})return {ResultCode::scope};
 if(!items::wire::encode(command))return {ResultCode::invalid};
 if(h.sequence<=s->itemSequence)return {ResultCode::replay};s->itemSequence=h.sequence;
 if(!active_||!s->state.alive||s->state.stunned||!world_||now<s->poseAt||now-s->poseAt>policy_.stalePoseMs)return {ResultCode::unauthorized};
 if((s->state.specialPc.kind==special_pc::Kind::gekko&&command.action!=Action::equip)||s->state.specialPc.action!=special_pc::Action::none||s->state.specialPhase!=SpecialPhase::none||s->state.evadeKind!=EvadeKind::none||s->state.ladderAnchor)return {ResultCode::unauthorized};
 finish_reload(*s,now);if(s->state.reloadUntil)return {ResultCode::unauthorized};
 items::Request request{h.scope,h.actor,h.sequence,true};items::Result result;
 auto clearSight=[&](Vec3 target){auto origin=s->state.pose.feet;origin[1]+=s->state.pose.capsule.height-150;Vec3 direction{};float distance=std::sqrt(distance2(target,origin));if(distance<.01f)return true;for(int i=0;i<3;++i)direction[i]=(target[i]-origin[i])/distance;for(const auto& collision:{movement_,targets_})if(collision)if(auto hit=collision->ray(origin,direction,distance);hit&&hit->distance<distance-4)return false;return true;};
 auto clearWeapon=[&]{s->accuracy.reset();s->sopView.spreadMilliRadians=0;s->state.weapon=0;s->state.ammo=s->state.reserve=0;s->state.reloadUntil=0;s->state.reloadLevel=0;s->reloadRefillAt=0;};
 if(command.action==Action::equip){
  if(command.heldSlot>=s->inventory.size())return {ResultCode::invalid};auto& held=s->inventory[command.heldSlot];
  if(held.revision!=command.heldRevision)return {ResultCode::stale};
  if(!held.contents.item||held.contents.item>UINT16_MAX)return {ResultCode::policy};
  if(command.heldSlot==3&&held.contents.domain==Domain::equipment){if(held.revision==UINT64_MAX)return {ResultCode::exhausted};s->selectedEquipment=3;++held.revision;++revision_;return {ResultCode::ok};}
  if(!items::weapon_slot(command.heldSlot)||held.contents.domain!=Domain::weapon)return {ResultCode::policy};
  return {equip(admitted,epoch_,uint16_t(held.contents.item),now,h.actor.life)==Reject::none?ResultCode::ok:ResultCode::unauthorized};
 }else if(command.action==Action::drop||command.action==Action::install){
  if(command.heldSlot>=s->inventory.size())return {ResultCode::invalid};auto& held=s->inventory[command.heldSlot];const auto old=held.contents;
  if(!item_template(old.domain,old.item))return {ResultCode::policy};
  // Mounted firing/recovery remains the reviewed AK102 adapter. HOST round
  // placement is independent and accepts all registered catalog items.
  if(command.action==Action::install&&(old.domain!=Domain::weapon||old.item!=25||!weapons_.contains(25)))return {ResultCode::policy};
  Vec3 origin=s->state.pose.feet;origin[0]+=std::sin(s->state.pose.yaw)*650;origin[2]+=std::cos(s->state.pose.yaw)*650;origin[1]+=500;
  std::optional<stage::CollisionHit> ground;for(const auto& collision:{movement_,targets_})if(collision)if(auto hit=collision->ray(origin,{0,-1,0},1000);hit&&std::abs(hit->normal[1])>=.5f&&(!ground||hit->distance<ground->distance))ground=hit;
  Vec3 target{};
  if(command.action==Action::drop){target=s->state.pose.feet;target[1]+=300;items::Position trial{target[0],target[1],target[2],s->state.pose.yaw};if(!itemPhysics_.clear(trial,old,movement_.get(),targets_.get()))return {ResultCode::unauthorized};}
  else {if(!ground)return {ResultCode::unauthorized};target=ground->position;target[1]+=2;auto sight=target;sight[1]+=40;if(!clearSight(sight))return {ResultCode::unauthorized};}
  items::Position position{target[0],target[1],target[2],s->state.pose.yaw};items::DropPolicy policy;auto entry=itemPolicies_.find(key(old.domain,old.item));if(entry!=itemPolicies_.end())policy=entry->second;
  if(command.action==Action::drop)result=placedItems_.drop(request,held,command.heldRevision,position,policy);
  else result=placedItems_.install(request,held,command.heldRevision,command.amount,position,policy);
  if(result){if(old.domain==Domain::weapon&&s->state.weapon==old.item&&!holding(*s,uint16_t(old.item)))clearWeapon();if(command.heldSlot==3&&!held.contents.item)s->selectedEquipment=255;}
 }else{
  const auto state=placedItems_.state();auto entity=std::find_if(state.entities.begin(),state.entities.end(),[&](const auto& e){return e.key.id==command.entity;});if(entity==state.entities.end())return {ResultCode::not_found};
  const auto& c=entity->contents;auto prototype=item_template(c.domain,c.item);
  if(c.domain==Domain::weapon&&special_pc::weapon(uint16_t(c.item)))return {ResultCode::policy};
  if(!prototype||c.quantity!=1||c.resource!=prototype->resource||c.magazine>prototype->magazine||c.reserve>prototype->reserve||c.charges>prototype->charges)return {ResultCode::policy};
  const bool recover=command.action==Action::recover||command.action==Action::use;
  if((recover&&entity->kind!=items::PlacementKind::installed)||(!recover&&entity->kind==items::PlacementKind::installed))return {ResultCode::invalid};
  if(recover&&!itemRecoverOthers_&&entity->owner.character!=admitted.character)return {ResultCode::unauthorized};
  Vec3 target{entity->position.x,entity->position.y,entity->position.z};if(distance2(target,s->state.pose.feet)>itemRange_*itemRange_)return {ResultCode::unauthorized};target[1]+=40;if(!clearSight(target))return {ResultCode::unauthorized};
  auto selected=command.action==Action::use?uint8_t(0):command.heldSlot;
  if(selected>=s->inventory.size()||(c.domain==Domain::equipment?selected!=items::equipment_slot:!items::weapon_slot(selected))||(selected==items::knife_slot&&c.item!=1))return {ResultCode::invalid};
  for(const auto& held:s->inventory)if(held.contents.domain==c.domain&&held.contents.item==c.item)return {ResultCode::occupied};
  auto& held=s->inventory[selected];const auto expected=command.action==Action::use?held.revision:command.heldRevision;
  result=placedItems_.pickup(request,entity->key,command.entityRevision,held,expected);
  if(result){if(c.domain==Domain::weapon){clearWeapon();s->state.weapon=uint16_t(c.item);s->state.ammo=uint16_t(c.magazine);s->state.reserve=uint16_t(c.reserve);}else s->selectedEquipment=3;}
 }
 if(result){++revision_;advance_accuracy(now);}return result;
}

void Authority::item_box_catalog(const weapons::Catalog& catalog){itemPhysics_.catalog(catalog);}
Decision Authority::advance_items(uint64_t now){
 Decision out;auto state=placedItems_.state();const auto moves=itemPhysics_.advance(state,movement_.get(),targets_.get(),now,active_);
 if(!moves.empty()){if(!placedItems_.move(state.scope,moves))return out;state=placedItems_.state();}
 if(!active_||!world_)return out;
 for(const auto&e:state.entities){if(e.kind==items::PlacementKind::installed)continue;
  const auto&c=e.contents;const auto prototype=item_template(c.domain,c.item);
  if(c.domain==items::Domain::weapon&&special_pc::weapon(uint16_t(c.item)))continue;
  if(!prototype||c.quantity!=1||c.resource!=prototype->resource||c.magazine>prototype->magazine||c.reserve>prototype->reserve||c.charges>prototype->charges)continue;
  for(auto& current:slots_)if(current){auto&s=*current;auto&p=s.state;items::Actor actor{p.identity.slot,p.identity.instance,p.identity.character,p.life};
   // Contact observation happens even if this player's slot is occupied so
   // dropping, walking out, then returning is a genuine new contact.
   const bool contact=itemPhysics_.contact(e,actor,p.pose.feet,p.pose.capsule,now);
   if(!contact||!p.alive||p.stunned||now<s.poseAt||now-s.poseAt>policy_.stalePoseMs||p.specialPc.kind!=special_pc::Kind::human||p.specialPhase!=SpecialPhase::none||p.evadeKind!=EvadeKind::none||p.ladderAnchor||p.reloadUntil)continue;
   uint8_t destination=255;
   if(c.domain==items::Domain::equipment)destination=3;
   else if(c.domain==items::Domain::weapon){
    // Prefer the catalog category; the existing three weapon slots also
    // accept registered weapons with no category row (e.g. knife/PATRIOT).
    auto category=itemPhysics_.category(c);
    if(category==item_box::Size::primary)destination=0;
    else if(category==item_box::Size::secondary)destination=1;
    else if(category==item_box::Size::reserve)destination=2;
   }
   if(c.domain==items::Domain::weapon){
    if(c.item==1)destination=items::knife_slot;
    else if(destination>=3||s.inventory[destination].contents.item){destination=255;for(uint8_t i=0;i<3;++i)if(!s.inventory[i].contents.item){destination=i;break;}}
   }
   if(destination>=s.inventory.size()||s.inventory[destination].contents.item)continue;
   if(std::any_of(s.inventory.begin(),s.inventory.end(),[&](const auto& held){return held.contents.item==c.item&&held.contents.domain==c.domain;}))continue;
   // The conservative box can overlap a capsule on the far side of a thin
   // wall. An unobstructed segment is required as well as physical overlap.
   Vec3 from=p.pose.feet;from[1]+=(std::min)(p.pose.capsule.height*.5f,itemPhysics_.extent(c)[1]);Vec3 to{e.position.x,e.position.y+itemPhysics_.extent(c)[1],e.position.z};
   auto direction=to;float length=0;for(int i=0;i<3;++i){direction[i]-=from[i];length+=direction[i]*direction[i];}length=std::sqrt(length);bool blocked=false;
   if(length>.001f){for(auto&x:direction)x/=length;for(const auto&collision:{movement_,targets_})if(collision)if(auto hit=collision->ray(from,direction,length);hit&&hit->distance<length-1)blocked=true;}
   if(blocked)continue;
   auto&held=s.inventory[destination];auto picked=placedItems_.contact_pickup(actor,e.key,e.revision,held,held.revision);if(!picked)continue;
   if(c.domain==items::Domain::weapon){s.accuracy.reset();s.sopView.spreadMilliRadians=0;p.weapon=uint16_t(c.item);p.ammo=uint16_t(c.magazine);p.reserve=uint16_t(c.reserve);p.reloadUntil=0;p.reloadLevel=0;s.reloadRefillAt=0;}else s.selectedEquipment=3;
   ++revision_;Event event;event.kind=EventKind::itemPickup;event.source=event.target=p.identity;event.sourceLife=event.targetLife=p.life;event.weapon=uint16_t(c.item);event.object=uint32_t(c.domain)+1;event.position=to;emit(out,event);break;
  }
 }
 return out;
}
}
