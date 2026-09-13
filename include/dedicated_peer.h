#pragma once
#include "host_protocol.h"
#include "host_roster.h"
#include "host_match.h"
#include "host_mandatory_backlog.h"
#include "stage_object_sync.h"
#include <map>
#include <set>
namespace mgo2win::host {
// Server half of the existing room-admission protocol. Lobby 4340 must approve
// a character before the caller publishes roster/global state to this peer.
struct ProfileNames {std::string name,clan;uint32_t clanId=0;std::array<uint8_t,28> appearance{};};
ProfileNames profile_names(std::span<const uint8_t>);
std::vector<uint8_t> roster_record(const Player&,const Hello&,uint32_t clanId=0);
std::vector<uint8_t> roster_remove(uint16_t instance);
std::vector<uint8_t> room_snapshot(std::span<const Rotation>,uint8_t generation,uint8_t index=0,uint8_t round=0);
std::vector<uint8_t> next_round_snapshot(uint8_t generation,uint8_t round);
enum class PeerCloseReason {none,protocol,mandatory_overflow,invalid_payload};
class DedicatedPeer {
 Hello local_,remote_;Keys keys_;uint16_t tx_=0,rx_=0;uint64_t replay_=0,last_=0,start_=0,helloNext_=0,keepalive_=0;
 bool received_=false,helloAcked_=false,closed_=false;uint8_t txApp_=0,rxApp_=0;
 struct Pending {Message message;uint64_t next=0;unsigned tries=0;uint64_t ticket=0;};
 uint64_t nextTicket_=1,deliveredThrough_=0;std::set<uint64_t> delivered_;
 MandatoryBacklog mandatory_;PeerCloseReason closeReason_=PeerCloseReason::none;
 bool writable_serial(unsigned limit)const;void promote(uint64_t now);
 std::map<uint8_t,Pending> pending_;std::map<uint8_t,Message> reordered_;
 std::vector<Message> acks_;std::vector<std::vector<uint8_t>> events_;
 std::vector<uint8_t> profile_;
 std::optional<LoadRequest> objectRequest_;std::optional<stage::SceneReceiver> objectCheck_;uint8_t objectSlot_=255,objectRx_=0,objectTx_=0;uint16_t objectPacket_=0;size_t objectSnapshotSize_=0;
 std::map<uint8_t,Pending> objectPending_;std::map<uint8_t,Message> objectReordered_;std::vector<uint8_t> snapshotRequests_;
public:
 DedicatedPeer(Hello local,Hello remote,uint64_t now);
 void receive(std::span<const uint8_t>,uint64_t now);
 std::vector<std::vector<uint8_t>> poll(uint64_t now);
 // All mandatory records enter the same bounded FIFO, including metadata.
 // A returned ticket is enqueued, not necessarily assigned a wire serial yet.
 // Receipt confirms this reliable application record reached the peer. It is
 // not a claim that the peer loaded models or completed scene state reception.
 uint64_t queue(std::vector<uint8_t>,uint64_t now);
 // Optional application traffic reserves capacity for room/combat records.
 // Failure returns zero and never closes the peer or consumes a serial/ticket.
 uint64_t optional_queue(std::span<const uint8_t>,uint64_t now);
 bool delivery_complete(uint64_t ticket)const{return !closed_&&ticket&&ticket<=deliveredThrough_;}
 void objects(const stage::ObjectRegistry&,const LoadRequest&,uint8_t admittedSlot);
 bool queue_object(std::vector<uint8_t>,uint64_t now);
 std::vector<uint8_t> snapshot_requests();
 std::vector<std::vector<uint8_t>> events();
 bool closed()const{return closed_;}
 PeerCloseReason close_reason()const{return closeReason_;}
 size_t mandatory_backlog()const{return mandatory_.size();}
 void close(PeerCloseReason reason=PeerCloseReason::protocol){if(!closed_)closeReason_=reason;closed_=true;mandatory_.clear();events_.clear();pending_.clear();reordered_.clear();acks_.clear();delivered_.clear();objectPending_.clear();objectReordered_.clear();objectCheck_.reset();objectRequest_.reset();snapshotRequests_.clear();}
 const Hello& hello()const{return remote_;}
};
}
