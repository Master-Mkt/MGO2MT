#pragma once
#include "host_protocol.h"
#include "host_roster.h"
#include "host_match.h"
#include <map>
#include <set>
namespace mgo2win::host {
// Server half of the existing room-admission protocol. Lobby 4340 must approve
// a character before the caller publishes roster/global state to this peer.
struct ProfileNames {std::string name,clan;uint32_t clanId=0;};
ProfileNames profile_names(std::span<const uint8_t>);
std::vector<uint8_t> roster_record(const Player&,const Hello&,uint32_t clanId=0);
std::vector<uint8_t> roster_remove(uint16_t instance);
std::vector<uint8_t> room_snapshot(std::span<const Rotation>,uint8_t generation);
class DedicatedPeer {
 Hello local_,remote_;Keys keys_;uint16_t tx_=0,rx_=0;uint64_t replay_=0,last_=0,start_=0,helloNext_=0,keepalive_=0;
 bool received_=false,helloAcked_=false,closed_=false;uint8_t txApp_=0,rxApp_=0;
 struct Pending {Message message;uint64_t next=0;unsigned tries=0;uint64_t ticket=0;};
 uint64_t nextTicket_=1,deliveredThrough_=0;std::set<uint64_t> delivered_;
 std::map<uint8_t,Pending> pending_;std::map<uint8_t,Message> reordered_;
 std::vector<Message> acks_;std::vector<std::vector<uint8_t>> events_;
 std::vector<uint8_t> profile_;
public:
 DedicatedPeer(Hello local,Hello remote,uint64_t now);
 void receive(std::span<const uint8_t>,uint64_t now);
 std::vector<std::vector<uint8_t>> poll(uint64_t now);
 // Receipt confirms this reliable application record reached the peer. It is
 // not a claim that the peer loaded models or completed scene state reception.
 uint64_t queue(std::vector<uint8_t>,uint64_t now);
 bool delivery_complete(uint64_t ticket)const{return !closed_&&ticket&&ticket<=deliveredThrough_;}
 std::vector<std::vector<uint8_t>> events();
 bool closed()const{return closed_;}
 void close(){closed_=true;events_.clear();pending_.clear();reordered_.clear();acks_.clear();delivered_.clear();}
 const Hello& hello()const{return remote_;}
};
}
