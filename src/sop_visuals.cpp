#include "sop_visuals.h"
#include "world_depth.h"
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace mgo2win::sop {
namespace {
using namespace DirectX;
using Microsoft::WRL::ComPtr;
void ok(HRESULT h){if(FAILED(h))throw std::runtime_error("SOP prototype D3D11 failure");}
bool finite(Vec3 p){return std::all_of(p.begin(),p.end(),[](float f){return std::isfinite(f)&&std::abs(f)<1e7f;});}
Vec3 add(Vec3 a,Vec3 b){return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};}
Vec3 sub(Vec3 a,Vec3 b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
Vec3 mul(Vec3 a,float s){return {a[0]*s,a[1]*s,a[2]*s};}
float length(Vec3 a){return std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);}
Vec3 cross(Vec3 a,Vec3 b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
constexpr uint32_t keys[]={0xa89233,0x7011c4,0x6c02b2,0xf8d3cc,0xf5d387,0x019543,0x027a4c,0xfafa8b,0xfbfa42,0x619d43,0x62824c,0x5b028c,0x5c0243,0x449d08,0xf7f0c4,0xfb4232,0xf81206,0x459d14,0xfaf104,0x5b4a33,0xfb1246};
constexpr unsigned starts[]={0,2,6,7,10,11,13,14,15,17,18,19};
constexpr unsigned ends[]  ={2,4,7,8,11,12,14,15,16,18,19,20};
constexpr float radii[]={115,135,65,55,65,55,85,65,50,85,65,50};
// Capped cylinders with hemispherical ends. Fixed 10x8 topology, no asset data.
void capsule(DollMesh& m,Vec3 a,Vec3 b,float radius){
 auto delta=sub(b,a);float size=length(delta);Vec3 axis=size>1e-4f?mul(delta,1/size):Vec3{0,1,0};
 auto side=cross(axis,std::abs(axis[1])<.9f?Vec3{0,1,0}:Vec3{1,0,0});side=mul(side,1/length(side));auto other=cross(axis,side);
 const uint32_t base=uint32_t(m.vertices.size());constexpr unsigned rings=10,slices=8;constexpr float pi=3.14159265358979323846f;
 for(unsigned r=0;r<rings;++r){float theta=-pi/2+pi*float(r)/float(rings-1);float axial=std::sin(theta)*radius;float radial=std::cos(theta)*radius;auto center=add(r<rings/2?a:b,mul(axis,axial));
  for(unsigned s=0;s<slices;++s){float angle=2*pi*float(s)/slices;m.vertices.push_back(add(center,add(mul(side,std::cos(angle)*radial),mul(other,std::sin(angle)*radial))));}}
 for(unsigned r=0;r+1<rings;++r)for(unsigned s=0;s<slices;++s){auto x=base+r*slices+s,y=base+r*slices+(s+1)%slices;m.indices.insert(m.indices.end(),{x,y,x+slices,y,y+slices,x+slices});}
}
bool camera_valid(const WorldView& c){if(!finite(c.eye)||!finite(c.direction)||!std::isfinite(c.aspect)||c.aspect<=0||c.aspect>32)return false;return length(c.direction)>.032f&&c.direction[0]*c.direction[0]+c.direction[2]*c.direction[2]>.00001f;}
XMMATRIX vp(const WorldView& c){return XMMatrixLookToLH(XMVectorSet(c.eye[0],c.eye[1],c.eye[2],1),XMVectorSet(c.direction[0],c.direction[1],c.direction[2],0),XMVectorSet(0,1,0,0))*world_projection(c.aspect);}
struct Constants {XMFLOAT4X4 wvp,world;XMFLOAT4 color,scan;XMFLOAT4 center;};
static_assert(sizeof(Constants)==176);
}
std::optional<DollMesh> doll_mesh(const PreparedCharacter& body){
 std::array<Vec3,21> p;for(unsigned i=0;i<p.size();++i){auto at=body.bone_position(keys[i]);if(!at||!finite(*at))return {};p[i]=*at;}
 DollMesh mesh;mesh.vertices.reserve(1040);mesh.indices.reserve(5616);
 for(unsigned i=0;i<12;++i){float size=length(sub(p[ends[i]],p[starts[i]]));if(size<1.f||size>5000.f)return {};float radius=std::min(radii[i],size*.48f);auto direction=mul(sub(p[ends[i]],p[starts[i]]),1/size);capsule(mesh,add(p[starts[i]],mul(direction,radius)),sub(p[ends[i]],mul(direction,radius)),radius);}
 capsule(mesh,p[4],p[4],140);return mesh;
}
struct Renderer::Impl {
 Policy policy;
 ComPtr<ID3D11Texture2D> mask,depth;ComPtr<ID3D11RenderTargetView> target;ComPtr<ID3D11ShaderResourceView> view;ComPtr<ID3D11DepthStencilView> depthView;
 ComPtr<ID3D11Buffer> vertices,indices,constants;ComPtr<ID3D11VertexShader> vs,fullVs,scanVs;ComPtr<ID3D11PixelShader> maskPs,compositePs,scanPs;ComPtr<ID3D11InputLayout> layout,scanLayout;
 ComPtr<ID3D11RasterizerState> raster;ComPtr<ID3D11DepthStencilState> writeDepth,readDepth,noDepth;ComPtr<ID3D11BlendState> alpha,additive;
 void upload(ID3D11DeviceContext*c,const Constants& data){c->UpdateSubresource(constants.Get(),0,nullptr,&data,0,0);auto b=constants.Get();c->VSSetConstantBuffers(0,1,&b);c->PSSetConstantBuffers(0,1,&b);}
 unsigned width=616,height=392;
 void size(ID3D11DeviceContext*c,unsigned w,unsigned h){
  if(width==w&&height==h)return;
  ComPtr<ID3D11Device>d;c->GetDevice(&d);ComPtr<ID3D11Texture2D> nextMask,nextDepth;ComPtr<ID3D11RenderTargetView> nextTarget;ComPtr<ID3D11ShaderResourceView> nextView;ComPtr<ID3D11DepthStencilView> nextDepthView;
  D3D11_TEXTURE2D_DESC t{};t.Width=w;t.Height=h;t.MipLevels=t.ArraySize=t.SampleDesc.Count=1;t.Format=DXGI_FORMAT_R8G8B8A8_UNORM;t.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
  ok(d->CreateTexture2D(&t,nullptr,&nextMask));ok(d->CreateRenderTargetView(nextMask.Get(),nullptr,&nextTarget));ok(d->CreateShaderResourceView(nextMask.Get(),nullptr,&nextView));
  t.Format=DXGI_FORMAT_D32_FLOAT;t.BindFlags=D3D11_BIND_DEPTH_STENCIL;ok(d->CreateTexture2D(&t,nullptr,&nextDepth));ok(d->CreateDepthStencilView(nextDepth.Get(),nullptr,&nextDepthView));
  mask=std::move(nextMask);depth=std::move(nextDepth);target=std::move(nextTarget);view=std::move(nextView);depthView=std::move(nextDepthView);width=w;height=h;
 }
 void common(ID3D11DeviceContext*c,unsigned w,unsigned h){size(c,w,h);ID3D11ShaderResourceView* nil[2]={};c->PSSetShaderResources(0,2,nil);D3D11_VIEWPORT viewport{0,0,float(width),float(height),0,1};c->RSSetViewports(1,&viewport);c->RSSetState(raster.Get());c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);}
 void finish(ID3D11DeviceContext*c){ID3D11ShaderResourceView*nil=nullptr;c->PSSetShaderResources(0,1,&nil);c->OMSetRenderTargets(0,nullptr,nullptr);c->OMSetBlendState(nullptr,nullptr,0xffffffff);}
};
Renderer::Renderer(ID3D11Device*d,Policy policy):impl_(std::make_unique<Impl>()){
 if(!d||!finite(policy.orange)||!std::all_of(policy.orange.begin(),policy.orange.end(),[](float v){return v>=0&&v<=1;})||!std::isfinite(policy.dollOpacity)||policy.dollOpacity<0||policy.dollOpacity>1)throw std::invalid_argument("Invalid SOP visual policy");
 auto&i=*impl_;i.policy=policy;
 D3D11_TEXTURE2D_DESC t{};t.Width=616;t.Height=392;t.MipLevels=t.ArraySize=t.SampleDesc.Count=1;t.Format=DXGI_FORMAT_R8G8B8A8_UNORM;t.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
 ok(d->CreateTexture2D(&t,nullptr,&i.mask));ok(d->CreateRenderTargetView(i.mask.Get(),nullptr,&i.target));ok(d->CreateShaderResourceView(i.mask.Get(),nullptr,&i.view));t.Format=DXGI_FORMAT_D32_FLOAT;t.BindFlags=D3D11_BIND_DEPTH_STENCIL;ok(d->CreateTexture2D(&t,nullptr,&i.depth));ok(d->CreateDepthStencilView(i.depth.Get(),nullptr,&i.depthView));
 auto buffer=[&](UINT n,UINT bind,ID3D11Buffer**out){D3D11_BUFFER_DESC b{};b.ByteWidth=n;b.BindFlags=bind;b.Usage=D3D11_USAGE_DEFAULT;ok(d->CreateBuffer(&b,nullptr,out));};buffer(1040*sizeof(Vec3),D3D11_BIND_VERTEX_BUFFER,&i.vertices);buffer(5616*4,D3D11_BIND_INDEX_BUFFER,&i.indices);buffer(sizeof(Constants),D3D11_BIND_CONSTANT_BUFFER,&i.constants);
 const char* shader=R"(
 cbuffer Frame:register(b0){row_major float4x4 wvp;row_major float4x4 world;float4 color;float4 scan;float4 center;};
 struct P{float4 position:SV_POSITION;float3 worldPosition:TEXCOORD0;float2 uv:TEXCOORD1;float discardAlpha:TEXCOORD2;};
 P vs(float3 pos:POSITION){P p;p.position=mul(float4(pos,1),wvp);p.worldPosition=mul(float4(pos,1),world).xyz;p.uv=0;p.discardAlpha=0;return p;}
 P scanVs(float3 pos:POSITION,float2 uv:TEXCOORD0,float4 light:COLOR0){P p=vs(pos);p.uv=uv;p.discardAlpha=light.a;return p;}
 float4 maskPs(P p):SV_TARGET{return color;}
 P fullVs(uint vertex:SV_VertexID){P p;float2 xy=float2((vertex<<1)&2,vertex&2);p.position=float4(xy*float2(2,-2)+float2(-1,1),0,1);p.worldPosition=0;p.uv=0;p.discardAlpha=0;return p;}
 Texture2D mask:register(t0);Texture2D original:register(t1);SamplerState sampleLinear:register(s0);
 float4 compositePs(P p):SV_TARGET{return mask.Load(int3(int2(p.position.xy),0));}
 float4 scanPs(P p):SV_TARGET{if(p.discardAlpha<.5)clip(original.Sample(sampleLinear,p.uv).a-.25);float band=saturate(1-abs(length(p.worldPosition-center.xyz)-scan.x)/scan.y);clip(band-.001);return float4(color.rgb,band*scan.z);}
 )";
 auto compile=[&](const char* entry,const char* profile){ComPtr<ID3DBlob>b,e;ok(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,entry,profile,D3DCOMPILE_ENABLE_STRICTNESS,0,&b,&e));return b;};
 auto v=compile("vs","vs_4_0"),f=compile("fullVs","vs_4_0");ok(d->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&i.vs));ok(d->CreateVertexShader(f->GetBufferPointer(),f->GetBufferSize(),nullptr,&i.fullVs));
 auto scanVertex=compile("scanVs","vs_4_0");ok(d->CreateVertexShader(scanVertex->GetBufferPointer(),scanVertex->GetBufferSize(),nullptr,&i.scanVs));
 for(auto [entry,target]:std::array<std::pair<const char*,ID3D11PixelShader**>,3>{{{"maskPs",&i.maskPs},{"compositePs",&i.compositePs},{"scanPs",&i.scanPs}}}){auto p=compile(entry,"ps_4_0");ok(d->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,target));}
 D3D11_INPUT_ELEMENT_DESC element{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0};ok(d->CreateInputLayout(&element,1,v->GetBufferPointer(),v->GetBufferSize(),&i.layout));
 D3D11_INPUT_ELEMENT_DESC scanElements[]={element,{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,40,D3D11_INPUT_PER_VERTEX_DATA,0}};ok(d->CreateInputLayout(scanElements,3,scanVertex->GetBufferPointer(),scanVertex->GetBufferSize(),&i.scanLayout));
 D3D11_RASTERIZER_DESC r{};r.FillMode=D3D11_FILL_SOLID;r.CullMode=D3D11_CULL_NONE;r.DepthClipEnable=TRUE;ok(d->CreateRasterizerState(&r,&i.raster));
 D3D11_DEPTH_STENCIL_DESC z{};z.DepthEnable=TRUE;z.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;z.DepthFunc=D3D11_COMPARISON_GREATER;ok(d->CreateDepthStencilState(&z,&i.writeDepth));z.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;z.DepthFunc=D3D11_COMPARISON_GREATER_EQUAL;ok(d->CreateDepthStencilState(&z,&i.readDepth));z.DepthEnable=FALSE;ok(d->CreateDepthStencilState(&z,&i.noDepth));
 D3D11_BLEND_DESC b{};auto&rt=b.RenderTarget[0];rt.BlendEnable=TRUE;rt.SrcBlend=D3D11_BLEND_SRC_ALPHA;rt.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOp=D3D11_BLEND_OP_ADD;rt.SrcBlendAlpha=D3D11_BLEND_ONE;rt.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;rt.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;ok(d->CreateBlendState(&b,&i.alpha));rt.DestBlend=D3D11_BLEND_ONE;rt.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_RED|D3D11_COLOR_WRITE_ENABLE_GREEN|D3D11_COLOR_WRITE_ENABLE_BLUE;ok(d->CreateBlendState(&b,&i.additive));
}
Renderer::~Renderer()=default;
bool Renderer::doll(ID3D11DeviceContext*c,const PreparedCharacter&body,CharacterRenderer&surface,const WorldView&camera,Vec3 origin,float yaw,Gate gate){
 if(!eligible(gate)||!c||!camera_valid(camera)||!finite(origin)||!std::isfinite(yaw)||impl_->policy.dollOpacity==0)return false;
 auto mesh=doll_mesh(body);if(!mesh)return false;auto&i=*impl_;Constants data{};auto world=XMMatrixRotationY(yaw)*XMMatrixTranslation(origin[0],origin[1],origin[2]);XMStoreFloat4x4(&data.world,world);XMStoreFloat4x4(&data.wvp,world*vp(camera));data.color={i.policy.orange[0],i.policy.orange[1],i.policy.orange[2],i.policy.dollOpacity};
 i.common(c,surface.width_,surface.height_);i.upload(c,data);c->UpdateSubresource(i.vertices.Get(),0,nullptr,mesh->vertices.data(),0,0);c->UpdateSubresource(i.indices.Get(),0,nullptr,mesh->indices.data(),0,0);
 float clear[4]={};c->ClearRenderTargetView(i.target.Get(),clear);c->ClearDepthStencilView(i.depthView.Get(),D3D11_CLEAR_DEPTH,0,0);auto target=i.target.Get();c->OMSetRenderTargets(1,&target,i.depthView.Get());c->OMSetDepthStencilState(i.writeDepth.Get(),0);c->OMSetBlendState(nullptr,nullptr,0xffffffff);
 c->VSSetShader(i.vs.Get(),nullptr,0);c->PSSetShader(i.maskPs.Get(),nullptr,0);c->IASetInputLayout(i.layout.Get());auto vb=i.vertices.Get();UINT stride=sizeof(Vec3),offset=0;c->IASetVertexBuffers(0,1,&vb,&stride,&offset);c->IASetIndexBuffer(i.indices.Get(),DXGI_FORMAT_R32_UINT,0);c->DrawIndexed(UINT(mesh->indices.size()),0,0);
 target=surface.target_.Get();c->OMSetRenderTargets(1,&target,nullptr);c->OMSetDepthStencilState(i.noDepth.Get(),0);c->OMSetBlendState(i.alpha.Get(),nullptr,0xffffffff);c->VSSetShader(i.fullVs.Get(),nullptr,0);c->PSSetShader(i.compositePs.Get(),nullptr,0);c->IASetInputLayout(nullptr);auto mask=i.view.Get();c->PSSetShaderResources(0,1,&mask);c->Draw(3,0);i.finish(c);return true;
}
bool Renderer::scan(ID3D11DeviceContext*c,CharacterRenderer&stage,const WorldView&camera,Scan scan,Gate gate){
 if(!eligible(gate)||!c||!camera_valid(camera)||!finite(scan.origin)||!std::isfinite(scan.radius)||scan.radius<=0||scan.radius>500000||!std::isfinite(scan.width)||scan.width<=0||scan.width>100000||!std::isfinite(scan.opacity)||scan.opacity<=0||scan.opacity>1)return false;
 auto&i=*impl_;Constants data{};XMStoreFloat4x4(&data.world,XMMatrixIdentity());XMStoreFloat4x4(&data.wvp,vp(camera));data.color={i.policy.orange[0],i.policy.orange[1],i.policy.orange[2],1};data.scan={scan.radius,scan.width,scan.opacity,0};data.center={scan.origin[0],scan.origin[1],scan.origin[2],0};
 i.common(c,stage.width_,stage.height_);i.upload(c,data);auto target=stage.target_.Get();c->OMSetRenderTargets(1,&target,stage.depthView_.Get());c->OMSetDepthStencilState(i.readDepth.Get(),0);c->OMSetBlendState(i.additive.Get(),nullptr,0xffffffff);c->VSSetShader(i.scanVs.Get(),nullptr,0);c->PSSetShader(i.scanPs.Get(),nullptr,0);c->IASetInputLayout(i.scanLayout.Get());auto vb=stage.vertices_.Get();UINT stride=sizeof(ModelVertex),offset=0;c->IASetVertexBuffers(0,1,&vb,&stride,&offset);c->IASetIndexBuffer(stage.indices_.Get(),DXGI_FORMAT_R32_UINT,0);auto sampler=stage.sampler_.Get();c->PSSetSamplers(0,1,&sampler);for(const auto&p:stage.parts_){auto tex=stage.textures_[p.texture].Get();c->PSSetShaderResources(1,1,&tex);c->DrawIndexed(p.count,p.first,0);}ID3D11ShaderResourceView*nil=nullptr;c->PSSetShaderResources(1,1,&nil);i.finish(c);return true;
}
}
