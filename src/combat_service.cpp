#include "combat_service.h"
#include "special_action_motion.h"
#include "evade_runtime_profile.h"
#include <algorithm>
#include <cmath>
#include <utility>
namespace mgo2win::combat {
Service::Service(uint64_t epoch,Policy policy):authority_(policy),epoch_(epoch),inputTimeout_(policy.stalePoseMs){if(!epoch)throw std::invalid_argument("Combat epoch");}
void Service::configure(std::shared_ptr<const stage::Collision>world,std::span<const Weapon>weapons,std::shared_ptr<const stage::Collision>targets){if(worldReady_)throw std::invalid_argument("Combat world already configured");authority_.begin(epoch_,std::move(world),weapons,std::move(targets));worldReady_=true;profileReady_=!weapons.empty();profiles_.assign(weapons.begin(),weapons.end());
 if(std::any_of(weapons.begin(),weapons.end(),[](const auto& w){return w.id==25&&w.nativePrimaryMastery;}))authority_.configure_sop(player::special_start_ms,player::special_end_ms);
 authority_.configure_evade(evade_runtime::roll,evade_runtime::backstep);
}
void Service::configure_round(RoundCoordinator::Policy policy,std::shared_ptr<const weapons::Catalog> catalog,RoundCoordinator::Spawn spawn){if(round_||!peers_.empty())throw std::invalid_argument("Round already admitted");round_=std::make_unique<RoundCoordinator>(epoch_,policy,std::move(catalog),profiles_,worldReady_?std::move(spawn):RoundCoordinator::Spawn{});}
wire::Status Service::status()const{if(ended())return wire::Status::ended;if(!worldReady_)return wire::Status::awaiting_world;if(!profileReady_)return wire::Status::awaiting_profile;auto s=authority_.snapshot();if(std::none_of(s.players.begin(),s.players.end(),[](auto&p){return bool(p);}))return wire::Status::awaiting_spawn;return authority_.active()?wire::Status::active:wire::Status::preparing;}
bool Service::install_loadout(const host_skills::Verified&v){auto s=v.scope();auto it=peers_.find(s.character);return worldReady_&&it!=peers_.end()&&it->second.id==Identity{s.slot,s.instance,s.character}&&authority_.install_loadout(v);}
bool Service::admit(Identity id,uint64_t now,std::optional<uint8_t> retainedTeam){if(id.slot>=24||!id.instance||!id.character||peers_.contains(id.character)||std::any_of(peers_.begin(),peers_.end(),[&](auto&p){return p.second.id.slot==id.slot;}))return false;if(round_&&!round_->join(id,now,retainedTeam))return false;peers_.emplace(id.character,Peer{id});deliveries_.push_back({id,wire::encode(wire::Offer{epoch_,id})});return true;}
void Service::remove(Identity id){auto it=peers_.find(id.character);if(it==peers_.end()||it->second.id!=id)return;authority_.leave(id);if(round_)round_->leave(id);peers_.erase(it);std::erase_if(deliveries_,[&](auto&d){return d.recipient==id;});broadcast();}
void Service::frame(Identity id,std::span<const Event>events){Snapshot s=authority_.snapshot();if(!worldReady_)s={epoch_,1,0,{}};
 auto sop=authority_.sop_view(id).value_or(SopView{});
 // GWCB10 adds host-owned oxygen to per-player action state. Find a bounded chunk that really fits
 // the unchanged 2000-byte datagram; never omit an event to fit a snapshot.
 do{size_t count=std::min<size_t>(events.size(),4);std::vector<uint8_t> encoded;
  for(;;){auto chunk=events.first(count);try{encoded=wire::encode(wire::Frame{s,status(),{chunk.begin(),chunk.end()},sop});break;}catch(const wire::Invalid&){if(count<=1)throw;--count;}}
  deliveries_.push_back({id,std::move(encoded)});events=events.subspan(count);
 }while(!events.empty());
}
void Service::broadcast(std::span<const Event>events){for(auto&[id,p]:peers_)if(p.accepted)frame(p.id,events);}
void Service::dispatch(std::span<const Event> events,uint64_t now){
 if(events.empty())return;
 std::vector<Event> all(events.begin(),events.end());
 if(eventHandler_){auto extra=eventHandler_(events,now);all.insert(all.end(),extra.events.begin(),extra.events.end());}
 // Object destruction and delayed damage create newer IDs. Preserve the single
 // event cursor order even when an earlier producer emitted several impacts.
 std::stable_sort(all.begin(),all.end(),[](const auto&a,const auto&b){return a.id<b.id;});broadcast(all);
}
bool Service::receive(Identity id,std::span<const uint8_t>b,uint64_t now){
 if(round_&&round_->expire(authority_,now))for(auto&[character,p]:peers_)p.stop();
 auto it=peers_.find(id.character);if(it==peers_.end()||it->second.id!=id)return false;
 try{auto r=wire::decode(b);auto&p=it->second;
  if(auto a=std::get_if<wire::Accept>(&r)){if(a->epoch!=epoch_)return false;if(!p.accepted){p.accepted=true;frame(id);if(round_){auto state=round_->state(id,now);p.roundRevision=state.revision;deliveries_.push_back({id,wire::encode(state)});}}return true;}
  if(auto command=std::get_if<wire::Command>(&r)){if(!p.accepted||!round_||!round_->command(id,*command,authority_,now))return false;auto state=round_->state(id,now);const auto&self=state.players[id.slot];if(self&&self->deployed&&p.life!=self->life){p.stop();p.life=self->life;p.sequenced=false;p.shotSequence=0;p.weapon=0;}p.roundRevision=state.revision;deliveries_.push_back({id,wire::encode(state)});return true;}
  auto input=std::get_if<wire::Input>(&r);if(!input||!p.accepted||!worldReady_||!authority_.active()||input->epoch!=epoch_)return false;
  const auto snapshot=authority_.snapshot();const auto&body=snapshot.players[id.slot];if(!body||body->identity!=id||!body->alive||input->life!=body->life)return false;
  if(round_){const auto state=round_->state(id,now);const auto&self=state.players[id.slot];if(!self||!self->loaded||!self->deployed||self->life!=input->life)return false;}
  if(p.life!=input->life){p.stop();p.life=input->life;p.sequenced=false;p.shotSequence=0;p.weapon=0;}
  if(p.sequenced&&(input->sequence==p.sequence||uint32_t(input->sequence-p.sequence)>=0x80000000u||now<p.lastInput))return false;
  p.sequenced=true;p.sequence=input->sequence;p.lastInput=now;
  // Only an explicit edge is a short press. A held packet after a timeout must
  // not synthesize a second semiautomatic shot without a physical new press.
  if(p.input)*input=wire::coalesce_input(*p.input,*input);
  p.input=*input;return true;

 }catch(const wire::Invalid&){return false;}
}
void Service::poll(uint64_t now,uint32_t subMsNs){
 if(subMsNs>=1000000)throw std::invalid_argument("Combat sub-millisecond clock");
 if(round_){round_->poll(authority_,now);for(auto&[id,p]:peers_)if(p.accepted){auto state=round_->state(p.id,now);if(state.revision!=p.roundRevision){p.roundRevision=state.revision;deliveries_.push_back({p.id,wire::encode(state)});}}}
 if(!ended()){authority_.environment(now);auto falls=authority_.advance_falling(now);dispatch(falls.events,now);auto burn=authority_.advance_burning(now);dispatch(burn.events,now);auto projectiles=authority_.advance_projectiles(now);dispatch(projectiles.events,now);auto actions=authority_.advance_special_pc(now);dispatch(actions.events,now);}
 for(auto&[character,peer]:peers_){auto id=peer.id;
  auto stop=[&]{peer.stop();authority_.aiming(id,false);authority_.clear_jump_velocity(id);authority_.release_special(id,now);authority_.release_evade(id,now);authority_.release_cover(id);};
  if(!authority_.active()||!peer.sequenced||now<peer.lastInput||now-peer.lastInput>=inputTimeout_){stop();continue;}
  if(round_){const auto state=round_->state(id,now);const auto&self=state.players[id.slot];if(!self||!self->loaded||!self->deployed||self->life!=peer.life){stop();continue;}}
  if(peer.input){auto input=*peer.input;peer.input.reset();
   if(input.suspended){stop();continue;}
   authority_.aiming(id,false);
   if(auto body=authority_.snapshot().players[id.slot];body&&body->specialPc.action!=special_pc::Action::none){peer.held=peer.pressed=peer.fireReady=false;continue;}
   if(authority_.pose(id,epoch_,input.sequence,input.pose,now,input.life)!=Reject::none){stop();continue;}
   auto falls=authority_.advance_falling(now);dispatch(falls.events,now);
   if(auto body=authority_.snapshot().players[id.slot];!body||!body->alive){stop();continue;}
   // Equipment changes are revision-checked GWIV commands. This field is only
   // the last observed weapon; an old heartbeat cannot reverse an accepted equip.
   const auto current=authority_.snapshot().players[id.slot];
   if(!current||input.weapon!=current->weapon){peer.held=peer.pressed=peer.fireReady=false;continue;}
   if(input.specialPc.request){authority_.special_action(id,epoch_,input.sequence,input.specialPc,now,input.life);broadcast();}
   if(authority_.snapshot().players[id.slot]->specialPc.action!=special_pc::Action::none){peer.held=peer.pressed=peer.fireReady=false;continue;}
   {auto action=authority_.ladder_action(id,epoch_,input.sequence,input.ladder,now,input.life);dispatch(action.events,now);if(input.ladder.action!=ladder::Action::none)broadcast();
    if(authority_.snapshot().players[id.slot]->ladderAnchor){peer.held=peer.pressed=peer.fireReady=false;continue;}}
   authority_.cover(id,epoch_,input.sequence,input.cover,now,input.life);
   if(input.cover.request)broadcast();
   if(input.evadeRequest){authority_.evade(id,epoch_,input.sequence,input.evadeKind,input.evadeRequest,now,input.life);broadcast();}
   authority_.special(id,epoch_,input.sequence,input.specialPressed,input.specialHeld,now,input.life);
   if(input.evadeRequest||authority_.snapshot().players[id.slot]->evadeKind!=EvadeKind::none){peer.held=peer.pressed=peer.fireReady=false;continue;}
   if(input.specialPressed||input.specialHeld||authority_.snapshot().players[id.slot]->specialPhase!=SpecialPhase::none){peer.held=peer.pressed=peer.fireReady=false;continue;}
   authority_.aiming(id,input.aiming);
   if(peer.weapon!=input.weapon)peer.pressed=false;
   peer.weapon=input.weapon;peer.firePose=input.pose;peer.fireReady=true;peer.held=input.fire;
   if(input.firePressed){peer.pressed=true;peer.pressAt=now;}
   if(input.reload){peer.held=peer.pressed=false;auto action=authority_.reload(id,epoch_,now,peer.life);if(!action.events.empty())broadcast(action.events);continue;}
  }
  if(peer.pressed&&(now<peer.pressAt||now-peer.pressAt>=inputTimeout_))peer.pressed=false;
  auto weapon=std::find_if(profiles_.begin(),profiles_.end(),[&](auto&w){return w.id==peer.weapon;});
  if(!peer.fireReady||weapon==profiles_.end()||!(peer.pressed||(peer.held&&weapon->automatic)))continue;
  auto&p=peer.firePose;Vec3 direction{std::sin(p.yaw)*std::cos(p.pitch),std::sin(p.pitch),std::cos(p.yaw)*std::cos(p.pitch)};
  // Fire sequence belongs to the host. Automatic fire runs on simulation ticks,
  // including ticks with no packet; one tick never replays a catch-up burst.
  auto action=authority_.fire(id,{epoch_,++peer.shotSequence,peer.weapon,direction,peer.life},now,subMsNs);
  if(action.reject!=Reject::interval)peer.pressed=false;
  if(action.reject==Reject::generation||action.reject==Reject::dead||action.reject==Reject::identity||action.reject==Reject::invalid_pose||action.reject==Reject::clock)peer.stop();
  dispatch(action.events,now);
 }
 if(!ended()){auto items=authority_.advance_items(now);dispatch(items.events,now);authority_.advance(now);}if(now>=nextState_||status()!=publishedStatus_){auto revision=authority_.snapshot().revision;auto currentStatus=status();if(revision!=publishedRevision_||currentStatus!=publishedStatus_){broadcast();publishedRevision_=revision;publishedStatus_=currentStatus;}nextState_=now+200;}
 // Conservative single-tick bound: 24*24*68 one-event frames + 24*24
 // control broadcasts + 24 phase + 24 state = 39792 before environmental
 // damage and the bounded projectile trail batch. The 65536 queue includes
 // those extra reliable records; no damage event is silently discarded. Evade records can
 // reduce a full roster to one event/frame. Callers must still drain each
 // tick; this does not guarantee accumulation over undrained multiple ticks.
 if(deliveries_.size()>65536)throw std::runtime_error("Combat delivery queue not drained");
}
std::vector<Delivery> Service::deliveries(){return std::exchange(deliveries_,{});}
}

