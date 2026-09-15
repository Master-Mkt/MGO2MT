#include "tracer_renderer.h"
#include "world_depth.h"
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace mgo2win::combat::tracers {
using Microsoft::WRL::ComPtr;using namespace DirectX;
namespace {
void check(HRESULT h){if(FAILED(h))throw std::runtime_error("Tracer D3D11 failure");}
struct Vertex {XMFLOAT4 clip,color;};
bool valid(Vec3 p){for(float x:p)if(!std::isfinite(x)||std::abs(x)>1000000)return false;return true;}
}
struct Renderer::Impl {ComPtr<ID3D11Buffer> vertices;ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;ComPtr<ID3D11InputLayout> layout;ComPtr<ID3D11DepthStencilState> depth;ComPtr<ID3D11BlendState> blend;ComPtr<ID3D11RasterizerState> raster;};
Renderer::Renderer(ID3D11Device*d):impl_(std::make_unique<Impl>()){
 if(!d)throw std::invalid_argument("Tracer device");auto&r=*impl_;
 D3D11_BUFFER_DESC b{};b.ByteWidth=UINT(Pool::capacity*12*sizeof(Vertex));b.Usage=D3D11_USAGE_DYNAMIC;b.BindFlags=D3D11_BIND_VERTEX_BUFFER;b.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;check(d->CreateBuffer(&b,nullptr,&r.vertices));
 const char* shader=R"(struct V{float4 p:POSITION;float4 c:COLOR;};struct O{float4 p:SV_POSITION;float4 c:COLOR;};O vs(V v){O o;o.p=v.p;o.c=v.c;return o;}float4 ps(O v):SV_TARGET{return v.c;})";
 auto compile=[&](const char*entry,const char*profile){ComPtr<ID3DBlob> code,error;check(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,entry,profile,D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&error));return code;};
 auto vs=compile("vs","vs_4_0"),ps=compile("ps","ps_4_0");check(d->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&r.vs));check(d->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&r.ps));
 D3D11_INPUT_ELEMENT_DESC input[]={{"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0}};check(d->CreateInputLayout(input,2,vs->GetBufferPointer(),vs->GetBufferSize(),&r.layout));
 D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthEnable=TRUE;depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;depth.DepthFunc=D3D11_COMPARISON_GREATER_EQUAL;check(d->CreateDepthStencilState(&depth,&r.depth));
 D3D11_BLEND_DESC blend{};auto&t=blend.RenderTarget[0];t.BlendEnable=TRUE;t.SrcBlend=D3D11_BLEND_SRC_ALPHA;t.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;t.BlendOp=D3D11_BLEND_OP_ADD;t.SrcBlendAlpha=D3D11_BLEND_ONE;t.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;t.BlendOpAlpha=D3D11_BLEND_OP_ADD;t.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;check(d->CreateBlendState(&blend,&r.blend));
 D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;check(d->CreateRasterizerState(&raster,&r.raster));
}
Renderer::~Renderer()=default;
bool Renderer::render(ID3D11DeviceContext*c,CharacterRenderer&surface,const WorldView&camera,std::span<const Segment>segments){
 if(!c||segments.empty()||segments.size()>Pool::capacity||!valid(camera.eye)||!valid(camera.direction)||!std::isfinite(camera.aspect)||camera.aspect<=0||camera.aspect>32)return false;
 const auto eye=XMVectorSet(camera.eye[0],camera.eye[1],camera.eye[2],1),raw=XMVectorSet(camera.direction[0],camera.direction[1],camera.direction[2],0);if(XMVectorGetX(XMVector3LengthSq(raw))<1e-8f)return false;
 const auto forward=XMVector3Normalize(raw),up=std::abs(XMVectorGetY(forward))>.999f?XMVectorSet(0,0,1,0):XMVectorSet(0,1,0,0);
 const auto vp=XMMatrixLookToLH(eye,forward,up)*world_projection(camera.aspect);
 std::vector<Vertex> vertices;vertices.reserve(segments.size()*12);
 for(const auto&s:segments){if(!valid(s.from)||!valid(s.to)||!std::isfinite(s.opacity)||s.opacity<=0||s.opacity>1)continue;
  auto a=XMVectorSet(s.from[0],s.from[1],s.from[2],1),b=XMVectorSet(s.to[0],s.to[1],s.to[2],1);
  float za=XMVectorGetX(XMVector3Dot(a-eye,forward)),zb=XMVectorGetX(XMVector3Dot(b-eye,forward));constexpr float nearZ=world_near_plane+.01f;if(za<nearZ&&zb<nearZ)continue;
  if(za<nearZ)a=XMVectorLerp(a,b,(nearZ-za)/(zb-za));else if(zb<nearZ)b=XMVectorLerp(b,a,(nearZ-zb)/(za-zb));
  XMFLOAT4 pa,pb;XMStoreFloat4(&pa,XMVector4Transform(a,vp));XMStoreFloat4(&pb,XMVector4Transform(b,vp));
  float dx=(pb.x/pb.w-pa.x/pa.w)*surface.width_*.5f,dy=(pb.y/pb.w-pa.y/pa.w)*surface.height_*.5f;float length=std::hypot(dx,dy);
  // A head-on ray has zero projected length. Its small core remains centered
  // on that ray and uses the segment's true depth, never an always-on-top dot.
  if(length<2){float x=length>1e-5f?dx/length:1,y=length>1e-5f?dy/length:0;float extra=(2-length)*.5f;pa.x-=x*extra*2/surface.width_*pa.w;pa.y-=y*extra*2/surface.height_*pa.w;pb.x+=x*extra*2/surface.width_*pb.w;pb.y+=y*extra*2/surface.height_*pb.w;dx=x;dy=y;length=1;}
  float nx=-dy/length,ny=dx/length;
  for(int layer=0;layer<2;++layer){float width=layer?1.f:2.5f;XMFLOAT4 color=layer?XMFLOAT4(1,1,.8f,s.opacity):XMFLOAT4(1,.65f,.1f,s.opacity*.4f);Vertex q[4];
   for(int i=0;i<4;++i){auto p=i<2?pa:pb;float side=(i==0||i==2)?-1.f:1.f;p.x+=side*nx*width*2/surface.width_*p.w;p.y+=side*ny*width*2/surface.height_*p.w;q[i]={p,color};}
   for(int i:{0,1,2,2,1,3})vertices.push_back(q[i]);
  }
 }
 if(vertices.empty())return false;auto&r=*impl_;D3D11_MAPPED_SUBRESOURCE mapped{};check(c->Map(r.vertices.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped));std::memcpy(mapped.pData,vertices.data(),vertices.size()*sizeof(Vertex));c->Unmap(r.vertices.Get(),0);
 ID3D11ShaderResourceView*nil=nullptr;c->PSSetShaderResources(0,1,&nil);auto target=surface.target_.Get();c->OMSetRenderTargets(1,&target,surface.depthView_.Get());c->OMSetDepthStencilState(r.depth.Get(),0);c->OMSetBlendState(r.blend.Get(),nullptr,~0u);
 D3D11_VIEWPORT viewport{0,0,float(surface.width_),float(surface.height_),0,1};c->RSSetViewports(1,&viewport);c->RSSetState(r.raster.Get());c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);c->IASetInputLayout(r.layout.Get());auto vb=r.vertices.Get();UINT stride=sizeof(Vertex),offset=0;c->IASetVertexBuffers(0,1,&vb,&stride,&offset);c->IASetIndexBuffer(nullptr,DXGI_FORMAT_R32_UINT,0);c->VSSetShader(r.vs.Get(),nullptr,0);c->PSSetShader(r.ps.Get(),nullptr,0);c->Draw(UINT(vertices.size()),0);c->OMSetRenderTargets(0,nullptr,nullptr);c->OMSetBlendState(nullptr,nullptr,~0u);return true;
}
}
