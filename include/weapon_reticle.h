#pragma once
#include "combat_authority.h"
#include <cstdint>
#include <optional>
#include <span>
namespace mgo2win::reticle {
// Native presentation geometry; no recovered reticle texture or original
// interpolation timing is claimed. Angles are HOST-provided cone half-angles.
struct Scope {
 uint64_t epoch=0,scene=0;combat::Identity identity;uint32_t life=0;uint16_t weapon=0;
 bool operator==(const Scope&)const=default;
};
struct Viewport {
 int left=0,top=0,width=1280,height=720;float aspect=1280.f/720.f;
 bool operator==(const Viewport&)const=default;
};
struct Model {
 Scope scope;std::optional<float> spreadRadians;uint64_t shotWatermark=0;
 float centerX=640,centerY=360;Viewport viewport;
 bool gameplay=false,active=false,alive=false,menuOpen=false,eligible=true;
};
struct Geometry {
 float centerX=0,centerY=0,radiusX=0,radiusY=0;Viewport viewport;
 bool operator==(const Geometry&)const=default;
};
// Uses the renderer's vertical FOV of 1 radian and its physical camera aspect.
// Pixel radii describe the supplied cone exactly. The drawn marks have a
// native minimum gap of six pixels for legibility, never a clamped center.
std::optional<Geometry> geometry(float angle,float centerX,float centerY,Viewport);
class Presentation {
 std::optional<Scope> scope_;uint64_t watermark_=0;
public:
 // The supplied angle is displayed exactly, with no temporal interpolation
 // or invented shot pulse. Watermark only rejects stale same-scope input.
 // Ammo is intentionally irrelevant: empty equipped guns keep their reticle.
 // Ineligible/invalid input clears old state. Same-scope stale watermarks hide
 // that frame without replacing the newer state. dt=0 remains valid.
 std::optional<Geometry> update(const Model&,double dt);
 void reset(){scope_.reset();watermark_=0;}
};
inline constexpr uint32_t orange=0xffffa52b;
// Opaque orange over a straight-alpha 1280x720 HUD, after menu color conversion.
// Does not clear the target: caller starts each HUD frame with a clean surface.
void paint(std::span<uint32_t> pixels,int width,int height,const Geometry&);
}
