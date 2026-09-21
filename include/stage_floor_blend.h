#pragma once
#include "character_model.h"
#include <filesystem>
namespace mgo2mt::stage {
size_t apply_floor_blend(CharacterModel&,std::span<const char> gwm,std::span<const char> sidecar);
size_t load_floor_blend(CharacterModel&,std::span<const char> gwm,const std::filesystem::path&);
}
