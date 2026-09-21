#pragma once
#include "character_renderer.h"
#include "combat_particle_effects.h"
#include <filesystem>
#include <memory>
namespace mgo2mt::combat::particles {
class Renderer {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 Renderer(ID3D11Device*,const std::filesystem::path& bundle);~Renderer();
 // Transactional append. Identical shared keys are allowed; conflicting
 // images, malformed bundles and more than 128 total images are rejected.
 void add_bundle(const std::filesystem::path& bundle);
 // Bounded transactional image import; originals are never modified. Paths
 // are resolved under the data root, including a canonical symlink check.
 void add_textures(const weapon_effect::Config&,const std::filesystem::path& dataRoot);
 bool render(ID3D11DeviceContext*,CharacterRenderer&,const WorldView&,std::span<const Sprite>);
 size_t textures() const noexcept;
};
}
