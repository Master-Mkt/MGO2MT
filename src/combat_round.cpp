#include "combat_round.h"
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace mgo2win::combat {
namespace {host::ParticipantToken token(Identity id){return {id.slot,id.instance,id.character};}}
RoundCoordinator::RoundCoordinator(uint64_t epoch,Policy policy,std::shared_ptr<const weapons::Catalog> catalog,std::span<const Weapon> profiles,Spawn spawn)
 :epoch_(epoch),policy_(policy),rules_({policy.countdownMs,1,policy.dpEnabled}),catalog_(std::move(catalog)),spawn_(std::move(spawn)){
 if(!epoch||!policy.generation||policy.respawnDelayMs>60000||policy.roundDurationMs>wire::maximumRoundDurationMs)throw std::invalid_argument("Round identity/respawn policy");
 if(catalog_)for(const auto&w:profiles){
  auto found=std::find_if(catalog_->entries().begin(),catalog_->entries().end(),[&](auto&e){return e.id==w.id;});
  if(found==catalog_->entries().end())throw std::invalid_argument("Weapon profile missing catalog identity");
  supported_.push_back(w.id);required_|=uint8_t(1u<<unsigned(found->category));
 }
 available_=!supported_.empty()&&bool(spawn_)&&(!policy.dpEnabled||(catalog_&&catalog_->initial_dp().has_value()));
}
RoundCoordinator::Participant* RoundCoordinator::find(Identity id){if(id.slot>=24)return nullptr;auto&p=players_[id.slot];return p&&p->state.id==id?&*p:nullptr;}
bool RoundCoordinator::join(Identity id,uint64_t now,std::optional<uint8_t> retainedTeam){
 if(id.slot>=24||!id.instance||!id.character||(retainedTeam&&*retainedTeam>2))return false;
 if(auto p=find(id))return true;
 if(players_[id.slot]||std::any_of(players_.begin(),players_.end(),[&](auto&p){return p&&p->state.id.character==id.character;}))return false;
 Participant p;p.state.id=id;
 if(policy_.freeForAll)p.state.team=0;
 else if(retainedTeam)p.state.team=*retainedTeam;
 else if(policy_.autoAssign){unsigned red=0,blue=0;for(auto&q:players_)if(q){red+=q->state.team==1;blue+=q->state.team==2;}p.state.team=red<=blue?1:2;}
 // Fresh room ledger; subsequent commands never reset this balance to the
 // catalog floor. DP respawn stays disabled until its allowance is reviewed.
 if(policy_.dpEnabled&&catalog_&&catalog_->initial_dp())p.balance=*catalog_->initial_dp();
 if(!rules_.join(token(id),host::ParticipantRole::player,now))return false;
 players_[id.slot]=p;++revision_;return true;
}
void RoundCoordinator::leave(Identity id){if(!find(id))return;players_[id.slot].reset();rules_.leave(token(id));++revision_;}
bool RoundCoordinator::grant(Participant&p,Authority&authority,uint64_t now){
 // The room ledger survives removal. Only a host-observed death can mint a
 // next-life entitlement; reconnecting or resending cannot mint one.
 const auto prior=grantedCharacters_.find(p.state.id.character);bool respawn=prior!=grantedCharacters_.end();
 if(respawn&&(!p.respawnEligible||prior->second==std::numeric_limits<uint32_t>::max()||p.state.life!=prior->second+1)){p.error=wire::CommandError::already_deployed;return false;}
 std::vector<const weapons::Entry*> entries;std::vector<uint16_t> inventory;
 for(unsigned c=0;c<3;++c){auto id=p.selected[c];if(!id){if(required_&(1u<<c)){p.error=wire::CommandError::weapon;return false;}continue;}
  auto e=catalog_->find(weapons::Category(c),id);if(!e||std::find(supported_.begin(),supported_.end(),id)==supported_.end()){p.error=wire::CommandError::weapon;return false;}
  entries.push_back(e);inventory.push_back(id);
 }
 weapons::SelectionContext context{policy_.dpEnabled,p.balance,policy_.restrictions};auto quote=weapons::quote(entries,context);
 if(quote.access!=weapons::Access::allowed){p.error=quote.access==weapons::Access::insufficient_dp?wire::CommandError::insufficient_dp:wire::CommandError::restricted;return false;}
 auto spawn=[&]{return spawn_(authority,p.state.id,p.state.team,inventory,now);};
 if(!(respawn?authority.respawn(p.state.id,p.state.life,spawn):spawn())){p.error=wire::CommandError::spawn;return false;}
 grantedCharacters_[p.state.id.character]=p.state.life;p.balance-=uint32_t(quote.cost);p.state.deployed=true;p.respawnEligible=false;p.diedAt.reset();p.error=wire::CommandError::none;authority.active(true);return true;
}
bool RoundCoordinator::command(Identity id,const wire::Command&command,Authority&authority,uint64_t now){
 expire(authority,now);
 auto*p=find(id);if(!p||command.epoch!=epoch_||!command.life||command.life!=p->state.life)return false;
 if(p->sequenced&&(command.sequence==p->lastCommand||uint32_t(command.sequence-p->lastCommand)>=0x80000000u))return false;
 p->sequenced=true;p->lastCommand=command.sequence;p->error=wire::CommandError::none;++revision_;
 if(ended_){p->error=wire::CommandError::phase;return true;}
 const auto phase=rules_.phase();
 switch(command.kind){
 case wire::CommandKind::loaded:
  if(command.generation!=policy_.generation||(command.enabled&&!command.sceneRevision)){p->error=wire::CommandError::not_loaded;break;}
  p->state.loaded=command.enabled;if(!command.enabled)p->state.ready=false;
  if(phase==host::RoundPhase::waiting)rules_.set_prepared(token(id),rules_.generation(),command.enabled&&(policy_.freeForAll||p->state.team!=0));break;
 case wire::CommandKind::ready:
  if(phase!=host::RoundPhase::waiting){p->error=wire::CommandError::phase;break;}
  if(command.enabled&&(!p->state.loaded||(!policy_.freeForAll&&!p->state.team))){p->error=wire::CommandError::not_loaded;break;}
  if(command.enabled&&!available_){p->error=wire::CommandError::unavailable;break;}
  rules_.set_ready(token(id),rules_.generation(),command.enabled);p->state.ready=command.enabled;break;
 case wire::CommandKind::team:
  if(policy_.freeForAll||policy_.autoAssign||p->state.deployed){p->error=wire::CommandError::team;break;}
  if(command.team<1||command.team>2){p->error=wire::CommandError::team;break;}
  p->state.team=command.team;p->state.ready=false;
  if(phase==host::RoundPhase::waiting){rules_.set_prepared(token(id),rules_.generation(),p->state.loaded);rules_.set_ready(token(id),rules_.generation(),false);}break;
 case wire::CommandKind::loadout:
  if(p->state.deployed){p->error=wire::CommandError::already_deployed;break;}
  if(!available_){p->error=wire::CommandError::unavailable;break;}
  if(phase==host::RoundPhase::waiting){p->error=wire::CommandError::phase;break;}
  if(!p->state.loaded||(!policy_.freeForAll&&!p->state.team)){p->error=wire::CommandError::not_loaded;break;}
  p->selected=command.weapons;grant(*p,authority,now);break;
 }
 return true;
}
bool RoundCoordinator::expire(Authority&authority,uint64_t now){
 if(ended_)return true;
 if(!policy_.endOnTimeout||!policy_.roundDurationMs||!roundStarted_||now<*roundStarted_||now-*roundStarted_<policy_.roundDurationMs)return false;
 ended_=true;roundRemainingMs_=0;startPending_=false;authority.active(false);++revision_;
 for(auto&p:players_)if(p){p->state.ready=false;p->respawnEligible=false;p->diedAt.reset();}
 return true;
}
void RoundCoordinator::poll(Authority&authority,uint64_t now){
 if(policy_.freeForAll)for(auto& participant:players_)if(participant){auto& p=*participant;const auto& score=authority.scores()[p.state.id.slot];
  if(score&&score->id==p.state.id&&(p.state.kills!=score->kills||p.state.deaths!=score->deaths)){p.state.kills=score->kills;p.state.deaths=score->deaths;++revision_;}
 }
 if(expire(authority,now))return;
 // Three seconds is a bounded native policy, not an original respawn timer.
 // Do not reset DP wallets until the original respawn allowance is reviewed.
 if(available_&&!policy_.dpEnabled){const auto snapshot=authority.snapshot();if(snapshot.epoch==epoch_)for(auto&participant:players_)if(participant&&participant->state.deployed){auto&p=*participant;const auto&body=snapshot.players[p.state.id.slot];
   if(!body||body->identity!=p.state.id||body->life!=p.state.life||body->alive||body->hp)continue;
   if(!p.diedAt)p.diedAt=now;
   if(now<*p.diedAt||now-*p.diedAt<policy_.respawnDelayMs||p.state.life==std::numeric_limits<uint32_t>::max())continue;
   ++p.state.life;p.state.deployed=false;p.state.ready=false;p.respawnEligible=true;p.error=wire::CommandError::none;++revision_;
  }}
 if(available_&&rules_.advance(now)!=host::StartReason::none){
  startPending_=true;++revision_;
  if(policy_.roundDurationMs){roundStarted_=now;roundRemainingMs_=uint32_t((uint64_t(policy_.roundDurationMs)+999)/1000*1000);}
 }
 if(roundStarted_&&now>=*roundStarted_){
  const uint64_t elapsed=now-*roundStarted_,left=elapsed>=policy_.roundDurationMs?0:uint64_t(policy_.roundDurationMs)-elapsed;
  // Publish whole seconds only. Clock regression, player deployment and life
  // changes cannot add time. Only explicit endOnTimeout policy ends the round.
  const auto remaining=uint32_t((left+999)/1000*1000);
  if(remaining<roundRemainingMs_){roundRemainingMs_=remaining;++revision_;}
 }
 if(rules_.deadline()&&now>=nextClock_){nextClock_=now+1000;++revision_;}
}
wire::Preparation RoundCoordinator::state(Identity id,uint64_t now)const{
 if(id.slot>=24||!players_[id.slot]||players_[id.slot]->state.id!=id)throw std::invalid_argument("Round recipient");
 const auto&p=*players_[id.slot];wire::Preparation out;out.epoch=epoch_;out.revision=revision_;out.self=id;out.generation=policy_.generation;
 out.runtimeReady=available_;out.freeForAll=policy_.freeForAll;out.autoAssign=policy_.freeForAll||policy_.autoAssign;out.dpEnabled=policy_.dpEnabled;out.dpBalance=p.balance;out.restrictions=policy_.restrictions;
 out.roundClock=roundStarted_.has_value();out.roundRemainingMs=roundRemainingMs_;
 out.lastCommand=p.lastCommand;out.error=p.error;out.supported=supported_;out.requiredCategories=required_;out.selected=p.selected;
 if(auto deadline=rules_.deadline()){out.countdown=true;out.remainingMs=uint32_t(std::min<uint64_t>(*deadline>now?*deadline-now:0,std::numeric_limits<uint32_t>::max()));}
 out.phase=rules_.phase()==host::RoundPhase::waiting?wire::RoundPhase::waiting:wire::RoundPhase::selecting;
 for(unsigned i=0;i<24;++i)if(players_[i]){out.players[i]=players_[i]->state;if(players_[i]->state.deployed)out.phase=wire::RoundPhase::active;}
 if(ended_){out.phase=wire::RoundPhase::ended;out.countdown=false;out.remainingMs=0;}
 return out;
}
}
