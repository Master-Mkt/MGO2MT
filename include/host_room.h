#pragma once
#include "host_match.h"
#include <atomic>
#include <functional>
#include <string>
namespace mgo2win { struct LobbyPacket; struct NetworkKeys; }
namespace mgo2win::host {
// Lobby-side dedicated-room control. The transport supplies an authenticated
// game-lobby connection and encrypts only host_room_wire_payload's output.
using RoomExchange=std::function<LobbyPacket(uint16_t,std::span<const uint8_t>)>;
// Retail menu callbacks assign the entire rotation mode byte, not an OR mask.
enum class RotationMode:uint8_t {normal=0,drebin_points=2,headshots_only=4};
struct Settings {
 std::wstring name=L"MGO2WIN HOST",comment=L"OpenMGO2 dedicated host",password;
 uint8_t subtype=2,capacity=17; // Server capacity includes the dedicated host.
 uint32_t briefing_minutes=2;
 std::vector<Rotation> rotations{{20,1,0}};
 bool friendly_fire=false,auto_aim=true,uniques=false,non_stat=false;
 bool enemy_nametags=true,silent=false,auto_assign=true,teams_switch=true;
 bool ghosts=false,voice_chat=true,level_limit=false;
 // Retail kick editors accept 0..99, with zero disabling the policy. LEVEL
 // tolerance has a native bound 0..63 within the original 64-entry table.
 uint8_t idle_kick_minutes=5,team_kill_kick=3,level_limit_tolerance=0;
 // Original standard-rate / level-limit-base field, native LEVEL range 0..64.
 // The current candidate
 // environment factory does not yet preserve a custom base (defaults to 22).
 uint32_t level_limit_base=22;
 uint8_t unique_red=0,unique_blue=2;
 std::array<uint8_t,16> weapon_restrictions{};
};
constexpr size_t room_settings_size=345,room_environment_size=204;
std::vector<uint8_t> settings_payload(const Settings&);
std::array<uint8_t,room_environment_size> room_environment(const Settings&);
std::vector<uint8_t> host_room_wire_payload(const NetworkKeys&,uint16_t,std::span<const uint8_t>);
enum class RoomControlStatus {success,rejected,cancelled,invalid_input,invalid_state,protocol_error,outcome_unknown};
struct CreateReply {RoomControlStatus status=RoomControlStatus::protocol_error;uint32_t room=0,error=0;bool room_may_exist=false;};
struct RoomControlReply {RoomControlStatus status=RoomControlStatus::protocol_error;uint32_t error=0;};
// No implicit create retries after a transport failure. The same connection
// owns close/lease/peer registration; reconstructing a Lifecycle is not recovery.
class Lifecycle {
 Settings settings_;uint32_t room_=0;bool may_exist_=false,attempted_=false;
 RoomControlReply control(uint16_t,std::span<const uint8_t>,const RoomExchange&,uint32_t identity=0);
public:
 CreateReply create(const Settings&,const RoomExchange&,const std::atomic_bool& cancel);
 RoomControlReply heartbeat(const RoomExchange&);
 RoomControlReply player_connected(uint32_t,const RoomExchange&);
 RoomControlReply player_disconnected(uint32_t,const RoomExchange&);
 RoomControlReply player_team(uint32_t,uint8_t,const RoomExchange&);
 RoomControlReply round_started(uint8_t,const RoomExchange&);
 RoomControlReply rotation(uint8_t,const RoomExchange&);
 RoomControlReply close(const RoomExchange&);
 uint32_t room()const{return room_;}
 bool room_may_exist()const{return may_exist_;}
 const Settings& settings()const{return settings_;}
};
}
