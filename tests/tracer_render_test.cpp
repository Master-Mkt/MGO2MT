#include "tracer_renderer.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2win;using Microsoft::WRL::ComPtr;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void ok(HRESULT h){check(SUCCEEDED(h),"WARP operation");}
using Pixels=std::vector<uint8_t>;
Pixels capture(ID3D11Device*d,ID3D11DeviceContext*c,const CharacterRenderer&r){
 ComPtr<ID3D11Resource> resource;r.view()->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;ok(resource.As(&texture));D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> staging;ok(d->CreateTexture2D(&desc,nullptr,&staging));c->CopyResource(staging.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE map{};ok(c->Map(staging.Get(),0,D3D11_MAP_READ,0,&map));Pixels pixels(desc.Width*desc.Height*4);for(size_t y=0;y<desc.Height;++y)std::copy_n(static_cast<uint8_t*>(map.pData)+y*map.RowPitch,desc.Width*4,pixels.data()+y*desc.Width*4);c->Unmap(staging.Get(),0);return pixels;
}
void bitmap(const std::filesystem::path&p,const Pixels&rgba){auto pixels=rgba;for(size_t i=0;i<pixels.size();i+=4)std::swap(pixels[i],pixels[i+2]);BITMAPFILEHEADER f{};BITMAPINFOHEADER b{};b.biSize=40;b.biWidth=616;b.biHeight=-392;b.biPlanes=1;b.biBitCount=32;b.biSizeImage=DWORD(pixels.size());f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(b);f.bfSize=f.bfOffBits+b.biSizeImage;std::ofstream out(p,std::ios::binary);out.write(reinterpret_cast<const char*>(&f),sizeof(f));out.write(reinterpret_cast<const char*>(&b),sizeof(b));out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());check(bool(out),"bitmap write");}
size_t differences(const Pixels&a,const Pixels&b){size_t count=0;for(size_t i=0;i<a.size();i+=4)if(!std::equal(a.begin()+i,a.begin()+i+4,b.begin()+i))++count;return count;}
CharacterModel panel(float z,float extent=10000){CharacterModel m;m.bounds={-extent,-extent,z,extent,extent,z};for(auto xy:std::array<std::array<float,2>,4>{{{-extent,-extent},{extent,-extent},{extent,extent},{-extent,extent}}}){ModelVertex v{};v.x=xy[0];v.y=xy[1];v.z=z;v.nz=-1;v.lr=.1f;v.lg=.15f;v.lb=.3f;v.lit=1;m.vertices.push_back(v);}m.indices={0,1,2,0,2,3};m.parts.push_back({0,6,0,0});m.textures.push_back({4,4,9,{255,255,255,255,0,0,0,0}});return m;}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid input rejected");}
}
int main(int argc,char**argv){try{
 std::filesystem::path out=argc>1?argv[1]:"";if(!out.empty())std::filesystem::create_directories(out);
 ComPtr<ID3D11Device>d;ComPtr<ID3D11DeviceContext>c;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c));
 combat::tracers::Renderer tracer(d.Get());auto background=panel(10000);CharacterRenderer surface(d.Get(),background);WorldView view{{0,0,0},{0,0,1}};
 auto base=[&]{surface.render(c.Get(),0,false,&view);return capture(d.Get(),c.Get(),surface);};auto before=base();
 combat::tracers::Segment streak{{-1200,0,3000},{1200,0,5000},1};
 check(tracer.render(c.Get(),surface,view,{&streak,1}),"third person streak render");auto after=capture(d.Get(),c.Get(),surface);check(differences(before,after)>100,"visible third person streak");
 auto wallModel=panel(2000);CharacterRenderer wall(d.Get(),wallModel);wall.render(c.Get(),0,false,&view);auto wallBefore=capture(d.Get(),c.Get(),wall);tracer.render(c.Get(),wall,view,{&streak,1});check(capture(d.Get(),c.Get(),wall)==wallBefore,"world opaque depth occlusion");
 auto bodyModel=panel(2000,250);CharacterRenderer body(d.Get(),bodyModel);base();body.render(c.Get(),0,false,&view,&surface);auto bodyBefore=capture(d.Get(),c.Get(),surface);tracer.render(c.Get(),surface,view,{&streak,1});auto bodyAfter=capture(d.Get(),c.Get(),surface);check(differences(bodyBefore,bodyAfter)>20,"visible segments remain outside foreground object");
 size_t center=(196*616+308)*4;for(size_t i=0;i<4;++i)check(bodyBefore[center+i]==bodyAfter[center+i],"body depth blocks center");
 combat::tracers::Segment head{{0,0,0},{0,0,1200},1};before=base();check(tracer.render(c.Get(),surface,view,{&head,1}),"head on near plane draw");auto self=capture(d.Get(),c.Get(),surface);check(differences(before,self)>=4,"exact coaxial self ray has visible core");
 combat::tracers::Segment crossing{{-1,0,-100},{20,0,1000},1};before=base();check(tracer.render(c.Get(),surface,view,{&crossing,1}),"near plane crossing clips");check(differences(before,capture(d.Get(),c.Get(),surface))>0,"clipped ray visible");
 combat::tracers::Segment behind{{0,0,-100},{0,0,-10},1};before=base();check(!tracer.render(c.Get(),surface,view,{&behind,1})&&before==capture(d.Get(),c.Get(),surface),"behind camera no draw");
 head.from={0,0,3000};head.to={0,0,5000};wall.render(c.Get(),0,false,&view);wallBefore=capture(d.Get(),c.Get(),wall);tracer.render(c.Get(),wall,view,{&head,1});check(wallBefore==capture(d.Get(),c.Get(),wall),"head on glow retains true depth");
 // Tracers must not poison depth for subsequent ordinary object rendering.
 before=base();tracer.render(c.Get(),surface,view,{&streak,1});auto nearerModel=panel(6000);CharacterRenderer nearer(d.Get(),nearerModel);nearer.render(c.Get(),0,false,&view,&surface);auto drawn=capture(d.Get(),c.Get(),surface);base();nearer.render(c.Get(),0,false,&view,&surface);check(drawn==capture(d.Get(),c.Get(),surface),"tracer does not write depth");
 auto invalid=view;invalid.aspect=0;check(!tracer.render(c.Get(),surface,invalid,{&streak,1}),"invalid view rejected");
 if(!out.empty()){bitmap(out/"third-person-streak.bmp",after);bitmap(out/"self-head-on.bmp",self);bitmap(out/"foreground-body.bmp",bodyAfter);bitmap(out/"occluded.bmp",wallBefore);}
 std::cout<<"Tracer WARP third/self/near plane/world and body depth/no writes PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
