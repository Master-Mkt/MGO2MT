#include "stage_weather_renderer.h"
#include "tracer_renderer.h"
#include <algorithm>
#include <cmath>
#include <iostream>
using namespace mgo2mt;using Microsoft::WRL::ComPtr;
namespace {
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}void ok(HRESULT h){check(SUCCEEDED(h),"WARP call");}
std::vector<uint8_t> pixels(ID3D11Device*d,ID3D11DeviceContext*c,const CharacterRenderer&r){ComPtr<ID3D11Resource>res;r.view()->GetResource(&res);ComPtr<ID3D11Texture2D>t;ok(res.As(&t));D3D11_TEXTURE2D_DESC desc{};t->GetDesc(&desc);desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D>s;ok(d->CreateTexture2D(&desc,nullptr,&s));c->CopyResource(s.Get(),t.Get());D3D11_MAPPED_SUBRESOURCE m{};ok(c->Map(s.Get(),0,D3D11_MAP_READ,0,&m));std::vector<uint8_t>p(desc.Width*desc.Height*4);for(size_t y=0;y<desc.Height;++y)std::copy_n(static_cast<uint8_t*>(m.pData)+y*m.RowPitch,desc.Width*4,p.data()+y*desc.Width*4);c->Unmap(s.Get(),0);return p;}
CharacterModel panel(float z){CharacterModel m;m.bounds={-100000,-100000,z,100000,100000,z};for(auto p:std::array<std::array<float,2>,4>{{{-100000,-100000},{100000,-100000},{100000,100000},{-100000,100000}}}){ModelVertex v{};v.x=p[0];v.y=p[1];v.z=z;v.nz=-1;v.lr=v.lg=v.lb=.1f;v.lit=1;m.vertices.push_back(v);}m.indices={0,1,2,0,2,3};m.parts.push_back({0,6,0,0});m.textures.push_back({4,4,9,{255,255,255,255,0,0,0,0}});return m;}
CharacterModel ground(){auto m=panel(0);m.bounds={-100000,0,-100000,100000,1,100000};for(auto&v:m.vertices){v.z=v.y;v.y=0;v.nz=0;v.ny=1;v.lr=v.lg=v.lb=.3f;}return m;}
}
int main(int argc,char**argv){try{
 check(argc==2,"weather original bundle required");ComPtr<ID3D11Device>d;ComPtr<ID3D11DeviceContext>c;D3D_FEATURE_LEVEL l;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&l,&c));stage::weather::Renderer fx(d.Get(),argv[1]);WorldView view{{0,0,0},{0,0,1}};
 auto f=stage::weather::Controller{}.sample("n022a",12,{0,1800,0});f.dust.clear();
 CharacterRenderer nearSurface(d.Get(),panel(3000)),farSurface(d.Get(),panel(40000));nearSurface.resize_target(d.Get(),320,180);farSurface.resize_target(d.Get(),320,180);view.aspect=320.f/180.f;
 nearSurface.render(c.Get(),0,false,&view);auto before=pixels(d.Get(),c.Get(),nearSurface);check(fx.render(c.Get(),nearSurface,view,f),"fog pass");check(before==pixels(d.Get(),c.Get(),nearSurface),"nearSurface plane pixels untouched");
 farSurface.render(c.Get(),0,false,&view);before=pixels(d.Get(),c.Get(),farSurface);fx.render(c.Get(),farSurface,view,f);auto after=pixels(d.Get(),c.Get(),farSurface);check(before!=after,"farSurface geometry receives fog");size_t at=(90*320+160)*4;float alpha=stage::weather::fog_amount(40000,f);for(unsigned i=0;i<3;++i){float expect=before[at+i]*(1-alpha)+255*f.color[i]*alpha;check(std::abs(float(after[at+i])-expect)<2.f,"fog RGB matches depth-derived blend");}check(after[at+3]==before[at+3],"fog preserves surface alpha");
 CharacterRenderer cover(d.Get(),panel(60000));cover.render(c.Get(),0,false,&view,&farSurface);check(after==pixels(d.Get(),c.Get(),farSurface),"fog never writes world depth");
 f.maximum=f.skyAmount=0;f.dust={{{0,0,3000},1200,0,{1,1,1,.8f},0xd39dd8,{0,0,1,1},false}};
 farSurface.render(c.Get(),0,false,&view);before=pixels(d.Get(),c.Get(),farSurface);fx.render(c.Get(),farSurface,view,f);check(before!=pixels(d.Get(),c.Get(),farSurface),"original sandstorm pixels visible");
 nearSurface.render(c.Get(),0,false,&view);before=pixels(d.Get(),c.Get(),nearSurface);f.dust[0].position[2]=6000;fx.render(c.Get(),nearSurface,view,f);check(before==pixels(d.Get(),c.Get(),nearSurface),"wall hides sandstorm billboard");
 CharacterRenderer floor(d.Get(),ground());floor.resize_target(d.Get(),320,180);WorldView down{{0,2000,-3000},{0,-.5f,1},320.f/180.f,.7f};floor.render(c.Get(),0,false,&down);before=pixels(d.Get(),c.Get(),floor);check(before[at]>20,"test ground visible");
 auto grid=std::make_shared<stage::weather::SurfaceGrid>();grid->originX=grid->originZ=-32000;grid->cells.resize(4096,{0,1,1,0});stage::weather::Frame surface;surface.surface=grid;check(fx.render(c.Get(),floor,down,surface),"wet surface pass without fog");after=pixels(d.Get(),c.Get(),floor);check(after[at]<before[at]-5,"wet ground darkens using scene pixels");check(after[at+3]==before[at+3],"wet preserves alpha");
 for(auto&cell:grid->cells){cell.wetness=0;cell.snow=1;}floor.render(c.Get(),0,false,&down);fx.render(c.Get(),floor,down,surface);after=pixels(d.Get(),c.Get(),floor);check(after[at]>before[at]+5,"exposed snow accumulates visibly");
 for(auto&cell:grid->cells)cell.height=2000;floor.render(c.Get(),0,false,&down);fx.render(c.Get(),floor,down,surface);check(before==pixels(d.Get(),c.Get(),floor),"roof height excludes interior floor");
 surface.surface.reset();floor.render(c.Get(),0,false,&down);check(!fx.render(c.Get(),floor,down,surface)&&before==pixels(d.Get(),c.Get(),floor),"disabled accumulation leaves image unchanged");
 combat::tracers::Renderer precipitation(d.Get());combat::tracers::Segment drop{{0,500,1500},{0,-500,1500},.8f,1.5f,{.8f,.9f,1.f}};
 nearSurface.render(c.Get(),0,false,&view);before=pixels(d.Get(),c.Get(),nearSurface);check(precipitation.render(c.Get(),nearSurface,view,std::array{drop}),"native precipitation drawn");after=pixels(d.Get(),c.Get(),nearSurface);check(after!=before&&after[at+2]>after[at],"precipitation uses cool weather color not bullet orange");drop.from[2]=drop.to[2]=6000;nearSurface.render(c.Get(),0,false,&view);precipitation.render(c.Get(),nearSurface,view,std::array{drop});check(before==pixels(d.Get(),c.Get(),nearSurface),"wall hides falling precipitation");
 std::cout<<"WARP weather nearSurface/farSurface pixels, depth reconstruction, alpha/depth preservation and original sand dust PASS\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

