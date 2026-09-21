#include "water_renderer.h"
#include "world_depth.h"
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <stdexcept>
namespace mgo2mt::water_visuals {
namespace {
using Microsoft::WRL::ComPtr;using namespace DirectX;
void ok(HRESULT value){if(FAILED(value))throw std::runtime_error("Water surface D3D11 failure");}
float linear_color(float value){return value<=.04045f?value/12.92f:std::pow((value+.055f)/1.055f,2.4f);}
bool finite(Vec3 p){return std::all_of(p.begin(),p.end(),[](float x){return std::isfinite(x)&&std::abs(x)<1e7f;});}
bool triangle(Vec3 a,Vec3 b,Vec3 c){
 const double x1=double(b[0])-a[0],y1=double(b[1])-a[1],z1=double(b[2])-a[2];
 const double x2=double(c[0])-a[0],y2=double(c[1])-a[1],z2=double(c[2])-a[2];
 const double x=y1*z2-z1*y2,y=z1*x2-x1*z2,z=x1*y2-y1*x2;return x*x+y*y+z*z>0;
}
bool valid(const Mesh& m){
 if(m.vertices.size()>maximum_triangles*3||m.indices.size()>maximum_triangles*3||m.indices.size()%3||m.vertices.empty()!=m.indices.empty())return false;
 for(auto p:m.vertices)if(!finite(p))return false;
 for(auto i:m.indices)if(i>=m.vertices.size())return false;
 for(size_t i=0;i<m.indices.size();i+=3)if(!triangle(m.vertices[m.indices[i]],m.vertices[m.indices[i+1]],m.vertices[m.indices[i+2]]))return false;
 return true;
}
struct Constants {XMFLOAT4X4 viewProjection;XMFLOAT4 color;};
static_assert(sizeof(Constants)==80);
}
Mesh mesh(const stage::Water* fields,const stage::WaterSurface* surfaces){
 if((fields&&fields->fields().size()>1024)||(surfaces&&surfaces->triangles().size()>4096))throw std::invalid_argument("Water surface source limit");
 Mesh result;std::set<std::array<Vec3,3>> seen;
 auto append=[&](Vec3 a,Vec3 b,Vec3 c){
  if(!finite(a)||!finite(b)||!finite(c)||!triangle(a,b,c))throw std::invalid_argument("Invalid water surface triangle");
  auto key=std::array{a,b,c};std::sort(key.begin(),key.end());if(!seen.insert(key).second)return;
  const auto base=uint32_t(result.vertices.size());result.vertices.insert(result.vertices.end(),{a,b,c});result.indices.insert(result.indices.end(),{base,base+1,base+2});
 };
 if(fields)for(const auto& f:fields->fields()){
  const float y=f.center[1]+f.halfSize[1],x0=f.center[0]-f.halfSize[0],x1=f.center[0]+f.halfSize[0],z0=f.center[2]-f.halfSize[2],z1=f.center[2]+f.halfSize[2];
  const Vec3 a{x0,y,z0},b{x1,y,z0},c{x1,y,z1},d{x0,y,z1};append(a,b,c);append(a,c,d);
 }
 if(surfaces)for(const auto& t:surfaces->triangles())append(t.vertices[0],t.vertices[1],t.vertices[2]);
 if(!valid(result))throw std::invalid_argument("Invalid water surface mesh");return result;
}
struct Renderer::Impl {
 ComPtr<ID3D11Buffer> vertices,indices,constants;ComPtr<ID3D11VertexShader> vertexShader;ComPtr<ID3D11PixelShader> pixelShader;
 ComPtr<ID3D11InputLayout> layout;ComPtr<ID3D11RasterizerState> raster;ComPtr<ID3D11DepthStencilState> depth;ComPtr<ID3D11BlendState> blend;
 Policy policy;UINT count=0;
};
Renderer::Renderer(ID3D11Device* device,const Mesh& data,Policy policy):impl_(std::make_unique<Impl>()){
 if(!device||!valid(data)||!std::isfinite(policy.gray)||policy.gray<0||policy.gray>1||!std::isfinite(policy.opacity)||policy.opacity<0||policy.opacity>1)throw std::invalid_argument("Invalid water surface renderer");
 auto& r=*impl_;r.policy=policy;r.count=UINT(data.indices.size());if(!r.count)return;
 auto buffer=[&](UINT bytes,UINT bind,const void* source,ID3D11Buffer** target){D3D11_BUFFER_DESC b{};b.ByteWidth=bytes;b.BindFlags=bind;b.Usage=source?D3D11_USAGE_IMMUTABLE:D3D11_USAGE_DEFAULT;D3D11_SUBRESOURCE_DATA s{source,0,0};ok(device->CreateBuffer(&b,source?&s:nullptr,target));};
 buffer(UINT(data.vertices.size()*sizeof(Vec3)),D3D11_BIND_VERTEX_BUFFER,data.vertices.data(),&r.vertices);buffer(UINT(data.indices.size()*4),D3D11_BIND_INDEX_BUFFER,data.indices.data(),&r.indices);buffer(sizeof(Constants),D3D11_BIND_CONSTANT_BUFFER,nullptr,&r.constants);
 const char* shader=R"(cbuffer Frame:register(b0){row_major float4x4 vp;float4 color;};float4 vs(float3 position:POSITION):SV_POSITION{return mul(float4(position,1),vp);}float4 ps():SV_TARGET{return color;})";
 auto compile=[&](const char* entry,const char* profile){ComPtr<ID3DBlob> code,error;ok(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,entry,profile,D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&error));return code;};
 auto vs=compile("vs","vs_4_0"),ps=compile("ps","ps_4_0");ok(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&r.vertexShader));ok(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&r.pixelShader));
 D3D11_INPUT_ELEMENT_DESC input{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0};ok(device->CreateInputLayout(&input,1,vs->GetBufferPointer(),vs->GetBufferSize(),&r.layout));
 D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;ok(device->CreateRasterizerState(&raster,&r.raster));
 D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthEnable=TRUE;depth.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;depth.DepthFunc=D3D11_COMPARISON_GREATER_EQUAL;ok(device->CreateDepthStencilState(&depth,&r.depth));
 D3D11_BLEND_DESC blend{};auto& target=blend.RenderTarget[0];target.BlendEnable=TRUE;target.SrcBlend=D3D11_BLEND_SRC_ALPHA;target.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;target.BlendOp=D3D11_BLEND_OP_ADD;target.SrcBlendAlpha=D3D11_BLEND_ONE;target.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;target.BlendOpAlpha=D3D11_BLEND_OP_ADD;target.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;ok(device->CreateBlendState(&blend,&r.blend));
}
Renderer::~Renderer()=default;
bool Renderer::render(ID3D11DeviceContext* context,CharacterRenderer& surface,const WorldView& camera)const{
 if(!context||!impl_->count||impl_->policy.opacity==0||!finite(camera.eye)||!finite(camera.direction)||!std::isfinite(camera.aspect)||camera.aspect<=0||camera.aspect>32||!valid_vertical_fov(camera.verticalFov)||camera.direction[0]*camera.direction[0]+camera.direction[2]*camera.direction[2]<=.00001f)return false;
 auto& r=*impl_;Constants data{};const auto eye=XMVectorSet(camera.eye[0],camera.eye[1],camera.eye[2],1),direction=XMVectorSet(camera.direction[0],camera.direction[1],camera.direction[2],0);
 // Policy colors are display/sRGB settings. Float16 world targets blend in
 // linear light; the ordinary target retains the original exact constants.
 const float gray=surface.hdr()?linear_color(r.policy.gray):r.policy.gray;
 XMStoreFloat4x4(&data.viewProjection,XMMatrixLookToLH(eye,direction,XMVectorSet(0,1,0,0))*world_projection(camera.aspect,camera.verticalFov));data.color={gray,gray,gray,r.policy.opacity};
 ID3D11ShaderResourceView* nil=nullptr;context->PSSetShaderResources(0,1,&nil);auto target=surface.target_.Get();context->OMSetRenderTargets(1,&target,surface.depthView_.Get());context->OMSetDepthStencilState(r.depth.Get(),0);context->OMSetBlendState(r.blend.Get(),nullptr,0xffffffff);
 D3D11_VIEWPORT viewport{0,0,float(surface.width_),float(surface.height_),0,1};context->RSSetViewports(1,&viewport);context->RSSetState(r.raster.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->IASetInputLayout(r.layout.Get());
 auto vertices=r.vertices.Get();UINT stride=sizeof(Vec3),offset=0;context->IASetVertexBuffers(0,1,&vertices,&stride,&offset);context->IASetIndexBuffer(r.indices.Get(),DXGI_FORMAT_R32_UINT,0);context->VSSetShader(r.vertexShader.Get(),nullptr,0);context->PSSetShader(r.pixelShader.Get(),nullptr,0);
 context->UpdateSubresource(r.constants.Get(),0,nullptr,&data,0,0);auto constants=r.constants.Get();context->VSSetConstantBuffers(0,1,&constants);context->PSSetConstantBuffers(0,1,&constants);context->DrawIndexed(r.count,0,0);
 context->OMSetRenderTargets(0,nullptr,nullptr);context->OMSetBlendState(nullptr,nullptr,0xffffffff);return true;
}
}

