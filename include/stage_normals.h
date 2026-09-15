#pragma once
#include "character_model.h"
#include <filesystem>
namespace mgo2win::stage {
std::array<float,3> rsx_cmp_normal(uint32_t packed);
// GWN1 contains original packed words, bound to the exact unmodified GWM SHA.
// Missing sidecars preserve legacy packages. Malformed/mismatched files fail.
size_t apply_original_normals(CharacterModel&,std::span<const char> gwm,std::span<const char> sidecar);
size_t load_original_normals(CharacterModel&,std::span<const char> gwm,const std::filesystem::path& sidecar);
}
