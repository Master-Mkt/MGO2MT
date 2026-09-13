#pragma once
#include "world_inventory.h"
#include <array>
#include <optional>
#include <span>
#include <variant>
namespace mgo2win::items::wire {
inline constexpr uint8_t marker=0xec,version=1;
inline constexpr uint16_t rows_per_page=16;
inline constexpr size_t maximum_body=1172;
enum class Action:uint8_t {drop=1,install=2,pickup=3,recover=4,use=5};
struct Header {Scope scope;uint64_t token=0;Actor actor;uint64_t sequence=0;bool operator==(const Header&)const=default;};
struct Probe {Header header;};
struct Offer {Header header;uint32_t capabilities=0;Capacity capacity;};
// Bits 0..4 explicitly advertise drop/install/pickup/recover/use support.
inline constexpr uint32_t all_capabilities=31;
struct Command {Header header;Action action=Action::drop;uint8_t heldSlot=255;Consume resource=Consume::magazine;uint32_t amount=0;uint64_t entity=0,heldRevision=0,entityRevision=0;};
struct Reply {Header header;Action action=Action::drop;uint8_t heldSlot=255;ResultCode result=ResultCode::invalid;bool destroyed=false;uint64_t entity=0,heldRevision=0,worldRevision=0,entityRevision=0;};
struct Page {Header header;uint64_t revision=0;uint32_t total=0;uint16_t index=0,pages=1;Capacity capacity;std::vector<Entity> entities;};
struct Held {Header header;uint8_t selectedSlot=255;std::array<HeldSlot,3> slots;};
// Kind 6 remains reserved. Kind 7 publishes primary/secondary/support holdings.
using Record=std::variant<Probe,Offer,Command,Reply,Page,Held>;
bool recognized(std::span<const uint8_t>)noexcept;
std::optional<std::vector<uint8_t>> encode(const Record&);
std::optional<Record> decode(std::span<const uint8_t>);
// Validated, deterministic pages. Header is addressed to this recipient/token;
// it does not authenticate a caller. Route only via admitted endpoint/keys.
std::optional<std::vector<Page>> pages(const SnapshotState&,Header recipient);
class Receiver {
 Header expected_;Capacity maximum_;uint64_t floor_=0;
 std::optional<Page> pending_;std::map<uint16_t,Page> pages_;
 std::optional<SnapshotState> state_;
public:
 // Bind on connection, epoch/generation, recipient identity/life or token change.
 // Configured row budgets are an explicit local memory/traffic policy.
 void bind(Header expected,Capacity maximum);
 bool receive(std::span<const uint8_t>);
 const std::optional<SnapshotState>& state()const noexcept{return state_;}
};
}
