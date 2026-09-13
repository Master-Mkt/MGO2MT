#pragma once
#include "combat_spawn.h"
#include <string>
namespace mgo2win::combat::spawn {
// GCX source arrays may have different lengths across maps/rules. Placement
// entity capacity is unrelated and is not encoded in these spawn arrays.
class StageProfile {
 std::array<std::vector<Entry>,12> groups_;
 uint8_t map_=0,rule_=0;std::string stage_;
public:
 static StageProfile read(std::istream&);
 uint8_t map()const{return map_;}uint8_t rule()const{return rule_;}
 std::string_view stage()const{return stage_;}
 const std::vector<Entry>& group(Variant,Kind,uint8_t team)const;
};
class StageSelector {
 StageProfile profile_;Context context_;Random random_;
 std::array<uint64_t,3> counters_{};
 std::array<std::vector<uint8_t>,3> permutations_;
 uint64_t revision_=1;
public:
 // Native CSPRNG seed; the original collision-probing permutation algorithm
 // and later xorshift are preserved. No claim about unknown original cache161.
 StageSelector(StageProfile,Context,Random);
 std::optional<Proposal> propose(Kind,uint8_t rawTeam)const;
 bool commit(const Proposal&);
 const auto& context()const{return context_;}
 const auto& profile()const{return profile_;}
 const auto& counters()const{return counters_;}
};
}
