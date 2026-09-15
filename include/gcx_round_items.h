#pragma once
#include "round_items.h"
#include "stage_cbox.h"
#include <istream>

namespace mgo2win::items {
struct GcxPickupAnchor {
 uint32_t sourceOffset=0,key=0;Position position;
};
struct GcxPickupGroup {
 uint8_t rule=0;uint32_t group=0,worldType=0;
 Domain domain=Domain::equipment;uint32_t item=0;
 std::vector<GcxPickupAnchor> anchors;
};
struct GcxItemLayout {
 uint8_t map=0;bool verified=false;
 std::vector<GcxPickupGroup> groups;
 static GcxItemLayout read(std::istream&);
};
// A missing sidecar means research is unavailable, not an original empty map.
std::optional<GcxItemLayout> load_gcx_item_layout(const std::filesystem::path&stageRoot,uint8_t map);
// Used by the two-choice HOST editor: expand a source item to that stage's
// own candidate offsets, preserving each rule's authored selection/position.
void replace_pickup_item(RoundItems&,const GcxItemLayout&,uint32_t originalItem,Domain,uint32_t replacementItem);
struct GcxPlacement {
 GcxSource source=GcxSource::pickup;uint32_t sourceOffset=0;
 Domain domain=Domain::equipment;uint32_t item=0;Position position;
 std::optional<uint32_t> cboxOrdinal;
};
struct GcxRoundPlan {
 bool verified=false;
 std::vector<GcxPlacement> items;
 std::vector<uint32_t> removeCboxBindings;
};
// HOST chooses from authored candidates once; it sends resulting concrete
// items. Pickup RNG is a native deterministic policy, not CBOX's original RNG.
GcxRoundPlan plan_gcx_round_items(const RoundItems&,const GcxItemLayout*,
 const stage::CboxLayout&,uint8_t map,uint8_t rule,uint8_t generation);
// Strict source X/Z/yaw. A missing floor/obstruction rejects the whole batch;
// never scatter a replacement onto an unrelated nearby spawn location.
std::vector<Seed> resolve_gcx_round_items(const GcxRoundPlan&,const stage::Collision&,const ItemTemplate&);
}
