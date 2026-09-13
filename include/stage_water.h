#pragma once
#include "stage_lighting.h"
#include <optional>
#include <span>
#include <memory>

namespace mgo2win::stage {
struct Collision;
// Narrow native movement adapter: normal control query requires authored bit0x10.
// Unknown attribute0 remains conservative native solid; water/decor is excluded.
// Keep the input collision for bullets/rendering. This is not the original full mask.
std::shared_ptr<const Collision> movement_collision(std::shared_ptr<const Collision>);
// Current ELF SHA 1a55a41ee5afdebd89075bd205e412c8311f569ba9dc022f629bb63fb4bfd13a.
// Root-0 world FIELD geometry only (180DE8, 18ABE0). Not a solid plane.
struct WaterField {Vec3 center{},halfSize{};};
enum class WaterFoot : unsigned {dry=0,aboveSurface=1,inWater=2};
struct WaterContact {float level=0,depthAboveFloor=0;WaterFoot foot=WaterFoot::dry;};
class Water {
 std::vector<WaterField> fields_;
public:
 static Water read(std::istream&); // local GWW1 version1; throws on invalid data
 static Water make(std::vector<WaterField>); // validates and removes exact duplicates
 std::span<const WaterField> fields()const{return fields_;}
 // Inclusive point containment, zero radius; 1B0F88 initializes query radius=0.
 // Ambiguous overlapping levels fail closed: original region/grid order is absent.
 std::optional<float> level(Vec3 point)const;
 // 395C0 uses floorY for the query, preserving control X/Z.
 std::optional<float> control_level(Vec3 controlPosition,float floorY)const;
 // 397D0 queries the actual control position, then compares floor/position to level.
 std::optional<WaterContact> on_foot(Vec3 controlPosition,float floorY)const;
 static WaterFoot classify(float level,float floorY,float controlY);
};
}
