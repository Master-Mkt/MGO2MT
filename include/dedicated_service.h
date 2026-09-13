#pragma once
#include "character_client.h"
#include "host_room.h"
#include "host_rules.h"
namespace mgo2win {
enum class DedicatedStatus {connecting,mapping,creating,hosting,closed,cancelled,rejected,network_error,protocol_error,outcome_unknown};
struct DedicatedReply {DedicatedStatus status=DedicatedStatus::connecting;uint32_t room=0,error=0;uint16_t local_port=0,public_port=0;std::vector<host::Player> players;bool room_may_exist=false;unsigned synchronized_players=0;std::optional<uint64_t> briefing_remaining_ms;host::RoundPhase phase=host::RoundPhase::waiting;combat::wire::Status combat_status=combat::wire::Status::awaiting_world;};
using DedicatedPublish=std::function<void(DedicatedReply)>;
// Uses the same authenticated lobby transport, reserved UDP socket, and
// reviewed room control payloads as the native client. Does not play a PC.
void run_dedicated_lobby(const std::filesystem::path&,const AuthReply&,const CharacterEntry&,const GameLobbyEntry&,const host::Settings&,const std::atomic_bool&,const DedicatedPublish&,uintptr_t udpSocket);
}
