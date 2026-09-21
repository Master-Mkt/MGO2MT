#include "character_renderer.h"
#include "stage_lighting.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
using namespace mgo2mt;
using Microsoft::WRL::ComPtr;
namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void ok(HRESULT h){check(SUCCEEDED(h),"Dynamic light WARP failure");}
using Pixels=std::vector<uint8_t>;
Pixels capture(ID3D11Device*d,ID3D11DeviceContext*c,const CharacterRenderer&r){
    ComPtr<ID3D11Resource> resource;r.view()->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;ok(resource.As(&texture));D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
    desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> staging;ok(d->CreateTexture2D(&desc,nullptr,&staging));c->CopyResource(staging.Get(),texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};ok(c->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));Pixels pixels(616*392*4);
    for(size_t y=0;y<392;++y)std::copy_n(static_cast<uint8_t*>(mapped.pData)+y*mapped.RowPitch,616*4,pixels.data()+y*616*4);c->Unmap(staging.Get(),0);return pixels;
}
void bitmap(const std::filesystem::path& path,const Pixels& rgba){
    Pixels bgra=rgba;for(size_t i=0;i<bgra.size();i+=4)std::swap(bgra[i],bgra[i+2]);BITMAPFILEHEADER f{};BITMAPINFOHEADER b{};b.biSize=40;b.biWidth=616;b.biHeight=-392;b.biPlanes=1;b.biBitCount=32;b.biSizeImage=DWORD(bgra.size());f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(b);f.bfSize=f.bfOffBits+b.biSizeImage;
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&f),sizeof(f));out.write(reinterpret_cast<const char*>(&b),sizeof(b));out.write(reinterpret_cast<const char*>(bgra.data()),bgra.size());check(bool(out),"BMP write");
}
CharacterModel panel(float z=2000,float extent=4000,bool authored=true){
    CharacterModel m;m.bounds={-extent,-extent,z,extent,extent,z};
    for(auto xy:std::array<std::array<float,2>,4>{{{-extent,-extent},{extent,-extent},{extent,extent},{-extent,extent}}}){
        ModelVertex v{};v.x=xy[0];v.y=xy[1];v.z=z;v.nz=-1;v.lr=v.lg=v.lb=.1f;v.lit=1;
        if(authored){v.ar=.2f;v.ag=.1f;v.ab=.05f;}m.vertices.push_back(v);
    }
    m.indices={0,1,2,0,2,3};m.parts.push_back({0,6,0,0,authored?0x120000u:0u});m.textures.push_back({4,4,9,{255,255,255,255,0,0,0,0}});return m;
}
void similar(const Pixels&a,const Pixels&b,int tolerance,const char*why){check(a.size()==b.size(),why);for(size_t i=0;i<a.size();++i)check(std::abs(int(a[i])-int(b[i]))<=tolerance,why);}
std::array<int,3> center(const Pixels&p){size_t at=(196*616+308)*4;return {p[at],p[at+1],p[at+2]};}
void rgb(const Pixels&p,std::array<int,3> expected,const char* why){auto v=center(p);for(unsigned i=0;i<3;++i)check(std::abs(v[i]-expected[i])<=1,why);}
std::vector<char> read(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);check(bool(f),"Input file");return {(std::istreambuf_iterator<char>(f)),{}};}
WorldView camera{{0,0,0},{0,0,1}};

#ifndef DYNAMIC_LIGHT_LEGACY_REFERENCE
void synthetic(ID3D11Device*d,ID3D11DeviceContext*c,const std::filesystem::path&out){
    auto model=panel();CharacterRenderer renderer(d,model);
    auto draw=[&](std::span<const DynamicPointLight> lights){renderer.render(c,0,false,&camera,nullptr,nullptr,lights);return capture(d,c,renderer);};
    auto baseline=draw({});rgb(baseline,{128,77,51},"Authored/hemisphere baseline preserved");
    {auto unlit=panel(2000,4000,false);for(auto&v:unlit.vertices)v.lit=0;CharacterRenderer actor(d,unlit);stage::Lighting field;field.axis={0,0,1};field.front={.6f,.2f,.1f};field.back={.1f,.2f,.6f};auto environment=field.environment({0,0,0});
     auto envDraw=[&](float yaw,const WorldView& view,const EnvironmentLight* env){std::array<float,3> origin{};actor.render(c,yaw,false,&view,nullptr,&origin,{},nullptr,nullptr,env);return capture(d,c,actor);};
     const auto previous=envDraw(0,camera,nullptr);rgb(envDraw(0,camera,&environment),{153,51,26},"GPU actor front hemisphere");WorldView reverse{{0,0,-4000},{0,0,1}};rgb(envDraw(3.14159265f,reverse,&environment),{26,51,153},"Actor world yaw rotates hemisphere normal");check(envDraw(0,camera,nullptr)==previous,"No environment clears previous actor constants");
     stage::Hemisphere volume;volume.flags=0x100;volume.minimum={-100,-100,-100};volume.maximum={100,100,100};volume.extent={10,10,10};volume.positive=volume.negative={.02f,.02f,.02f};volume.direction={0,0,1};volume.front={.2f,.8f,.1f};volume.back={.1f,.1f,.1f};field.hemispheres.push_back(volume);environment=field.environment({0,0,0});check(environment.volumes==1,"Actor inside original hemisphere volume");rgb(envDraw(0,camera,&environment),{51,204,26},"Actor position selects volume color");environment=field.environment({101,0,0});check(!environment.volumes,"Actor outside volume uses global fallback");rgb(envDraw(0,camera,&environment),{153,51,26},"Outside volume restores global hemisphere");
     environment.front[0]=std::numeric_limits<float>::quiet_NaN();bool rejected=false;try{envDraw(0,camera,&environment);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Nonfinite actor environment rejected");}
    DynamicPointLight light{{0,0,1000},{1,0,0},2000,.4f};std::array lights{light};
    auto lit=draw(lights);rgb(lit,{179,77,51},"Red linear half-radius contribution");
    lights[0].radius=4000;rgb(draw(lights),{204,77,51},"Larger radius linear attenuation");
    lights[0]=light;lights[0].position[2]=3000;check(draw(lights)==baseline,"Back-facing light dark");
    lights[0]=light;lights[0].radius=1000;check(draw(lights)==baseline,"Strict radius boundary/outside unchanged");
    lights[0]=light;lights[0].intensity=0;check(draw(lights)==baseline,"Zero intensity unchanged");
    check(draw({})==baseline,"Removal restores exact authored lighting");
    std::array<DynamicPointLight,8> eight;for(auto& l:eight)l={{0,0,1000},{.1f,0,0},2000,.1f};rgb(draw(eight),{138,77,51},"Eight bounded additive lights");
    // Compare GPU center with existing CPU point sample (equal range subset).
    stage::PointLight cpu;cpu.position=light.position;cpu.color={.4f,0,0};cpu.range=cpu.extendedRange=2000;cpu.flags=0x100;
    const auto contribution=stage::Lighting::point_sample(cpu,{0,0,2000},{0,0,-1});check(std::abs(contribution[0]-.2f)<.00001f,"Existing CPU linear point oracle");
    auto before=draw(lights);
    auto reject=[&](std::span<const DynamicPointLight> invalid){bool threw=false;try{draw(invalid);}catch(const std::invalid_argument&){threw=true;}check(threw,"Invalid dynamic light rejected");check(capture(d,c,renderer)==before,"Invalid input changed render target");};
    std::array<DynamicPointLight,9> nine;for(auto& l:nine)l=light;reject(nine);
    for(unsigned field=0;field<8;++field){lights[0]=light;switch(field){case 0:lights[0].position[0]=std::numeric_limits<float>::quiet_NaN();break;case 1:lights[0].color[1]=std::numeric_limits<float>::infinity();break;case 2:lights[0].radius=0;break;case 3:lights[0].intensity=-1;break;case 4:lights[0].color[0]=2;break;case 5:lights[0].position[1]=1e7f;break;case 6:lights[0].radius=1e6f+1;break;case 7:lights[0].intensity=17;break;}reject(lights);}
    lights[0]=light;
    const std::array<float,3> shift{12345,6789,-5432};WorldView shifted{shift,{0,0,1}};
    auto movedLight=light;for(unsigned i=0;i<3;++i)movedLight.position[i]+=shift[i];std::array moved{movedLight};
    renderer.render(c,0,false,&shifted,nullptr,&shift,moved);similar(capture(d,c,renderer),lit,1,"World/camera/light common translation");
    renderer.render(c,0,false,&shifted,nullptr,&shift,std::array{light});rgb(capture(d,c,renderer),{128,77,51},"World light must not follow translated actor");
    WorldView rotated{{0,0,0},{1,0,0}};auto rotatedLight=light;rotatedLight.position={1000,0,0};std::array<float,3> origin{};
    renderer.render(c,1.57079632679f,false,&rotated,nullptr,&origin,std::array{rotatedLight});similar(capture(d,c,renderer),lit,1,"World normal/light/camera common rotation");
    auto shiftedModel=model;const std::array<float,3> presentationShift{4096,2048,8192};
    for(auto&v:shiftedModel.vertices){v.x+=presentationShift[0];v.y+=presentationShift[1];v.z+=presentationShift[2];}
    for(unsigned i=0;i<3;++i){shiftedModel.bounds[i]+=presentationShift[i];shiftedModel.bounds[i+3]+=presentationShift[i];}
    CharacterRenderer presentation(d,shiftedModel);auto presentationLight=light;for(unsigned i=0;i<3;++i)presentationLight.position[i]+=presentationShift[i];
    for(bool overview:{false,true}){
        renderer.render(c,.3f,overview);auto dark=capture(d,c,renderer);
        renderer.render(c,.3f,overview,nullptr,nullptr,nullptr,std::array{light});auto bright=capture(d,c,renderer);check(bright!=dark,"Preview/overview receives authored-position light");
        presentation.render(c,.3f,overview,nullptr,nullptr,nullptr,std::array{presentationLight});similar(capture(d,c,presentation),bright,1,"Preview/overview geometry and lights share recenter/yaw");
    }
    // Stage plus two independently transformed actors use the same frame lights.
    auto stage=panel(4000,6000,false),actor=panel(0,500,false),combined=stage;
    std::array<float,3> self{-600,0,2000},remote{700,0,2200};
    for(auto position:{self,remote}){uint32_t first=uint32_t(combined.vertices.size()),indexAt=uint32_t(combined.indices.size());for(auto v:actor.vertices){v.x+=position[0];v.y+=position[1];v.z+=position[2];combined.vertices.push_back(v);}for(auto i:actor.indices)combined.indices.push_back(i+first);combined.parts.push_back({indexAt,6,0,0,0});}
    CharacterRenderer world(d,stage),local(d,actor),peer(d,actor),baked(d,combined);std::array shared{DynamicPointLight{{0,0,1000},{1,.5f,.1f},5000,.3f}};
    world.render(c,0,false,&camera,nullptr,nullptr,shared);local.render(c,0,false,&camera,&world,&self,shared);peer.render(c,0,false,&camera,&world,&remote,shared);auto sharedFrame=capture(d,c,world);
    baked.render(c,0,false,&camera,nullptr,nullptr,shared);similar(sharedFrame,capture(d,c,baked),1,"Stage/self/remote light positions agree with baked world geometry");
    if(!out.empty()){bitmap(out/"panel-no-lights.bmp",baseline);bitmap(out/"panel-red-light.bmp",lit);bitmap(out/"shared-stage-actors.bmp",sharedFrame);}
    std::cout<<"dynamic lights: authored baseline, linear radius/RGB/backface,8lights, invalid-before-GPU, world/camera transforms and stage/self/remote passed\n";
}
void original(ID3D11Device*d,ID3D11DeviceContext*c,const std::filesystem::path&out,const char* modelPath,const char* lightingPath){
    CharacterModel model(read(modelPath));std::ifstream in(lightingPath);auto lighting=stage::Lighting::read(in);
    for(auto&v:model.vertices){auto sample=lighting.sample({v.x,v.y,v.z},{v.nx,v.ny,v.nz});v.lr=sample.color[0];v.lg=sample.color[1];v.lb=sample.color[2];v.lit=1;}
    CharacterRenderer renderer(d,model);WorldView view{{-45300,2400,31500},{2421,-1250,-1718}};
    renderer.render(c,0,false,&view);auto before=capture(d,c,renderer);
    std::array lights{DynamicPointLight{{-42700,1600,30200},{1,.65f,.25f},4500,1.5f}};
    renderer.render(c,0,false,&view,nullptr,nullptr,lights);auto after=capture(d,c,renderer);size_t changed=0;uint64_t a=0,b=0;
    for(size_t i=0;i<before.size();i+=4){if(!std::equal(before.begin()+i,before.begin()+i+3,after.begin()+i))++changed;for(unsigned k=0;k<3;++k){a+=before[i+k];b+=after[i+k];}}
    check(changed>500&&b>a,"Original n022a illumination response");renderer.render(c,0,false,&view);check(capture(d,c,renderer)==before,"Original stage baseline restored after light removal");
    bitmap(out/"n022a-no-lights.bmp",before);bitmap(out/"n022a-dynamic-light.bmp",after);
    std::cout<<"original n022a WARP changed_pixels="<<changed<<" sum_rgb="<<a<<"->"<<b<<" light removal exact\n";
}
#endif
}
int main(int argc,char**argv){try{
    ComPtr<ID3D11Device>d;ComPtr<ID3D11DeviceContext>c;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c));
    std::filesystem::path out=argc>1?argv[1]:"";if(!out.empty())std::filesystem::create_directories(out);
    // This branch also builds against the saved pre-change renderer source.
    for(unsigned mode=0;mode<2;++mode){auto model=panel();if(mode)for(auto&v:model.vertices)v.lit=0;CharacterRenderer r(d.Get(),model);r.render(c.Get(),0,false,&camera);auto pixels=capture(d.Get(),c.Get(),r);
        if(!out.empty()){auto path=out/("legacy-reference-"+std::to_string(mode)+".rgba");
#ifdef DYNAMIC_LIGHT_LEGACY_REFERENCE
            std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());check(bool(f),"Baseline write");
#else
            if(std::filesystem::exists(path)){auto reference=read(path);check(reference.size()==pixels.size()&&std::equal(pixels.begin(),pixels.end(),reinterpret_cast<const uint8_t*>(reference.data())),"Empty lights differ from pre-change shader pixels");std::cout<<"saved legacy renderer mode"<<mode<<" byte-identical\n";}
#endif
        }
    }
#ifndef DYNAMIC_LIGHT_LEGACY_REFERENCE
    synthetic(d.Get(),c.Get(),out);if(argc==4)original(d.Get(),c.Get(),out,argv[2],argv[3]);
#endif
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
