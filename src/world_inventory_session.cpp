#include "world_inventory_session.h"
#include <limits>
#include <algorithm>
#include <cmath>
namespace mgo2win::items {
void ClientSession::reset(){auto generation=state_.connection+1;state_={};state_.connection=generation;header_={};receiver_={};next_=probeAt_=sentAt_=0;attempted_=false;queued_.reset();pending_.reset();}
void ClientSession::disconnect(){std::lock_guard lock(mutex_);reset();}
ClientState ClientSession::state()const{std::lock_guard lock(mutex_);return state_;}
bool ClientSession::submit(wire::Action action,uint8_t slot,uint64_t entity){
 std::lock_guard lock(mutex_);if(state_.status!=ClientStatus::ready||!state_.context.active||!state_.held||queued_||pending_||state_.delivery==Delivery::unconfirmed||unsigned(action)<1||unsigned(action)>5||!(state_.capabilities&(1u<<(unsigned(action)-1)))||next_==std::numeric_limits<uint64_t>::max())return false;
 wire::Command command;command.header=header_;command.header.sequence=++next_;command.action=action;
 command.heldSlot=slot;command.amount=action==wire::Action::install?1:0;
 if(action==wire::Action::use){command.heldSlot=255;command.amount=1;command.resource=Consume::magazine;}
 else{if(slot>=3)return false;command.heldRevision=state_.held->slots[slot].revision;}
 if(action==wire::Action::pickup||action==wire::Action::recover||action==wire::Action::use){
  if(!state_.world)return false;auto it=std::find_if(state_.world->entities.begin(),state_.world->entities.end(),[&](const Entity&e){return e.key.id==entity;});if(it==state_.world->entities.end())return false;
  command.entity=entity;command.entityRevision=it->revision;
 }
 if(!wire::encode(command))return false;queued_=command;state_.delivery=Delivery::pending;return true;
}
std::vector<std::vector<uint8_t>> ClientSession::pump(ClientContext context,uint64_t now,uint64_t nonce,bool writable,std::span<const std::vector<uint8_t>> incoming){
 std::lock_guard lock(mutex_);std::vector<std::vector<uint8_t>> out;
 if(context.scope!=header_.scope||context.actor!=header_.actor){reset();header_.scope=context.scope;header_.actor=context.actor;}
 state_.context=context;
 if(!context.scope.epoch||!context.scope.generation||context.actor.slot>=24||!context.actor.instance||!context.actor.character||!context.actor.life||!context.active){if(pending_)state_.delivery=Delivery::unconfirmed;else if(queued_)state_.delivery=Delivery::none;queued_.reset();pending_.reset();state_.held.reset();state_.world.reset();state_.status=ClientStatus::unavailable;attempted_=false;return out;}
 if(state_.status==ClientStatus::probing&&(now<probeAt_||now-probeAt_>=3000))state_.status=ClientStatus::unavailable;
 for(const auto&body:incoming){auto record=wire::decode(body);if(!record)continue;const auto h=std::visit([](const auto&v){return v.header;},*record);if(h.scope!=header_.scope||h.actor!=header_.actor||h.token!=header_.token)continue;
  if(auto offer=std::get_if<wire::Offer>(&*record)){if(state_.status!=ClientStatus::probing||h.sequence||offer->capacity.dropped>4096||offer->capacity.installed>4096)continue;state_.status=ClientStatus::ready;state_.capabilities=offer->capabilities;receiver_.bind(header_,offer->capacity);}
  else if(state_.status==ClientStatus::ready){
   if(auto held=std::get_if<wire::Held>(&*record)){
    bool fresh=true,changed=false;for(size_t i=0;state_.held&&i<3;++i){const auto&a=held->slots[i];const auto&b=state_.held->slots[i];if(a.revision<b.revision||(a.revision==b.revision&&a.contents!=b.contents))fresh=false;changed|=a.revision!=b.revision;}
    if(state_.held&&!changed&&held->selectedSlot!=state_.held->selectedSlot)fresh=false;
    if(fresh)state_.held=*held;
   }else if(std::holds_alternative<wire::Page>(*record)){receiver_.receive(body);state_.world=receiver_.state();}
   else if(auto reply=std::get_if<wire::Reply>(&*record);reply&&pending_&&reply->header.sequence==pending_->header.sequence&&reply->action==pending_->action&&reply->heldSlot==pending_->heldSlot){state_.delivery=reply->result==ResultCode::ok?Delivery::confirmed:Delivery::rejected;state_.result=reply->result;pending_.reset();}
  }
 }
 if(pending_&&(now<sentAt_||now-sentAt_>=5000)){state_.delivery=Delivery::unconfirmed;pending_.reset();queued_.reset();}
 if(!attempted_&&writable&&nonce){attempted_=true;probeAt_=now;header_.token=nonce;state_.status=ClientStatus::probing;if(auto body=wire::encode(wire::Probe{header_}))out.push_back(std::move(*body));}
 else if(state_.status==ClientStatus::ready&&queued_&&writable){if(auto body=wire::encode(*queued_)){out.push_back(std::move(*body));pending_=queued_;sentAt_=now;}queued_.reset();}
 return out;
}
}
