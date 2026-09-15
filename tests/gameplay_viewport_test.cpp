#include "sop_visuals.h"
#include "material_effects_overlay.h"
#include "water_effects_overlay.h"
#include "bullet_decals_overlay.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2win;
using Microsoft::WRL::ComPtr;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void ok(HRESULT result){check(SUCCEEDED(result),"viewport D3D11 operation");}
struct Image {unsigned width=0,height=0;std::vector<uint8_t> pixels;};
Image capture(ID3D11Device*d,ID3D11DeviceContext*c,const CharacterRenderer&r){
 ComPtr<ID3D11Resource> resource;r.view()->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;ok(resource.As(&texture));D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
 Image result{desc.Width,desc.Height,{}};result.pixels.resize(size_t(result.width)*result.height*4);desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
 ComPtr<ID3D11Texture2D> staging;ok(d->CreateTexture2D(&desc,nullptr,&staging));c->CopyResource(staging.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE map{};ok(c->Map(staging.Get(),0,D3D11_MAP_READ,0,&map));
 for(unsigned y=0;y<result.height;++y)std::copy_n(static_cast<const uint8_t*>(map.pData)+size_t(y)*map.RowPitch,size_t(result.width)*4,result.pixels.data()+size_t(y)*result.width*4);c->Unmap(staging.Get(),0);return result;
}
size_t different(const Image&a,const Image&b){check(a.width==b.width&&a.height==b.height,"same viewport extent");size_t n=0;for(size_t i=0;i<a.pixels.size();i+=4)if(!std::equal(a.pixels.begin()+i,a.pixels.begin()+i+4,b.pixels.begin()+i))++n;return n;}
void bitmap(const std::filesystem::path&path,const Image&image){if(path.empty())return;auto data=image.pixels;for(size_t i=0;i<data.size();i+=4)std::swap(data[i],data[i+2]);BITMAPFILEHEADER f{};BITMAPINFOHEADER h{};h.biSize=sizeof(h);h.biWidth=LONG(image.width);h.biHeight=-LONG(image.height);h.biPlanes=1;h.biBitCount=32;h.biSizeImage=DWORD(data.size());f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(h);f.bfSize=f.bfOffBits+h.biSizeImage;std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&f),sizeof(f));out.write(reinterpret_cast<const char*>(&h),sizeof(h));out.write(reinterpret_cast<const char*>(data.data()),std::streamsize(data.size()));check(bool(out),"viewport bitmap write");}
CharacterModel panel(float z,float extent,float red=.1f){CharacterModel m;m.bounds={-extent,-extent,z,extent,extent,z};for(auto xy:std::array<std::array<float,2>,4>{{{-extent,-extent},{extent,-extent},{extent,extent},{-extent,extent}}}){ModelVertex v{};v.x=xy[0];v.y=xy[1];v.z=z;v.nz=-1;v.lr=red;v.lg=.15f;v.lb=.3f;v.lit=1;m.vertices.push_back(v);}m.indices={0,1,2,0,2,3};m.parts.push_back({0,6,0,0});m.textures.push_back({4,4,9,{255,255,255,255,0,0,0,0}});return m;}
PreparedCharacter fixture(){
 PreparedCharacter b;constexpr uint32_t keys[]={0xa89233,0x7011c4,0x6c02b2,0xf8d3cc,0xf5d387,0x019543,0x027a4c,0xfafa8b,0xfbfa42,0x619d43,0x62824c,0x5b028c,0x5c0243,0x449d08,0xf7f0c4,0xfb4232,0xf81206,0x459d14,0xfaf104,0x5b4a33,0xfb1246};
 constexpr sop::Vec3 p[]={{0,850,0},{0,1000,0},{0,1150,0},{0,1400,0},{0,1500,0},{-120,1250,0},{-220,1250,0},{-430,1120,0},{-620,1000,0},{120,1250,0},{220,1250,0},{430,1120,0},{620,1000,0},{-120,830,0},{-130,440,0},{-130,80,0},{-130,60,180},{120,830,0},{130,440,0},{130,80,0},{130,60,180}};
 for(unsigned i=0;i<21;++i)b.bonePositions[keys[i]]=p[i];return b;
}
void cpu_overlays(enemy_tag::Viewport vp){
 const stage::Collision world;const stage::Vec3 eye{0,900,0},direction{0,0,1},center{250,900,1000};
 auto point=enemy_tag::project(center,eye,direction,vp.left,vp.top,vp.width,vp.height,vp.aspect);check(bool(point),"CPU enemy-tag projection in viewport");
 std::vector<uint32_t> pixels(1280*720);auto checkPixels=[&]{size_t n=0;for(int y=0;y<720;++y)for(int x=0;x<1280;++x)if(pixels[size_t(y)*1280+x]){++n;check(x>=vp.left&&x<vp.left+vp.width&&y>=vp.top&&y<vp.top+vp.height,"overlay clipped to selected viewport");}check(n>0,"overlay draws visible viewport pixels");check(pixels[size_t(point->y)*1280+point->x]!=0,"overlay agrees with enemy-tag projection");};
 combat::material_effects::Line line{{230,900,1000},{270,900,1000},{1,.5f,.1f,1}};
 combat::material_effects::paint(pixels,std::span(&line,1),eye,direction,world,nullptr,vp);checkPixels();std::fill(pixels.begin(),pixels.end(),0);
 stage::WaterEffectLine water{{230,900,1000},{270,900,1000},1};stage::paint_water(pixels,1280,720,std::span(&water,1),eye,direction,world,nullptr,vp);checkPixels();std::fill(pixels.begin(),pixels.end(),0);
 combat::decals::Decal decal;decal.position=center;decal.normal={0,0,-1};decal.tangent={1,0,0};decal.bitangent={0,1,0};decal.radius=20;decal.alpha=1;
 combat::decals::paint(pixels,std::span(&decal,1),eye,direction,world,nullptr,vp);checkPixels();
 std::fill(pixels.begin(),pixels.end(),0);vp.left=-1;combat::material_effects::paint(pixels,std::span(&line,1),eye,direction,world,nullptr,vp);stage::paint_water(pixels,1280,720,std::span(&water,1),eye,direction,world,nullptr,vp);combat::decals::paint(pixels,std::span(&decal,1),eye,direction,world,nullptr,vp);check(std::all_of(pixels.begin(),pixels.end(),[](auto p){return !p;}),"invalid overlay viewport is atomic no-op");
}
}
int main(int argc,char**argv){try{
 std::filesystem::path out=argc>2?argv[2]:"";if(!out.empty())std::filesystem::create_directories(out);
 ComPtr<ID3D11Device>d;ComPtr<ID3D11DeviceContext>c;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c));
 auto wallModel=panel(1500,5000),markerModel=panel(0,60,1);CharacterRenderer wall(d.Get(),wallModel),marker(d.Get(),markerModel);sop::Renderer sopRenderer(d.Get());auto body=fixture();sop::Gate gate{true,true,true,false};
 WorldView view{{0,900,0},{0,0,1}};wall.render(c.Get(),0,false,&view);auto initial=capture(d.Get(),c.Get(),wall);check(initial.width==616&&initial.height==392,"existing 616x392 target unchanged");
 for(const auto vp:std::array<enemy_tag::Viewport,3>{{{620,120,616,392,616.f/392.f},{0,0,1280,720,1280.f/720.f},{100,60,1000,600,1000.f/600.f}}}){
  wall.resize_target(d.Get(),unsigned(vp.width),unsigned(vp.height));view.aspect=vp.aspect;wall.render(c.Get(),0,false,&view);auto baseline=capture(d.Get(),c.Get(),wall);check(baseline.width==unsigned(vp.width)&&baseline.height==unsigned(vp.height),"resize changes both surface dimensions");
  auto* same=wall.view();wall.resize_target(d.Get(),unsigned(vp.width),unsigned(vp.height));check(same==wall.view()&&capture(d.Get(),c.Get(),wall).pixels==baseline.pixels,"same-size resize preserves resources and pixels");
  const std::array<float,3> origin{250,900,1000};marker.render(c.Get(),0,false,&view,&wall,&origin);auto actor=capture(d.Get(),c.Get(),wall);check(different(baseline,actor)>100,"ordinary actor draws onto resized world surface");
  auto point=enemy_tag::project(origin,view.eye,view.direction,0,0,vp.width,vp.height,vp.aspect);check(bool(point),"project marker center");size_t at=(size_t(point->y)*vp.width+point->x)*4;check(actor.pixels[at]>baseline.pixels[at]+50,"GPU actor and CPU enemy-tag projection agree");
  check(sopRenderer.doll(c.Get(),body,wall,view,{0,0,2600},0,gate),"SOP doll uses resized shared target");auto doll=capture(d.Get(),c.Get(),wall);check(different(actor,doll)>500,"SOP visible in resized framebuffer");
  wall.render(c.Get(),0,false,&view);check(sopRenderer.scan(c.Get(),wall,view,{{0,900,0},1600,500,.4f},gate),"SOP scan on resized stage");auto scan=capture(d.Get(),c.Get(),wall);check(different(baseline,scan)>500,"SOP scan shares world projection and depth");
  wall.render(c.Get(),0,false,&view);check(capture(d.Get(),c.Get(),wall).pixels==baseline.pixels,"ordinary draw exactly restored after SOP pass");
  for(auto extent:std::array<std::array<unsigned,2>,4>{{{0,720},{1280,0},{8193,720},{1280,8193}}}){bool rejected=false;try{wall.resize_target(d.Get(),extent[0],extent[1]);}catch(const std::invalid_argument&){rejected=true;}check(rejected&&same==wall.view()&&capture(d.Get(),c.Get(),wall).pixels==baseline.pixels,"invalid resize atomically preserves resource and contents");}
  bool rejected=false;try{wall.resize_target(nullptr,1280,720);}catch(const std::invalid_argument&){rejected=true;}check(rejected&&same==wall.view(),"null-device resize rejected atomically");
  for(float aspect:{0.f,-1.f,33.f,std::numeric_limits<float>::quiet_NaN()}){auto invalid=view;invalid.aspect=aspect;rejected=false;try{wall.render(c.Get(),0,false,&invalid);}catch(const std::invalid_argument&){rejected=true;}check(rejected&&capture(d.Get(),c.Get(),wall).pixels==baseline.pixels,"invalid world aspect preserves target");check(!sopRenderer.doll(c.Get(),body,wall,invalid,{0,0,2600},0,gate)&&!sopRenderer.scan(c.Get(),wall,invalid,{{0,900,0},1600,500,.4f},gate)&&capture(d.Get(),c.Get(),wall).pixels==baseline.pixels,"SOP rejects invalid aspect without output changes");}
  cpu_overlays(vp);if(!out.empty()){auto prefix=std::to_string(vp.width)+"x"+std::to_string(vp.height);bitmap(out/(prefix+"-character-sop.bmp"),doll);bitmap(out/(prefix+"-scan.bmp"),scan);}
 }
 wall.resize_target(d.Get(),616,392);view.aspect=616.f/392.f;wall.render(c.Get(),0,false,&view);check(capture(d.Get(),c.Get(),wall).pixels==initial.pixels,"return to legacy viewport is pixel-identical");
 if(argc>1){std::ifstream in(argv[1],std::ios::binary);check(bool(in),"original appearance fixture");std::vector<char> bytes{std::istreambuf_iterator<char>(in),{}};CharacterCatalog catalog(bytes);wall.resize_target(d.Get(),1280,720);view.aspect=1280.f/720.f;
  for(unsigned gender=0;gender<2;++gender){std::array<uint8_t,28>a{};a[0]=uint8_t(gender);a[2]=11;a[3]=22;a[15]=46;a[17]=57;auto actual=catalog.assemble(a);catalog.pose(actual,.1);wall.render(c.Get(),0,false,&view);auto baseline=capture(d.Get(),c.Get(),wall);check(sopRenderer.doll(c.Get(),actual,wall,view,{0,0,2600},0,gate),"original male/female rig on full-size target");auto result=capture(d.Get(),c.Get(),wall);check(different(baseline,result)>500,"original rig full-size SOP pixels");if(!out.empty())bitmap(out/("original-"+std::to_string(gender)+".bmp"),result);}
 }
 std::cout<<"Gameplay viewport PASS: legacy/full/non16:9, shared character/SOP surfaces, projection, 3 CPU overlays, atomic invalid resize/aspect, optional original rigs\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
