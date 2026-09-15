#pragma once
#include "hold_selection.h"
#include "world_inventory_session.h"
namespace mgo2win::hold_selection {
inline bool inventory_blocks_gameplay(items::Delivery delivery){return delivery==items::Delivery::pending||delivery==items::Delivery::unconfirmed;}
inline Snapshot inventory_snapshot(const items::ClientState& state,bool eligible){
 Snapshot result;const auto& context=state.context;const auto& actor=context.actor;
 result.scope={state.connection,context.scope.epoch,context.scope.generation,actor.slot,actor.instance,actor.character,actor.life};
 result.eligible=eligible&&context.active&&state.status==items::ClientStatus::ready&&(state.capabilities&(1u<<5))&&
  state.delivery!=items::Delivery::pending&&state.delivery!=items::Delivery::unconfirmed&&state.held&&
  state.held->header.scope==context.scope&&state.held->header.actor==actor;
 if(!result.eligible)return result;
 for(uint8_t slot=0;slot<state.held->slots.size();++slot){
  const auto& held=state.held->slots[slot];const auto& value=held.contents;if(!value.item||!value.quantity)continue;
  Owned item{slot,value.item,held.revision,value.quantity,value.magazine,value.reserve,value.charges,value.resource==items::Resource::ammunition};
  if(value.domain==items::Domain::weapon){result.weapons.push_back(item);if(state.held->selectedSlot==slot)result.selectedWeapon=slot;}
  else if(value.domain==items::Domain::equipment){result.equipment.push_back(item);if(state.held->selectedEquipment==slot)result.selectedEquipment=slot;}
 }
 return result;
}
inline bool submit_inventory_selection(items::ClientSession& session,const Request& request){
 const auto& s=request.scope;
 if(s[3]>=24||s[4]>65535||s[5]>0xffffffffULL||s[6]>0xffffffffULL)return false;
 if(request.action==Action::drop)return session.submit_drop(s[0],{s[1],s[2]},{uint8_t(s[3]),uint16_t(s[4]),uint32_t(s[5]),uint32_t(s[6])},request.item.slot,request.item.item,request.item.revision);
 if(request.action!=Action::equip)return false;
 return session.submit_equip(s[0],{s[1],s[2]},{uint8_t(s[3]),uint16_t(s[4]),uint32_t(s[5]),uint32_t(s[6])},request.item.slot,request.item.item,request.item.revision);
}
}
