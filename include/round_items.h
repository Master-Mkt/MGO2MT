#pragma once
#include "world_inventory.h"
#include "stage_collision.h"
#include <functional>
#include <string>
namespace mgo2mt::items {
struct SpawnRule {
 uint8_t map=20;Domain domain=Domain::weapon;uint32_t item=25,count=1;
 // No coordinates means validated floor near actual initial spawn anchors.
 std::optional<Position> position;
 bool operator==(const SpawnRule&)const=default;
};
enum class GcxSource {pickup,cbox};
struct GcxReplacement {
 uint8_t map=20;GcxSource source=GcxSource::cbox;
 // GEOM source offset; zero replaces all selected sources of this kind.
 // This is not a network actor index and not a shared property hash.
 uint32_t sourceOffset=0;Domain domain=Domain::weapon;uint32_t item=25;
 bool operator==(const GcxReplacement&)const=default;
};
struct RoundItems {
 bool enabled=false;std::vector<SpawnRule> rules;
 // Separate from optional manual additions. V1 files preserve their old
 // behavior; newly created V2 settings use the reviewed GCX defaults.
 bool useGcx=true;std::vector<GcxReplacement> replacements;
 bool valid()const;
 bool parse(std::string_view,std::string& error);
 bool load(const std::filesystem::path&,std::string& error);
 bool save(const std::filesystem::path&,std::string& error)const;
 std::string serialize()const;
};
using ItemTemplate=std::function<std::optional<Contents>(Domain,uint32_t)>;
std::vector<Seed> resolve_round_items(const RoundItems&,uint8_t map,const stage::Collision&,std::span<const stage::Vec3> anchors,const ItemTemplate&);
// Native presentation labels; fallback is explicit catalog domain/id.
std::string item_label(Domain,uint32_t);
}
