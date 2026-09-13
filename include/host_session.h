#pragma once
#include "host_protocol.h"
#include "host_roster.h"
#include "host_match.h"
#include "host_placements.h"
#include "stage_object_sync.h"
#include "combat_wire.h"
#include "native_radio.h"
#include "radio_session.h"
#include "world_inventory_session.h"
#include <memory>
#include <atomic>
#include <functional>
#include <map>
#include <optional>
namespace mgo2win::host {
enum class Stage {connecting,profile,synchronizing,joined,cancelled,timeout,rejected,disconnected,network_error,protocol_error,unavailable};
// Metadata only; never expose a HELLO seed, identity, endpoint or packet body.
struct ConnectionDiagnostics {Stage stage,last_active_stage;bool profile_sent,was_joined,hello_received,hello_acked;};
struct Result {Stage stage=Stage::unavailable;unsigned error=0;bool profile_sent=false,was_joined=false;Roster roster;MatchState match;Placements placements;std::optional<stage::SceneSnapshot> scene;stage::SceneSyncStatus scene_status=stage::SceneSyncStatus::idle;std::optional<combat::wire::Offer> combat_offer;std::optional<combat::Snapshot> combat_state;combat::wire::Status combat_status=combat::wire::Status::awaiting_world;std::vector<combat::Event> combat_events;std::optional<combat::wire::Preparation> preparation;combat::SopView combat_sop;};
bool active(Stage);
// The caller owns the checked UDP socket for the entire worker lifetime.
struct Local {uintptr_t socket=~uintptr_t(0);Endpoint private_endpoint,public_endpoint;uint32_t character=0;};
struct Admission {uint32_t character=0;std::array<Endpoint,2> endpoints;};
std::vector<uint8_t> profile_payload(uint32_t,std::span<const uint8_t> info,std::span<const uint8_t> personal,std::span<const uint8_t> skills);
std::optional<uint8_t> global_generation(std::span<const uint8_t>);
class Machine {
 Hello local_;uint32_t host_=0;std::vector<uint8_t> profile_;Keys keys_{};
 bool hello_received_=false,hello_acked_=false,profile_sent_=false,was_joined_=false;
 uint32_t peer_seed_=0;uint16_t tx_=0,rx_=0;bool received_=false;uint64_t replay_=0;
 uint8_t tx_app_=0,rx_app_=0;uint64_t start_=0,last_=0,stage_at_=0,keepalive_at_=0;
 Stage stage_=Stage::connecting,lastActiveStage_=Stage::connecting;unsigned error_=0;
 Roster roster_;
 std::optional<combat::wire::Offer> combatOffer_;combat::Replica combat_;combat::wire::Status combatStatus_=combat::wire::Status::awaiting_world;std::vector<combat::Event> combatEvents_;
 combat::SopView sop_;
 std::optional<combat::wire::Input> combatPendingInput_;
 std::optional<combat::wire::Preparation> preparation_;
 std::vector<radio::Body> radioMessages_;
 std::vector<std::vector<uint8_t>> inventoryMessages_;std::map<uint8_t,stage::ObjectRegistry> objectRegistries_;
 MatchState match_;
 PlacementReceiver placements_;std::map<uint8_t,Message> itemReordered_;
 uint8_t itemSerial_=0;uint16_t generationPacket_=0;
 std::optional<stage::ObjectRegistry> objectRegistry_;std::optional<stage::SceneReceiver> objects_;std::optional<uint8_t> objectSlot_;
 uint8_t objectRx_=0,objectTx_=0;uint64_t objectRequestAt_=0;
 std::map<uint8_t,Message> objectReordered_;
 struct Pending {Message message;uint64_t next=0;unsigned tries=0;};
 std::map<uint8_t,Pending> objectPending_;
 std::map<uint8_t,Pending> pending_;std::map<uint8_t,Message> reordered_;
 std::vector<Message> acks_;uint64_t hello_next_=0;
 void sync_objects();
 void application(std::span<const uint8_t>,uint64_t);
 void queue(std::vector<uint8_t>,uint64_t);
 void flush_combat_input(uint64_t);
 void fail(Stage,unsigned=0);
public:
 Machine(Hello,uint32_t host,std::vector<uint8_t> profile,uint64_t now,std::optional<stage::ObjectRegistry> registry=std::nullopt);
 void registries(std::vector<stage::ObjectRegistry>);
 bool inventory_send(std::span<const uint8_t>,uint64_t);
 std::vector<std::vector<uint8_t>> inventory_messages();
 void receive(std::span<const uint8_t>,uint64_t);
 std::vector<std::vector<uint8_t>> poll(uint64_t);
 void cancel();
 bool combat_input(const combat::wire::Input&,uint64_t now);
 bool combat_command(const combat::wire::Command&,uint64_t now);
 std::vector<combat::Event> combat_events();
 bool radio_send(std::span<const uint8_t>,uint64_t now);
 bool radio_writable()const{if(stage_!=Stage::joined||!combatOffer_||pending_.size()>=8||pending_.contains(tx_app_))return false;for(const auto& [serial,p]:pending_)if(uint8_t(tx_app_-serial)>=8)return false;return true;}
 std::vector<radio::Body> radio_messages();
 ConnectionDiagnostics diagnostics()const{return {stage_,active(stage_)?stage_:lastActiveStage_,profile_sent_,was_joined_,hello_received_,hello_acked_};}
 Result result()const{return {stage_,error_,profile_sent_,was_joined_,roster_,match_,placements_.result(),objects_?objects_->snapshot():std::nullopt,objects_?objects_->status():stage::SceneSyncStatus::idle,combatOffer_,combat_.state(),combatStatus_,combatEvents_,preparation_,sop_};}
 std::optional<std::vector<uint8_t>> leave_packet();
};
Result run(const Local&,const Admission&,std::span<const uint8_t> profile,const std::atomic_bool& stop,const std::atomic_bool& cancel,const std::function<void(Result)>& publish,const std::function<bool()>& lobbyAlive={},std::optional<stage::ObjectRegistry> registry=std::nullopt,const std::function<std::optional<combat::wire::Input>()>& combatInput={},const std::function<std::optional<combat::wire::Command>()>& combatCommand={},std::shared_ptr<radio::Session> radioSession={},std::shared_ptr<items::ClientSession> inventorySession={},std::vector<stage::ObjectRegistry> registries={});
}
