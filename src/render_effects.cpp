#include "render_effects.h"
#include <wrl/client.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace mgo2mt::effects {
using Microsoft::WRL::ComPtr;
namespace {
void check(HRESULT h,const char* message){if(FAILED(h))throw std::runtime_error(message);}
bool range(float x,float low,float high){return std::isfinite(x)&&x>=low&&x<=high;}
constexpr unsigned maxPixels=16777216;
struct Surface {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> target;
    ComPtr<ID3D11ShaderResourceView> view;
};
struct alignas(16) Constants {
    std::array<float,16> projection{},inverseProjection{};
    std::array<float,4> lens{},ao{},reflection{},bloom{},flags{},direction{};
};
static_assert(sizeof(Constants)==224);
const char* shader=R"(
Texture2D<float4> scene:register(t0);
Texture2D<float> worldDepth:register(t1);
Texture2D<float4> reflectMask:register(t2);
Texture2D<float4> glow:register(t3);
SamplerState linearClamp:register(s0);
cbuffer Frame:register(b0) {
 row_major float4x4 projection;
 row_major float4x4 inverseProjection;
 float4 lens;       // source dimensions and inverse dimensions
 float4 ao;         // radius, strength, bias, enabled
 float4 reflection; // maximum ray distance, thickness, strength, enabled
 float4 bloom;      // threshold, strength, exposure, enabled
 float4 flags;      // HDR tone map, source linear, unused, unused
 float4 direction;  // blur UV step
};
struct V {float4 p:SV_POSITION;float2 uv:TEXCOORD0;};
V vs(uint id:SV_VertexID){V o;o.p=float4(id==1?3:-1,id==2?-3:1,0,1);o.uv=float2(id==1?2:0,id==2?2:0);return o;}
float4 finiteColor(float4 c){return float4(isfinite(c.x)?clamp(c.x,0,65504):0,isfinite(c.y)?clamp(c.y,0,65504):0,isfinite(c.z)?clamp(c.z,0,65504):0,isfinite(c.w)?saturate(c.w):0);}
float3 decode(float3 c){return float3(c.x<=.04045?c.x/12.92:pow((c.x+.055)/1.055,2.4),c.y<=.04045?c.y/12.92:pow((c.y+.055)/1.055,2.4),c.z<=.04045?c.z/12.92:pow((c.z+.055)/1.055,2.4));}
float3 encode(float3 c){c=max(c,0);return float3(c.x<=.0031308?c.x*12.92:1.055*pow(c.x,1/2.4)-.055,c.y<=.0031308?c.y*12.92:1.055*pow(c.y,1/2.4)-.055,c.z<=.0031308?c.z*12.92:1.055*pow(c.z,1/2.4)-.055);}
float depthAt(float2 uv){int2 p=clamp(int2(uv*lens.xy),int2(0,0),int2(lens.xy)-1);return worldDepth.Load(int3(p,0));}
float3 position(float2 uv,float d){float4 p=mul(float4(uv.x*2-1,1-uv.y*2,d,1),inverseProjection);return p.xyz/max(abs(p.w),1e-8)*sign(p.w);}
bool surfaceAt(float2 uv,out float3 p){float d=depthAt(uv);p=position(uv,max(d,0));return d>0&&d<=1&&all(isfinite(p))&&p.z>0;}
float3 normalAt(float2 uv,float3 p){
 float3 l=position(uv-float2(lens.z,0),depthAt(uv-float2(lens.z,0)));
 float3 r=position(uv+float2(lens.z,0),depthAt(uv+float2(lens.z,0)));
 float3 u=position(uv-float2(0,lens.w),depthAt(uv-float2(0,lens.w)));
 float3 d=position(uv+float2(0,lens.w),depthAt(uv+float2(0,lens.w)));
 float3 dx=abs(l.z-p.z)<abs(r.z-p.z)?p-l:r-p;
 float3 dy=abs(u.z-p.z)<abs(d.z-p.z)?p-u:d-p;
 float3 n=cross(dx,dy);float lengthN=length(n);n=lengthN>1e-6&&all(isfinite(n))?n/lengthN:float3(0,0,-1);
 return dot(n,-p)<0?-n:n;
}
float occlusion(float2 uv,float3 p,float3 n){
 float2 radius=min(float2(.35,.35),ao.x*float2(abs(projection[0][0]),abs(projection[1][1]))/(max(p.z,1)*2));
 float sum=0;
 [unroll]for(int i=0;i<24;i++){
  float a=i*2.39996323;float ring=sqrt((i+.5)/24.0);float2 tap=uv+float2(cos(a),sin(a))*radius*ring;
  if(any(tap<=0)||any(tap>=1))continue;
  float3 q;if(!surfaceAt(tap,q))continue;
  float3 delta=q-p;float distance=length(delta);if(distance<=1e-4||distance>=ao.x)continue;
  float weight=pow(saturate(1-distance/ao.x),2);
  sum+=max(0,(dot(n,delta)-ao.z)/distance)*weight;
 }
 return saturate(1-ao.y*sum/24*4);
}
float3 reflected(float2 uv,float3 p,float3 n,out float confidence){
 confidence=0;float2 mask=reflectMask.SampleLevel(linearClamp,uv,0).rg;
 if(!all(isfinite(mask))||mask.x<=0||mask.y>=.98)return 0;
 float3 ray=reflect(normalize(p),n);float3 start=p+n*max(ao.z,reflection.y*.1);
 float previousGap=-reflection.y;float previousT=0;
 [loop]for(int i=0;i<64;i++){
  float t=reflection.x*(i+1)/64;float3 q=start+ray*t;
  if(q.z<=0)return 0;
  float4 projected=mul(float4(q,1),projection);if(projected.w<=0)return 0;
  float2 hitUv=float2(projected.x/projected.w*.5+.5,.5-projected.y/projected.w*.5);
  if(any(hitUv<=0)||any(hitUv>=1))return 0;
  float3 surface;if(!surfaceAt(hitUv,surface)){previousGap=-reflection.y;previousT=t;continue;}
  float gap=q.z-surface.z;
  if(gap>=0&&previousGap<0&&t>reflection.y){
   float lo=previousT,hi=t;float2 hit=hitUv;float finalGap=gap;
   [unroll]for(int refine=0;refine<5;refine++){
    float middle=(lo+hi)*.5;float3 test=start+ray*middle;float4 hp=mul(float4(test,1),projection);
    float2 hu=float2(hp.x/hp.w*.5+.5,.5-hp.y/hp.w*.5);float3 hs;
    if(!surfaceAt(hu,hs)){lo=middle;continue;}
    float dg=test.z-hs.z;if(dg>=0){hi=middle;hit=hu;finalGap=dg;}else lo=middle;
   }
   if(finalGap<=reflection.y){
    float edge=min(min(hit.x,hit.y),min(1-hit.x,1-hit.y));
    float sourceDistance=length((hit-uv)*lens.xy);
    if(sourceDistance<2)return 0;
    float fresnel=.15+.85*pow(1-saturate(dot(n,-normalize(p))),5);
    confidence=saturate(mask.x)*pow(1-saturate(mask.y),2)*reflection.z*fresnel*saturate(edge*12)*saturate(1-hi/reflection.x);
    return finiteColor(scene.SampleLevel(linearClamp,hit,0)).rgb;
   }
  }
  previousGap=gap;previousT=t;
 }
 return 0;
}
float4 opaque(V input):SV_TARGET{
 float4 color=finiteColor(scene.Load(int3(int2(input.p.xy),0)));float3 p;
 if(!surfaceAt(input.uv,p))return color;
 float3 n=normalAt(input.uv,p);
 if(ao.w>.5)color.rgb*=occlusion(input.uv,p,n);
 if(reflection.w>.5){float weight;float3 reflectedColor=reflected(input.uv,p,n,weight);color.rgb=lerp(color.rgb,reflectedColor,saturate(weight));}
 return color;
}
float3 bright(float2 uv){float3 c=finiteColor(scene.SampleLevel(linearClamp,uv,0)).rgb;if(flags.x>.5&&flags.y<.5)c=decode(c);float luminance=dot(c,float3(.2126,.7152,.0722));return c*max(luminance-bloom.x,0)/max(luminance,1e-6);}
float4 extract(V input):SV_TARGET{
 float2 step=lens.zw*.5;return float4((bright(input.uv+step)+bright(input.uv-step)+bright(input.uv+float2(step.x,-step.y))+bright(input.uv+float2(-step.x,step.y)))*.25,1);
}
float4 blur(V input):SV_TARGET{
 float3 c=scene.SampleLevel(linearClamp,input.uv,0).rgb*.227027;
 c+=(scene.SampleLevel(linearClamp,input.uv+direction.xy*1.384615,0).rgb+scene.SampleLevel(linearClamp,input.uv-direction.xy*1.384615,0).rgb)*.316216;
 c+=(scene.SampleLevel(linearClamp,input.uv+direction.xy*3.230769,0).rgb+scene.SampleLevel(linearClamp,input.uv-direction.xy*3.230769,0).rgb)*.070270;
 return float4(c,1);
}
float4 compose(V input):SV_TARGET{
 float4 color=finiteColor(scene.Load(int3(int2(input.p.xy),0)));float3 c=color.rgb;
 if(flags.x>.5&&flags.y<.5)c=decode(c);
 if(bloom.w>.5)c+=finiteColor(glow.SampleLevel(linearClamp,input.uv,0)).rgb*bloom.y;
 if(flags.x>.5){c=min(c*bloom.z,65504);c=saturate((c*(2.51*c+.03))/(c*(2.43*c+.59)+.14));}
 if(flags.x>.5||flags.y>.5)c=encode(c);
 return finiteColor(float4(c,color.a));
}
float luma(float3 c){return dot(c,float3(.299,.587,.114));}
float4 fxaa(V input):SV_TARGET{
 float4 center=finiteColor(scene.SampleLevel(linearClamp,input.uv,0));
 float nw=luma(finiteColor(scene.SampleLevel(linearClamp,input.uv+lens.zw*float2(-1,-1),0)).rgb);
 float ne=luma(finiteColor(scene.SampleLevel(linearClamp,input.uv+lens.zw*float2(1,-1),0)).rgb);
 float sw=luma(finiteColor(scene.SampleLevel(linearClamp,input.uv+lens.zw*float2(-1,1),0)).rgb);
 float se=luma(finiteColor(scene.SampleLevel(linearClamp,input.uv+lens.zw,0)).rgb);
 float mid=luma(center.rgb),low=min(mid,min(min(nw,ne),min(sw,se))),high=max(mid,max(max(nw,ne),max(sw,se)));
 if(high-low<max(.0312,high*.125))return center;
 float2 dir=float2(-((nw+ne)-(sw+se)),(nw+sw)-(ne+se));
 float reduce=max((nw+ne+sw+se)*.03125,.0078125);dir=clamp(dir/(min(abs(dir.x),abs(dir.y))+reduce),-8,8)*lens.zw;
 float3 a=(finiteColor(scene.SampleLevel(linearClamp,input.uv-dir/6,0)).rgb+finiteColor(scene.SampleLevel(linearClamp,input.uv+dir/6,0)).rgb)*.5;
 float3 b=a*.5+(finiteColor(scene.SampleLevel(linearClamp,input.uv-dir*.5,0)).rgb+finiteColor(scene.SampleLevel(linearClamp,input.uv+dir*.5,0)).rgb)*.25;
 float lb=luma(b);return float4(lb<low||lb>high?a:b,center.a);
}
float4 copy(V input):SV_TARGET{return scene.Load(int3(int2(input.p.xy),0));}
)";
struct TextureInfo {unsigned width=0,height=0;ComPtr<ID3D11Resource> resource;};
TextureInfo info(ID3D11ShaderResourceView* view){
    if(!view)throw std::invalid_argument("Effects source is required");
    TextureInfo result;view->GetResource(&result.resource);ComPtr<ID3D11Texture2D> texture;
    check(result.resource.As(&texture),"Effects require a 2D texture");
    D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);D3D11_SHADER_RESOURCE_VIEW_DESC v{};view->GetDesc(&v);
    if(d.SampleDesc.Count!=1||v.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D||v.Texture2D.MostDetailedMip!=0||d.ArraySize!=1||!d.Width||!d.Height||d.Width>8192||d.Height>8192||uint64_t(d.Width)*d.Height>maxPixels)throw std::invalid_argument("Effects texture dimensions/budget");
    result.width=d.Width;result.height=d.Height;return result;
}
void matching(const TextureInfo& color,ID3D11ShaderResourceView* view){auto other=info(view);if(color.width!=other.width||color.height!=other.height)throw std::invalid_argument("Effects input sizes differ");}
bool cameraValid(const Camera& camera){
    for(float value:camera.projection)if(!std::isfinite(value))return false;
    for(float value:camera.inverseProjection)if(!std::isfinite(value))return false;
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col){double value=0;for(unsigned k=0;k<4;++k)value+=double(camera.projection[row*4+k])*camera.inverseProjection[k*4+col];if(std::abs(value-(row==col?1.:0.))>.01)return false;}
    return true;
}
}
bool valid(const Settings& s) noexcept {
    return range(s.exposure,.125f,8.f)&&range(s.aoRadius,1.f,10000.f)&&range(s.aoStrength,0.f,2.f)&&range(s.aoBias,0.f,1000.f)&&s.aoBias<s.aoRadius&&range(s.ssrDistance,1.f,200000.f)&&range(s.ssrThickness,.1f,2000.f)&&range(s.ssrStrength,0.f,1.f)&&range(s.bloomThreshold,0.f,32.f)&&range(s.bloomStrength,0.f,4.f);
}

struct Renderer::Impl {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11VertexShader> vertex;ComPtr<ID3D11PixelShader> opaqueShader,extractShader,blurShader,composeShader,fxaaShader,copyShader;
    ComPtr<ID3D11Buffer> buffer;ComPtr<ID3D11SamplerState> sampler;ComPtr<ID3D11RasterizerState> raster;ComPtr<ID3D11DepthStencilState> depth;
    std::array<Surface,2> full,half;unsigned width=0,height=0;size_t bytes=0;
    explicit Impl(ID3D11Device* d):device(d){
        if(!d)throw std::invalid_argument("Effects device is required");
        check(d->CreateDeferredContext(0,&context),"Effects deferred context");
        auto compile=[&](const char* entry,const char* profile){ComPtr<ID3DBlob> code,error;HRESULT h=D3DCompile(shader,std::strlen(shader),"native_render_effects",nullptr,nullptr,entry,profile,D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&error);if(FAILED(h))throw std::runtime_error(error?std::string(static_cast<const char*>(error->GetBufferPointer()),error->GetBufferSize()):"Effects shader compilation");return code;};
        auto vs=compile("vs","vs_4_0");check(d->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex),"Effects vertex shader");
        for(auto item:{std::pair{"opaque",opaqueShader.GetAddressOf()},std::pair{"extract",extractShader.GetAddressOf()},std::pair{"blur",blurShader.GetAddressOf()},std::pair{"compose",composeShader.GetAddressOf()},std::pair{"fxaa",fxaaShader.GetAddressOf()},std::pair{"copy",copyShader.GetAddressOf()}}){auto ps=compile(item.first,"ps_4_0");check(d->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,item.second),"Effects pixel shader");}
        D3D11_BUFFER_DESC bd{};bd.ByteWidth=sizeof(Constants);bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;check(d->CreateBuffer(&bd,nullptr,&buffer),"Effects constants");
        D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;check(d->CreateSamplerState(&sd,&sampler),"Effects sampler");
        D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;check(d->CreateRasterizerState(&rd,&raster),"Effects raster state");
        D3D11_DEPTH_STENCIL_DESC dd{};dd.DepthEnable=FALSE;check(d->CreateDepthStencilState(&dd,&depth),"Effects depth state");
    }
    Surface create(unsigned w,unsigned h){
        D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        Surface result;check(device->CreateTexture2D(&d,nullptr,&result.texture),"Effects render texture");check(device->CreateRenderTargetView(result.texture.Get(),nullptr,&result.target),"Effects render view");check(device->CreateShaderResourceView(result.texture.Get(),nullptr,&result.view),"Effects sample view");return result;
    }
    void resize(unsigned w,unsigned h){
        if(width==w&&height==h)return;
        std::array<Surface,2> f{create(w,h),create(w,h)};std::array<Surface,2> b{create((w+1)/2,(h+1)/2),create((w+1)/2,(h+1)/2)};
        full=std::move(f);half=std::move(b);width=w;height=h;bytes=(size_t(w)*h*2+size_t((w+1)/2)*((h+1)/2)*2)*8;
    }
    Constants values(const Settings& s){Constants c{};c.ao={s.aoRadius,s.aoStrength,s.aoBias,s.ssao?1.f:0.f};c.reflection={s.ssrDistance,s.ssrThickness,s.ssrStrength,s.ssr?1.f:0.f};c.bloom={s.bloomThreshold,s.bloomStrength,s.exposure,s.bloom?1.f:0.f};c.flags[0]=s.hdr?1.f:0.f;return c;}
    void draw(ID3D11PixelShader* ps,ID3D11ShaderResourceView* color,ID3D11RenderTargetView* target,Constants values,ID3D11ShaderResourceView* worldDepth=nullptr,ID3D11ShaderResourceView* mask=nullptr,ID3D11ShaderResourceView* bloom=nullptr){
        auto input=info(color);values.lens={float(input.width),float(input.height),1.f/input.width,1.f/input.height};
        ComPtr<ID3D11Resource> output;target->GetResource(&output);ComPtr<ID3D11Texture2D> texture;check(output.As(&texture),"Effects output texture");D3D11_TEXTURE2D_DESC dimensions{};texture->GetDesc(&dimensions);
        if(output.Get()==input.resource.Get())throw std::invalid_argument("Effects read/write alias");
        context->UpdateSubresource(buffer.Get(),0,nullptr,&values,0,0);
        context->OMSetRenderTargets(1,&target,nullptr);context->OMSetDepthStencilState(depth.Get(),0);context->OMSetBlendState(nullptr,nullptr,~0u);
        D3D11_VIEWPORT viewport{0,0,float(dimensions.Width),float(dimensions.Height),0,1};context->RSSetViewports(1,&viewport);context->RSSetState(raster.Get());
        context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->VSSetShader(vertex.Get(),nullptr,0);context->PSSetShader(ps,nullptr,0);
        auto constants=buffer.Get();auto sample=sampler.Get();context->PSSetConstantBuffers(0,1,&constants);context->PSSetSamplers(0,1,&sample);
        ID3D11ShaderResourceView* views[]{color,worldDepth,mask,bloom};context->PSSetShaderResources(0,4,views);context->Draw(3,0);
        ID3D11ShaderResourceView* empty[4]{};context->PSSetShaderResources(0,4,empty);context->OMSetRenderTargets(0,nullptr,nullptr);
    }
    void execute(ID3D11DeviceContext* immediate){
        ComPtr<ID3D11CommandList> commands;check(context->FinishCommandList(FALSE,&commands),"Effects finish commands");immediate->ExecuteCommandList(commands.Get(),TRUE);
    }
    void validateContext(ID3D11DeviceContext* immediate)const{
        if(!immediate||immediate->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)throw std::invalid_argument("Effects require the immediate context");
        ComPtr<ID3D11Device> owner;immediate->GetDevice(&owner);if(owner.Get()!=device.Get())throw std::invalid_argument("Effects context device mismatch");
    }
    void validateResource(ID3D11DeviceChild* resource)const{
        if(!resource)throw std::invalid_argument("Effects input is required");
        ComPtr<ID3D11Device> owner;resource->GetDevice(&owner);if(owner.Get()!=device.Get())throw std::invalid_argument("Effects resource device mismatch");
    }
    unsigned destination(ID3D11ShaderResourceView* source)const{return source==full[0].view.Get()?1u:0u;}
};
Renderer::Renderer(ID3D11Device* device):impl_(std::make_unique<Impl>(device)){}
Renderer::~Renderer()=default;
unsigned Renderer::width()const noexcept{return impl_->width;}
unsigned Renderer::height()const noexcept{return impl_->height;}
size_t Renderer::allocated_bytes()const noexcept{return impl_->bytes;}
ID3D11ShaderResourceView* Renderer::opaque(ID3D11DeviceContext* context,const Inputs& input,const Settings& settings){
    if(!valid(settings))throw std::invalid_argument("Invalid effects settings");
    if(!settings.ssao&&(!settings.ssr||!input.reflectionMask))return input.color;
    impl_->validateContext(context);impl_->validateResource(input.color);impl_->validateResource(input.depth);if(input.reflectionMask)impl_->validateResource(input.reflectionMask);
    auto color=info(input.color);matching(color,input.depth);if(settings.ssr&&input.reflectionMask)matching(color,input.reflectionMask);
    D3D11_SHADER_RESOURCE_VIEW_DESC depthFormat{};input.depth->GetDesc(&depthFormat);if(depthFormat.Format!=DXGI_FORMAT_R32_FLOAT)throw std::invalid_argument("Effects require R32_FLOAT reverse depth");
    if(!cameraValid(input.camera))throw std::invalid_argument("Effects projection/inverse mismatch");
    auto& r=*impl_;r.resize(color.width,color.height);auto values=r.values(settings);values.projection=input.camera.projection;values.inverseProjection=input.camera.inverseProjection;if(!input.reflectionMask)values.reflection[3]=0;
    unsigned target=r.destination(input.color);r.draw(r.opaqueShader.Get(),input.color,r.full[target].target.Get(),values,input.depth,input.reflectionMask);r.execute(context);return r.full[target].view.Get();
}
ID3D11ShaderResourceView* Renderer::finish(ID3D11DeviceContext* context,ID3D11ShaderResourceView* source,const Settings& settings,bool linearInput){
    if(!valid(settings))throw std::invalid_argument("Invalid effects settings");
    if(!settings.bloom&&!settings.hdr&&!settings.fxaa&&!linearInput)return source;
    impl_->validateContext(context);impl_->validateResource(source);
    auto input=info(source);auto& r=*impl_;r.resize(input.width,input.height);auto values=r.values(settings);values.flags[1]=linearInput?1.f:0.f;
    if(settings.bloom){
        r.draw(r.extractShader.Get(),source,r.half[0].target.Get(),values);
        values.direction={1.f/((input.width+1)/2),0,0,0};r.draw(r.blurShader.Get(),r.half[0].view.Get(),r.half[1].target.Get(),values);
        values.direction={0,1.f/((input.height+1)/2),0,0};r.draw(r.blurShader.Get(),r.half[1].view.Get(),r.half[0].target.Get(),values);
    }
    ID3D11ShaderResourceView* output=source;
    if(settings.bloom||settings.hdr||linearInput){unsigned target=r.destination(output);r.draw(r.composeShader.Get(),output,r.full[target].target.Get(),values,nullptr,nullptr,settings.bloom?r.half[0].view.Get():nullptr);output=r.full[target].view.Get();}
    if(settings.fxaa){unsigned target=r.destination(output);r.draw(r.fxaaShader.Get(),output,r.full[target].target.Get(),values);output=r.full[target].view.Get();}
    r.execute(context);return output;
}
void Renderer::copy_to(ID3D11DeviceContext* context,ID3D11ShaderResourceView* source,ID3D11RenderTargetView* target){
    if(!target)throw std::invalid_argument("Effects destination is required");impl_->validateContext(context);impl_->validateResource(source);impl_->validateResource(target);auto input=info(source);ComPtr<ID3D11Resource> output;target->GetResource(&output);if(output.Get()==input.resource.Get())return;
    ComPtr<ID3D11Texture2D> texture;check(output.As(&texture),"Effects copy target");D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);if(desc.Width!=input.width||desc.Height!=input.height||desc.SampleDesc.Count!=1)throw std::invalid_argument("Effects copy size mismatch");
    D3D11_RENDER_TARGET_VIEW_DESC targetDesc{};target->GetDesc(&targetDesc);if(targetDesc.ViewDimension!=D3D11_RTV_DIMENSION_TEXTURE2D||targetDesc.Texture2D.MipSlice!=0)throw std::invalid_argument("Effects copy target view");
    impl_->draw(impl_->copyShader.Get(),source,target,{});impl_->execute(context);
}
}
