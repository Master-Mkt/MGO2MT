#pragma once
#include "character_renderer.h"
#include "stage_water.h"
#include <memory>
namespace mgo2mt::water_visuals {
using Vec3=std::array<float,3>;
// Native gray overlay, not the recovered original material/shader.
struct Policy {float gray=.5f,opacity=.35f;};
struct Mesh {std::vector<Vec3> vertices;std::vector<uint32_t> indices;};
inline constexpr size_t maximum_triangles=1024*2+4096;
// Authored GWW finite top rectangles are a native geometry proxy. GWS triangles
// preserve their source plane/slope. Exact coincident triangles render once.
Mesh mesh(const stage::Water* fields,const stage::WaterSurface* surfaces);
class Renderer {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 Renderer(ID3D11Device*,const Mesh&,Policy={});~Renderer();
 Renderer(const Renderer&)=delete;Renderer& operator=(const Renderer&)=delete;
 // Both sides visible, including camera below water. World depth is read only;
 // render after ordinary stage/actors, before SOP and 2D UI. Caller restores
 // its full 2D pipeline state as it does after CharacterRenderer::render.
 bool render(ID3D11DeviceContext*,CharacterRenderer& surface,const WorldView&)const;
};
}
