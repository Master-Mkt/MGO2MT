#include "shadow_renderer.h"
#include "character_renderer.h"
#include <d3dcompiler.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace mgo2win::shadows {
namespace {
void ok(HRESULT h){if(FAILED(h))throw std::runtime_error("Shadow GPU resource unavailable");}
struct Receiver {std::array<std::array<float,16>,6> matrices;std::array<float,8> splits;std::array<float,4> parameters,bias,eye,forward,direction,sun;};
static_assert(sizeof(Receiver)==512);
}
const char* receiver_shader(){return R"(
 cbuffer ShadowFrame:register(b1){row_major float4x4 shadowMatrices[6];float4 shadowSplits[2];float4 shadowParams;float4 shadowBias;float4 shadowEye;float4 shadowForward;float4 shadowDirection;float4 shadowSun;};
 #ifdef NO_CSM
 float shadowVisibility(float3 position,float3 normal,out float3 debugColor){debugColor=1;return 1;}
 #else
 Texture2DArray<float> shadowMap:register(t4);SamplerComparisonState shadowSampler:register(s1);
 float splitEnd(uint index){return shadowSplits[index/4][index%4];}
 float shadowSlice(int index,float3 position,float3 normal){
  float facing=saturate(dot(normal,-shadowDirection.xyz));float3 shifted=position+normal*(shadowParams.w*(1-facing));
  float3 p=mul(float4(shifted,1),shadowMatrices[index]).xyz;float2 uv=float2(p.x*.5+.5,.5-p.y*.5);
  if(p.z<=0||p.z>=1||any(uv<0)||any(uv>1))return 1;
  int radius=(int)shadowParams.y;float result=0;
  [loop]for(int y=-radius;y<=radius;++y)[loop]for(int x=-radius;x<=radius;++x)
   result+=shadowMap.SampleCmpLevelZero(shadowSampler,float3(uv+float2(x,y)/shadowParams.z,index),p.z-shadowBias.x);
  return result/((radius*2+1)*(radius*2+1));
 }
 float shadowVisibility(float3 position,float3 normal,out float3 debugColor){
  debugColor=float3(1,1,1);int count=(int)shadowParams.x;if(count==0)return 1;
  float depth=dot(position-shadowEye.xyz,shadowForward.xyz);if(depth<shadowBias.z||depth>splitEnd(count-1))return 1;
  int index=0;[loop]while(index<count-1&&depth>splitEnd(index))++index;
  float3 colors[6]={float3(1,.2,.2),float3(.2,1,.2),float3(.2,.4,1),float3(1,1,.2),float3(1,.2,1),float3(.2,1,1)};debugColor=colors[index];
  float value=shadowSlice(index,position,normal);float start=index==0?shadowBias.z:splitEnd(index-1);float end=splitEnd(index);float blend=saturate((depth-(end-(end-start)*.1))/((end-start)*.1));
  if(blend>0){float next=index+1<count?shadowSlice(index+1,position,normal):1;value=lerp(value,next,blend);if(index+1<count)debugColor=lerp(debugColor,colors[index+1],blend);}
  return value;
 }
 #endif
 )";}
bool Renderer::configure(ID3D11Device*d,const Settings&s){
 if(!d||!valid(s))return false;if(configured_&&settings_==s)return !s.enabled||allocation_.cascades!=0;
 configured_=true;settings_=s;stats_={};allocation_={};depth_.Reset();view_.Reset();for(auto&t:targets_)t.Reset();
 if(!s.enabled){status_="OFF";return true;}
 try{
  if(d->GetFeatureLevel()<D3D_FEATURE_LEVEL_11_0)throw std::runtime_error("CSM requires feature level 11; Legacy remains available");
  uint64_t budget=64ull*1024*1024;Ptr<IDXGIDevice> dxgi;Ptr<IDXGIAdapter> adapter;DXGI_ADAPTER_DESC info{};
  if(SUCCEEDED(d->QueryInterface(IID_PPV_ARGS(&dxgi)))&&SUCCEEDED(dxgi->GetAdapter(&adapter))&&SUCCEEDED(adapter->GetDesc(&info))&&info.DedicatedVideoMemory)budget=std::clamp<uint64_t>(info.DedicatedVideoMemory/8,32ull*1024*1024,256ull*1024*1024);
  allocation_=allocation(s,budget);if(!allocation_.cascades)throw std::runtime_error("Shadow minimum exceeds GPU budget");
  UINT support=0;ok(d->CheckFormatSupport(DXGI_FORMAT_R32_FLOAT,&support));if(!(support&D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_COMPARISON))throw std::runtime_error("Shadow comparison sampling unavailable");
  // Retry progressively smaller maps when a supported allocation still fails.
  HRESULT allocated=E_OUTOFMEMORY;
  while(allocation_.resolution>=1024){D3D11_TEXTURE2D_DESC t{};t.Width=t.Height=allocation_.resolution;t.MipLevels=1;t.ArraySize=s.cascades;t.Format=DXGI_FORMAT_R32_TYPELESS;t.SampleDesc.Count=1;t.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;allocated=d->CreateTexture2D(&t,nullptr,&depth_);if(SUCCEEDED(allocated))break;allocation_.resolution/=2;allocation_.reduced=true;}
  ok(allocated);allocation_.bytes=uint64_t(allocation_.resolution)*allocation_.resolution*s.cascades*4;
  D3D11_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R32_FLOAT;srv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2DARRAY;srv.Texture2DArray.MipLevels=1;srv.Texture2DArray.ArraySize=s.cascades;ok(d->CreateShaderResourceView(depth_.Get(),&srv,&view_));
  for(unsigned i=0;i<s.cascades;++i){D3D11_DEPTH_STENCIL_VIEW_DESC z{};z.Format=DXGI_FORMAT_D32_FLOAT;z.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2DARRAY;z.Texture2DArray.FirstArraySlice=i;z.Texture2DArray.ArraySize=1;ok(d->CreateDepthStencilView(depth_.Get(),&z,&targets_[i]));}
  if(!vs_){const char* shader=R"(
   cbuffer Draw:register(b0){row_major float4x4 wvp;float4 palette;float4 transformUV;};
   struct V{float3 p:POSITION;float2 uv:TEXCOORD0;float4 light:COLOR0;};struct P{float4 p:SV_POSITION;float2 uv:TEXCOORD0;float clipAlpha:TEXCOORD1;};
   P vs(V v){P o;o.p=mul(float4(v.p,1),wvp);o.uv=palette.x>.5?v.uv*transformUV.xy+transformUV.zw+palette.yz:v.uv;o.clipAlpha=v.light.a<.5?1:0;return o;}
   Texture2D tex:register(t0);SamplerState alphaSampler:register(s0);void ps(P p){if(p.clipAlpha>.5)clip(tex.Sample(alphaSampler,p.uv).a-.25);}
  )";Ptr<ID3DBlob>v,p,error;ok(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,"vs","vs_4_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&v,&error));ok(D3DCompile(shader,std::strlen(shader),nullptr,nullptr,nullptr,"ps","ps_4_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&p,&error));ok(d->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&vs_));ok(d->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&ps_));
   D3D11_INPUT_ELEMENT_DESC layout[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,40,D3D11_INPUT_PER_VERTEX_DATA,0}};ok(d->CreateInputLayout(layout,3,v->GetBufferPointer(),v->GetBufferSize(),&layout_));
  }
  auto buffer=[&](unsigned bytes,ID3D11Buffer** out){D3D11_BUFFER_DESC desc{};desc.ByteWidth=bytes;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;ok(d->CreateBuffer(&desc,nullptr,out));};casterBuffer_.Reset();receiverBuffer_.Reset();buffer(96,&casterBuffer_);buffer(sizeof(Receiver),&receiverBuffer_);
  D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;raster.DepthClipEnable=TRUE;raster.SlopeScaledDepthBias=s.slopeBias;raster.DepthBiasClamp=.01f;raster_.Reset();ok(d->CreateRasterizerState(&raster,&raster_));
  D3D11_DEPTH_STENCIL_DESC z{};z.DepthEnable=TRUE;z.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;z.DepthFunc=D3D11_COMPARISON_LESS;depthState_.Reset();ok(d->CreateDepthStencilState(&z,&depthState_));
  D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_BORDER;std::fill_n(sampler.BorderColor,4,1.f);sampler.ComparisonFunc=D3D11_COMPARISON_LESS_EQUAL;sampler.MaxLOD=D3D11_FLOAT32_MAX;compare_.Reset();ok(d->CreateSamplerState(&sampler,&compare_));alpha_=render_backend::Device(d).sampler(0);
  stats_.cascades=allocation_.cascades;stats_.resolution=allocation_.resolution;stats_.allocatedBytes=allocation_.bytes;stats_.reduced=allocation_.reduced;status_=allocation_.reduced?"GPU budget: reduced resolution":"Ready";return true;
 }catch(const std::exception&e){status_=e.what();depth_.Reset();view_.Reset();for(auto&t:targets_)t.Reset();vs_.Reset();ps_.Reset();layout_.Reset();allocation_={};stats_={};return false;}
}
void Renderer::unbind(ID3D11DeviceContext*c){ID3D11ShaderResourceView*view=nullptr;c->PSSetShaderResources(4,1,&view);ID3D11Buffer*buffer=nullptr;c->PSSetConstantBuffers(1,1,&buffer);ID3D11SamplerState*sampler=nullptr;c->PSSetSamplers(1,1,&sampler);}
void Renderer::bind(ID3D11DeviceContext*c)const{if(!stats_.active){unbind(c);return;}auto view=view_.Get();auto buffer=receiverBuffer_.Get();auto sampler=compare_.Get();c->PSSetShaderResources(4,1,&view);c->PSSetConstantBuffers(1,1,&buffer);c->PSSetSamplers(1,1,&sampler);}
bool Renderer::render(ID3D11DeviceContext*c,const WorldView& camera,std::array<float,3> direction,std::array<float,3> sun,const std::array<float,6>& bounds,std::span<const Caster> casters){
 stats_.active=false;stats_.drawCalls=stats_.triangles=0;unbind(c);if(!settings_.enabled||!allocation_.cascades)return false;
 try{
  if(casters.size()>512)throw std::invalid_argument("Shadow caster limit");for(const auto&caster:casters){if(!caster.renderer||!std::isfinite(caster.yaw))throw std::invalid_argument("Shadow caster");for(auto v:caster.origin)if(!std::isfinite(v)||std::abs(v)>=1e7)throw std::invalid_argument("Shadow origin");}
  plan_=plan(camera,direction,sun,bounds,settings_,allocation_);using namespace DirectX;
  Receiver receiver{};receiver.matrices=plan_.matrices;std::copy(plan_.splits.begin(),plan_.splits.end(),receiver.splits.begin());receiver.parameters={float(plan_.count),float(settings_.pcfRadius),float(plan_.resolution),settings_.normalBias};receiver.bias={settings_.depthBias,settings_.visualize?1.f:0.f,plan_.nearPlane,0};for(unsigned i=0;i<3;++i){receiver.eye[i]=plan_.eye[i];receiver.forward[i]=plan_.forward[i];receiver.direction[i]=plan_.direction[i];receiver.sun[i]=plan_.sun[i];}c->UpdateSubresource(receiverBuffer_.Get(),0,nullptr,&receiver,0,0);
  c->IASetInputLayout(layout_.Get());c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);c->VSSetShader(vs_.Get(),nullptr,0);c->PSSetShader(ps_.Get(),nullptr,0);c->RSSetState(raster_.Get());c->OMSetDepthStencilState(depthState_.Get(),0);c->OMSetBlendState(nullptr,nullptr,0xffffffff);D3D11_VIEWPORT viewport{0,0,float(plan_.resolution),float(plan_.resolution),0,1};c->RSSetViewports(1,&viewport);auto buffer=casterBuffer_.Get();c->VSSetConstantBuffers(0,1,&buffer);auto sampler=alpha_.Get();c->PSSetSamplers(0,1,&sampler);
  for(unsigned cascade=0;cascade<plan_.count;++cascade){auto target=targets_[cascade].Get();c->OMSetRenderTargets(0,nullptr,target);c->ClearDepthStencilView(target,D3D11_CLEAR_DEPTH,1,0);XMFLOAT4X4 projection;std::memcpy(&projection,plan_.matrices[cascade].data(),64);
   for(const auto&caster:casters){const auto& r=*caster.renderer;if(r.sky_)continue;auto world=XMMatrixRotationY(caster.yaw)*XMMatrixTranslation(caster.origin[0],caster.origin[1],caster.origin[2]);struct Draw {XMFLOAT4X4 wvp;XMFLOAT4 palette,uv;} draw{};XMStoreFloat4x4(&draw.wvp,world*XMLoadFloat4x4(&projection));auto vb=r.vertices_.Get();UINT stride=sizeof(ModelVertex),offset=0;c->IASetVertexBuffers(0,1,&vb,&stride,&offset);c->IASetIndexBuffer(r.indices_.Get(),DXGI_FORMAT_R32_UINT,0);
    for(size_t i=0;i<r.parts_.size();++i){const auto&part=r.parts_[i];const auto&o=part.original;draw.palette={};draw.uv={1,1,0,0};if((r.materialRules_[i]&4)&&o.paletteSlot<o.textures.size()){draw.palette={1,o.parameters[2][1],-o.parameters[2][2],0};float values[4];for(unsigned j=0;j<4;++j){uint32_t bits=0;for(unsigned k=0;k<4;++k)bits=(bits<<8)|o.textures[o.paletteSlot].raw[8+j*4+k];std::memcpy(&values[j],&bits,4);}draw.uv={values[0],values[1],values[2],values[3]};}
     // Entirely prelit models never alpha-clip in the original forward path.
     // Coalesce only contiguous submitted ranges; gaps/hidden parts stay excluded.
     auto count=part.count;if(r.opaqueShadow_)while(i+1<r.parts_.size()&&uint64_t(part.first)+count==r.parts_[i+1].first){++i;count+=r.parts_[i].count;}
     c->UpdateSubresource(casterBuffer_.Get(),0,nullptr,&draw,0,0);auto texture=r.textures_.at(part.texture).Get();c->PSSetShaderResources(0,1,&texture);c->DrawIndexed(count,part.first,0);++stats_.drawCalls;stats_.triangles+=count/3;
    }
   }
  }
  c->OMSetRenderTargets(0,nullptr,nullptr);ID3D11ShaderResourceView*nil=nullptr;c->PSSetShaderResources(0,1,&nil);stats_.active=true;return true;
 }catch(const std::exception&e){status_=e.what();c->OMSetRenderTargets(0,nullptr,nullptr);unbind(c);return false;}
}
}
