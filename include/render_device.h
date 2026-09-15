#pragma once
#include "render_options.h"
#include <d3d11.h>
#include <wrl/client.h>
namespace mgo2win::render_backend {
template<class T>using Handle=Microsoft::WRL::ComPtr<T>;
// Thin D3D11 backend boundary. No scene, simulation, asset or UI ownership.
struct RenderTarget {
 Handle<ID3D11Texture2D> color,depth;
 Handle<ID3D11RenderTargetView> target;
 Handle<ID3D11DepthStencilView> depthView;
 Handle<ID3D11ShaderResourceView> view;
};
class Device {
 ID3D11Device* device_;
public:
 explicit Device(ID3D11Device* device):device_(device){}
 Options options()const;
 void configure(Options)const;
 Handle<ID3D11SamplerState> sampler(unsigned anisotropy)const;
 RenderTarget target(unsigned width,unsigned height)const;
 // Original BC bytes are retained; a separate color view decodes sRGB.
 Handle<ID3D11ShaderResourceView> color_view(ID3D11ShaderResourceView*)const;
 // Generates a complete RGBA mip chain through an isolated command list.
 // Color views use sRGB filtering; normal/data views remain UNORM.
 Handle<ID3D11ShaderResourceView> mip_chain(ID3D11DeviceContext*,ID3D11ShaderResourceView*,bool srgb)const;
};
}
