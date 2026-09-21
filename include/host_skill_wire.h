#pragma once
#include "skill_settings.h"
#include <array>
#include <deque>
#include <map>
#include <span>
#include <utility>
namespace mgo2mt::host_skills {
inline constexpr uint16_t request_opcode=0x41e4,reply_opcode=0x41e5;
inline constexpr std::array<uint8_t,32> catalog_digest{
 0x67,0xe9,0x0a,0xec,0xa0,0xa6,0xb9,0xdc,0x8b,0x08,0x67,0x0d,0xd3,0x2d,0xac,0x15,
 0x3f,0xad,0x26,0x73,0x0c,0xf2,0xd7,0x7c,0x91,0x39,0x12,0x77,0x44,0xad,0x9b,0x0b};
// Epoch and UDP incarnation are local nonce bindings. The lobby server knows
// room/PC membership, not the native host's round or UDP slot numbering.
struct Scope {
 uint32_t room=0,host=0,character=0;uint64_t epoch=0;uint8_t slot=255;uint16_t instance=0;
 bool operator==(const Scope&)const=default;
};
struct Request {Scope scope;uint64_t nonce=0;};
struct Reply {
 uint8_t status=0;uint64_t nonce=0;uint32_t room=0,character=0,host=0,membership=0,revision=0;
 uint8_t capacity=0,used=0;std::array<uint8_t,32> catalog{};skills::Loadout loadout;
};
std::vector<uint8_t> encode_request(const Request&);
Reply decode_reply(std::span<const uint8_t>);
class Receiver;
// Only the authenticated lobby receiver creates this value. UDP input/profile
// decoders cannot mark a local skill selection as server-verified.
class Verified {
 Scope scope_;Reply profile_;
 Verified(Scope scope,Reply profile):scope_(scope),profile_(std::move(profile)){}
 friend class Receiver;
public:
 const Scope& scope()const{return scope_;}
 const Reply& profile()const{return profile_;}
 uint8_t level(uint16_t id)const{for(const auto&c:profile_.loadout.entries)if(c.id==id)return c.level;return 0;}
};
enum class Availability {unknown,supported,unsupported};
class Receiver {
 std::shared_ptr<const skills::Catalog> catalog_;uint64_t nonce_,epoch_=0,deadline_=0,lastNow_=0;
 uint32_t room_=0,host_=0;Availability availability_=Availability::unknown;
 std::deque<Scope> queued_;std::map<uint32_t,Scope> wanted_;
 struct Revision {uint32_t revision;skills::Loadout loadout;};
 std::map<uint32_t,Revision> revisions_;
 std::optional<Request> flight_;std::vector<Verified> ready_;
public:
 Receiver(std::shared_ptr<const skills::Catalog>,uint64_t nonceSeed);
 // Unsupported status is session-scoped and persists across round changes.
 void begin(uint32_t room,uint32_t host,uint64_t epoch);
 bool want(uint8_t slot,uint16_t instance,uint32_t character);
 void forget(uint8_t slot,uint16_t instance,uint32_t character);
 std::optional<Request> take(uint64_t now);
 bool receive(std::span<const uint8_t>,uint64_t now);
 void tick(uint64_t now);
 Availability availability()const{return availability_;}
 std::vector<Verified> drain();
};
}
