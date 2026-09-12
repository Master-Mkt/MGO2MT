#pragma once
#include "host_protocol.h"
#include <map>
#include <optional>
namespace mgo2win::host {
// Retail ELF SHA-256 / PPC evidence: notes/SCENE_REPLICATION_20260912.md.
// 7285C0 registers 592, 12 slots. 261AB0 adds generation parity at bit 11.
constexpr uint16_t item_channel=592;
struct ItemPlacement {
 uint16_t id=0,packedOwner=0,quantity=0;
 uint8_t operation=0,state=0,type=0,variant=0,parameter=0;
 std::array<int16_t,3> coordinates{};
 std::array<int8_t,3> angles{};
 std::array<uint8_t,8> auxiliary{};
 bool transform=false;
 std::array<float,3> position()const;
 std::array<float,3> degrees()const;
 bool operator==(const ItemPlacement&)const=default;
};
struct Placements {
 std::optional<uint8_t> generation;
 uint64_t revision=0;
 std::map<uint16_t,ItemPlacement> items;
 // Complete individual records do not establish whole-stage readiness.
 bool partial=false;
 bool operator==(const Placements&)const=default;
};
class PlacementReceiver {
 Placements state_;ItemPlacement assembling_;unsigned next_=0;
public:
 void begin(uint8_t generation);
 void clear();
 void receive(uint8_t generation,std::span<const uint8_t> record);
 const Placements& result()const{return state_;}
};
// OLObjMan 739300/739578: authored registration order and bit widths MUST match
// GCX actor creation. A guessed registry cannot decode the E0 bitstream.
class ObjectStates {
public:
 enum class Update {bits,maximum};
private:
 std::vector<uint8_t> widths_,values_,initial_;std::vector<Update> updates_;uint8_t slot_=255;bool complete_=false;
public:
 ObjectStates(uint8_t localSlot,std::vector<uint8_t> widths,std::vector<Update> updates={});
 std::optional<std::array<uint8_t,2>> snapshot_request()const{return complete_?std::nullopt:std::optional{std::array<uint8_t,2>{0xe1,slot_}};}
 bool receive(std::span<const uint8_t> record);
 bool complete()const{return complete_;}
 const std::vector<uint8_t>& widths()const{return widths_;}
 const std::vector<uint8_t>& values()const{return values_;}
 // 737840 assigns both +4/current and +8/initial. Later OR/max callbacks
 // only change current: late-join restoration must not replay old break FX.
 const std::vector<uint8_t>& initial_values()const{return initial_;}
};
}
