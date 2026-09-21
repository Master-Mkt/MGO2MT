#include "world_inventory_host.h"
namespace mgo2mt::items {
namespace {
bool admitted(const combat::Authority&a,combat::Identity id,const wire::Header&h){
 if(id.slot>=24||h.actor.slot!=id.slot||h.actor.instance!=id.instance||h.actor.character!=id.character||!a.active())return false;
 const auto snapshot=a.snapshot();const auto& p=snapshot.players[id.slot];
 return p&&p->identity==id&&p->life==h.actor.life&&p->alive&&!p->stunned&&snapshot.epoch==h.scope.epoch;
}
}
bool HostSession::receive(combat::Authority&a,combat::Identity id,std::span<const uint8_t>bytes,uint64_t now){
 auto record=wire::decode(bytes);if(!record)return false;auto header=std::visit([](const auto&v){return v.header;},*record);
 if(!admitted(a,id,header)||header.scope!=a.item_state().scope)return false;
 if(std::holds_alternative<wire::Probe>(*record)){
  auto it=peers_.find(id.character);if(it!=peers_.end()&&it->second.header==header)return true;
  if(it!=peers_.end()&&it->second.header.actor==header.actor)return false;
  Peer p;p.header=header;const auto state=a.item_state();
  // AK102 world operations plus selection of owned slots. The equip bit does
  // not advertise attack support for an explosive or a held-only sidearm.
  auto offer=wire::encode(wire::Offer{header,wire::all_capabilities,state.capacity});if(!offer)return false;
  p.pending.push_back(std::move(*offer));peers_.insert_or_assign(id.character,std::move(p));return true;
 }
 auto command=std::get_if<wire::Command>(&*record);auto it=peers_.find(id.character);
 if(!command||it==peers_.end()||it->second.header.scope!=header.scope||it->second.header.actor!=header.actor||it->second.header.token!=header.token||it->second.priority.size()>4)return false;
 auto result=a.item_action(id,*command,now);auto held=a.item_held(id,header.token);const auto state=a.item_state();
 wire::Reply reply;reply.header=header;reply.action=command->action;reply.heldSlot=command->heldSlot;reply.result=result.code;reply.destroyed=result.destroyed;reply.worldRevision=state.revision;
 if(held&&command->heldSlot<held_slot_count)reply.heldRevision=held->slots[command->heldSlot].revision;
 if(result.entity){reply.entity=result.entity->key.id;reply.entityRevision=result.entity->revision;}
 if(auto b=wire::encode(reply))it->second.priority.push_back(std::move(*b));if(held)if(auto b=wire::encode(*held)){it->second.priority.push_back(std::move(*b));it->second.held=*held;}it->second.nextPublish=0;return true;
}
void HostSession::poll(combat::Authority&a,uint64_t now){
 for(auto it=peers_.begin();it!=peers_.end();){auto& p=it->second;auto id=combat::Identity{p.header.actor.slot,p.header.actor.instance,p.header.actor.character};
  if(!admitted(a,id,p.header)||p.header.scope!=a.item_state().scope){it=peers_.erase(it);continue;}
  if(p.pending.empty()&&now>=p.nextPublish){
   p.nextPublish=now+200;auto held=a.item_held(id,p.header.token);
   if(held&&(!p.held||held->slots!=p.held->slots||held->selectedSlot!=p.held->selectedSlot)){if(auto b=wire::encode(*held)){p.pending.push_back(std::move(*b));p.held=*held;}}
   const auto world=a.item_state();if(world.revision!=p.worldRevision){if(auto pages=wire::pages(world,p.header)){for(const auto& page:*pages)if(auto b=wire::encode(page))p.pending.push_back(std::move(*b));p.worldRevision=world.revision;}}
  }++it;
 }
}
const std::vector<uint8_t>* HostSession::front(combat::Identity id)const{auto it=peers_.find(id.character);if(it==peers_.end()||it->second.header.actor.slot!=id.slot||it->second.header.actor.instance!=id.instance)return nullptr;if(!it->second.priority.empty())return &it->second.priority.front();return it->second.pending.empty()?nullptr:&it->second.pending.front();}
void HostSession::pop(combat::Identity id){if(front(id)){auto&p=peers_.at(id.character);if(!p.priority.empty())p.priority.pop_front();else p.pending.pop_front();}}
}
