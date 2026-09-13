#include "character_renderer.h"
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <cmath>
namespace mgo2win {
static void ok(HRESULT h){if(FAILED(h))throw std::runtime_error("Character D3D11 resource failure");}
CharacterRenderer::CharacterRenderer(ID3D11Device*d,const CharacterModel&m):parts_(m.parts),bounds_(m.bounds),vertexCount_(m.vertices.size()){
 overviewBounds_=m.overviewBounds;hasOverviewBounds_=m.hasOverviewBounds;
 D3D11_TEXTURE2D_DESC t{};t.Width=616;t.Height=392;t.MipLevels=1;t.ArraySize=1;t.Format=DXGI_FORMAT_R8G8B8A8_UNORM;t.SampleDesc.Count=1;t.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
 ok(d->CreateTexture2D(&t,nullptr,&color_));ok(d->CreateRenderTargetView(color_.Get(),nullptr,&target_));ok(d->CreateShaderResourceView(color_.Get(),nullptr,&view_));
 t.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;t.BindFlags=D3D11_BIND_DEPTH_STENCIL;ok(d->CreateTexture2D(&t,nullptr,&depth_));ok(d->CreateDepthStencilView(depth_.Get(),nullptr,&depthView_));
 auto buffer=[&](UINT size,UINT bind,const void* data,ID3D11Buffer**out){D3D11_BUFFER_DESC b{};b.ByteWidth=size;b.BindFlags=bind;b.Usage=bind==D3D11_BIND_VERTEX_BUFFER?D3D11_USAGE_DEFAULT:(data?D3D11_USAGE_IMMUTABLE:D3D11_USAGE_DEFAULT);D3D11_SUBRESOURCE_DATA s{};s.pSysMem=data;ok(d->CreateBuffer(&b,data?&s:nullptr,out));};
 buffer(UINT(m.vertices.size()*sizeof(ModelVertex)),D3D11_BIND_VERTEX_BUFFER,m.vertices.data(),&vertices_);buffer(UINT(m.indices.size()*4),D3D11_BIND_INDEX_BUFFER,m.indices.data(),&indices_);buffer(432,D3D11_BIND_CONSTANT_BUFFER,nullptr,&constants_);
 const char*shader=R"(
 struct PointLight{float4 positionRadius;float4 colorIntensity;};
 cbuffer Camera:register(b0){row_major float4x4 wvp;row_major float4x4 world;float4 material;float4 tint;float4 lightCount;PointLight pointLights[8];};
 struct V{float3 p:POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;float2 uv1:TEXCOORD1;float4 light:COLOR0;float4 authored:COLOR1;};
 struct P{float4 p:SV_POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;float2 uv1:TEXCOORD1;float3 worldPosition:TEXCOORD2;float4 light:COLOR0;float4 authored:COLOR1;};
 P vs(V v){P o;o.p=mul(float4(v.p,1),wvp);o.n=mul(float4(v.n,0),world).xyz;o.worldPosition=mul(float4(v.p,1),world).xyz;o.uv=v.uv;o.uv1=v.uv1;o.light=v.light;o.authored=v.authored;return o;}
 Texture2D tex:register(t0);Texture2D pattern:register(t1);SamplerState sampleLinear:register(s0);
 // Material 0x120000: base FP714..717 / patch FP731..734 add COLOR0.rgb*2
 // independently of its alpha. The three-basis dynamic term still uses the
 // native LT3 diffuse approximation; this is not complete RSX shader parity.
 float4 ps(P p):SV_TARGET{float4 c=tex.Sample(sampleLinear,p.uv);if(p.light.a<.5)clip(c.a-.25);if(material.x>0)c.rgb*=pattern.Sample(sampleLinear,p.uv1).rgb;float light=.48+.52*abs(dot(normalize(p.n),normalize(float3(-.3,.7,1))));float3 illumination=p.light.a>.5?p.light.rgb:float3(light,light,light);if(p.light.a>.5)illumination+=p.authored.rgb*material.y;
 // Native temporary point lighting: Lambert times linear falloff. The single
 // radius is the existing CPU PointLight range==extendedRange subset.
 // Added to existing lighting, with no shadow or original RSX parity claim.
 if(lightCount.x>0){float normalSquared=dot(p.n,p.n);float3 normal=normalSquared>=1e-16?p.n*rsqrt(normalSquared):float3(0,1,0);
  [loop]for(int i=0;i<(int)lightCount.x;++i){float3 delta=pointLights[i].positionRadius.xyz-p.worldPosition;
   float distanceSquared=dot(delta,delta);float distance=sqrt(distanceSquared);
   float falloff=saturate(1-distance/pointLights[i].positionRadius.w);
   float diffuse=distanceSquared>1e-12?saturate(dot(normal,delta*rsqrt(distanceSquared))):1;
   illumination+=pointLights[i].colorIntensity.rgb*(pointLights[i].colorIntensity.w*falloff*diffuse);
  }
 }
 return float4(c.rgb*tint.rgb*illumination,1);}
 )";
 Ptr<ID3DBlob>v,p,error;ok(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,"vs","vs_4_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&v,&error));ok(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,"ps","ps_4_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&p,&error));
 ok(d->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&vs_));ok(d->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&ps_));
 D3D11_INPUT_ELEMENT_DESC e[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,40,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",1,DXGI_FORMAT_R32G32B32A32_FLOAT,0,56,D3D11_INPUT_PER_VERTEX_DATA,0}};ok(d->CreateInputLayout(e,6,v->GetBufferPointer(),v->GetBufferSize(),&layout_));
 D3D11_SAMPLER_DESC s{};s.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;s.MaxLOD=D3D11_FLOAT32_MAX;ok(d->CreateSamplerState(&s,&sampler_));
 D3D11_RASTERIZER_DESC r{};r.FillMode=D3D11_FILL_SOLID;r.CullMode=D3D11_CULL_NONE;r.DepthClipEnable=TRUE;ok(d->CreateRasterizerState(&r,&raster_));
 D3D11_DEPTH_STENCIL_DESC z{};z.DepthEnable=TRUE;z.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;z.DepthFunc=D3D11_COMPARISON_LESS;ok(d->CreateDepthStencilState(&z,&depthState_));
 for(const auto&im:m.textures){D3D11_TEXTURE2D_DESC td{};td.Width=im.width;td.Height=im.height;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;td.Format=im.codec==9?DXGI_FORMAT_BC1_UNORM:DXGI_FORMAT_BC3_UNORM;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
  D3D11_SUBRESOURCE_DATA sd{};sd.pSysMem=im.pixels.data();sd.SysMemPitch=((im.width+3)/4)*(im.codec==9?8:16);Ptr<ID3D11Texture2D>image;Ptr<ID3D11ShaderResourceView>view;ok(d->CreateTexture2D(&td,&sd,&image));ok(d->CreateShaderResourceView(image.Get(),nullptr,&view));textures_.push_back(view);}
}
void CharacterRenderer::preview_vertical_offset(float heightFraction){
 if(!std::isfinite(heightFraction))throw std::runtime_error("Invalid PC preview vertical offset");
 previewVerticalOffset_=std::clamp(heightFraction,-.25f,.25f);
}
void CharacterRenderer::update_vertices(ID3D11DeviceContext*c,std::span<const ModelVertex> v){
 if(v.size()!=vertexCount_)throw std::runtime_error("Character vertex count changed");
 c->UpdateSubresource(vertices_.Get(),0,nullptr,v.data(),0,0);
}
void CharacterRenderer::render(ID3D11DeviceContext*c,float yaw,bool overview,const WorldView* camera,CharacterRenderer* surface,const std::array<float,3>* origin,std::span<const DynamicPointLight> lights){
 validate_dynamic_lights(lights); // Reject the entire call before GPU state/target changes.
 using namespace DirectX;ID3D11ShaderResourceView*nil=nullptr;c->PSSetShaderResources(0,1,&nil);
 auto destination=surface?surface:this;
 if(!surface){float clear[4]={};c->ClearRenderTargetView(target_.Get(),clear);c->ClearDepthStencilView(depthView_.Get(),D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);}
 auto target=destination->target_.Get();c->OMSetRenderTargets(1,&target,destination->depthView_.Get());c->OMSetDepthStencilState(depthState_.Get(),0);c->OMSetBlendState(nullptr,nullptr,0xffffffff);
 D3D11_VIEWPORT vp{0,0,616,392,0,1};c->RSSetViewports(1,&vp);c->RSSetState(raster_.Get());
 const auto&frameBounds=overview&&hasOverviewBounds_?overviewBounds_:bounds_;
 float center[3];for(int i=0;i<3;++i)center[i]=(frameBounds[i]+frameBounds[i+3])*.5f;
 float height=bounds_[4]-bounds_[1],radius=std::max(height,bounds_[3]-bounds_[0]);
 // Move only the displayed preview framing. Authored vertices, world cameras,
 // standing bounds, view distance and projection scale remain unchanged.
 auto world=XMMatrixTranslation(-center[0],-center[1]+(!overview&&!camera?height*previewVerticalOffset_:0.f),-center[2])*XMMatrixRotationY(yaw);
 auto view=XMMatrixLookAtLH(XMVectorSet(0,height*.04f,radius*1.65f,1),XMVectorZero(),XMVectorSet(0,1,0,0));
 auto projection=XMMatrixPerspectiveFovLH(.65f,440.f/280,10,radius*5);
 if(overview){float dx=frameBounds[3]-frameBounds[0],dy=frameBounds[4]-frameBounds[1],dz=frameBounds[5]-frameBounds[2];float sphere=std::max(1.f,std::sqrt(dx*dx+dy*dy+dz*dz)*.5f);view=XMMatrixLookAtLH(XMVectorSet(0,sphere*2.3f,sphere*2.8f,1),XMVectorZero(),XMVectorSet(0,1,0,0));projection=XMMatrixPerspectiveFovLH(.65f,616.f/392,std::max(.01f,sphere*.001f),sphere*16);}
 if(camera){
  for(auto x:camera->eye)if(!std::isfinite(x)||std::abs(x)>=1e7f)throw std::runtime_error("Invalid stage camera position");
  float length=0;for(auto x:camera->direction){if(!std::isfinite(x))throw std::runtime_error("Invalid stage camera direction");length+=x*x;}
  if(!std::isfinite(length)||length<.001f||camera->direction[0]*camera->direction[0]+camera->direction[2]*camera->direction[2]<.00001f)throw std::runtime_error("Degenerate stage camera");
  world=XMMatrixIdentity();
  if(origin){for(float v:*origin)if(!std::isfinite(v)||std::abs(v)>=1e7f)throw std::runtime_error("Invalid avatar position");world=XMMatrixRotationY(yaw)*XMMatrixTranslation((*origin)[0],(*origin)[1],(*origin)[2]);}
  auto eye=XMVectorSet(camera->eye[0],camera->eye[1],camera->eye[2],1);auto direction=XMVectorSet(camera->direction[0],camera->direction[1],camera->direction[2],0);
  view=XMMatrixLookToLH(eye,direction,XMVectorSet(0,1,0,0));projection=XMMatrixPerspectiveFovLH(1.0f,616.f/392,10,500000);
 }
 struct PointLight{XMFLOAT4 positionRadius,colorIntensity;};
 struct Constants{XMFLOAT4X4 wvp,world;XMFLOAT4 material,tint,lightCount;PointLight lights[maximum_dynamic_lights];} data{};
 static_assert(sizeof(Constants)==432);
 XMStoreFloat4x4(&data.wvp,world*view*projection);XMStoreFloat4x4(&data.world,world);data.lightCount.x=float(lights.size());
 for(size_t i=0;i<lights.size();++i){const auto&light=lights[i];XMFLOAT3 position{light.position[0],light.position[1],light.position[2]};
  // Overview/preview recenter authored geometry for presentation. Move the
  // lights with that same frame; real world cameras leave world lights fixed.
  if(!camera)XMStoreFloat3(&position,XMVector3TransformCoord(XMLoadFloat3(&position),world));
  data.lights[i]={{position.x,position.y,position.z,light.radius},{light.color[0],light.color[1],light.color[2],light.intensity}};
 }
 c->UpdateSubresource(constants_.Get(),0,nullptr,&data,0,0);
 auto cb=constants_.Get();c->VSSetConstantBuffers(0,1,&cb);c->PSSetConstantBuffers(0,1,&cb);c->VSSetShader(vs_.Get(),nullptr,0);c->PSSetShader(ps_.Get(),nullptr,0);auto samp=sampler_.Get();c->PSSetSamplers(0,1,&samp);c->IASetInputLayout(layout_.Get());
 auto vb=vertices_.Get();UINT stride=sizeof(ModelVertex),offset=0;c->IASetVertexBuffers(0,1,&vb,&stride,&offset);c->IASetIndexBuffer(indices_.Get(),DXGI_FORMAT_R32_UINT,0);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 for(const auto&p:parts_){data.material.x=p.flags?1.f:0.f;data.material.y=p.materialShader==0x120000?2.f:0.f;data.tint={p.tint[0],p.tint[1],p.tint[2],1};c->UpdateSubresource(constants_.Get(),0,nullptr,&data,0,0);ID3D11ShaderResourceView* tex[]={textures_[p.texture].Get(),p.flags?textures_.at(p.flags-1).Get():nullptr};c->PSSetShaderResources(0,2,tex);c->DrawIndexed(p.count,p.first,0);}
 c->PSSetShaderResources(0,1,&nil);c->OMSetRenderTargets(0,nullptr,nullptr);
}
}
