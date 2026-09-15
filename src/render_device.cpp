#include "render_device.h"
#include <d3dcompiler.h>
#include <cstring>
namespace mgo2win::render_backend {
namespace {
constexpr GUID optionsKey{0x1f2672ec,0x7c82,0x418b,{0x81,0x3c,0x44,0xda,0x66,0xbd,0x85,0xa9}};
void ok(HRESULT h){if(FAILED(h))throw std::runtime_error("D3D11 render resource allocation failed");}
}
Options Device::options()const{Options value{};UINT bytes=sizeof(value);if(FAILED(device_->GetPrivateData(optionsKey,&bytes,&value)))return {};return value;}
void Device::configure(Options value)const{if(!valid(value))throw std::invalid_argument("Render options");ok(device_->SetPrivateData(optionsKey,sizeof(value),&value));}
Handle<ID3D11SamplerState> Device::sampler(unsigned anisotropy)const{
 if(!valid({anisotropy}))throw std::invalid_argument("Anisotropy");
 D3D11_SAMPLER_DESC s{};s.Filter=anisotropy?D3D11_FILTER_ANISOTROPIC:D3D11_FILTER_MIN_MAG_MIP_LINEAR;s.MaxAnisotropy=std::max(1u,anisotropy);
 s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;s.MaxLOD=D3D11_FLOAT32_MAX;
 Handle<ID3D11SamplerState> result;ok(device_->CreateSamplerState(&s,&result));return result;
}
RenderTarget Device::target(unsigned w,unsigned h)const{
 if(!w||!h||w>8192||h>8192||uint64_t(w)*h>16777216)throw std::invalid_argument("Render target budget");
 RenderTarget r;D3D11_TEXTURE2D_DESC t{};t.Width=w;t.Height=h;t.MipLevels=t.ArraySize=t.SampleDesc.Count=1;t.Format=DXGI_FORMAT_R8G8B8A8_UNORM;t.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
 ok(device_->CreateTexture2D(&t,nullptr,&r.color));ok(device_->CreateRenderTargetView(r.color.Get(),nullptr,&r.target));ok(device_->CreateShaderResourceView(r.color.Get(),nullptr,&r.view));
 t.Format=DXGI_FORMAT_D32_FLOAT;t.BindFlags=D3D11_BIND_DEPTH_STENCIL;ok(device_->CreateTexture2D(&t,nullptr,&r.depth));ok(device_->CreateDepthStencilView(r.depth.Get(),nullptr,&r.depthView));return r;
}
Handle<ID3D11ShaderResourceView> Device::color_view(ID3D11ShaderResourceView* source)const{
 Handle<ID3D11Resource> resource;source->GetResource(&resource);D3D11_SHADER_RESOURCE_VIEW_DESC desc{};source->GetDesc(&desc);
 desc.Format=desc.Format==DXGI_FORMAT_BC1_UNORM?DXGI_FORMAT_BC1_UNORM_SRGB:DXGI_FORMAT_BC3_UNORM_SRGB;
 Handle<ID3D11ShaderResourceView> result;ok(device_->CreateShaderResourceView(resource.Get(),&desc,&result));return result;
}
Handle<ID3D11ShaderResourceView> Device::mip_chain(ID3D11DeviceContext* immediate,ID3D11ShaderResourceView* source,bool srgb)const{
 Handle<ID3D11Resource> original;source->GetResource(&original);Handle<ID3D11Texture2D> image;ok(original.As(&image));D3D11_TEXTURE2D_DESC desc{};image->GetDesc(&desc);
 desc.MipLevels=0;desc.Format=srgb?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=0;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;desc.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
 UINT support=0;ok(device_->CheckFormatSupport(desc.Format,&support));if(!(support&D3D11_FORMAT_SUPPORT_MIP_AUTOGEN))throw std::runtime_error("Mip generation unavailable");
 Handle<ID3D11Texture2D> texture;Handle<ID3D11ShaderResourceView> view;Handle<ID3D11RenderTargetView> target;ok(device_->CreateTexture2D(&desc,nullptr,&texture));ok(device_->CreateShaderResourceView(texture.Get(),nullptr,&view));
 D3D11_RENDER_TARGET_VIEW_DESC rt{};rt.Format=desc.Format;rt.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;ok(device_->CreateRenderTargetView(texture.Get(),&rt,&target));
 const char* code=R"(
 Texture2D inputImage:register(t0);
 float4 vs(uint id:SV_VertexID):SV_POSITION{return float4(id==1?3:-1,id==2?-3:1,0,1);}
 float4 ps(float4 p:SV_POSITION):SV_TARGET{return inputImage.Load(int3(int2(p.xy),0));}
 )";
 Handle<ID3DBlob> vb,pb,error;ok(D3DCompile(code,std::strlen(code),nullptr,nullptr,nullptr,"vs","vs_4_0",0,0,&vb,&error));ok(D3DCompile(code,std::strlen(code),nullptr,nullptr,nullptr,"ps","ps_4_0",0,0,&pb,&error));
 Handle<ID3D11VertexShader> vs;Handle<ID3D11PixelShader> ps;ok(device_->CreateVertexShader(vb->GetBufferPointer(),vb->GetBufferSize(),nullptr,&vs));ok(device_->CreatePixelShader(pb->GetBufferPointer(),pb->GetBufferSize(),nullptr,&ps));
 Handle<ID3D11DeviceContext> context;ok(device_->CreateDeferredContext(0,&context));auto r=target.Get();context->OMSetRenderTargets(1,&r,nullptr);D3D11_VIEWPORT viewport{0,0,float(desc.Width),float(desc.Height),0,1};context->RSSetViewports(1,&viewport);
 D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;Handle<ID3D11RasterizerState> raster;ok(device_->CreateRasterizerState(&rd,&raster));context->RSSetState(raster.Get());
 context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);context->PSSetShaderResources(0,1,&source);context->Draw(3,0);context->OMSetRenderTargets(0,nullptr,nullptr);ID3D11ShaderResourceView* nil=nullptr;context->PSSetShaderResources(0,1,&nil);context->GenerateMips(view.Get());
 Handle<ID3D11CommandList> commands;ok(context->FinishCommandList(FALSE,&commands));immediate->ExecuteCommandList(commands.Get(),TRUE);return view;
}
}
