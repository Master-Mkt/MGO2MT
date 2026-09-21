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


} // namespace
int main(int argc,char**argv){try{
 check(argc==4,"sky test: output sky model stage model");std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 ComPtr<ID3D11Device>d;ComPtr<ID3D11DeviceContext>c;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c));
 auto original=read(argv[2]);bool ordinaryRejected=false;try{CharacterModel reject(original);}catch(const std::runtime_error&){ordinaryRejected=true;}check(ordinaryRejected,"Ordinary model extent preserved");CharacterModel authored(original,ModelExtent::sky),terrain(read(argv[3]));check(authored.indices.size()==608*3&&authored.textures.size()==1,"Original sky asset counts");
 CharacterRenderer sky(d.Get(),authored,true),world(d.Get(),terrain),occluder(d.Get(),panel(3000,450,false));
 WorldView view{{-42878,6500,29781},{0,.35f,1}};
 size_t allChanged=0;for(int face=0;face<4;++face){float a=face*1.57079632679f;view.direction={std::sin(a),.35f,std::cos(a)};world.render(c.Get(),0,false,&view);auto before=capture(d.Get(),c.Get(),world);
 sky.render(c.Get(),0,false,&view,&world);auto after=capture(d.Get(),c.Get(),world);size_t changed=0,solid=0;
 for(size_t p=0;p<before.size();p+=4){if(before[p+3]){++solid;check(std::equal(before.begin()+p,before.begin()+p+4,after.begin()+p),"Sky overwrote opaque terrain");}else if(after[p+3])++changed;}
 check(changed>10000,"Original sky must fill a visible far-depth region");allChanged+=changed;bitmap(out/("n022a-sky-"+std::to_string(face)+".bmp"),after);
 std::cout<<"view "<<face<<" terrain pixels retained="<<solid<<" sky pixels="<<changed<<'\n';
 }
 // Sky writes no depth: a later avatar must give identical pixels to one
 // rendered before the sky. These are the two actual pass orders in question.
 view={{-42878,6500,29781},{0,.35f,1}};std::array<float,3> origin=view.eye;
 world.render(c.Get(),0,false,&view);sky.render(c.Get(),0,false,&view,&world);occluder.render(c.Get(),0,false,&view,&world,&origin);auto first=capture(d.Get(),c.Get(),world);
 world.render(c.Get(),0,false,&view);occluder.render(c.Get(),0,false,&view,&world,&origin);sky.render(c.Get(),0,false,&view,&world);check(first==capture(d.Get(),c.Get(),world),"Sky depth must not occlude later avatars");
 bitmap(out/"n022a-sky-occluder.bmp",first);
 bool threw=false;try{sky.render(c.Get(),0,false,&view);}catch(const std::invalid_argument&){threw=true;}check(threw,"Sky cannot clear its own pass");check(first==capture(d.Get(),c.Get(),world),"Invalid sky call touched surface");
 // Resize matches the current gameplay viewport and preserves ordinary pass API.
 world.resize_target(d.Get(),1280,720);view.aspect=1280.f/720;world.render(c.Get(),0,false,&view);sky.render(c.Get(),0,false,&view,&world);
 ComPtr<ID3D11Resource>resource;world.view()->GetResource(&resource);ComPtr<ID3D11Texture2D>texture;ok(resource.As(&texture));D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);check(desc.Width==1280&&desc.Height==720,"Sky follows viewport resize");
 std::cout<<"Original sky WARP passed, changed="<<allChanged<<"; original texture/geometry, native unlit world-space pass; shader/rotation/fog parity unresolved\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
