#pragma once
#include "authentication.h"
#include <filesystem>
#include <vector>
#include <span>
#include <functional>
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
// An authenticated account connection is required. No automatic retry after 3101.
CharacterCreateReply exchange_character_create(const CharacterCreateRequest&,const CharacterExchange&,const std::atomic_bool&);
CharacterCreateReply create_character(const std::filesystem::path&,const AuthReply&,const CharacterCreateRequest&,const std::atomic_bool&);
}
