#pragma once
#include "character_renderer.h"
#include "combat_tracer.h"
#include <memory>
namespace mgo2win::combat::tracers {
class Renderer {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 explicit Renderer(ID3D11Device*);~Renderer();
 bool render(ID3D11DeviceContext*,CharacterRenderer&,const WorldView&,std::span<const Segment>);
};
}
