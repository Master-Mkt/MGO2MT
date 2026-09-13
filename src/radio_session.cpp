#include "radio_session.h"
#include "preset_radio_wire.h"
#include <algorithm>
#include <limits>
#include <utility>
namespace mgo2win::radio {
void Session::reset_locked(){const auto generation=state_.generation+1;state_={};state_.generation=generation;client_.bind(0,{});queued_.reset();flight_=0;deadline_=lastSend_=lastNow_=0;everSent_=false;events_.clear();}
void Session::disconnect(){std::lock_guard lock(mutex_);reset_locked();}
SessionState Session::state()const{std::lock_guard lock(mutex_);return state_;}
bool Session::submit(uint64_t generation,uint8_t preset,uint64_t now){
 std::lock_guard lock(mutex_);
 if(generation!=state_.generation||!state_.epoch||!state_.eligible||state_.status!=Status::ready||queued_||flight_||
    now<lastNow_||now>std::numeric_limits<uint64_t>::max()-5000||(everSent_&&(now<lastSend_||now-lastSend_<1000))||!mgo2::radio::wire::reviewed_id(preset))return false;
 queued_=preset;deadline_=now+5000;state_.delivery=DeliveryState::queued;return true;
}
std::vector<Body> Session::pump(const Context& context,Identity self,uint64_t now,uint64_t nonce,bool writable,const std::vector<Body>& incoming,const Filter& filter){
 std::lock_guard lock(mutex_);std::vector<Body> outgoing;
 if(!context.epoch||!self.character){if(state_.epoch)reset_locked();return outgoing;}
 if(context.epoch!=state_.epoch||self!=state_.self){reset_locked();state_.epoch=context.epoch;state_.self=self;client_.bind(context.epoch,self);}
 auto own=std::find_if(context.members.begin(),context.members.end(),[&](const Member&m){return m.identity==self;});
 const auto life=own==context.members.end()?0:own->life;
 const bool lifeChanged=life!=state_.life;
 if(lifeChanged){++state_.generation;state_.life=life;queued_.reset();flight_=0;events_.clear();state_.delivery=DeliveryState::none;}
 state_.eligible=own!=context.members.end()&&own->eligible&&life;
 if(!state_.eligible){queued_.reset();flight_=0;state_.delivery=DeliveryState::none;events_.clear();}
 if(now<lastNow_){reset_locked();return outgoing;}lastNow_=now;
 if((flight_||queued_)&&now>=deadline_){queued_.reset();flight_=0;state_.delivery=DeliveryState::unconfirmed;}
 if(writable)if(auto probe=client_.probe(nonce,now))outgoing.push_back(*probe);
 for(const auto& body:incoming)client_.receive(body,context,now,filter);
 client_.tick(now);state_.status=client_.status();
 for(auto event:client_.drain()){
  // Consume the replay watermark, but never give a notification batched
  // before respawn the new receiver life. Offers remain usable.
  if(lifeChanged)continue;
  if(event.sender==self&&event.life==state_.life&&event.sequence==flight_){flight_=0;state_.delivery=DeliveryState::confirmed;}
  if(events_.size()<history_limit)events_.push_back(event);
 }
 if(writable&&outgoing.empty()&&queued_){
  const auto preset=*queued_;queued_.reset();
  if(now>std::numeric_limits<uint64_t>::max()-5000){state_.delivery=DeliveryState::unconfirmed;return outgoing;}
  auto submission=client_.submit(preset,context,now);
  if(submission.body){outgoing.push_back(*submission.body);flight_=submission.sequence;deadline_=now+5000;lastSend_=now;everSent_=true;state_.delivery=DeliveryState::awaiting_echo;}
  else state_.delivery=DeliveryState::unconfirmed;
 }
 return outgoing;
}
std::vector<Event> Session::drain(){std::lock_guard lock(mutex_);return std::exchange(events_,{});}
}
