#pragma once
#include "combat_authority.h"
#include <istream>

namespace mgo2win::combat::spawn {
enum class Kind : uint8_t { initial, respawn };
enum class Variant : uint8_t { normal, mini };
struct Entry {
 Variant variant{};Kind kind{};uint8_t team=0,index=0;uint32_t hash=0;
 Vec3 position{};int16_t yaw=0;
 bool operator==(const Entry&)const=default;
};
class Profile {
 std::array<Entry,128> entries_{};
public:
 static Profile read(std::istream&);
 const Entry& at(Variant,Kind,uint8_t normalizedTeam,uint8_t index)const;
 const auto& entries()const{return entries_;}
};
// Original 759BD0 uses FOUR 64-bit words, not xorshift32. stage_setup preserves
// the three tail words, just as 759558 does; cold_start uses their ELF defaults.
// cacheByte161 is deliberately not named "round" until that field is verified.
struct Random {
 std::array<uint64_t,4> words{};
 static Random cold_start(uint8_t localMemberId,uint8_t cacheByte161,std::array<uint8_t,3> initialCounts={16,16,32});
 void stage_setup(uint8_t localMemberId,uint8_t cacheByte161,std::array<uint8_t,3> initialCounts={16,16,32});
 uint64_t next();
 bool valid()const;
 bool operator==(const Random&)const=default;
};
struct Context {
 uint64_t epoch=0;uint8_t participantCapacity=0;bool teamSwap=false;
 uint8_t map=20,rule=1;
 bool operator==(const Context&)const=default;
};
struct Proposal {
 uint64_t epoch=0,revision=0;Kind kind{};Variant variant{};
 uint8_t rawTeam=0,normalizedTeam=0,arrayIndex=0;uint32_t hash=0;
 Vec3 sourcePosition{};Pose creationPose{};Random randomAfter{};
 bool operator==(const Proposal&)const=default;
};
class Selector {
 Profile profile_;Context context_;Random random_;
 std::array<uint32_t,2> initial_{};uint64_t revision_=1;
public:
 // Random is the state AFTER stage_setup, not a raw seed. Copy random() into
 // the next round, call stage_setup with host-owned metadata, then construct.
 Selector(Profile,Context,Random);
 // Const proposal: failed admission consumes neither counter nor randomness.
 // rawTeam is NT 0/1, NOT the native combat team 1/2.
 std::optional<Proposal> propose(Kind,uint8_t rawTeam)const;
 bool commit(const Proposal&);
 const auto& counters()const{return initial_;}
 const Random& random()const{return random_;}
 const Context& context()const{return context_;}
};
std::optional<uint8_t> raw_team(uint8_t nativeTeam);

enum class PlacementReject { none, invalid, obstructed, no_floor, steep, occupied };
struct Placement {
 PlacementReject reject=PlacementReject::invalid;Pose pose{};float drop=0,ceilingCorrection=0;
 explicit operator bool()const{return reject==PlacementReject::none;}
};
// Native conservative landing, explicitly distinct from the original creation
// transform. Initial creation may resolve an downward-facing ceiling overlap downward
// by no more than the original 800-unit lift, but walls/floors/unknown contacts
// remain blocked. Only vertical descent is permitted: no invented formation offsets,
// lateral searches or alternate RNG draws. Caller must also Authority::join and
// only then commit. maximumDrop is a native admission bound, not a PS3 constant.
Placement place(const Proposal&,const stage::Collision&,std::span<const Pose> occupied,
                float maximumDrop=10000);
}
