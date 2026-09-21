#pragma once
#include "character_model.h"
#include <filesystem>
namespace mgo2mt::stage {
// Reviewed per-part alpha adapter. Missing sidecar leaves opaque rendering.
size_t apply_surface_alpha(CharacterModel&,std::span<const char> gwm,std::span<const char> sidecar);
size_t load_surface_alpha(CharacterModel&,std::span<const char> gwm,const std::filesystem::path&);
}
