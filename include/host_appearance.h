#pragma once
#include "host_roster.h"
namespace mgo2win::host {
// Windows-only informational extension on the existing reliable channel 1.
// This extension leaves GWCB and original roster bytes unchanged. Older hosts provide no
// appearance; older clients ignore this separate application opcode.
constexpr uint8_t appearance_opcode=0xee;
std::vector<uint8_t> appearance_record(std::span<const Player>);
// Atomic full-identity check against the admitted roster. No account/name,
// routing data, skill ownership or gameplay authority is carried here.
void update_appearance(Roster&,std::span<const uint8_t>);
}
