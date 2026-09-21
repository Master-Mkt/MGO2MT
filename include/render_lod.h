#pragma once
#include "character_model.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace mgo2mt::render_lod {
// Immutable-source, static-stage-only index LOD. Never apply to skinned meshes.
struct BuildOptions {float midCell=80,farCell=240,maxUvDelta=.125f;size_t maxVertices=500000,maxIndices=1500000,maxParts=65536;};
struct Variant {std::vector<uint32_t> indices;float maxError=0;};
struct Part {Variant mid,coarse;std::array<float,3> center{};float radius=0;uint32_t originalCount=0;bool protectedPart=false;};
struct Mesh {std::vector<Part> parts;size_t originalTriangles=0,midTriangles=0,farTriangles=0;bool budgetExceeded=false;};
// Empty variant means no worthwhile safe simplification; draw original indices.
// Part boundaries, disconnected components and alpha surfaces are protected.
Mesh build(const CharacterModel&,BuildOptions={});
// viewDepth is dot(center-eye,cameraForward), in model units. Uses nearest bounding-sphere depth,
// explicit 1.5px default displacement bound and 20% entry hysteresis. Invalid or
// near-plane/inside-bound inputs always choose original (0).
unsigned select(const Part&,float viewDepth,float verticalFov,unsigned viewportHeight,unsigned previousLevel=0,float pixelBudget=1.5f,float aspect=16.f/9.f);
float projected_error(const Part&,const Variant&,float viewDepth,float verticalFov,unsigned viewportHeight,float aspect=16.f/9.f);
}

