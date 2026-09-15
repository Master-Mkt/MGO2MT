#include "character_renderer.h"
#include "stage_surface_layers.h"
#include "stage_lighting.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace mgo2win;
using Microsoft::WRL::ComPtr;
namespace {
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
void ok(HRESULT h){check(SUCCEEDED(h),"Surface WARP resource failure");}
using Pixels=std::vector<uint8_t>;
constexpr unsigned width=616,height=392;
Pixels capture(ID3D11Device*d,ID3D11DeviceContext*c,const CharacterRenderer&r){
 ComPtr<ID3D11Resource>resource;r.view()->GetResource(&resource);ComPtr<ID3D11Texture2D>texture;ok(resource.As(&texture));D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
 desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D>copy;ok(d->CreateTexture2D(&desc,nullptr,&copy));c->CopyResource(copy.Get(),texture.Get());
 D3D11_MAPPED_SUBRESOURCE mapped{};ok(c->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped));Pixels p(width*height*4);
 for(unsigned y=0;y<height;++y)std::copy_n(static_cast<const uint8_t*>(mapped.pData)+y*mapped.RowPitch,width*4,p.data()+y*width*4);c->Unmap(copy.Get(),0);return p;
}
void save(const std::filesystem::path&path,const Pixels&rgba){
 auto p=rgba;for(size_t i=0;i<p.size();i+=4)std::swap(p[i],p[i+2]);BITMAPFILEHEADER f{};BITMAPINFOHEADER h{};h.biSize=40;h.biWidth=width;h.biHeight=-int(height);h.biPlanes=1;h.biBitCount=32;h.biSizeImage=DWORD(p.size());f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(h);f.bfSize=f.bfOffBits+h.biSizeImage;
 std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&f),sizeof(f));out.write(reinterpret_cast<const char*>(&h),sizeof(h));out.write(reinterpret_cast<const char*>(p.data()),p.size());check(bool(out),"Surface image write");
}
std::vector<char>read(const std::filesystem::path&p){std::ifstream in(p,std::ios::binary);check(bool(in),"Stage input read");return {(std::istreambuf_iterator<char>(in)),{}};}
void quad(CharacterModel&m,float extent,float z,std::array<float,3>color,bool alternate=false){
 const auto start=uint32_t(m.vertices.size()),first=uint32_t(m.indices.size());
 for(auto xy:std::array<std::array<float,2>,4>{{{-extent,-extent},{extent,-extent},{extent,extent},{-extent,extent}}}){ModelVertex v{};v.x=xy[0];v.y=xy[1];v.z=z+.013f*v.x;v.nz=-1;v.lit=1;m.vertices.push_back(v);}
 for(auto i:alternate?std::array<unsigned,6>{0,3,1,1,3,2}:std::array<unsigned,6>{0,2,1,0,3,2})m.indices.push_back(start+i);
 ModelPart p{first,6,0,0};p.tint=color;m.parts.push_back(p);
}
CharacterModel empty(float extent,float z){CharacterModel m;m.bounds={-extent,-extent,z-extent,extent,extent,z+extent};m.textures.push_back({4,4,9,{255,255,255,255,0,0,0,0}});return m;}
void center_color(const Pixels&p,std::array<int,3>rgb,const char*why){
 for(int y=int(height)/2-8;y<int(height)/2+8;++y)for(int x=int(width)/2-8;x<int(width)/2+8;++x)for(unsigned k=0;k<3;++k)check(std::abs(int(p[(y*width+x)*4+k])-rgb[k])<=1,why);
}
void synthetic(ID3D11Device*d,ID3D11DeviceContext*c,const std::filesystem::path&out){
 unsigned checked=0;
 for(float distance:{2000.f,50000.f,200000.f}){
  const float extent=distance*.35f;auto base=empty(extent,distance);quad(base,extent,distance,{1,0,0});quad(base,extent*.8f,distance+.015625f,{0,1,0},true);
  auto fixed=base;auto stats=stage::separate_stage_surfaces(fixed);check(stats.shiftedTriangles==2&&stats.maxLayer==1,"Only two overlapping overlay triangles are separated");
  CharacterRenderer before(d,base),after(d,fixed);
  auto wall=empty(extent,distance);quad(wall,extent,distance-20,{0,0,1});CharacterRenderer occluder(d,wall);
  for(int frame=0;frame<24;++frame){
   WorldView view{{float(frame-12)*extent*.001f,extent*.002f*std::sin(float(frame)),0},{.012f*std::sin(frame*.2f),.003f,1}};
   after.render(c,0,false,&view);auto pixels=capture(d,c,after);center_color(pixels,{0,255,0},"Overlay flickered through its base during camera motion");
   if(frame==0){before.render(c,0,false,&view);save(out/("synthetic-"+std::to_string(int(distance))+"-before.bmp"),capture(d,c,before));save(out/("synthetic-"+std::to_string(int(distance))+"-after.bmp"),pixels);}
   occluder.render(c,0,false,&view,&after);center_color(capture(d,c,after),{0,0,255},"Separated overlay leaks through foreground wall");
   // Reverse the ordinary draw order: the nearer wall must still win.
   occluder.render(c,0,false,&view);after.render(c,0,false,&view,&occluder);center_color(capture(d,c,occluder),{0,0,255},"Order-dependent world depth occlusion");++checked;
  }
 }
 std::cout<<"synthetic moving-camera frames="<<checked<<" stable overlay; foreground depth retained at 2/50/200m\n";
}
void original(ID3D11Device*d,ID3D11DeviceContext*c,const std::filesystem::path&out,const std::filesystem::path&root){
 for(auto name:{"n007a","n001a","n004a","n022a","n023a"}){
  const auto path=root/(std::string(name)+".gwm");if(!std::filesystem::exists(path))continue;auto bytes=read(path);CharacterModel raw(bytes);
  const auto lightPath=root/(std::string(name)+".lighting.cfg");if(std::filesystem::exists(lightPath)){std::ifstream in(lightPath);auto light=stage::Lighting::read(in);for(auto&v:raw.vertices){auto l=light.sample({v.x,v.y,v.z},{v.nx,v.ny,v.nz});v.lr=l.color[0];v.lg=l.color[1];v.lb=l.color[2];v.lit=1;}raw.hasOverviewBounds=true;raw.overviewBounds=light.cameraBounds;}
  auto fixed=raw;auto began=std::chrono::steady_clock::now();auto stats=stage::separate_stage_surfaces(fixed);auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();
  check(stats.shiftedTriangles>0&&stats.maxLayer<=16&&!stats.layerLimitExceeded&&!stats.searchLimitExceeded,"Original stage overlap preparation failed");
  check(fixed.vertices.size()==raw.vertices.size()+3*stats.shiftedTriangles&&fixed.indices.size()==raw.indices.size()&&fixed.parts.size()==raw.parts.size(),"Original geometry was removed or nonoverlap vertices were overwritten");
  check(read(path)==bytes,"Original stage asset was modified");
  std::cout<<name<<" triangles="<<stats.triangles<<" pairs="<<stats.overlapPairs<<" shifted="<<stats.shiftedTriangles<<" maxLayer="<<stats.maxLayer<<" seconds="<<elapsed<<'\n';
  CharacterRenderer before(d,raw),after(d,fixed);before.render(c,0,true);after.render(c,0,true);
  save(out/(std::string(name)+"-overview-before.bmp"),capture(d,c,before));save(out/(std::string(name)+"-overview-after.bmp"),capture(d,c,after));
  if(std::string_view(name)=="n007a"){
   // Camera from the actual BB overlapping pair; full scene is retained.
   for(int frame=0;frame<12;++frame){WorldView view{{-23973.9544f+(frame-6)*8.f,2877.6105f,20998.002f},{(6-frame)*.004f,0,-1}};
    before.render(c,0,false,&view);after.render(c,0,false,&view);save(out/("bb-full-before-"+std::to_string(frame)+".bmp"),capture(d,c,before));save(out/("bb-full-after-"+std::to_string(frame)+".bmp"),capture(d,c,after));}
   // Isolate the two authored parts for inspection, keeping every original
   // vertex attribute, index and texture. This is an asset test, not gameplay.
   raw.parts={raw.parts.at(112),raw.parts.at(120)};fixed.parts={fixed.parts.at(112),fixed.parts.at(120)};
   CharacterRenderer pairBefore(d,raw),pairAfter(d,fixed);
   for(int frame=0;frame<24;++frame){WorldView view{{-23973.9544f+(frame-12)*3.f,2877.6105f,19798.002f},{(12-frame)*.002f,0,-1}};
    pairBefore.render(c,0,false,&view);pairAfter.render(c,0,false,&view);save(out/("bb-pair-before-"+std::to_string(frame)+".bmp"),capture(d,c,pairBefore));save(out/("bb-pair-after-"+std::to_string(frame)+".bmp"),capture(d,c,pairAfter));}
  }
 }
}
}
int main(int argc,char**argv){try{
 check(argc>=2,"Surface test output folder required");std::filesystem::path out=argv[1];std::filesystem::create_directories(out);ComPtr<ID3D11Device>d;ComPtr<ID3D11DeviceContext>c;D3D_FEATURE_LEVEL level;
 ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c));
 if(argc>2)original(d.Get(),c.Get(),out,argv[2]);else synthetic(d.Get(),c.Get(),out);
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
