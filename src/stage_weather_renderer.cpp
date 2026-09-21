#include "stage_weather_renderer.h"
#include "combat_particle_renderer.h"
#include "world_depth.h"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstring>
#include <stdexcept>
#include <cmath>
namespace mgo2mt::stage::weather {
using Microsoft::WRL::ComPtr;
namespace {void check(HRESULT h){if(FAILED(h))throw std::runtime_error("Weather renderer D3D11 failure");}}
struct Renderer::Impl {
 combat::particles::Renderer dust;
 ComPtr<ID3D11Texture2D> depthCopy;ComPtr<ID3D11ShaderResourceView> depthView;
 ComPtr<ID3D11Texture2D> colorCopy,heightGrid;ComPtr<ID3D11ShaderResourceView> colorView,heightView;
 ComPtr<ID3D11Buffer> constants;ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;
 ComPtr<ID3D11RasterizerState> raster;ComPtr<ID3D11DepthStencilState> depth;ComPtr<ID3D11BlendState> blend;
 unsigned width=0,height=0;bool hdr=false;
 Impl(ID3D11Device*d,const std::filesystem::path&p):dust(d,p){}
};
Renderer::Renderer(ID3D11Device*d,const std::filesystem::path&p):impl_(std::make_unique<Impl>(d,p)){
 auto&r=*impl_;const char*shader=R"(
 Texture2D<float> worldDepth:register(t0);
 Texture2D<float4> sceneColor:register(t1);Texture2D<float4> heights:register(t2);
 cbuffer Weather:register(b0){float4 fog;float4 color;float4 lens;float4 grid;float4 eye;row_major float4x4 inverseViewProjection;};
 struct V{float4 p:SV_POSITION;};
 V vs(uint id:SV_VertexID){V o;o.p=float4(id==1?3:-1,id==2?-3:1,0,1);return o;}
 // Setting colors are sRGB; sampled HDR sceneColor is already linear.
 float3 settingColor(float3 value){
  if(eye.w<.5)return value;
  value=max(value,0);
  return float3(value.r<=.04045?value.r/12.92:pow((value.r+.055)/1.055,2.4),value.g<=.04045?value.g/12.92:pow((value.g+.055)/1.055,2.4),value.b<=.04045?value.b/12.92:pow((value.b+.055)/1.055,2.4));
 }
 float3 world(float2 screen,float depth){float4 p=mul(float4(screen.x/lens.z*2-1,1-screen.y/lens.w*2,depth,1),inverseViewProjection);return p.xyz/p.w;}
 float4 ps(V v):SV_TARGET{
  float d=worldDepth.Load(int3(int2(v.p.xy),0));
  float z=lens.x*lens.y/(lens.x+saturate(d)*(lens.y-lens.x));
  float amount=d<=0?fog.w:saturate((z-fog.x)/(fog.y-fog.x))*fog.z;
  float3 position=world(v.p.xy,d);
  float3 surfaceNormal=cross(ddx(position),ddy(position));surfaceNormal/=max(length(surfaceNormal),.0001);
  if(dot(surfaceNormal,eye.xyz-position)<0)surfaceNormal=-surfaceNormal;
  if(grid.w>=2&&d>0&&surfaceNormal.y>.35){
   float2 uv=(position.xz-grid.xy)/grid.z;int2 cell=int2(floor(uv));
   if(all(cell>=0)&&all(cell<int(grid.w)-1)){
    float4 a=heights.Load(int3(cell,0)),b=heights.Load(int3(cell+int2(1,0),0)),c=heights.Load(int3(cell+int2(0,1),0)),e=heights.Load(int3(cell+int2(1,1),0));
    float low=min(min(a.x,b.x),min(c.x,e.x)),high=max(max(a.x,b.x),max(c.x,e.x));
    float height=lerp(lerp(a.x,b.x,frac(uv.x)),lerp(c.x,e.x,frac(uv.x)),frac(uv.y));
    if(min(min(a.y,b.y),min(c.y,e.y))>.35&&high-low<=grid.z&&abs(position.y-height)<100){
     float coverage=saturate((surfaceNormal.y-.35)/.45);float wet=min(min(a.z,b.z),min(c.z,e.z))*coverage,snow=min(min(a.w,b.w),min(c.w,e.w))*coverage;
     float3 original=sceneColor.Load(int3(int2(v.p.xy),0)).rgb;
     // Native mathematical appearance, not an original rain/snow material shader.
     float sheen=pow(saturate(dot(reflect(-normalize(eye.xyz-position),surfaceNormal),normalize(float3(.2,1,.3)))),24)*wet;
     float3 result=original*(1-.28*wet)+settingColor(float3(.07,.09,.11))*sheen;
     float illumination=clamp(dot(original,float3(.2126,.7152,.0722))*1.5,.22,1);
     result=lerp(result,settingColor(float3(.84,.88,.94))*illumination,snow*.92);
     return float4(lerp(result,settingColor(color.rgb),saturate(amount)),1);
    }
   }
  }
  return float4(settingColor(color.rgb),saturate(amount));
 })";
 auto compile=[&](const char*entry,const char*profile){ComPtr<ID3DBlob>code,error;check(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,entry,profile,D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&error));return code;};
 auto vs=compile("vs","vs_4_0"),ps=compile("ps","ps_4_0");check(d->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&r.vs));check(d->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&r.ps));
 D3D11_BUFFER_DESC b{};b.ByteWidth=144;b.Usage=D3D11_USAGE_DEFAULT;b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;check(d->CreateBuffer(&b,nullptr,&r.constants));
 D3D11_TEXTURE2D_DESC grid{};grid.Width=grid.Height=64;grid.MipLevels=grid.ArraySize=1;grid.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;grid.SampleDesc.Count=1;grid.Usage=D3D11_USAGE_DEFAULT;grid.BindFlags=D3D11_BIND_SHADER_RESOURCE;check(d->CreateTexture2D(&grid,nullptr,&r.heightGrid));check(d->CreateShaderResourceView(r.heightGrid.Get(),nullptr,&r.heightView));
 D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;check(d->CreateRasterizerState(&raster,&r.raster));
 D3D11_DEPTH_STENCIL_DESC depth{};depth.DepthEnable=FALSE;check(d->CreateDepthStencilState(&depth,&r.depth));
 D3D11_BLEND_DESC blend{};auto&t=blend.RenderTarget[0];t.BlendEnable=TRUE;t.SrcBlend=D3D11_BLEND_SRC_ALPHA;t.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;t.BlendOp=D3D11_BLEND_OP_ADD;t.SrcBlendAlpha=D3D11_BLEND_ZERO;t.DestBlendAlpha=D3D11_BLEND_ONE;t.BlendOpAlpha=D3D11_BLEND_OP_ADD;t.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;check(d->CreateBlendState(&blend,&r.blend));
}
Renderer::~Renderer()=default;
bool Renderer::render(ID3D11DeviceContext*c,CharacterRenderer&surface,const WorldView&camera,const Frame&f){
 if(!c||(!(f.active&&f.strength>0)&&!f.surface))return false;auto&r=*impl_;
 if(!valid_vertical_fov(camera.verticalFov)||!std::isfinite(camera.aspect)||camera.aspect<=0)return false;
 if(r.width!=surface.width_||r.height!=surface.height_||r.hdr!=surface.hdr()){
  ComPtr<ID3D11Device>d;c->GetDevice(&d);D3D11_TEXTURE2D_DESC desc{};surface.depth_->GetDesc(&desc);if((desc.Format!=DXGI_FORMAT_D32_FLOAT&&desc.Format!=DXGI_FORMAT_R32_TYPELESS)||desc.SampleDesc.Count!=1)throw std::runtime_error("Weather depth format");
  desc.Format=DXGI_FORMAT_R32_TYPELESS;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=desc.MiscFlags=0;
  r.depthView.Reset();r.depthCopy.Reset();check(d->CreateTexture2D(&desc,nullptr,&r.depthCopy));D3D11_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R32_FLOAT;srv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;srv.Texture2D.MipLevels=1;check(d->CreateShaderResourceView(r.depthCopy.Get(),&srv,&r.depthView));r.width=surface.width_;r.height=surface.height_;r.hdr=surface.hdr();
  surface.color_->GetDesc(&desc);desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=desc.MiscFlags=0;r.colorView.Reset();r.colorCopy.Reset();check(d->CreateTexture2D(&desc,nullptr,&r.colorCopy));check(d->CreateShaderResourceView(r.colorCopy.Get(),nullptr,&r.colorView));
 }
 c->OMSetRenderTargets(0,nullptr,nullptr);c->CopyResource(r.depthCopy.Get(),surface.depth_.Get());
 float values[36]={f.nearDistance,f.farDistance,f.active?f.maximum*f.strength:0,f.active?f.skyAmount*f.strength:0,f.color[0],f.color[1],f.color[2],1,10,500000,float(r.width),float(r.height)};
 values[16]=camera.eye[0];values[17]=camera.eye[1];values[18]=camera.eye[2];values[19]=surface.hdr()?1.f:0.f;
 using namespace DirectX;auto view=XMMatrixLookToLH(XMVectorSet(camera.eye[0],camera.eye[1],camera.eye[2],1),XMVectorSet(camera.direction[0],camera.direction[1],camera.direction[2],0),XMVectorSet(0,1,0,0));XMFLOAT4X4 inverse;XMStoreFloat4x4(&inverse,XMMatrixInverse(nullptr,view*world_projection(camera.aspect,camera.verticalFov)));std::memcpy(values+20,&inverse,64);
 if(f.surface){auto&g=*f.surface;if(g.width!=64||g.cells.size()!=4096||!std::isfinite(g.cellSize)||g.cellSize<=0)throw std::runtime_error("Weather surface grid extent");static_assert(sizeof(SurfaceCell)==16);values[12]=g.originX;values[13]=g.originZ;values[14]=g.cellSize;values[15]=float(g.width);c->UpdateSubresource(r.heightGrid.Get(),0,nullptr,g.cells.data(),g.width*sizeof(SurfaceCell),0);c->CopyResource(r.colorCopy.Get(),surface.color_.Get());}
 c->UpdateSubresource(r.constants.Get(),0,nullptr,values,0,0);auto target=surface.target_.Get();c->OMSetRenderTargets(1,&target,nullptr);c->OMSetDepthStencilState(r.depth.Get(),0);c->OMSetBlendState(r.blend.Get(),nullptr,~0u);
 D3D11_VIEWPORT viewport{0,0,float(surface.width_),float(surface.height_),0,1};c->RSSetViewports(1,&viewport);c->RSSetState(r.raster.Get());c->IASetInputLayout(nullptr);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);c->VSSetShader(r.vs.Get(),nullptr,0);c->PSSetShader(r.ps.Get(),nullptr,0);auto cb=r.constants.Get();c->PSSetConstantBuffers(0,1,&cb);ID3D11ShaderResourceView*srvs[]={r.depthView.Get(),r.colorView.Get(),r.heightView.Get()};c->PSSetShaderResources(0,3,srvs);c->Draw(3,0);
 ID3D11ShaderResourceView*nil[3]={};c->PSSetShaderResources(0,3,nil);c->OMSetRenderTargets(0,nullptr,nullptr);c->OMSetBlendState(nullptr,nullptr,~0u);
 if(!f.dust.empty())r.dust.render(c,surface,camera,f.dust);return true;
}
}
