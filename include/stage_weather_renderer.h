#pragma once
#include "stage_weather.h"
#include "character_renderer.h"
#include <filesystem>
#include <memory>
namespace mgo2mt::stage::weather {
class Renderer {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 Renderer(ID3D11Device*,const std::filesystem::path& weatherBundle);~Renderer();
 // Full scene postprocess, then depth-tested original dust sprites. Caller restores 2D state.
 bool render(ID3D11DeviceContext*,CharacterRenderer&,const WorldView&,const Frame&);
};
}
