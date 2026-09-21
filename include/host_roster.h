#pragma once
#include "host_protocol.h"
#include <optional>
#include <string>
namespace mgo2mt::host {
struct Player {
 uint8_t slot=0;uint16_t instance=0;uint32_t character=0;
 std::string name,clan;
 uint32_t clanId=0;uint8_t emblem=0;
 // Optional Windows GWAV appearance; absent on older hosts.
 std::optional<std::array<uint8_t,28>> appearance;
 // Personal-info 0x4122 byte 239, relayed as display metadata, never authority.
 std::optional<uint8_t> level;
 bool operator==(const Player&)const=default;
};
struct Roster {
 std::array<std::optional<Player>,24> slots{};
 bool complete=false;uint64_t revision=0;
 size_t count()const;
 bool operator==(const Roster&)const=default;
};
// Reliable application opcode 7, class 0 only. Other classes are not players.
// Parses an entire record before committing it; no peer connections are opened.
bool update_roster(Roster&,std::span<const uint8_t>);
}
