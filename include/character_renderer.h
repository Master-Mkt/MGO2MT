#pragma once
#include "character_model.h"
#include <d3d11.h>
#include <wrl/client.h>
namespace mgo2win {
class CharacterRenderer {
 template<class T>using Ptr=Microsoft::WRL::ComPtr<T>;
 Ptr<ID3D11Texture2D> color_,depth_;Ptr<ID3D11RenderTargetView> target_;Ptr<ID3D11DepthStencilView> depthView_;Ptr<ID3D11ShaderResourceView> view_;
 Ptr<ID3D11Buffer> vertices_,indices_,constants_;Ptr<ID3D11VertexShader> vs_;Ptr<ID3D11PixelShader> ps_;Ptr<ID3D11InputLayout> layout_;
 Ptr<ID3D11SamplerState> sampler_;Ptr<ID3D11RasterizerState> raster_;Ptr<ID3D11DepthStencilState> depthState_;
 std::vector<Ptr<ID3D11ShaderResourceView>>textures_;std::vector<ModelPart>parts_;std::array<float,6>bounds_;
 size_t vertexCount_=0;std::array<float,6> overviewBounds_{};bool hasOverviewBounds_=false;
public:
 CharacterRenderer(ID3D11Device*,const CharacterModel&);
 void update_vertices(ID3D11DeviceContext*,std::span<const ModelVertex>);
 // Owns the offscreen pass state. Caller restores its complete 2D pass state.
 void render(ID3D11DeviceContext*,float yaw,bool overview=false);
 ID3D11ShaderResourceView* view()const{return view_.Get();}
};
}
