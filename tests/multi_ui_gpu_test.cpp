#include "multi_ui.h"
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
using namespace mgo2mt::multi_ui;
namespace {
void check(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
void hr(HRESULT result){if(FAILED(result))throw std::runtime_error("D3D11/WARP HRESULT "+std::to_string(uint32_t(result)));}
struct Fixture {
 std::filesystem::path root=std::filesystem::temp_directory_path()/("mgo2mt-ui-gpu-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 Fixture(){std::filesystem::create_directory(root);}
 ~Fixture(){std::error_code error;std::filesystem::remove_all(root,error);}
};
struct Vertex {float x,y,u,v,r=1,g=1,b=1,a=1;};
// These shader equations, UNORM texture, sampler, blend and contain viewport are
// the title_preview.cpp custom-UI path, exercised on an actual WARP device.
const char* shader=R"(struct V{float2 p:POSITION;float2 uv:TEXCOORD;float4 c:COLOR;};
struct P{float4 p:SV_POSITION;float2 uv:TEXCOORD;float4 c:COLOR;};
P vs(V v){P o;o.p=float4(v.p.x/640-1,1-v.p.y/360,0,1);o.uv=v.uv;o.c=v.c;return o;}
Texture2D tex:register(t0);SamplerState smp:register(s0);
float4 ps(P p):SV_TARGET{return tex.Sample(smp,p.uv)*p.c;})";
struct Gpu {
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
 ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;ComPtr<ID3D11InputLayout> layout;
 ComPtr<ID3D11Buffer> vertices;ComPtr<ID3D11SamplerState> sampler;ComPtr<ID3D11BlendState> blend;ComPtr<ID3D11RasterizerState> raster;
 Gpu(){
  hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
  ComPtr<ID3DBlob> v,p,errors;hr(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,"vs","vs_4_0",0,0,&v,&errors));
  hr(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,"ps","ps_4_0",0,0,&p,&errors));
  hr(device->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&vs));
  hr(device->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&ps));
  const D3D11_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0}};
  hr(device->CreateInputLayout(elements,3,v->GetBufferPointer(),v->GetBufferSize(),&layout));
  const Vertex quad[]={{0,0,0,0},{1280,0,1,0},{1280,720,1,1},{0,0,0,0},{1280,720,1,1},{0,720,0,1}};
  D3D11_BUFFER_DESC bd{};bd.ByteWidth=sizeof quad;bd.Usage=D3D11_USAGE_IMMUTABLE;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;D3D11_SUBRESOURCE_DATA data{quad};hr(device->CreateBuffer(&bd,&data,&vertices));
  D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;hr(device->CreateSamplerState(&sd,&sampler));
  D3D11_BLEND_DESC bs{};auto& rt=bs.RenderTarget[0];rt.BlendEnable=TRUE;rt.SrcBlend=D3D11_BLEND_SRC_ALPHA;rt.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOp=D3D11_BLEND_OP_ADD;rt.SrcBlendAlpha=D3D11_BLEND_ONE;rt.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;rt.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;hr(device->CreateBlendState(&bs,&blend));
  D3D11_RASTERIZER_DESC rs{};rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_NONE;rs.DepthClipEnable=TRUE;hr(device->CreateRasterizerState(&rs,&raster));
 }
 std::vector<uint8_t> render(std::span<const uint8_t> canonical,unsigned width,unsigned height,std::array<uint8_t,4> background){
  D3D11_TEXTURE2D_DESC td{};td.Width=1280;td.Height=720;td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
  ComPtr<ID3D11Texture2D> texture;ComPtr<ID3D11ShaderResourceView> view;hr(device->CreateTexture2D(&td,nullptr,&texture));context->UpdateSubresource(texture.Get(),0,nullptr,canonical.data(),1280*4,0);hr(device->CreateShaderResourceView(texture.Get(),nullptr,&view));
  td.Width=width;td.Height=height;td.BindFlags=D3D11_BIND_RENDER_TARGET;ComPtr<ID3D11Texture2D> target,readback;ComPtr<ID3D11RenderTargetView> targetView;hr(device->CreateTexture2D(&td,nullptr,&target));hr(device->CreateRenderTargetView(target.Get(),nullptr,&targetView));
  td.Usage=D3D11_USAGE_STAGING;td.BindFlags=0;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;hr(device->CreateTexture2D(&td,nullptr,&readback));
  const float scale=std::min(width/1280.f,height/720.f);const D3D11_VIEWPORT vp{(width-1280*scale)/2,(height-720*scale)/2,1280*scale,720*scale,0,1};context->RSSetViewports(1,&vp);context->RSSetState(raster.Get());
  float clear[4];for(unsigned k=0;k<4;++k)clear[k]=background[k]/255.f;context->ClearRenderTargetView(targetView.Get(),clear);
  auto* rt=targetView.Get();context->OMSetRenderTargets(1,&rt,nullptr);context->OMSetBlendState(blend.Get(),nullptr,0xffffffff);
  auto* vb=vertices.Get();UINT stride=sizeof(Vertex),offset=0;context->IASetVertexBuffers(0,1,&vb,&stride,&offset);context->IASetInputLayout(layout.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);
  auto* srv=view.Get();auto* ss=sampler.Get();context->PSSetShaderResources(0,1,&srv);context->PSSetSamplers(0,1,&ss);context->Draw(6,0);
  context->CopyResource(readback.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE mapped{};hr(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));
  std::vector<uint8_t> result(size_t(width)*height*4);for(unsigned y=0;y<height;++y)std::memcpy(result.data()+size_t(y)*width*4,static_cast<const uint8_t*>(mapped.pData)+size_t(y)*mapped.RowPitch,size_t(width)*4);context->Unmap(readback.Get(),0);
  srv=nullptr;context->PSSetShaderResources(0,1,&srv);context->OMSetRenderTargets(0,nullptr,nullptr);return result;
 }
};
}
int main(){try{
 Fixture fixture;Image image{16,16,std::vector<uint8_t>(16*16*4)};
 for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x){const size_t at=(y*16+x)*4;image.rgba[at]=uint8_t(x*17);image.rgba[at+1]=uint8_t(y*17);image.rgba[at+2]=uint8_t((x^y)*17);image.rgba[at+3]=uint8_t((x+y)*255/30);}
 const auto dds=encode_dds(image);{std::ofstream file(fixture.root/"own.dds",std::ios::binary);file.write(reinterpret_cast<const char*>(dds.data()),dds.size());}
 Runtime runtime;check(runtime.load_json(R"({"format":"MGO2MT.UI_LAYOUT.1","width":1280,"height":720,"elements":[
 {"id":"tl","kind":"panel","width":99,"height":73,"color":[255,37,19,128]},
 {"id":"tr","kind":"panel","anchor":"top-right","width":103,"height":77,"color":[13,231,57,193]},
 {"id":"bl","kind":"panel","anchor":"bottom-left","width":107,"height":81,"color":[31,67,241,63]},
 {"id":"br","kind":"panel","anchor":"bottom-right","width":111,"height":85,"color":[227,163,49,255]},
 {"id":"center","kind":"image","anchor":"center","width":257,"height":129,"texture":"own.dds"},
 {"id":"overlap","kind":"panel","anchor":"center","x":31,"y":17,"width":87,"height":61,"color":[177,19,231,117],"z":2},
 {"id":"text","kind":"text","x":191,"y":151,"width":400,"height":63,"fontSize":31,"text":"GPU alpha / UI","color":[201,219,237,177]}
 ]})",fixture.root),"layout load");
 Context state;std::vector<uint8_t> canonical(1280*720*4);check(runtime.paint(canonical,1280,720,state),"canonical paint");Gpu gpu;
 // Opaque framebuffer is deliberate: title_preview composites UI over the
 // already rendered native UI/world and clears its final target alpha to one.
 // Transparent export buffers use straight-alpha storage, unlike GPU RT RGB.
 for(const auto background:{std::array<uint8_t,4>{0,0,0,255},std::array<uint8_t,4>{29,67,103,255}})for(const auto size:{std::array<unsigned,2>{800,800},std::array<unsigned,2>{1280,720},std::array<unsigned,2>{1920,1080}}){
  const auto width=size[0],height=size[1];std::vector<uint8_t> cpu(size_t(width)*height*4);for(size_t i=0;i<cpu.size();i+=4)std::copy(background.begin(),background.end(),cpu.begin()+i);check(runtime.paint(cpu,width,height,state),"target paint");auto actual=gpu.render(canonical,width,height,background);
  unsigned maximum=0;size_t exceeded=0,changed=0;for(size_t i=0;i<cpu.size();++i){unsigned difference=unsigned(std::abs(int(cpu[i])-int(actual[i])));maximum=std::max(maximum,difference);exceeded+=difference>1;changed+=cpu[i]!=background[i%4];}
  std::cout<<width<<'x'<<height<<" background="<<unsigned(background[0])<<" max_channel_delta="<<maximum<<" outside_tolerance="<<exceeded<<" changed_channels="<<changed<<'\n';
  // Check every pixel, including center, all four corners, letterbox and
  // translucent element/glyph/image edges. Allow +/-1 for UNORM/filter rounding.
  check(changed>1000,"nonempty rendered fixture");check(exceeded==0,"CPU / real D3D11 linear straight-alpha parity exceeds +/-1 UNORM quantization");
  if(width==800)check(std::equal(background.begin(),background.end(),actual.begin()),"letterbox remains original background");
 }
 std::cout<<"multi UI D3D11 WARP parity passed (all pixels, +/-1 channel tolerance)\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
