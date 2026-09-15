#pragma once
#include "combat_authority.h"
#include <set>
namespace mgo2win::combat::particles {
enum class Kind {casing,smoke,flash};
struct Segment {Vec3 from{},to{};std::array<float,4> rgba{};float widthPixels=1;Kind kind=Kind::casing;};
// Native visual policy, not recovered original particle physics/hand transforms.
struct Policy {size_t capacity=256;uint64_t smokeMs=1600,casingMs=900;};
class Pool {
 struct Entry {Kind kind;Identity owner;uint32_t life;Vec3 origin,velocity;uint64_t born,id;};
 Policy policy_;std::vector<Entry> entries_;std::set<uint64_t> seen_;
 uint64_t epoch_=0,scene_=0,floor_=0,now_=0;
 void expire(const Snapshot&,uint64_t);
public:
 explicit Pool(Policy={});
 void clear();
 // New scope seeds a historical-event baseline. On later snapshots, delayed
 // event chunks up to the current snapshot watermark are accepted once.
 void synchronize(uint64_t epoch,uint64_t scene,uint64_t watermark,uint64_t now);
 void dispatch(std::span<const Event>,const Snapshot&,uint64_t now);
 std::vector<Segment> sample(const Snapshot&,uint64_t now);
 size_t size()const noexcept{return entries_.size();}
};
}
