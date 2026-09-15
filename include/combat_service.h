#pragma once
#include "combat_wire.h"
#include "combat_round.h"
#include <functional>
namespace mgo2win::combat {
struct Delivery {Identity recipient;std::vector<uint8_t> payload;};
// Only host-owned code may configure a scene, grant a spawn/inventory and start
// a round. No network message can grant HP, ammunition, team or spawn positions.
class Service {
 Authority authority_;uint64_t epoch_=0,nextState_=0;bool worldReady_=false,profileReady_=false;uint64_t publishedRevision_=0;wire::Status publishedStatus_=wire::Status::awaiting_world;
 std::vector<Weapon> profiles_;std::unique_ptr<RoundCoordinator> round_;uint32_t inputTimeout_;
 struct Peer {
  Identity id;bool accepted=false,sequenced=false;uint32_t sequence=0;uint64_t lastInput=0;
  std::optional<wire::Input> input;uint64_t roundRevision=0;
  uint16_t weapon=0;bool held=false,pressed=false,fireReady=false;uint32_t life=1;
  Pose firePose;uint32_t shotSequence=0;uint64_t pressAt=0;
  void stop(){input.reset();held=pressed=fireReady=false;}
 };
 std::map<uint32_t,Peer> peers_;std::vector<Delivery> deliveries_;
 std::function<Decision(std::span<const Event>,uint64_t)> eventHandler_;
 void dispatch(std::span<const Event>,uint64_t);
 wire::Status status()const;void frame(Identity,std::span<const Event> = {});void broadcast(std::span<const Event> = {});
public:
 explicit Service(uint64_t epoch,Policy={});
 wire::Status current_status()const{return status();}
 Authority& authority(){return authority_;}
 void configure(std::shared_ptr<const stage::Collision>,std::span<const Weapon>,std::shared_ptr<const stage::Collision> targets={});
 void configure_round(RoundCoordinator::Policy,std::shared_ptr<const weapons::Catalog>,RoundCoordinator::Spawn);
 bool take_round_start(){return round_&&round_->take_start();}
 bool ended()const{return round_&&round_->ended();}
 std::optional<wire::Preparation> preparation(Identity id,uint64_t now)const {return round_?std::optional(round_->state(id,now)):std::nullopt;}
 bool install_loadout(const host_skills::Verified&);
 bool admit(Identity,uint64_t now=0,std::optional<uint8_t> retainedTeam=std::nullopt);void remove(Identity);
 // Valid requests are coalesced per host tick; poll performs authority checks.
 bool receive(Identity,std::span<const uint8_t>,uint64_t now);
 void poll(uint64_t now,uint32_t subMsNs=0);
 std::vector<Delivery> deliveries();
 bool pending_deliveries()const{return !deliveries_.empty();}
 void event_handler(std::function<Decision(std::span<const Event>,uint64_t)> handler){eventHandler_=std::move(handler);}
 // Keep operator form transitions in the same reliable FIFO as other states;
 // a rapid human -> special transition must not collapse into one timed frame.
 void publish_operator_state(){broadcast();}
};
}
