#include "render_effects.h"
#include "world_depth.h"
#include <DirectXPackedVector.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace mgo2mt;
using Microsoft::WRL::ComPtr;
namespace {
using Pixel=std::array<float,4>;
int checks=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
void ok(HRESULT h){if(FAILED(h))throw std::runtime_error("Effects WARP resource creation");}
struct Image {ComPtr<ID3D11Texture2D> texture;ComPtr<ID3D11ShaderResourceView> view;ComPtr<ID3D11RenderTargetView> target;};
Image image(ID3D11Device* d,unsigned w,unsigned h,const std::vector<Pixel>& pixels){
    D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
    D3D11_SUBRESOURCE_DATA data{pixels.data(),w*UINT(sizeof(Pixel)),0};Image result;ok(d->CreateTexture2D(&desc,pixels.empty()?nullptr:&data,&result.texture));ok(d->CreateShaderResourceView(result.texture.Get(),nullptr,&result.view));ok(d->CreateRenderTargetView(result.texture.Get(),nullptr,&result.target));return result;
}
Image depth(ID3D11Device* d,unsigned w,unsigned h,const std::vector<float>& pixels){
    D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R32_FLOAT;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels.data(),w*UINT(sizeof(float)),0};Image result;ok(d->CreateTexture2D(&desc,&data,&result.texture));ok(d->CreateShaderResourceView(result.texture.Get(),nullptr,&result.view));return result;
}
std::vector<Pixel> read(ID3D11Device* d,ID3D11DeviceContext* c,ID3D11ShaderResourceView* source){
    ComPtr<ID3D11Resource> resource;source->GetResource(&resource);ComPtr<ID3D11Texture2D> original;ok(resource.As(&original));D3D11_TEXTURE2D_DESC desc{};original->GetDesc(&desc);auto format=desc.Format;desc.BindFlags=desc.MiscFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    require(format==DXGI_FORMAT_R32G32B32A32_FLOAT||format==DXGI_FORMAT_R16G16B16A16_FLOAT,"Effects output format");
    ComPtr<ID3D11Texture2D> staging;ok(d->CreateTexture2D(&desc,nullptr,&staging));c->CopyResource(staging.Get(),original.Get());D3D11_MAPPED_SUBRESOURCE map{};ok(c->Map(staging.Get(),0,D3D11_MAP_READ,0,&map));std::vector<Pixel> result(size_t(desc.Width)*desc.Height);
    for(unsigned y=0;y<desc.Height;++y){auto row=static_cast<const uint8_t*>(map.pData)+y*map.RowPitch;for(unsigned x=0;x<desc.Width;++x)for(unsigned k=0;k<4;++k){float value=0;if(format==DXGI_FORMAT_R32G32B32A32_FLOAT)std::memcpy(&value,row+(x*4+k)*4,4);else {uint16_t half;std::memcpy(&half,row+(x*4+k)*2,2);value=DirectX::PackedVector::XMConvertHalfToFloat(half);}result[size_t(y)*desc.Width+x][k]=value;}}
    c->Unmap(staging.Get(),0);return result;
}
effects::Camera camera(){using namespace DirectX;effects::Camera c;XMFLOAT4X4 p,i;auto projection=world_projection(1.f,1.f);XMStoreFloat4x4(&p,projection);XMStoreFloat4x4(&i,XMMatrixInverse(nullptr,projection));std::memcpy(c.projection.data(),&p,64);std::memcpy(c.inverseProjection.data(),&i,64);return c;}
float depthOf(float z,const effects::Camera& c){return (z*c.projection[10]+c.projection[14])/(z*c.projection[11]+c.projection[15]);}
bool close_value(float a,float b,float tolerance=.002f){return std::abs(a-b)<=tolerance;}
float transfer(float v){return v<=.0031308f?v*12.92f:1.055f*std::pow(v,1.f/2.4f)-.055f;}
float tone(float v){return transfer(std::clamp(v*(2.51f*v+.03f)/(v*(2.43f*v+.59f)+.14f),0.f,1.f));}
template<class F>void rejected(F&& action,const char* label){bool caught=false;try{action();}catch(const std::invalid_argument&){caught=true;}require(caught,label);}
void run(ID3D11Device* d,ID3D11DeviceContext* c){
    effects::Renderer effects(d);effects::Settings settings;auto projection=camera();constexpr unsigned w=64,h=64;
    std::vector<Pixel> solid(w*h,Pixel{.25f,.5f,4.f,.375f});auto source=image(d,w,h,solid);
    require(effects.finish(c,source.view.Get(),settings)==source.view.Get(),"All OFF preserves exact source identity");
    require(effects.opaque(c,{source.view.Get()},settings)==source.view.Get()&&effects.allocated_bytes()==0,"All OFF allocates no render targets");
    require(read(d,c,source.view.Get())==solid,"All OFF preserves HDR and alpha bytes");
    auto bad=settings;bad.exposure=std::numeric_limits<float>::quiet_NaN();rejected([&]{effects.finish(c,source.view.Get(),bad);},"Reject nonfinite settings");
    settings.hdr=true;auto hdr=read(d,c,effects.finish(c,source.view.Get(),settings,true));
    require(close_value(hdr[0][0],tone(.25f))&&close_value(hdr[0][1],tone(.5f))&&close_value(hdr[0][2],tone(4.f)),"HDR keeps above-one source energy until exposure/ACES and one output transfer");
    require(close_value(hdr[0][3],.375f,.0001f),"Tone mapping preserves alpha");
    settings.exposure=.5f;auto darker=read(d,c,effects.finish(c,source.view.Get(),settings,true));require(darker[0][1]<hdr[0][1]&&close_value(darker[0][1],tone(.25f)),"Exposure is active");
    settings={};require(effects.finish(c,source.view.Get(),settings)==source.view.Get(),"OFF restores exact input after ON");
    auto linear=read(d,c,effects.finish(c,source.view.Get(),settings,true));require(close_value(linear[0][0],transfer(.25f)),"Linear-only final transfer");

    // A deferred list must restore caller state and transient input bindings.
    D3D11_VIEWPORT viewport{17,29,31,37,.2f,.8f};c->RSSetViewports(1,&viewport);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);auto rt=source.target.Get();c->OMSetRenderTargets(1,&rt,nullptr);
    auto sentinel=image(d,w,h,solid);auto srv=sentinel.view.Get();c->PSSetShaderResources(3,1,&srv);
    settings.hdr=true;auto result=effects.finish(c,source.view.Get(),settings,true);
    D3D11_VIEWPORT after{};UINT count=1;c->RSGetViewports(&count,&after);D3D11_PRIMITIVE_TOPOLOGY topology;c->IAGetPrimitiveTopology(&topology);ComPtr<ID3D11RenderTargetView> restoredTarget;c->OMGetRenderTargets(1,&restoredTarget,nullptr);ComPtr<ID3D11ShaderResourceView> restoredView;c->PSGetShaderResources(3,1,&restoredView);
    require(after.TopLeftX==17&&after.Height==37&&after.MinDepth==.2f&&topology==D3D11_PRIMITIVE_TOPOLOGY_LINELIST&&restoredTarget.Get()==rt&&restoredView.Get()==srv,"Full caller context restored after effects");
    effects.copy_to(c,result,sentinel.target.Get());require(close_value(read(d,c,sentinel.view.Get())[0][2],tone(4.f)),"Opaque handoff copies processed float16 result to caller target");
    c->OMSetRenderTargets(0,nullptr,nullptr);ID3D11ShaderResourceView* nullView=nullptr;c->PSSetShaderResources(3,1,&nullView);

    std::vector<Pixel> bright(w*h,Pixel{0,0,0,.5f});for(unsigned y=28;y<36;++y)for(unsigned x=28;x<36;++x)bright[y*w+x]={8,8,8,.5f};auto light=image(d,w,h,bright);
    settings={};settings.bloom=true;settings.bloomThreshold=1;settings.bloomStrength=1;
    auto glow=read(d,c,effects.finish(c,light.view.Get(),settings));require(glow[32*w+24][0]>.01f&&close_value(glow[32*w+24][3],.5f),"Bloom extracts bright energy, blurs beyond source and preserves scene alpha");
    settings.bloomThreshold=16;auto threshold=read(d,c,effects.finish(c,light.view.Get(),settings));require(threshold[32*w+24][0]==0,"Bloom threshold rejects subthreshold source");

    std::vector<Pixel> edge(w*h);for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){float v=float(x)>float(y)*.63f+12?1.f:0.f;edge[y*w+x]={v,v,v,1};}auto staircase=image(d,w,h,edge);settings={};settings.fxaa=true;auto aa=read(d,c,effects.finish(c,staircase.view.Get(),settings));
    size_t smoothed=0;for(const auto&p:aa)smoothed+=p[0]>.01f&&p[0]<.99f;require(smoothed>30,"FXAA smooths a real diagonal staircase");
    auto constant=image(d,w,h,std::vector<Pixel>(w*h,Pixel{.25f,.5f,.75f,.375f}));auto noEdge=read(d,c,effects.finish(c,constant.view.Get(),settings));require(close_value(noEdge[0][0],.25f,.0001f)&&close_value(noEdge[0][3],.375f,.0001f),"FXAA leaves flat color and alpha unchanged");

    std::vector<Pixel> white(w*h,Pixel{1,1,1,.75f});auto whiteImage=image(d,w,h,white);std::vector<float> depths(w*h,depthOf(4000,projection));auto flat=depth(d,w,h,depths);
    settings={};settings.ssao=true;settings.aoRadius=1200;settings.aoStrength=2;auto flatAo=read(d,c,effects.opaque(c,{whiteImage.view.Get(),flat.view.Get(),nullptr,projection},settings));require(flatAo==white,"SSAO does not darken an isolated flat plane");
    for(unsigned y=16;y<48;++y)for(unsigned x=28;x<36;++x)depths[y*w+x]=depthOf(3300,projection);auto raised=depth(d,w,h,depths);
    auto ao=read(d,c,effects.opaque(c,{whiteImage.view.Get(),raised.view.Get(),nullptr,projection},settings));float contact=1;for(unsigned y=20;y<44;++y)for(unsigned x=21;x<28;++x)contact=std::min(contact,ao[y*w+x][0]);require(contact<.97f&&ao[2*w+2][0]>.999f,"SSAO derives normals and local occlusion from reverse depth");
    for(unsigned y=16;y<48;++y)for(unsigned x=28;x<36;++x)depths[y*w+x]=depthOf(1000,projection);auto distant=depth(d,w,h,depths);auto farAo=read(d,c,effects.opaque(c,{whiteImage.view.Get(),distant.view.Get(),nullptr,projection},settings));require(farAo[32*w+26][0]>.999f,"SSAO distance cutoff prevents remote silhouette halos");
    auto sky=depth(d,w,h,std::vector<float>(w*h,0));auto skyAo=read(d,c,effects.opaque(c,{whiteImage.view.Get(),sky.view.Get(),nullptr,projection},settings));require(skyAo==white,"SSAO leaves clear reverse-Z sky unchanged");

    settings={};settings.ssr=true;settings.ssrDistance=8000;settings.ssrThickness=120;settings.ssrStrength=1;settings.aoBias=5;
    require(effects.opaque(c,{whiteImage.view.Get(),flat.view.Get(),nullptr,projection},settings)==whiteImage.view.Get(),"SSR requires an explicit reflective material mask");
    auto zeroMask=image(d,w,h,std::vector<Pixel>(w*h,Pixel{0,0,0,0}));auto maskedOff=read(d,c,effects.opaque(c,{whiteImage.view.Get(),flat.view.Get(),zeroMask.view.Get(),projection},settings));require(maskedOff==white,"SSR mask zero prevents reflection");
    std::vector<Pixel> scene(w*h,Pixel{.125f,.125f,.125f,.5f}),mask(w*h,Pixel{});
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){float rx=((float(x)+.5f)/w*2-1)/projection.projection[0];float z=4000/(1-2*rx);depths[y*w+x]=z>10&&z<500000?depthOf(z,projection):0;if(x>=16&&x<32){depths[y*w+x]=depthOf(3500,projection);scene[y*w+x]={4,.125f,.125f,.5f};}if(x>=40&&x<48&&y>=16&&y<48)mask[y*w+x]={1,0,0,0};}
    auto reflectiveScene=image(d,w,h,scene),materials=image(d,w,h,mask);auto reflectiveDepth=depth(d,w,h,depths);
    auto reflection=read(d,c,effects.opaque(c,{reflectiveScene.view.Get(),reflectiveDepth.view.Get(),materials.view.Get(),projection},settings));float reflectedRed=0;for(unsigned y=16;y<48;++y)for(unsigned x=40;x<48;++x)reflectedRed=std::max(reflectedRed,reflection[y*w+x][0]-.125f);std::cout<<"SSAO contact="<<contact<<" SSR reflected red="<<reflectedRed<<" FXAA softened="<<smoothed<<'\n';require(reflectedRed>.02f,"SSR ray marching finds a real in-screen depth intersection");
    auto noHit=read(d,c,effects.opaque(c,{whiteImage.view.Get(),flat.view.Get(),materials.view.Get(),projection},settings));require(noHit==white,"SSR no hit/offscreen ray adds no invented reflection");
    for(auto&p:mask)if(p[0]>0)p[1]=1;auto rough=image(d,w,h,mask);auto roughResult=read(d,c,effects.opaque(c,{reflectiveScene.view.Get(),reflectiveDepth.view.Get(),rough.view.Get(),projection},settings));require(close_value(roughResult[32*w+44][0],.125f,.0001f),"SSR roughness mask disables mirror reflection");

    auto invalidProjection=projection;invalidProjection.inverseProjection.fill(0);rejected([&]{effects.opaque(c,{whiteImage.view.Get(),flat.view.Get(),materials.view.Get(),invalidProjection},settings);},"Reject invalid inverse projection");
    rejected([&]{effects.opaque(c,{whiteImage.view.Get(),whiteImage.view.Get(),materials.view.Get(),projection},settings);},"Reject color texture used as reverse depth");
    rejected([&]{effects.finish(nullptr,source.view.Get(),settings,true);},"Reject null context before recording commands");
    ComPtr<ID3D11DeviceContext> deferred;ok(d->CreateDeferredContext(0,&deferred));rejected([&]{effects.finish(deferred.Get(),source.view.Get(),settings,true);},"Reject deferred caller before recording commands");
    auto tinyImage=image(d,17,9,std::vector<Pixel>(17*9,Pixel{.2f,.4f,.6f,.8f}));settings={};settings.fxaa=true;auto resized=read(d,c,effects.finish(c,tinyImage.view.Get(),settings));require(resized.size()==17*9&&effects.width()==17&&effects.height()==9&&effects.allocated_bytes()==(17*9*2+9*5*2)*8,"Resize recreates bounded odd-size targets");
    auto nonfinite=image(d,1,1,{Pixel{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-1,.5f}});settings.hdr=true;auto sanitized=read(d,c,effects.finish(c,nonfinite.view.Get(),settings,true));require(std::all_of(sanitized[0].begin(),sanitized[0].end(),[](float v){return std::isfinite(v);})&&sanitized[0][0]==0&&sanitized[0][1]==0,"Enabled effects sanitize nonfinite input channels");
}
}
int main(){try{
    for(auto requested:{D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_0}){
        ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL actual{};
        ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,&requested,1,D3D11_SDK_VERSION,&device,&actual,&context));
        require(actual==requested,"Requested WARP feature level");run(device.Get(),context.Get());
    }
    std::cout<<"Native optional post effects WARP PASS: "<<checks<<" checks (feature levels 11.0 and 10.0)\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
