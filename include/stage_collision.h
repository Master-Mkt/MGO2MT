#pragma once
#include "stage_lighting.h"
#include "stage_collision_attributes.h"
#include <cstdint>
#include <optional>
#include <memory>
#include <span>
namespace mgo2mt::stage {
struct CollisionMaterial {uint32_t id=0;float friction=.5f,restitution=.5f;bool verified=false;int32_t resistance=1000;bool resistanceVerified=false;};
// Programmatically authored triangles default to the explicit native solid
// role. read() always overwrites this field with the original 64-bit value,
// including None=0; decoded untagged surfaces are never promoted to solids.
struct CollisionTriangle {std::array<unsigned,3> vertices{};uint64_t attribute=attribute::native_solid;unsigned polygonAttribute=0;unsigned material=~0u;uint32_t object=0;};
struct CollisionHit {float distance=0;Vec3 position{};size_t triangle=0;Vec3 normal{};};
// Winding normal is not flipped toward the ray; every triangle is retained.
struct CollisionRayHit {float distance=0;Vec3 position{};size_t triangle=0;Vec3 normal{};bool frontFace=false;uint64_t attribute=0;unsigned polygonAttribute=0;uint32_t object=0;CollisionMaterial material{};};
struct Capsule {float radius=350,height=1700,skin=2;};
struct CapsuleHit {float fraction=0;Vec3 normal{};size_t triangle=0;};
struct CollisionContact {Vec3 point{},normal{};float penetration=0;size_t triangle=0;};
struct Collision;
struct CollisionInstance {uint32_t id=0;std::shared_ptr<const Collision> geometry;Vec3 position{},degrees{};};
// Double-sided geometric queries with explicit surface-purpose masks.
struct Collision {
 std::vector<Vec3> vertices;std::vector<CollisionTriangle> triangles;std::vector<CollisionMaterial> materials;
 static Collision read(std::istream&);
 static Collision make(std::vector<Vec3>,std::vector<CollisionTriangle>,std::vector<CollisionMaterial> = {});
 // Immutable assembled world: callers publish it with the matching model/light
 // revision. Geometry must be recovered collision, never a guessed model AABB.
 static Collision combine(const Collision&,std::span<const CollisionInstance>);
 CollisionMaterial material(size_t triangle)const;
 std::optional<CollisionHit> ray(Vec3 origin,Vec3 direction,float maximum,CollisionQuery = {})const;
 // Sorted distance, then triangle index. No truncation or shared-edge merging.
 std::vector<CollisionRayHit> ray_all(Vec3 origin,Vec3 direction,float maximum,CollisionQuery = {})const;
 std::optional<CapsuleHit> sweep(Vec3 feet,Vec3 displacement,Capsule,CollisionQuery = {})const;
 std::optional<CapsuleHit> sweep_segment(Vec3 a,Vec3 b,Vec3 displacement,float radius,float skin=1,CollisionQuery = {})const;
 std::vector<CollisionContact> contacts(Vec3 a,Vec3 b,float radius,float margin=1,size_t limit=32,CollisionQuery = {})const;
 bool clear(Vec3 feet,Capsule,CollisionQuery = {})const;
 // Ground travel only. A missing lower floor stops travel only while leaving
 // support explicitly marked Don't Fall. Airborne travel is not constrained.
 float fall_prevention_fraction(Vec3 feet,Vec3 displacement,Capsule)const;
 // Native finite-box broad phase. The caller validates finite ordered bounds
 // and performs exact triangle contact; these are candidates, not solid hits.
 std::vector<unsigned> bounds_candidates(Vec3 lo,Vec3 hi)const{return candidates(lo,hi);}
private:
 struct Node {Vec3 lo{},hi{};unsigned first=0,count=0,left=0,right=0;};
 std::vector<Node> nodes_;std::vector<unsigned> order_;
 void build();unsigned build_node(unsigned,unsigned);
 std::vector<unsigned> candidates(Vec3,Vec3)const;
 std::vector<unsigned> ray_candidates(Vec3,Vec3,float)const;
};
}
