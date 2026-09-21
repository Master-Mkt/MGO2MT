#pragma once
#include "combat_wire.h"
namespace mgo2mt::combat {
class EnvironmentReceiver {
 std::optional<wire::Environment> state_;
public:
 void clear(){state_.reset();}
 bool receive(const wire::Environment&e,const wire::Offer&o){
  if(!e.epoch||e.epoch!=o.epoch||!e.revision||!environment::valid(e.config))return false;
  if(state_&&(state_->epoch!=e.epoch||e.revision<state_->revision||(e.revision==state_->revision&&e.config!=state_->config)))return false;
  state_=e;return true;
 }
 const std::optional<wire::Environment>& state()const{return state_;}
};
}
