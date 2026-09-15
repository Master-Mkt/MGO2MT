#pragma once
#include "character_model.h"
#include "render_device.h"
#include "dynamic_light.h"
#include "original_color_mask.h"
#include "environment_light.h"
#include <d3d11.h>
#include <wrl/client.h>
namespace mgo2win {
namespace sop { class Renderer; }
namespace shadows { class Renderer; }
namespace water_visuals { class Renderer; }
namespace combat::tracers { class Renderer; }
struct WorldView {std::array<float,3> eye{},direction{0,0,1};float aspect=616.f/392.f;};
class CharacterRenderer {
 friend class sop::Renderer;
 friend class shadows::Renderer;
 friend class water_visuals::Renderer;
 friend class combat::tracers::Renderer;
 template<class T>using Ptr=Microsoft::WRL::ComPtr<T>;
 Ptr<ID3D11Texture2D> color_,depth_;Ptr<ID3D11RenderTargetView> target_;Ptr<ID3D11DepthStencilView> depthView_;Ptr<ID3D11ShaderResourceView> view_;
 Ptr<ID3D11Buffer> vertices_,indices_,constants_;Ptr<ID3D11VertexShader> vs_;Ptr<ID3D11PixelShader> ps_;Ptr<ID3D11InputLayout> layout_;
 Ptr<ID3D11SamplerState> sampler_;Ptr<ID3D11RasterizerState> raster_;Ptr<ID3D11DepthStencilState> depthState_;
 std::vector<Ptr<ID3D11ShaderResourceView>>textures_;std::vector<ModelPart>parts_;std::array<float,6>bounds_;
 std::vector<Ptr<ID3D11ShaderResourceView>> colorTextures_,mipTextures_;
 render_backend::Options textureOptions_{};unsigned samplerAnisotropy_=0;bool textureOptionsReady_=false;
 float previewVerticalOffset_=0;
 unsigned width_=616,height_=392;
 bool sky_=false,opaqueShadow_=false;
 size_t vertexCount_=0;std::array<float,6> overviewBounds_{};bool hasOverviewBounds_=false;
 std::vector<uint32_t> materialRules_;
 std::vector<std::string> materialDiagnostics_;
 Ptr<ID3D11Buffer> environmentBuffer_;
public:
 // Sky uses authored world vertices, diffuse texture only and far depth with
 // no depth writes. Its caller supplies a cleared, already rendered surface.
 CharacterRenderer(ID3D11Device*,const CharacterModel&,bool sky=false);
 void resize_target(ID3D11Device*,unsigned width,unsigned height);
 // Screen presentation shift as a fraction of standing height; ignored by world/overview passes.
 void preview_vertical_offset(float heightFraction);
 void update_vertices(ID3D11DeviceContext*,std::span<const ModelVertex>);
 // Owns the offscreen pass state. Caller restores its complete 2D pass state.
 void render(ID3D11DeviceContext*,float yaw,bool overview=false,const WorldView* camera=nullptr,
             CharacterRenderer* surface=nullptr,const std::array<float,3>* origin=nullptr,
             std::span<const DynamicPointLight> lights={},
             const OriginalColorMaskDraw* colorMask=nullptr,const shadows::Renderer* shadow=nullptr,const EnvironmentLight* environment=nullptr);
 const std::array<float,6>& bounds()const{return bounds_;}
 ID3D11ShaderResourceView* view()const{return view_.Get();}
 // Developer diagnostics; no material-analysis text is added to player UI.
 const std::vector<std::string>& material_diagnostics()const{return materialDiagnostics_;}
};
}
