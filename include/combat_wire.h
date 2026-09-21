#pragma once
#include "combat_authority.h"
#include "environment_settings.h"
#include <variant>
#include <stdexcept>
namespace mgo2mt::combat::wire {
// Native extension: reliable channel 1, an explicit versioned offer/accept.
// Original damage packets have no shot identity or round epoch. This extension
// adds those for host validation and exactly-once effects. It is not an original
// MGO2 opcode. Unrecognized peers never receive inputs or state records.
constexpr uint8_t opcode=0xef;
// v22 adds a debug request byte and a separate <=1092-byte flight record.
// Normal Snapshot/player record positions remain unchanged.
constexpr uint8_t version=27; // v27 permits HOST-configured weapon cones up to 800 mrad.
constexpr uint32_t maximumRoundDurationMs=24u*60*60*1000;
enum class Status:uint8_t {awaiting_world,awaiting_profile,awaiting_spawn,preparing,active,ended};
struct Offer {uint64_t epoch=0;Identity self;uint64_t configuration=0;bool operator==(const Offer&)const=default;};
struct Accept {uint64_t epoch=0;uint64_t configuration=0;bool operator==(const Accept&)const=default;};
// fire is the newest held level; firePressed retains one short press while
// congested. They cannot be collapsed to one bit without phantom auto fire.
struct Input {uint64_t epoch=0;uint32_t sequence=0;Pose pose;uint16_t weapon=0;bool fire=false,reload=false,firePressed=false,suspended=false;uint32_t life=1;bool specialPressed=false,specialHeld=false;EvadeKind evadeKind=EvadeKind::none;uint32_t evadeRequest=0;cover::Intent cover;special_pc::Intent specialPc;ladder::Intent ladder;bool aiming=false,meleePressed=false,debugPhysics=false;mounted::Intent mounted;bool operator==(const Input&)const=default;};
Input coalesce_input(const Input& older,const Input& newer);
struct Frame {Snapshot snapshot;Status status=Status::awaiting_world;std::vector<Event> events;SopView sop;bool operator==(const Frame&)const=default;};
struct DebugFlights {uint64_t epoch=0,scene=0;Identity recipient;uint32_t life=0,sequence=0,inputSequence=0;uint64_t at=0;bool truncated=false;std::vector<projectile::DebugFlight> flights;bool operator==(const DebugFlights&)const=default;};
enum class RoundPhase:uint8_t {waiting,selecting,active,ended};
enum class CommandKind:uint8_t {loaded,ready,team,loadout};
enum class CommandError:uint8_t {none,sequence,unavailable,not_loaded,phase,team,weapon,restricted,insufficient_dp,spawn,already_deployed};
// The envelope's admitted peer supplies identity; never trust a requested ID.
struct Command {
 uint64_t epoch=0;uint32_t sequence=0;CommandKind kind=CommandKind::loaded;
 bool enabled=false;uint8_t team=0,generation=0;uint64_t sceneRevision=0;
 std::array<uint16_t,3> weapons{};
 uint32_t life=1;
 bool operator==(const Command&)const=default;
};
// While selecting, life is the next grant entitlement. A dead Player snapshot
// retains its previous life until the host spawn transaction succeeds.
struct RoundPlayer {Identity id;uint8_t team=0;bool loaded=false,ready=false,deployed=false;uint32_t life=1;uint32_t kills=0,deaths=0;bool operator==(const RoundPlayer&)const=default;};
struct Preparation {
 uint64_t epoch=0,revision=0;Identity self;uint8_t generation=0;
 RoundPhase phase=RoundPhase::waiting;bool runtimeReady=false,autoAssign=true;
 bool countdown=false;uint32_t remainingMs=0,lastCommand=0;CommandError error=CommandError::none;
 bool dpEnabled=false;uint32_t dpBalance=0;std::array<uint8_t,16> restrictions{};
 std::vector<uint16_t> supported;uint8_t requiredCategories=0;
 std::array<uint16_t,3> selected{};
 std::array<std::optional<RoundPlayer>,24> players{};
 // Independent native host clock; zero duration policy means unknown.
 bool roundClock=false;uint32_t roundRemainingMs=0;
 bool freeForAll=false;
 bool respawnWaiting=false;uint32_t respawnRemainingMs=0;
 bool operator==(const Preparation&)const=default;
};
struct Environment {uint64_t epoch=0,revision=0;environment::Config config;bool operator==(const Environment&)const=default;};
using Record=std::variant<Offer,Accept,Input,Frame,Command,Preparation,DebugFlights,Environment>;
struct Invalid:std::runtime_error {Invalid():runtime_error("Invalid native combat record"){};};
bool recognized(std::span<const uint8_t>);
std::vector<uint8_t> encode(const Record&);
Record decode(std::span<const uint8_t>);
}
