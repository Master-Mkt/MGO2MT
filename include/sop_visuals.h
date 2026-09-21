#pragma once
#include "character_catalog.h"
#include "character_renderer.h"
#include <memory>
namespace mgo2mt::sop {
using Vec3=std::array<float,3>;
// HOST scope/full identity/life are rechecked by the caller. Defaults fail closed.
struct Gate {bool linked=false,friendly=false,alive=false,jammed=false;};
constexpr bool eligible(Gate gate){return gate.linked&&gate.friendly&&gate.alive&&!gate.jammed;}
// Native prototype parameters; no recovered SOP shader/color/timing claim.
struct Policy {Vec3 orange{1.f,.48f,.08f};float dollOpacity=.42f;};
struct Scan {Vec3 origin{};float radius=0,width=300,opacity=.4f;};
struct DollMesh {std::vector<Vec3> vertices;std::vector<uint32_t> indices;};
// Known original skeleton hashes, native 12 capsule + head sphere shape.
// Missing/nonfinite bone => no partial/guessed doll. No clothing or weapons.
std::optional<DollMesh> doll_mesh(const PreparedCharacter&);
class Renderer {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 explicit Renderer(ID3D11Device*,Policy={});~Renderer();
 Renderer(const Renderer&)=delete;Renderer& operator=(const Renderer&)=delete;
 // Composite after all ordinary world/actor draws. Both passes retain world
 // depth; doll ignores walls using its own self-depth mask, scan tests depth.
 // Like CharacterRenderer::render, caller restores complete 2D pipeline state.
 bool doll(ID3D11DeviceContext*,const PreparedCharacter&,CharacterRenderer& surface,
           const WorldView&,Vec3 origin,float yaw,Gate);
 bool scan(ID3D11DeviceContext*,CharacterRenderer& stage,const WorldView&,Scan,Gate);
};
}
