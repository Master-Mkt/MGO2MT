#pragma once
#include "character_model.h"
#include "render_device.h"
#include "dynamic_light.h"
#include "original_color_mask.h"
#include "environment_light.h"
#include "render_lod.h"
#include <d3d11.h>
#include <wrl/client.h>
namespace mgo2mt {
namespace sop { class Renderer; }
namespace shadows { class Renderer; }
namespace water_visuals { class Renderer; }
namespace combat::tracers { class Renderer; }
namespace combat::particles { class Renderer; }
namespace stage::weather { class Renderer; }
struct WorldView {std::array<float,3> eye{},direction{0,0,1};float aspect=616.f/392.f;float verticalFov=1.f;};
enum class CharacterPass {all,opaque,alpha};
struct SkyFrame {std::array<float,3> position{},degrees{},color{1,1,1},fogColor{};float fog=0,cloudU=0;};
class CharacterRenderer {
 friend class sop::Renderer;
 friend class shadows::Renderer;
 friend class water_visuals::Renderer;
 friend class combat::tracers::Renderer;
 friend class combat::particles::Renderer;
 friend class stage::weather::Renderer;
 template<class T>using Ptr=Microsoft::WRL::ComPtr<T>;
 Ptr<ID3D11Texture2D> color_,depth_;Ptr<ID3D11RenderTargetView> target_;Ptr<ID3D11DepthStencilView> depthView_;Ptr<ID3D11ShaderResourceView> view_;
 Ptr<ID3D11DepthStencilView> readOnlyDepthView_;Ptr<ID3D11ShaderResourceView> depthResource_,reflectionView_;
 Ptr<ID3D11Texture2D> reflection_;Ptr<ID3D11RenderTargetView> reflectionTarget_;
 bool targetHdr_=false;
 Ptr<ID3D11Buffer> vertices_,indices_,constants_;Ptr<ID3D11VertexShader> vs_;Ptr<ID3D11PixelShader> ps_;Ptr<ID3D11InputLayout> layout_;
 Ptr<ID3D11SamplerState> sampler_;Ptr<ID3D11RasterizerState> raster_;Ptr<ID3D11DepthStencilState> depthState_;
 Ptr<ID3D11BlendState> fadeBlend_;Ptr<ID3D11DepthStencilState> fadeDepth_;
 std::vector<Ptr<ID3D11ShaderResourceView>>textures_;std::vector<ModelPart>parts_;std::array<float,6>bounds_;
 std::vector<Ptr<ID3D11ShaderResourceView>> colorTextures_,mipTextures_;
 render_backend::Options textureOptions_{};unsigned samplerAnisotropy_=0;bool textureOptionsReady_=false;
 float previewVerticalOffset_=0;
 unsigned width_=616,height_=392;
 bool sky_=false,opaqueShadow_=false;
 size_t vertexCount_=0;std::array<float,6> overviewBounds_{};bool hasOverviewBounds_=false;
 std::vector<uint32_t> materialRules_;
 std::vector<size_t> opaqueParts_,alphaParts_;
 std::vector<std::array<float,3>> partCenters_;
 std::vector<std::string> materialDiagnostics_;
 Ptr<ID3D11Buffer> environmentBuffer_;
 render_lod::Mesh lod_;std::vector<std::array<Ptr<ID3D11Buffer>,2>> lodIndices_;std::vector<unsigned> lodLevels_;
 size_t lodDrawn_=0,lodOriginal_=0,lodParts_=0;bool lodReady_=false;
public:
 // Sky uses authored world vertices, diffuse texture only and far depth with
 // no depth writes. Its caller supplies a cleared, already rendered surface.
 CharacterRenderer(ID3D11Device*,const CharacterModel&,bool sky=false);
 void resize_target(ID3D11Device*,unsigned width,unsigned height,bool hdr=false,bool reflections=false);
 // Screen presentation shift as a fraction of standing height; ignored by world/overview passes.
 void preview_vertical_offset(float heightFraction);
 void update_vertices(ID3D11DeviceContext*,std::span<const ModelVertex>);
 // Owns the offscreen pass state. Caller restores its complete 2D pass state.
 void render(ID3D11DeviceContext*,float yaw,bool overview=false,const WorldView* camera=nullptr,
             CharacterRenderer* surface=nullptr,const std::array<float,3>* origin=nullptr,
             std::span<const DynamicPointLight> lights={},
             const OriginalColorMaskDraw* colorMask=nullptr,const shadows::Renderer* shadow=nullptr,const EnvironmentLight* environment=nullptr,float opacity=1.f,CharacterPass pass=CharacterPass::all,const SkyFrame* skyFrame=nullptr);
 const std::array<float,6>& bounds()const{return bounds_;}
 ID3D11ShaderResourceView* view()const{return view_.Get();}
 ID3D11ShaderResourceView* depth_view()const{return depthResource_.Get();}
 ID3D11ShaderResourceView* reflection_view()const{return reflectionView_.Get();}
 ID3D11RenderTargetView* target_view()const{return target_.Get();}
 bool hdr()const{return targetHdr_;}
 // Static stages only. Authored vertices and collision geometry are unchanged.
 void prepare_lod(ID3D11Device*,const CharacterModel&);
 size_t lod_triangles()const{return lodDrawn_;}
 size_t original_triangles()const{return lodOriginal_;}
 size_t simplified_parts()const{return lodParts_;}
 // Developer diagnostics; no material-analysis text is added to player UI.
 const std::vector<std::string>& material_diagnostics()const{return materialDiagnostics_;}
};
}
