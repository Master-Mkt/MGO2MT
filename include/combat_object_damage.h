#pragma once
#include "combat_world.h"
#include "combat_authority.h"
#include <set>
namespace mgo2win::combat {
// Reviewed n022a drums plus explicitly bound n007a breakable lights.
// Durability is a native accepted-impact count, not an original HP threshold.
class ObjectDamage {
 enum class Kind {drum,light};
 struct Target {uint32_t binding=0;stage::Vec3 center{};Kind kind=Kind::drum;};
 std::map<uint32_t,Target> components_;
 std::map<uint32_t,uint16_t> hits_;
 std::set<uint64_t> seen_;
 uint64_t epoch_=0,highest_=0;
 std::optional<host::LoadRequest> request_;
 uint8_t map_=0;
 uint16_t lightHits_=1;
 void remember(uint64_t id);
public:
 struct Policy {uint16_t lightHits=1;bool valid()const noexcept{return lightHits>=1&&lightHits<=10000;}};
 struct Result {Decision combat;std::vector<std::vector<uint8_t>> records;size_t destroyed=0;};
 static ObjectDamage load(const std::filesystem::path& stageRoot,uint8_t map=20);
 static ObjectDamage load(const std::filesystem::path& stageRoot,uint8_t map,Policy);
 // Accept only HOST-generated events, never a client-reported impact. Scene
 // transactions publish geometry and state together; light hits never explode.
 Result apply(std::span<const Event>,stage::SceneAuthority&,World&,Authority&,uint64_t now);
 size_t component_count()const noexcept{return components_.size();}
};
}
