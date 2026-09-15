#include "host_special_pc.h"
#include <algorithm>
namespace mgo2win::special_pc {
bool Control::apply(combat::Authority&authority,uint64_t now){
 std::lock_guard lock(mutex_);auto snapshot=authority.snapshot();if(!snapshot.epoch)return false;if(epoch_!=snapshot.epoch)randomWinner_.reset();epoch_=snapshot.epoch;
 auto before=players_;players_.clear();
 // Stable one-winner draw among the synchronized non-HOST connection roster.
 // The chosen participant may not have spawned yet; wait instead of favoring
 // whichever READY/loadout packet happened to create the first player body.
 // Explicit operator assignments override the lottery and may select duplicates.
 if(!config_.allow||!config_.random||(randomWinner_&&direct_.contains(randomWinner_->character)))randomWinner_.reset();
 if(config_.allow&&config_.random&&!randomWinner_){uint64_t best=UINT64_MAX;for(auto id:roster_)if(!direct_.contains(id.character)){const auto rank=weapon_accuracy::mix(snapshot.epoch^id.character);if(rank<best){randomWinner_=id;best=rank;}}}
 for(auto&p:snapshot.players)if(p){auto desired=Kind::human;if(config_.allow){if(auto d=direct_.find(p->identity.character);d!=direct_.end()&&d->second.first==p->identity)desired=d->second.second;else if(randomWinner_==p->identity)desired=Kind::gekko;}
  auto& previous=attempted_[p->identity.character];combat::Reject result=previous.result;
  const bool completedAction=(previous.result==combat::Reject::unavailable||previous.result==combat::Reject::dead||previous.result==combat::Reject::obstructed)&&desired==Kind::human&&p->specialPc.kind==Kind::gekko&&p->alive&&!p->stunned&&p->specialPc.action==Action::none;
  if(completedAction||previous.epoch!=epoch_||previous.revision!=revision_||previous.id!=p->identity||previous.life!=p->life||previous.desired!=desired||previous.names!=config_.showNames){result=authority.assign_special(p->identity,desired,config_.showNames,now);previous={epoch_,revision_,p->identity,p->life,desired,config_.showNames,result};}
  const auto current=authority.snapshot().players[p->identity.slot];players_.push_back({p->identity,p->life,desired,current?current->specialPc.kind:Kind::human,result});
 }
 return before!=players_;
}
}
