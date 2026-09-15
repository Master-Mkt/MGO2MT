#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>
#include <optional>
#include <mutex>
#include <atomic>
namespace mgo2win {
enum class LobbyDisconnectReason {none,beacon_timeout,invalid_beacon,network_error};
struct LobbyMonitorState {
 uint64_t generation=0;bool connected=false;std::optional<uint32_t> pingMs;uint64_t lastBeaconMs=0;
 LobbyDisconnectReason reason=LobbyDisconnectReason::none;
 bool operator==(const LobbyMonitorState&)const=default;
};
// One authenticated game connection, independent of room join/leave. Generation
// guards prevent an old worker's teardown from clearing a replacement login.
class LobbyMonitor {
 mutable std::mutex mutex_;LobbyMonitorState state_;
public:
 LobbyMonitorState state()const{std::lock_guard lock(mutex_);return state_;}
 void begin(uint64_t generation,std::atomic_bool* cancelJoin=nullptr){if(!generation)throw std::invalid_argument("Lobby monitor generation");std::lock_guard lock(mutex_);state_={generation,true,{},{},LobbyDisconnectReason::none};if(cancelJoin)*cancelJoin=false;}
 void beacon(uint64_t generation,uint32_t ping,uint64_t at){std::lock_guard lock(mutex_);if(state_.generation!=generation||!state_.connected)return;state_.pingMs=ping;state_.lastBeaconMs=at;}
 void disconnect(uint64_t generation,LobbyDisconnectReason reason=LobbyDisconnectReason::none,std::atomic_bool* cancelJoin=nullptr){std::lock_guard lock(mutex_);if(state_.generation!=generation)return;state_.connected=false;if(cancelJoin&&reason!=LobbyDisconnectReason::none)*cancelJoin=true;if(state_.reason==LobbyDisconnectReason::none)state_.reason=reason;}
};
class LobbyBeaconError:public std::runtime_error {
public:
 LobbyDisconnectReason reason;
 explicit LobbyBeaconError(LobbyDisconnectReason value):std::runtime_error(value==LobbyDisconnectReason::beacon_timeout?"Lobby beacon timeout":"Invalid lobby beacon"),reason(value){}
};
// NomadLobby 0005 -> 0005 / exact BE32 zero. Native cadence, not PS3 timing.
// Start only after successful authenticated game-session admission. First probe
// is immediate; a nonce-less original reply permits only one outstanding probe.
class LobbyKeepalive {
 uint64_t start_,lastGood_,sentAt_=0;bool pending_=false,sent_=false;std::optional<uint32_t> ping_;
public:
 static constexpr uint64_t interval_ms=15000,response_timeout_ms=30000;
 explicit LobbyKeepalive(uint64_t now):start_(now),lastGood_(now){}
 void check(uint64_t now)const{
  if(now<start_||now<lastGood_||(sent_&&now<sentAt_))throw LobbyBeaconError(LobbyDisconnectReason::invalid_beacon);
  if(now-lastGood_>=response_timeout_ms)throw LobbyBeaconError(LobbyDisconnectReason::beacon_timeout);
 }
 bool poll(uint64_t now){check(now);if(pending_||(sent_&&now-sentAt_<interval_ms))return false;pending_=sent_=true;sentAt_=now;return true;}
 bool receive(uint16_t command,std::span<const uint8_t> payload,uint64_t now){
  check(now);if(command!=0x0005)return false;
  if(!pending_||payload.size()!=4||payload[0]||payload[1]||payload[2]||payload[3])throw LobbyBeaconError(LobbyDisconnectReason::invalid_beacon);
  pending_=false;ping_=uint32_t(now-sentAt_);lastGood_=now;return true;
 }
 bool pending()const{return pending_;}
 std::optional<uint32_t> ping_ms()const{return ping_;}
 uint64_t last_beacon_ms()const{return ping_?lastGood_:0;}
};
}

