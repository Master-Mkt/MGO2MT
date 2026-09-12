#pragma once
#include "authentication.h"
#include "host_session.h"
#include <filesystem>
#include <vector>
#include <span>
#include <functional>
#include <mutex>
#include <optional>
#include <memory>
namespace mgo2win {
// Native adapter contract from the reviewed OpenMGO2 candidate; not an ELF port.
struct NetworkKeys {
 std::array<uint32_t,1042> packet{},auth{};
 std::array<uint8_t,16> hmac{};std::array<uint8_t,4> wire{};std::array<uint8_t,8> salt{};
 static NetworkKeys load(const std::filesystem::path&);
};
void network_block(std::span<uint8_t>,const std::array<uint32_t,1042>&,bool encrypt);
struct LobbyPacket {uint16_t command=0;uint32_t sequence=0;std::vector<uint8_t> payload;};
std::vector<uint8_t> encode_lobby(const NetworkKeys&,uint16_t,uint32_t,std::span<const uint8_t>);
LobbyPacket decode_lobby(const NetworkKeys&,std::span<const uint8_t>,uint32_t expected);
std::vector<uint8_t> session_payload(const NetworkKeys&,const AuthReply&);
struct CharacterEntry {uint32_t id=0;std::wstring name;std::array<uint8_t,28> appearance{};bool main=false;};
struct CharacterList {unsigned slots=0;std::vector<CharacterEntry> entries;};
// OpenMGO2 native product policy: four included slots, then server-granted
// additional slots. Zero/invalid capacity stays unavailable, never unlimited.
constexpr unsigned character_slot_capacity(unsigned slots){return slots&&slots<=8?(slots<4?4:slots):0;}
CharacterList parse_characters(std::span<const uint8_t>);
// Gate records are treated as untrusted: never follow a different IP or port.
uint16_t account_endpoint(std::span<const uint8_t>);
enum class CharacterStatus { success,network_error,protocol_error,server_error,cancelled };
enum class CharacterStage { keys,gate_connect,gate_list,account_connect,session,list };
struct CharacterReply {CharacterStatus status=CharacterStatus::network_error;CharacterStage stage=CharacterStage::keys;unsigned error=0;uint16_t account_port=0;CharacterList list;};
CharacterReply fetch_characters(const std::filesystem::path&,const AuthReply&,const std::atomic_bool&);
CharacterReply probe_character_gate(const std::filesystem::path&,const std::atomic_bool&);
// Explicit wire values only. Never pass unreviewed native catalog IDs here.
// ELF F03D24: name16 + appearance9 + reserved4 + equipment14 = 43 bytes.
struct CharacterCreateRequest {std::wstring name;std::array<uint8_t,27> wire_appearance{};};
// ELF 985AA4/98B930..98BA90/F03D24: catalog IDs/colors, optional sentinels,
// male voice +7, female +16; displayed pitch +15. No asset conversion.
CharacterCreateRequest character_create_request(std::wstring,const std::array<uint8_t,28>&,int pitch);
enum class CharacterCreateStatus {success,rejected,full,cancelled,network_error,protocol_error,outcome_unknown};
struct CharacterCreateReply {CharacterCreateStatus status=CharacterCreateStatus::protocol_error;uint32_t created_id=0,error=0;bool request_may_have_been_sent=false;};
std::vector<uint8_t> character_create_payload(const CharacterCreateRequest&);
using CharacterExchange=std::function<LobbyPacket(uint16_t,std::span<const uint8_t>)>;
// Gate 2003: reviewed candidate Hub.java, 46-byte records. Directory display
// never authorizes connecting to an advertised port.
struct GameLobbyEntry {uint16_t id=0,port=0,players=0;std::wstring name;uint8_t restrictions=0;uint8_t subtype=0;};
struct LobbyDirectory {uint16_t account_port=0;std::vector<GameLobbyEntry> games;};
LobbyDirectory read_lobby_directory(const std::function<LobbyPacket()>&);
enum class CharacterSelectionContract {unverified,channel_snapshot_v1};
enum class CharacterSelectionStatus {success,unavailable,missing,rejected,cancelled,network_error,protocol_error,outcome_unknown};
struct CharacterSelectionReply {
 CharacterSelectionStatus status=CharacterSelectionStatus::protocol_error;
 CharacterEntry character;std::vector<GameLobbyEntry> lobbies;uint32_t error=0;
 bool request_may_have_been_sent=false;
};
// ELF F0394C / F02E9C: 3103 index byte, 3104 result32 (no ID echo).
// Only the reviewed server's same-channel list snapshot establishes identity.
CharacterSelectionReply exchange_character_selection(uint32_t,const CharacterExchange&,const std::atomic_bool&,CharacterSelectionContract);
CharacterSelectionReply select_character(const std::filesystem::path&,const AuthReply&,uint32_t,const std::atomic_bool&,CharacterSelectionContract=CharacterSelectionContract::unverified);
struct RoomEntry {uint32_t id=0;std::wstring name;uint8_t players=0,capacity=0,rule=0,map=0;bool password=false;};
enum class RoomStatus {connecting,ready,network_error,protocol_error,rejected,cancelled};
struct RoomPlayer {uint32_t id=0;std::wstring name;};
struct RoomDetail {uint32_t id=0;std::wstring name,comment;uint8_t subtype=0,capacity=0,players=0;bool password=false,dedicated=false;std::vector<RoomPlayer> roster;
 bool environment_known=false;uint32_t briefing_minutes=0;std::array<uint8_t,16> weapon_restrictions{};
};
enum class RoomEvent {list,detail,join};
enum class RoomJoinStatus {none,rejected,permission_checked,outcome_unknown,invalid_input,host_connecting,host_profile,host_sync,joined,host_cancelled,host_timeout,host_rejected,host_disconnected,host_network_error,host_protocol_error,host_unavailable};
struct RoomReply {RoomStatus status=RoomStatus::connecting;std::vector<RoomEntry> rooms;uint32_t error=0;RoomEvent event=RoomEvent::list;uint32_t requested_room=0;std::optional<RoomDetail> detail;RoomJoinStatus join_status=RoomJoinStatus::none;std::optional<host::Roster> host_roster;std::optional<host::MatchState> host_match;std::optional<host::Placements> host_placements;};
struct RoomAction {
 RoomEvent event=RoomEvent::detail;uint32_t id=0;uint8_t subtype=0;std::array<wchar_t,17> password{};
 ~RoomAction(){volatile wchar_t*p=password.data();for(size_t i=0;i<password.size();++i)p[i]=0;}
};
class RoomRequests {
 std::mutex mutex_;std::optional<RoomAction> action_;
public:
 std::atomic_bool cancel_join=false;
 std::shared_ptr<std::atomic_bool> uncertain=std::make_shared<std::atomic_bool>(false);
 bool submit(const RoomAction&a){std::lock_guard lock(mutex_);if(action_||*uncertain)return false;cancel_join=false;action_=a;return true;}
 std::optional<RoomAction> take(){std::lock_guard lock(mutex_);auto a=action_;action_.reset();return a;}
 void clear(){std::lock_guard lock(mutex_);action_.reset();}
};
RoomDetail parse_room_detail(std::span<const uint8_t>,uint32_t expectedId);
std::vector<uint8_t> room_join_payload(uint32_t,uint8_t,std::wstring_view);
std::vector<uint8_t> room_action_wire_payload(const NetworkKeys&,uint16_t,std::span<const uint8_t>);
using HostConnect=std::function<host::Result(const host::Admission&)>;
RoomJoinStatus room_host_status(host::Stage);
// Empty HostConnect is the explicit permission-only test adapter. Live callers
// always provide host transport; cancellation is separate from lobby shutdown.
RoomReply exchange_room_action(const RoomAction&,const CharacterExchange&,std::atomic_bool& uncertain,const std::atomic_bool& cancel,const HostConnect& host={});
std::vector<RoomEntry> read_room_directory(const std::function<LobbyPacket()>&);
std::vector<uint8_t> game_session_payload(const NetworkKeys&,const AuthReply&,uint32_t characterId);
using RoomPublish=std::function<void(RoomReply)>;
using RoomTransport=std::function<void(uint32_t,const GameLobbyEntry&,const std::atomic_bool&,std::atomic_bool&,const RoomPublish&,RoomRequests&)>;
// A live session owns its connection until cancellation/error. Refresh is read-only.
void run_game_lobby(const std::filesystem::path&,const AuthReply&,uint32_t,const GameLobbyEntry&,const std::atomic_bool&,std::atomic_bool&,const RoomPublish&,RoomRequests&,uintptr_t udpSocket=~uintptr_t(0));
// An authenticated account connection is required. No automatic retry after 3101.
CharacterCreateReply exchange_character_create(const CharacterCreateRequest&,const CharacterExchange&,const std::atomic_bool&);
CharacterCreateReply create_character(const std::filesystem::path&,const AuthReply&,const CharacterCreateRequest&,const std::atomic_bool&);
}
