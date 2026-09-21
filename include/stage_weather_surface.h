#pragma once
#include "stage_collision.h"
#include <istream>
#include <memory>
#include <string_view>
namespace mgo2mt::stage::weather {
enum class Preset {clear,rain,snow,fog_sand};
struct Settings {
 bool enabled=true;Preset preset=Preset::clear;float intensity=1,wetSeconds=30,snowSeconds=90,drySeconds=120,meltSeconds=180;
 // HOST controls can override either channel independently, including both ON.
 // Unset retains the legacy cfg's exclusive preset behavior.
 std::optional<bool> rainEnabled,snowEnabled;
 static Settings read(std::istream&);
 static Settings defaults(std::string_view stage);
};
struct SurfaceCell {float height=0,normalY=0,wetness=0,snow=0;};
struct SurfaceGrid {
 unsigned width=64;float cellSize=1000,originX=0,originZ=0;
 std::vector<SurfaceCell> cells;uint64_t collisionRevision=0;
};
// Native weather accumulation over the actual collision's highest exposed face.
// Work is bounded to 64 rays per frame; unknown cells receive no visual effect.
class SurfaceController {
 std::shared_ptr<const Collision> collision_;SurfaceGrid grid_;std::vector<size_t> order_;
 size_t next_=0;double previous_=-1;float ceiling_=0,floor_=0;uint64_t revision_=0;
 void reset_grid(Vec3 eye);
public:
 std::shared_ptr<const SurfaceGrid> sample(const Settings&,double seconds,Vec3 eye,
  std::shared_ptr<const Collision>,uint64_t revision=0);
 void reset();
};
float surface_coverage(const SurfaceGrid&,Vec3 world,float upwardNormal);
}
