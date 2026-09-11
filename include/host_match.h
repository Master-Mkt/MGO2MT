#pragma once
#include "host_protocol.h"
#include <optional>
namespace mgo2win::host {
enum class MatchTransition {initial=0,round_restart=1,map_change=2,next_round=3,map_and_round_change=4};
struct Rotation {uint8_t map=0,rule=0,flags=0;bool operator==(const Rotation&)const=default;};
struct LoadRequest {
 uint64_t sequence=0;uint8_t generation=0,index=0,round=0;Rotation rotation;
 MatchTransition transition=MatchTransition::initial;
 bool operator==(const LoadRequest&)const=default;
};
struct MatchState {
 std::optional<uint8_t> phase,index,round,generation;
 std::array<Rotation,16> rotations{};bool rotations_known=false;
 bool selection_dirty=false,round_dirty=false,pending=false;
 MatchTransition pending_transition=MatchTransition::initial;
 uint64_t revision=0,sequence=0;
 std::optional<LoadRequest> request;
 bool operator==(const MatchState&)const=default;
};
// Returns the generation field only when present in this validated update.
// A request is preparation work, never a completed map load or gameplay start.
std::optional<uint8_t> update_match(MatchState&,std::span<const uint8_t>);
std::optional<uint8_t> global_generation(std::span<const uint8_t>);
}
