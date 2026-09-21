#pragma once
#include "stage_collision.h"
#include "rigid_physics.h"
#include "character_catalog.h"
#include "camera_projection.h"
#include "host_hit_geometry.h"
namespace mgo2mt::physics_debug {
using Vec3=stage::Vec3;
// Colors distinguish actual solver geometry from visual-only diagnostics.
enum class Kind {terrain,body,rigid,bone,visual,attachment,hit,trigger,fallBarrier};
struct Line {Vec3 a{},b{};Kind kind=Kind::terrain;};
struct Budget {size_t lines=8192,terrainTriangles=384,pixels=180000;float terrainRange=8000;};
struct Stats {size_t lines=0,triangles=0,omitted=0,pixels=0;};
class Frame {
 Budget budget_;std::vector<Line> lines_;Stats stats_;
public:
 explicit Frame(Budget={});
 void line(Vec3,Vec3,Kind);
 void sphere(Vec3,float,Kind=Kind::rigid);
 void capsule(Vec3 a,Vec3 b,float radius,Kind=Kind::body);
 void standing(Vec3 feet,stage::Capsule);
 // Exact fixed-pose male BOX proxies used by HOST firearm damage queries.
 void hit_regions(Vec3 feet,float yaw,host_hit::Stance,Vec3 coverOffset={});
 void box(Vec3 center,Vec3 halfExtent,Kind=Kind::rigid);
 void rigid(const physics::RigidBody&);
 void skeleton(const PreparedCharacter&,std::span<const CatalogBone>,Vec3 origin,float yaw);
 void terrain(const stage::Collision&,Vec3 eye);
 std::span<const Line> lines()const{return lines_;}
 const Stats& stats()const{return stats_;}
 const Budget& budget()const{return budget_;}
};
struct View {Vec3 eye{},direction{0,0,1};int left=0,top=0,width=1280,height=720;float aspect=1280.f/720.f,verticalFov=default_vertical_fov;};
// X-ray debug overlay: no world occlusion rays, no depth mutation. ARGB32,
// straight-alpha composition matching the existing UI surfaces. Clipped and
// pixel-budgeted; physics state is never changed by this function.
Stats paint(std::span<uint32_t> pixels,unsigned width,unsigned height,const Frame&,const View&);
}
