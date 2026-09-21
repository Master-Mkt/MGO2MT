#pragma once
#include "combat_authority.h"
#include <array>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace mgo2mt::radio {
using Identity = combat::Identity;
using Body = std::array<uint8_t,44>;
inline constexpr uint8_t marker = 0xed;
inline constexpr uint64_t probe_timeout_ms = 3000;
inline constexpr size_t history_limit = 64;
enum class Kind : uint8_t { probe=1, offer=2, request=3, notification=4 };
struct Record {
 Kind kind=Kind::probe; uint64_t epoch=0,token=0; Identity identity;
 uint32_t life=0,sequence=0; uint8_t preset=0,third=0;
 bool operator==(const Record&) const=default;
};
bool recognized(std::span<const uint8_t>) noexcept;
std::optional<Body> encode(const Record&) noexcept;
std::optional<Record> decode(std::span<const uint8_t>) noexcept;
struct Member { Identity identity; uint32_t life=0; uint8_t team=0; bool eligible=false;bool freeForAll=false; };
struct Context { uint64_t epoch=0; std::vector<Member> members; };
// Eligibility and filtering are authoritative caller policy, not original-wire
// claims. Current native scope: active/alive map20, rule1, flags0, same team.
using Filter = std::function<bool(const Member&,const Member&,uint8_t)>;
struct Policy { uint64_t minimumIntervalMs; };
struct Delivery { Identity recipient; Body body; };
struct Event { uint64_t epoch=0; Identity sender; uint32_t life=0,sequence=0; uint8_t preset=0,third=2; };
// Existing admitted endpoint/key binding MUST be checked by the caller. GWRA
// does not add signatures or authenticate an arbitrary decoded identity. Queue
// returned bodies through existing reliable channel1, never as original 1/2.
class Service {
 struct Peer { Identity identity; uint64_t token=0,lastAt=0; uint32_t sequence=0; bool sent=false; };
 Policy policy_; uint64_t epoch_=0; std::array<std::optional<Peer>,24> peers_{};
public:
 explicit Service(Policy policy):policy_(policy){}
 void reset(uint64_t epoch) noexcept;
 void remove(Identity) noexcept;
 std::vector<Delivery> receive(Identity admittedPeer,std::span<const uint8_t>,const Context&,uint64_t now,const Filter&);
};
enum class Status { unavailable, probing, ready };
enum class SubmitResult { submitted, unavailable, ineligible, invalid_preset, exhausted };
struct Submission { SubmitResult result=SubmitResult::unavailable; std::optional<Body> body; uint32_t sequence=0; };
class Client {
 struct Seen { Identity identity; uint32_t sequence=0; };
 uint64_t epoch_=0,token_=0,probeAt_=0; Identity self_; Status status_=Status::unavailable;
 bool attempted_=false; uint32_t sequence_=0;
 std::array<std::optional<Seen>,24> seen_{}; std::vector<Event> events_;
public:
 // Caller must bind/reset on connection change even if epoch/identity match.
 // A bind always starts a new connection/epoch scope and clears all state.
 void bind(uint64_t epoch,Identity self) noexcept;
 std::optional<Body> probe(uint64_t token,uint64_t now) noexcept;
 void tick(uint64_t now) noexcept;
 Status status() const noexcept { return status_; }
 bool receive(std::span<const uint8_t>,const Context&,uint64_t now,const Filter&);
 Submission submit(uint8_t preset,const Context&,uint64_t now) noexcept;
 std::vector<Event> drain();
};
}
