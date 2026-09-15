#pragma once
#include "combat_authority.h"
#include <set>
namespace mgo2win::combat::tracers {
struct Segment {Vec3 from{},to{};float opacity=1;};
// Native cosmetic timing/length, independent from the instantaneous HOST hit.
class Pool {
 struct Entry {Event shot;uint64_t born;};
 std::vector<Entry> entries_;std::set<uint64_t> seen_;
 uint64_t epoch_=0,scene_=0,floor_=0,now_=0;Identity self_{};uint32_t life_=0;
 void expire(const Snapshot&,uint64_t);
public:
 static constexpr size_t capacity=256;static constexpr uint64_t lifetimeMs=100;
 void clear();
 void synchronize(uint64_t epoch,uint64_t scene,Identity self,uint32_t life,uint64_t watermark,uint64_t now);
 void dispatch(std::span<const Event>,const Snapshot&,uint64_t now);
 std::vector<Segment> sample(const Snapshot&,uint64_t now);
 size_t size()const{return entries_.size();}
};
}
