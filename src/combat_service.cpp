#include "combat_service.h"
#include "special_action_motion.h"
#include <algorithm>
#include <cmath>
#include <utility>
namespace mgo2win::combat {
Service::Service(uint64_t epoch,Policy policy):authority_(policy),epoch_(epoch),inputTimeout_(policy.stalePoseMs){if(!epoch)throw std::invalid_argument("Combat epoch");}
void Service::configure(std::shared_ptr<const stage::Collision>world,std::span<const Weapon>weapons,std::shared_ptr<const stage::Collision>targets){if(worldReady_)throw std::invalid_argument("Combat world already configured");authority_.begin(epoch_,std::move(world),weapons,std::move(targets));worldReady_=true;profileReady_=!weapons.empty();profiles_.assign(weapons.begin(),weapons.end());
 if(std::any_of(weapons.begin(),weapons.end(),[](const auto& w){return w.id==25&&w.nativePrimaryMastery;}))authority_.configure_sop(player::special_start_ms,player::special_end_ms);
}
void Service::configure_round(RoundCoordinator::Policy policy,std::shared_ptr<const weapons::Catalog> catalog,RoundCoordinator::Spawn spawn){if(round_||!peers_.empty())throw std::invalid_argument("Round already admitted");round_=std::make_unique<RoundCoordinator>(epoch_,policy,std::move(catalog),profiles_,worldReady_?std::move(spawn):RoundCoordinator::Spawn{});}
wire::Status Service::status()const{if(ended())return wire::Status::ended;if(!worldReady_)return wire::Status::awaiting_world;if(!profileReady_)return wire::Status::awaiting_profile;auto s=authority_.snapshot();if(std::none_of(s.players.begin(),s.players.end(),[](auto&p){return bool(p);}))return wire::Status::awaiting_spawn;return authority_.active()?wire::Status::active:wire::Status::preparing;}
bool Service::install_loadout(const host_skills::Verified&v){auto s=v.scope();auto it=peers_.find(s.character);return worldReady_&&it!=peers_.end()&&it->second.id==Identity{s.slot,s.instance,s.character}&&authority_.install_loadout(v);}
bool Service::admit(Identity id,uint64_t now,std::optional<uint8_t> retainedTeam){if(id.slot>=24||!id.instance||!id.character||peers_.contains(id.character)||std::any_of(peers_.begin(),peers_.end(),[&](auto&p){return p.second.id.slot==id.slot;}))return false;if(round_&&!round_->join(id,now,retainedTeam))return false;peers_.emplace(id.character,Peer{id});deliveries_.push_back({id,wire::encode(wire::Offer{epoch_,id})});return true;}
void Service::remove(Identity id){auto it=peers_.find(id.character);if(it==peers_.end()||it->second.id!=id)return;authority_.leave(id);if(round_)round_->leave(id);peers_.erase(it);std::erase_if(deliveries_,[&](auto&d){return d.recipient==id;});broadcast();}
void Service::frame(Identity id,std::span<const Event>events){Snapshot s=authority_.snapshot();if(!worldReady_)s={epoch_,1,0,{}};
 auto sop=authority_.sop_view(id).value_or(SopView{});
 // GWCB 8 includes a recipient-specific footer. A full 24-player snapshot
 // fits three events with that footer inside the unchanged 2000-byte limit.
 // Every chunk uses the same final snapshot, footer and event watermark.
 const size_t capacity=sop.recipient!=Identity{}&&std::count_if(s.players.begin(),s.players.end(),[](auto& p){return bool(p);})==24?3:4;
 do{auto count=std::min(events.size(),capacity);auto chunk=events.first(count);deliveries_.push_back({id,wire::encode(wire::Frame{s,status(),{chunk.begin(),chunk.end()},sop})});events=events.subspan(count);}while(!events.empty());
}
void Service::broadcast(std::span<const Event>events){for(auto&[id,p]:peers_)if(p.accepted)frame(p.id,events);}
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
 for(auto&[character,peer]:peers_){auto id=peer.id;
  auto stop=[&]{peer.stop();authority_.release_special(id,now);};
  if(!authority_.active()||!peer.sequenced||now<peer.lastInput||now-peer.lastInput>=inputTimeout_){stop();continue;}
  if(round_){const auto state=round_->state(id,now);const auto&self=state.players[id.slot];if(!self||!self->loaded||!self->deployed||self->life!=peer.life){stop();continue;}}
  if(peer.input){auto input=*peer.input;peer.input.reset();
   if(input.suspended){stop();continue;}
   if(authority_.pose(id,epoch_,input.sequence,input.pose,now,input.life)!=Reject::none){stop();continue;}
   // Input carries the last observed equipped weapon. A delayed unarmed pose
   // must not undo an authoritative pickup before its Frame reaches the client.
   const auto current=authority_.snapshot().players[id.slot];
   if((!input.weapon&&current&&current->weapon)||authority_.equip(id,epoch_,input.weapon,now,input.life)!=Reject::none){stop();continue;}
   authority_.special(id,epoch_,input.sequence,input.specialPressed,input.specialHeld,now,input.life);
   if(input.specialPressed||input.specialHeld||authority_.snapshot().players[id.slot]->specialPhase!=SpecialPhase::none){peer.held=peer.pressed=peer.fireReady=false;continue;}
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
  if(!action.events.empty())broadcast(action.events);
 }
 if(!ended())authority_.advance(now);if(now>=nextState_||status()!=publishedStatus_){auto revision=authority_.snapshot().revision;auto currentStatus=status();if(revision!=publishedRevision_||currentStatus!=publishedStatus_){broadcast();publishedRevision_=revision;publishedStatus_=currentStatus;}nextState_=now+200;}
 // Up to 24 shooters x 24 recipients x 23 three-event frames, plus state/phase.
 // The transport keeps a separately bounded FIFO; callers still drain per tick.
 if(deliveries_.size()>16384)throw std::runtime_error("Combat delivery queue not drained");
}
std::vector<Delivery> Service::deliveries(){return std::exchange(deliveries_,{});}
}
