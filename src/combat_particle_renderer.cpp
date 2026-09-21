#include "combat_particle_renderer.h"
#include "world_depth.h"
#include "source_coordinates.h"
#include "multi_ui.h"
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
namespace mgo2mt::combat::particles {
using Microsoft::WRL::ComPtr;using namespace DirectX;
namespace {
void check(HRESULT h){if(FAILED(h))throw std::runtime_error("Original effect renderer D3D11 failure");}
struct Vertex {XMFLOAT4 clip,color;XMFLOAT2 uv,local;float fadeDistance=250;};
bool valid(Vec3 p){for(float x:p)if(!std::isfinite(x)||std::abs(x)>1000000)return false;return true;}
}
struct Renderer::Impl {struct Image {unsigned width,height,codec;std::vector<unsigned char> bytes;bool operator==(const Image&)const=default;};ComPtr<ID3D11Device> device;ComPtr<ID3D11Texture2D> depthCopy;ComPtr<ID3D11ShaderResourceView> depthCopyView;unsigned depthWidth=0,depthHeight=0;ComPtr<ID3D11Buffer> vertices,constants;ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;ComPtr<ID3D11InputLayout> layout;ComPtr<ID3D11DepthStencilState> depth;ComPtr<ID3D11BlendState> blend,add;ComPtr<ID3D11RasterizerState> raster;ComPtr<ID3D11SamplerState> sampler;std::map<uint32_t,ComPtr<ID3D11ShaderResourceView>> textures;std::map<uint32_t,Image> images;};
void Renderer::add_bundle(const std::filesystem::path&path){
 auto&r=*impl_;auto*d=r.device.Get();
 std::ifstream file(path,std::ios::binary|std::ios::ate);auto n=file.tellg();if(n<12||n>16*1024*1024)throw std::runtime_error("Original effect bundle extent");std::vector<unsigned char> data(static_cast<size_t>(n));file.seekg(0);if(!file.read(reinterpret_cast<char*>(data.data()),n))throw std::runtime_error("Original effect bundle read");size_t off=4;
 auto u32=[&](){if(data.size()-off<4)throw std::runtime_error("Effect truncated");uint32_t v;std::memcpy(&v,data.data()+off,4);off+=4;return v;};
 if(std::memcmp(data.data(),"GWFX",4)||u32()!=1)throw std::runtime_error("Original effect bundle format");auto count=u32();if(!count||count>128)throw std::runtime_error("Original effect texture count");
 std::map<uint32_t,Impl::Image> images;std::map<uint32_t,ComPtr<ID3D11ShaderResourceView>> textures;size_t total=0;for(const auto&[key,image]:r.images)total+=image.bytes.size();
 for(uint32_t i=0;i<count;++i){auto key=u32(),w=u32(),h=u32(),codec=u32(),size=u32();if(!key||images.contains(key)||!w||!h||w>2048||h>2048||w%4||h%4||(codec!=9&&codec!=11)||size!=w/4*(h/4)*(codec==9?8:16)||size>data.size()-off)throw std::runtime_error("Original effect texture invalid");
  Impl::Image image{w,h,codec,{data.begin()+off,data.begin()+off+size}};images.emplace(key,image);
  if(auto found=r.images.find(key);found!=r.images.end()){if(found->second!=image)throw std::runtime_error("Original effect shared texture conflict");off+=size;continue;}
  total+=size;if(r.images.size()+textures.size()>=128||total>16*1024*1024)throw std::runtime_error("Original effect aggregate extent");
  D3D11_TEXTURE2D_DESC td{};td.Width=w;td.Height=h;td.MipLevels=td.ArraySize=1;td.Format=codec==9?DXGI_FORMAT_BC1_UNORM:DXGI_FORMAT_BC3_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA initial{data.data()+off,w/4*(codec==9?8u:16u),size};ComPtr<ID3D11Texture2D> texture;check(d->CreateTexture2D(&td,&initial,&texture));check(d->CreateShaderResourceView(texture.Get(),nullptr,&textures[key]));off+=size;
 }if(off!=data.size())throw std::runtime_error("Original effect bundle trailing bytes");
 r.images.merge(images);r.textures.merge(textures);
}
void Renderer::add_textures(const weapon_effect::Config&config,const std::filesystem::path&dataRoot){
 auto&r=*impl_;std::map<uint32_t,Impl::Image>images;std::map<uint32_t,ComPtr<ID3D11ShaderResourceView>>textures;size_t total=0;for(const auto&[key,image]:r.images)total+=image.bytes.size();const auto root=std::filesystem::canonical(dataRoot);
 for(const auto&[path,key]:config.textures()){
  const auto resolved=std::filesystem::canonical(root/std::filesystem::u8path(path));auto rel=resolved.lexically_relative(root);if(rel.empty()||rel.is_absolute()||*rel.begin()=="..")throw std::runtime_error("Effect texture escapes data root");
  auto decoded=multi_ui::decode_image(resolved);if(decoded.width>2048||decoded.height>2048)throw std::runtime_error("Effect image exceeds 2048 pixels");Impl::Image image{decoded.width,decoded.height,0,std::move(decoded.rgba)};
  if(auto i=r.images.find(key);i!=r.images.end()){if(i->second!=image)throw std::runtime_error("Effect imported texture key conflict");continue;}
  total+=image.bytes.size();if(r.images.size()+textures.size()>=128||total>64*1024*1024)throw std::runtime_error("Effect imported texture aggregate extent");
  D3D11_TEXTURE2D_DESC td{};td.Width=image.width;td.Height=image.height;td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA initial{image.bytes.data(),image.width*4,UINT(image.bytes.size())};ComPtr<ID3D11Texture2D>texture;check(r.device->CreateTexture2D(&td,&initial,&texture));check(r.device->CreateShaderResourceView(texture.Get(),nullptr,&textures[key]));images.emplace(key,std::move(image));
 }r.images.merge(images);r.textures.merge(textures);
}
Renderer::Renderer(ID3D11Device*d,const std::filesystem::path&path):impl_(std::make_unique<Impl>()){
 if(!d)throw std::invalid_argument("Effect device");auto&r=*impl_;r.device=d;add_bundle(path);
 D3D11_BUFFER_DESC b{};b.ByteWidth=UINT(16512*6*sizeof(Vertex));b.Usage=D3D11_USAGE_DYNAMIC;b.BindFlags=D3D11_BIND_VERTEX_BUFFER;b.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;check(d->CreateBuffer(&b,nullptr,&r.vertices));
 b={};b.ByteWidth=16;b.Usage=D3D11_USAGE_DEFAULT;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;check(d->CreateBuffer(&b,nullptr,&r.constants));
 const char* shader=R"(
 Texture2D tex:register(t0);Texture2D<float> worldDepth:register(t1);SamplerState linearClamp:register(s0);
 cbuffer Frame:register(b0){float softEnabled;float linearTarget;float nearPlane;float farPlane;};
 struct V{float4 p:POSITION;float4 c:COLOR;float2 uv:TEXCOORD0;float2 local:TEXCOORD1;float fade:TEXCOORD2;};
 struct O{float4 p:SV_POSITION;float4 c:COLOR;float2 uv:TEXCOORD0;float2 local:TEXCOORD1;float fade:TEXCOORD2;};
 O vs(V v){O o;o.p=v.p;o.c=v.c;o.uv=v.uv;o.local=v.local;o.fade=v.fade;return o;}
 float3 decodeColor(float3 v){return float3(v.x<=.04045?v.x/12.92:pow((v.x+.055)/1.055,2.4),v.y<=.04045?v.y/12.92:pow((v.y+.055)/1.055,2.4),v.z<=.04045?v.z/12.92:pow((v.z+.055)/1.055,2.4));}
 float distanceAt(float d){return nearPlane*farPlane/(nearPlane+saturate(d)*(farPlane-nearPlane));}
 float4 ps(O v):SV_TARGET{
  float4 color=tex.Sample(linearClamp,v.uv)*v.c;if(linearTarget>.5)color.rgb=decodeColor(color.rgb);
  if(softEnabled>.5){float scene=distanceAt(worldDepth.Load(int3(int2(v.p.xy),0))),particle=distanceAt(v.p.z);
   float fade=saturate((scene-particle)/v.fade)*saturate((particle-nearPlane)/v.fade);
   float2 edge=min(v.local,1-v.local);fade*=smoothstep(0,.06,min(edge.x,edge.y));color.a*=fade;}
  return color;
 })";
 auto compile=[&](const char*entry,const char*profile){ComPtr<ID3DBlob> code,error;check(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,entry,profile,D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&error));return code;};
 auto vs=compile("vs","vs_4_0"),ps=compile("ps","ps_4_0");check(d->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&r.vs));check(d->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&r.ps));
 D3D11_INPUT_ELEMENT_DESC input[]={{"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,40,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",2,DXGI_FORMAT_R32_FLOAT,0,48,D3D11_INPUT_PER_VERTEX_DATA,0}};check(d->CreateInputLayout(input,5,vs->GetBufferPointer(),vs->GetBufferSize(),&r.layout));
 D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthEnable=TRUE;depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;depth.DepthFunc=D3D11_COMPARISON_GREATER_EQUAL;check(d->CreateDepthStencilState(&depth,&r.depth));
 D3D11_BLEND_DESC blend{};auto&t=blend.RenderTarget[0];t.BlendEnable=TRUE;t.SrcBlend=D3D11_BLEND_SRC_ALPHA;t.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;t.BlendOp=D3D11_BLEND_OP_ADD;t.SrcBlendAlpha=D3D11_BLEND_ONE;t.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;t.BlendOpAlpha=D3D11_BLEND_OP_ADD;t.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;check(d->CreateBlendState(&blend,&r.blend));t.DestBlend=D3D11_BLEND_ONE;check(d->CreateBlendState(&blend,&r.add));
 D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;check(d->CreateRasterizerState(&raster,&r.raster));D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sampler.MaxLOD=0;check(d->CreateSamplerState(&sampler,&r.sampler));
}
Renderer::~Renderer()=default;
size_t Renderer::textures()const noexcept{return impl_->textures.size();}
bool Renderer::render(ID3D11DeviceContext*c,CharacterRenderer&surface,const WorldView&camera,std::span<const Sprite>sprites){
 if(!c||sprites.empty()||sprites.size()>16512||!valid(camera.eye)||!valid(camera.direction)||!std::isfinite(camera.aspect)||camera.aspect<=0||camera.aspect>32||!valid_vertical_fov(camera.verticalFov))return false;
 const auto eye=XMVectorSet(camera.eye[0],camera.eye[1],camera.eye[2],1),raw=XMVectorSet(camera.direction[0],camera.direction[1],camera.direction[2],0);if(XMVectorGetX(XMVector3LengthSq(raw))<1e-8f)return false;
 const auto forward=XMVector3Normalize(raw),rawUp=std::abs(XMVectorGetY(forward))>.999f?XMVectorSet(0,0,1,0):XMVectorSet(0,1,0,0);const auto sourceRight=XMVector3Normalize(XMVector3Cross(rawUp,forward)),right=sourceRight*source_screen_x,up=XMVector3Cross(forward,sourceRight);const auto vp=XMMatrixLookToLH(eye,forward,rawUp)*world_projection(camera.aspect,camera.verticalFov);auto&r=*impl_;
 struct Draw{uint32_t key;bool add;float z;std::array<Vertex,6> vertices;};std::vector<Draw> draws;draws.reserve(sprites.size());
 for(const auto&s:sprites){if(!std::all_of(s.stretch.begin(),s.stretch.end(),[](float v){return std::isfinite(v)&&v>0&&v<=32;})||!valid(s.position)||!std::isfinite(s.radius)||s.radius<=0||s.radius>20000||!std::isfinite(s.rotation)||!r.textures.contains(s.texture)||!std::all_of(s.rgba.begin(),s.rgba.end(),[](float v){return std::isfinite(v)&&v>=0&&v<=1;})||!std::all_of(s.uv.begin(),s.uv.end(),[](float v){return std::isfinite(v)&&v>=0&&v<=1;}))continue;
  auto center=XMVectorSet(s.position[0],s.position[1],s.position[2],1);float z=XMVectorGetX(XMVector3Dot(center-eye,forward));if(z<world_near_plane)continue;auto x=(right*std::cos(s.rotation)+up*std::sin(s.rotation))*s.radius*s.stretch[0],y=(up*std::cos(s.rotation)-right*std::sin(s.rotation))*s.radius*s.stretch[1];Vertex q[4];for(unsigned i=0;i<4;++i){XMStoreFloat4(&q[i].clip,XMVector4Transform(center+x*(i&1?1.f:-1.f)+y*(i&2?-1.f:1.f),vp));q[i].color={s.rgba[0],s.rgba[1],s.rgba[2],s.rgba[3]};q[i].uv={s.uv[i&1?2:0],s.uv[i&2?3:1]};q[i].local={i&1?1.f:0.f,i&2?1.f:0.f};q[i].fadeDistance=std::clamp(s.radius*.35f,80.f,1200.f);}Draw draw{s.texture,s.additive,z,{}};unsigned at=0;for(int i:{0,1,2,2,1,3})draw.vertices[at++]=q[i];draws.push_back(draw);
 }
 if(draws.empty())return false;std::stable_sort(draws.begin(),draws.end(),[](const Draw&a,const Draw&b){return a.z>b.z;});D3D11_MAPPED_SUBRESOURCE mapped{};check(c->Map(r.vertices.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped));for(size_t i=0;i<draws.size();++i)std::memcpy(static_cast<Vertex*>(mapped.pData)+6*i,draws[i].vertices.data(),6*sizeof(Vertex));c->Unmap(r.vertices.Get(),0);
 ComPtr<ID3D11Device> device;c->GetDevice(&device);const bool soft=render_backend::Device(device.Get()).options().softParticles;ID3D11ShaderResourceView* depthResource=soft?surface.depthResource_.Get():nullptr;
 if(soft&&!surface.readOnlyDepthView_){
  if(r.depthWidth!=surface.width_||r.depthHeight!=surface.height_){D3D11_TEXTURE2D_DESC desc{};surface.depth_->GetDesc(&desc);desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;r.depthCopyView.Reset();r.depthCopy.Reset();check(device->CreateTexture2D(&desc,nullptr,&r.depthCopy));D3D11_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R32_FLOAT;srv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;srv.Texture2D.MipLevels=1;check(device->CreateShaderResourceView(r.depthCopy.Get(),&srv,&r.depthCopyView));r.depthWidth=surface.width_;r.depthHeight=surface.height_;}
  c->OMSetRenderTargets(0,nullptr,nullptr);c->CopyResource(r.depthCopy.Get(),surface.depth_.Get());depthResource=r.depthCopyView.Get();
 }
 auto target=surface.target_.Get();c->OMSetRenderTargets(1,&target,soft&&surface.readOnlyDepthView_?surface.readOnlyDepthView_.Get():surface.depthView_.Get());c->PSSetShaderResources(1,1,&depthResource);c->OMSetDepthStencilState(r.depth.Get(),0);D3D11_VIEWPORT viewport{0,0,float(surface.width_),float(surface.height_),0,1};c->RSSetViewports(1,&viewport);c->RSSetState(r.raster.Get());c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);c->IASetInputLayout(r.layout.Get());auto vb=r.vertices.Get();UINT stride=sizeof(Vertex),offset=0;c->IASetVertexBuffers(0,1,&vb,&stride,&offset);c->IASetIndexBuffer(nullptr,DXGI_FORMAT_R32_UINT,0);c->VSSetShader(r.vs.Get(),nullptr,0);c->PSSetShader(r.ps.Get(),nullptr,0);auto sampler=r.sampler.Get();c->PSSetSamplers(0,1,&sampler);
 for(size_t i=0;i<draws.size();){size_t end=i+1;while(end<draws.size()&&draws[end].key==draws[i].key&&draws[end].add==draws[i].add)++end;const float settings[]={soft&&!draws[i].add?1.f:0.f,surface.hdr()?1.f:0.f,world_near_plane,world_far_plane};c->UpdateSubresource(r.constants.Get(),0,nullptr,settings,0,0);auto cb=r.constants.Get();c->PSSetConstantBuffers(0,1,&cb);auto srv=r.textures.at(draws[i].key).Get();c->PSSetShaderResources(0,1,&srv);c->OMSetBlendState(draws[i].add?r.add.Get():r.blend.Get(),nullptr,~0u);c->Draw(UINT((end-i)*6),UINT(i*6));i=end;}
 ID3D11ShaderResourceView*nil[2]={};c->PSSetShaderResources(0,2,nil);c->OMSetRenderTargets(0,nullptr,nullptr);c->OMSetBlendState(nullptr,nullptr,~0u);return true;
}
}
