#include "stage_weather_renderer.h"
#include "water_renderer.h"
#include "tracer_renderer.h"
#include <DirectXPackedVector.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace mgo2mt;
using Microsoft::WRL::ComPtr;
namespace {
using Pixel=std::array<float,4>;
using Pixels=std::vector<Pixel>;
int checks=0;
void check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
void ok(HRESULT h){if(FAILED(h))throw std::runtime_error("HDR auxiliary WARP resource creation");}
float linear(float value){return value<=.04045f?value/12.92f:std::pow((value+.055f)/1.055f,2.4f);}
float color(float value,bool hdr){return hdr?linear(value):value;}
bool close(float actual,float expected,bool hdr){return std::abs(actual-expected)<(hdr?.0025f:.008f);}
Pixels capture(ID3D11Device* device,ID3D11DeviceContext* context,const CharacterRenderer& surface){
    ComPtr<ID3D11Resource> resource;surface.view()->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;ok(resource.As(&texture));D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);auto format=desc.Format;
    check(format==(surface.hdr()?DXGI_FORMAT_R16G16B16A16_FLOAT:DXGI_FORMAT_R8G8B8A8_UNORM),"Actual target format follows HDR switch");
    desc.BindFlags=desc.MiscFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> copy;ok(device->CreateTexture2D(&desc,nullptr,&copy));context->CopyResource(copy.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE mapped{};ok(context->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped));
    Pixels output(size_t(desc.Width)*desc.Height);
    for(unsigned y=0;y<desc.Height;++y)for(unsigned x=0;x<desc.Width;++x)for(unsigned channel=0;channel<4;++channel){
        const auto row=static_cast<const uint8_t*>(mapped.pData)+y*mapped.RowPitch;float value;
        if(surface.hdr()){uint16_t half;std::memcpy(&half,row+(x*4+channel)*2,2);value=DirectX::PackedVector::XMConvertHalfToFloat(half);}else value=row[x*4+channel]/255.f;
        output[size_t(y)*desc.Width+x][channel]=value;
    }
    context->Unmap(copy.Get(),0);return output;
}
CharacterModel panel(float z){
    CharacterModel model;model.bounds={-100000,-100000,z,100000,100000,z};
    for(auto p:std::array<std::array<float,2>,4>{{{-100000,-100000},{100000,-100000},{100000,100000},{-100000,100000}}}){
        ModelVertex vertex{};vertex.x=p[0];vertex.y=p[1];vertex.z=z;vertex.nz=-1;vertex.lr=.1f;vertex.lg=.15f;vertex.lb=.3f;vertex.lit=1;model.vertices.push_back(vertex);
    }
    model.indices={0,1,2,0,2,3};model.parts.push_back({0,6,0,0});model.textures.push_back({4,4,9,{255,255,255,255,0,0,0,0}});return model;
}
CharacterModel ground(){
    auto model=panel(0);model.bounds={-100000,0,-100000,100000,1,100000};
    for(auto& vertex:model.vertices){vertex.z=vertex.y;vertex.y=0;vertex.nz=0;vertex.ny=1;vertex.lr=vertex.lg=vertex.lb=.3f;}return model;
}
void bundle(const std::filesystem::path& path){
    std::ofstream output(path,std::ios::binary);output.write("GWFX",4);
    for(uint32_t value:{1u,1u,1u,4u,4u,9u,8u})output.write(reinterpret_cast<const char*>(&value),4);
    const unsigned char bc1[]{255,255,255,255,0,0,0,0};output.write(reinterpret_cast<const char*>(bc1),8);check(bool(output),"Synthetic texture bundle written");
}
void run(ID3D11Device* device,ID3D11DeviceContext* context,const std::filesystem::path& path){
    bundle(path);stage::weather::Renderer weather(device,path);
    water_visuals::Mesh waterMesh;waterMesh.vertices={{{-100000,-100000,3000}},{{100000,-100000,3000}},{{100000,100000,3000}},{{-100000,100000,3000}}};waterMesh.indices={0,1,2,0,2,3};
    water_visuals::Renderer water(device,waterMesh,{.5f,.35f});combat::tracers::Renderer tracer(device);
    CharacterRenderer surface(device,panel(6000)),floor(device,ground());
    WorldView view{{0,0,0},{0,0,1},1,1};WorldView down{{0,2000,-3000},{0,-.5f,1},1,.7f};
    constexpr unsigned width=64,height=64;constexpr size_t at=32*width+32;
    auto grid=std::make_shared<stage::weather::SurfaceGrid>();grid->originX=grid->originZ=-32000;grid->cells.resize(4096,{0,1,0,1});stage::weather::Frame snow;snow.surface=grid;
    stage::weather::Frame fog;fog.active=true;fog.strength=1;fog.nearDistance=1000;fog.farDistance=10000;fog.maximum=.65f;fog.color={.5f,.25f,.75f};
    std::array<Pixels,5> legacy;
    for(unsigned round=0;round<3;++round){
        const bool hdr=round==1;surface.resize_target(device,width,height,hdr);floor.resize_target(device,width,height,hdr);
        auto reset=[&]{surface.render(context,0,false,&view);return capture(device,context,surface);};
        auto baseline=reset();check(weather.render(context,surface,view,fog),"Fog renders");auto fogged=capture(device,context,surface);
        const float fogAlpha=(6000-1000.f)/(10000-1000)*.65f;
        for(unsigned channel=0;channel<3;++channel)check(close(fogged[at][channel],baseline[at][channel]*(1-fogAlpha)+color(fog.color[channel],hdr)*fogAlpha,hdr),"Fog settings decoded exactly once before blending");
        check(fogged[at][3]==baseline[at][3],"Fog alpha preserved");

        baseline=reset();check(water.render(context,surface,view),"Water renders");auto waterPixels=capture(device,context,surface);
        for(unsigned channel=0;channel<3;++channel)check(close(waterPixels[at][channel],baseline[at][channel]*.65f+color(.5f,hdr)*.35f,hdr),"Water setting decoded before linear alpha blend");

        combat::tracers::Segment segment{{-1200,0,3000},{1200,0,3000},.5f,4,{.5f,.25f,.75f}};
        baseline=reset();check(tracer.render(context,surface,view,{&segment,1}),"Custom precipitation segment renders");auto tracerPixels=capture(device,context,surface);
        for(unsigned channel=0;channel<3;++channel)check(close(tracerPixels[at][channel],baseline[at][channel]*.5f+color(segment.color[channel],hdr)*.5f,hdr),"Custom tracer/precipitation setting decoded before blending");
        segment.widthPixels=0;segment.opacity=1;baseline=reset();check(tracer.render(context,surface,view,{&segment,1}),"Default two-layer tracer renders");auto bulletPixels=capture(device,context,surface);
        check(close(bulletPixels[at][0],1,hdr)&&close(bulletPixels[at][1],1,hdr)&&close(bulletPixels[at][2],color(.8f,hdr),hdr),"Original native bullet core color decoded once");

        floor.render(context,0,false,&down);auto groundPixels=capture(device,context,floor);check(weather.render(context,floor,down,snow),"Snow accumulation renders without fog");auto snowPixels=capture(device,context,floor);
        float illumination=std::clamp((groundPixels[at][0]*.2126f+groundPixels[at][1]*.7152f+groundPixels[at][2]*.0722f)*1.5f,.22f,1.f);
        const float tint[]{.84f,.88f,.94f};for(unsigned channel=0;channel<3;++channel)check(close(snowPixels[at][channel],groundPixels[at][channel]*.08f+color(tint[channel],hdr)*illumination*.92f,hdr),"Snow tint decoded while sampled linear scene remains linear");
        check(snowPixels[at][3]==groundPixels[at][3],"Snow alpha preserved");
        std::array<Pixels,5> outputs{fogged,waterPixels,tracerPixels,bulletPixels,snowPixels};
        if(round==0)legacy=outputs;if(round==2)check(outputs==legacy,"HDR OFF restores every previous weather/water/tracer pixel exactly");
        if(hdr)std::cout<<"linear fog="<<fogged[at][0]<<" water="<<waterPixels[at][0]<<" tracer="<<tracerPixels[at][0]<<" snow="<<snowPixels[at][0]<<'\n';
    }
}
}
int main(int argc,char** argv){try{
    const auto folder=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/("mgo-hdr-aux-"+std::to_string(GetCurrentProcessId()));std::filesystem::create_directories(folder);
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
    run(device.Get(),context.Get(),folder/"synthetic-weather.gwfx");std::cout<<"HDR auxiliary WARP PASS: "<<checks<<" checks\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
