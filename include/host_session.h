#pragma once
#include "host_protocol.h"
#include "host_roster.h"
#include "host_match.h"
#include "host_placements.h"
#include <atomic>
#include <functional>
#include <map>
#include <optional>
namespace mgo2win::host {
enum class Stage {connecting,profile,synchronizing,joined,cancelled,timeout,rejected,disconnected,network_error,protocol_error,unavailable};
struct Result {Stage stage=Stage::unavailable;unsigned error=0;bool profile_sent=false,was_joined=false;Roster roster;MatchState match;Placements placements;};
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
 Stage stage_=Stage::connecting;unsigned error_=0;
 Roster roster_;
 MatchState match_;
 PlacementReceiver placements_;std::map<uint8_t,Message> itemReordered_;
 uint8_t itemSerial_=0;uint16_t generationPacket_=0;
 struct Pending {Message message;uint64_t next=0;unsigned tries=0;};
 std::map<uint8_t,Pending> pending_;std::map<uint8_t,Message> reordered_;
 std::vector<Message> acks_;uint64_t hello_next_=0;
 void application(std::span<const uint8_t>,uint64_t);
 void queue(std::vector<uint8_t>,uint64_t);
 void fail(Stage,unsigned=0);
public:
 Machine(Hello,uint32_t host,std::vector<uint8_t> profile,uint64_t now);
 void receive(std::span<const uint8_t>,uint64_t);
 std::vector<std::vector<uint8_t>> poll(uint64_t);
 void cancel();
 Result result()const{return {stage_,error_,profile_sent_,was_joined_,roster_,match_,placements_.result()};}
 std::optional<std::vector<uint8_t>> leave_packet();
};
Result run(const Local&,const Admission&,std::span<const uint8_t> profile,const std::atomic_bool& stop,const std::atomic_bool& cancel,const std::function<void(Result)>& publish,const std::function<bool()>& lobbyAlive={});
}
