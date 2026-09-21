#pragma once
#include "combat_authority.h"
#include <filesystem>
#include <memory>

struct ID3D11Device;
struct ID3D11DeviceContext;
namespace mgo2mt::shadows { struct Caster; }
namespace mgo2mt::mortar_shells {
using Vec3=stage::Vec3;
inline constexpr size_t capacity=32;
struct Flight {
 uint64_t shot=0,born=0,at=0;
 combat::Identity source;
 uint32_t life=0,ttlMs=0;
 Vec3 position{},velocity{},traceFrom{},traceTo{};
 float gravity=0,traveled=0,blastRadius=0;
};
// Cosmetic point-swept flight only. No damage, ammo, impact or network events
// are emitted. Range must be the HOST's configured weapon 103 range.
class Simulation {
 std::array<std::optional<Flight>,capacity> flights_{};
 float range_=0;
 uint64_t epoch_=0,scene_=0,now_=0,eventFloor_=0,generation_=0;
 uint8_t map_=0;
 bool initialized_=false;
 void advance(const stage::Collision&,uint64_t);
public:
 explicit Simulation(float range);
 void update(std::span<const combat::Event>,const combat::Snapshot*,const mounted::Registry&,
             uint8_t map,const std::shared_ptr<const stage::Collision>& world,uint64_t now,uint64_t sceneToken);
 void clear();
 size_t size()const;
 const auto& flights()const{return flights_;}
 uint64_t generation()const{return generation_;}
};
// The recovered MDN shell points along +Y. Rotate its original positions and
// normals together, preserving UVs, colors, materials, topology and scale.
void orient_vertices(std::span<const ModelVertex>,Vec3 velocity,std::span<ModelVertex>);
class Renderer {
 struct Impl;
 std::unique_ptr<Impl> impl_;
public:
 Renderer(ID3D11Device*,const std::filesystem::path& modelPath,float range);
 ~Renderer();
 Renderer(const Renderer&)=delete;
 Renderer& operator=(const Renderer&)=delete;
 void update(ID3D11DeviceContext*,std::span<const combat::Event>,const combat::Snapshot*,const mounted::Registry&,
             uint8_t map,const std::shared_ptr<const stage::Collision>& world,uint64_t now,uint64_t sceneToken);
 // Append these to the ordinary opaque world pass and shadow caster list.
 // Each renderer has already oriented its local vertices; caster yaw is zero.
 void shadow_casters(std::vector<shadows::Caster>&)const;
 void clear();
 size_t size()const;
 const Simulation& simulation()const;
};
}
