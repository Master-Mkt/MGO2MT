#pragma once
#include "stage_lighting.h"
#include <optional>
#include <span>
#include <memory>

namespace mgo2mt::stage {
struct Collision;
// Immutable Player/Don't Fall view. None, actor-only triggers, Type Through,
// Recoil and unprotected Cliff bands do not become player walls. Keep the
// original input for purpose-specific bullets, sight, IK and camera queries.
std::shared_ptr<const Collision> movement_collision(std::shared_ptr<const Collision>);
// Current ELF SHA 1a55a41ee5afdebd89075bd205e412c8311f569ba9dc022f629bb63fb4bfd13a.
// Root-0 world FIELD geometry only (180DE8, 18ABE0). Not a solid plane.
struct WaterField {Vec3 center{},halfSize{};};
enum class WaterFoot : unsigned {dry=0,aboveSurface=1,inWater=2};
struct WaterContact {float level=0,depthAboveFloor=0;WaterFoot foot=WaterFoot::dry;};
// 395C0/397D0/39A40 explicitly reject every kind except BOX3/FIELD4.
// This predicate describes the original dispatch, not native BOX support.
constexpr bool original_water_level_kind(unsigned kind){return kind==3||kind==4;}
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
// Separate finite authored surfaces: never yield a water level, volume or speed.
// Native segment/triangle adapter, not the original zero-radius point query.
struct WaterTriangle {std::array<Vec3,3> vertices{};};
struct WaterSurfaceHit {float fraction=0;Vec3 position{},normal{};size_t triangle=0;};
class WaterSurface {
 std::vector<WaterTriangle> triangles_;
public:
 static WaterSurface read(std::istream&); // local GWS1 v1, at most4096 triangles
 static WaterSurface make(std::vector<WaterTriangle>);
 std::span<const WaterTriangle> triangles()const{return triangles_;}
 // First two-sided crossing in (from,to], inclusive triangle edges. Winding normal.
 // A zero-length or coplanar segment has no crossing; no invented contact radius.
 std::optional<WaterSurfaceHit> crossing(Vec3 from,Vec3 to)const;
};
}
