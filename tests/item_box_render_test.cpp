#include "item_box_presentation.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt;using Microsoft::WRL::ComPtr;
namespace {
void check(bool x,const char*s){if(!x)throw std::runtime_error(s);}void ok(HRESULT h){check(SUCCEEDED(h),"Item box WARP");}
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

CharacterModel wall(float z){CharacterModel m;m.bounds={-5000,-5000,z,5000,5000,z};for(auto xy:std::array<std::array<float,2>,4>{{{-5000,-5000},{5000,-5000},{5000,5000},{-5000,5000}}}){ModelVertex v{};v.x=xy[0];v.y=xy[1];v.z=z;v.nz=-1;v.lr=v.lg=v.lb=.08f;v.lit=1;m.vertices.push_back(v);}m.indices={0,1,2,0,2,3};m.parts.push_back({0,6,0,0});m.textures.push_back({4,4,9,{255,255,255,255,0,0,0,0}});return m;}
size_t changed(const Pixels&a,const Pixels&b){size_t n=0;for(size_t i=0;i<a.size();i+=4)if(a[i]!=b[i]||a[i+1]!=b[i+1]||a[i+2]!=b[i+2])++n;return n;}
}
int main(int argc,char**argv){try{
 check(argc==3,"Original items root and WARP output directory required");
 ComPtr<ID3D11Device>d;ComPtr<ID3D11DeviceContext>c;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c));
 CharacterRenderer stage(d.Get(),wall(5000));item_box::Renderer boxes(d.Get(),argv[1]);WorldView camera{{0,130,0},{0,0,1},616.f/392};stage.render(c.Get(),0,false,&camera);const auto baseline=capture(d.Get(),c.Get(),stage);
 item_box::Box box{{{1,2},1},{0,0,1200},0,item_box::Size::primary,true,item_box::Asset::large};std::array input{box};
 auto draw=[&]{stage.render(c.Get(),0,false,&camera);boxes.draw(c.Get(),input,stage,camera);return capture(d.Get(),c.Get(),stage);};
 auto primary=draw();const auto primaryCount=changed(baseline,primary);check(primaryCount>1500,"real 3D primary box pixels");
 input[0].yaw=-1.57079633f;auto rotated=draw();check(changed(primary,rotated)>1000,"quarter-turn changes long-box orientation");
 input[0].yaw=-.785398163f;auto diagonal=draw();check(changed(primary,diagonal)>1000,"oblique box exposes actual side faces");
 input[0]=box;input[0].size=item_box::Size::secondary;input[0].asset=item_box::Asset::medium;auto secondary=draw();input[0].size=item_box::Size::reserve;input[0].asset=item_box::Asset::smallBox;auto reserve=draw();check(primaryCount>changed(baseline,secondary)&&primaryCount>changed(baseline,reserve)&&changed(secondary,reserve)>150,"three original meshes produce distinct native-sized silhouettes");
 input[0]=box;input[0].position[2]=6000;check(draw()==baseline,"world wall fully occludes box behind it");input[0].position[2]=-2000;check(draw()==baseline,"box behind camera clipped");
 // An independently rendered body-sized occluder uses the same surface depth.
 auto bodyModel=wall(800);for(auto& v:bodyModel.vertices){v.x*=.07f;v.y=v.y*.17f+650;v.lr=.18f;v.lg=.3f;v.lb=.5f;}CharacterRenderer body(d.Get(),bodyModel);
 stage.render(c.Get(),0,false,&camera);body.render(c.Get(),0,false,&camera,&stage);auto bodyOnly=capture(d.Get(),c.Get(),stage);
 input[0]=box;draw();body.render(c.Get(),0,false,&camera,&stage);check(capture(d.Get(),c.Get(),stage)==bodyOnly,"later foreground body hides box through shared depth");
 stage.render(c.Get(),0,false,&camera);body.render(c.Get(),0,false,&camera,&stage);boxes.draw(c.Get(),input,stage,camera);check(capture(d.Get(),c.Get(),stage)==bodyOnly,"foreground body also hides later box draw");
 input[0]=box;auto valid=draw();input[0].position[0]=std::numeric_limits<float>::quiet_NaN();bool rejected=false;try{boxes.draw(c.Get(),input,stage,camera);}catch(const std::invalid_argument&){rejected=true;}check(rejected&&capture(d.Get(),c.Get(),stage)==valid,"invalid transform rejected before rendering");
 input[0]=box;input[0].asset=item_box::Asset::medium;rejected=false;try{boxes.draw(c.Get(),input,stage,camera);}catch(const std::invalid_argument&){rejected=true;}check(rejected&&capture(d.Get(),c.Get(),stage)==valid,"invalid original/category pair rejected before rendering");
 rejected=false;try{item_box::Renderer missing(d.Get(),std::filesystem::path(argv[1])/"missing-original-models");}catch(const std::runtime_error&){rejected=true;}check(rejected&&capture(d.Get(),c.Get(),stage)==valid,"missing original model produces no replacement visual");
 input[0]=box;input[0].size=item_box::Size::unknown;input[0].asset=item_box::Asset::unavailable;check(draw()==baseline,"unknown visual is omitted, never rendered as a fabricated cube");
 input[0]=box;valid=draw();
 input[0]=box;std::vector<item_box::Box> many(129,box);rejected=false;try{boxes.draw(c.Get(),many,stage,camera);}catch(const std::invalid_argument&){rejected=true;}check(rejected&&capture(d.Get(),c.Get(),stage)==valid,"render capacity rejected atomically");
 camera.aspect=2;stage.render(c.Get(),0,false,&camera);const auto wideBaseline=capture(d.Get(),c.Get(),stage);auto wide=draw();check(changed(wideBaseline,wide)<primaryCount,"shared camera aspect controls projection");
 {std::filesystem::path out=argv[2];std::filesystem::create_directories(out);bitmap(out/"primary.bmp",primary);bitmap(out/"primary-quarter-left.bmp",rotated);bitmap(out/"primary-oblique.bmp",diagonal);bitmap(out/"body-occlusion.bmp",bodyOnly);bitmap(out/"secondary.bmp",secondary);bitmap(out/"reserve.bmp",reserve);bitmap(out/"wide-aspect.bmp",wide);std::ofstream f(out/"render.json");f<<"{\"warp\":true,\"primaryPixels\":"<<primaryCount<<",\"wallOcclusion\":true,\"bodyOcclusionBothOrders\":true,\"nativeBox\":false,\"originalMesh\":true,\"originalDiffuse\":true,\"uniformCategoryFit\":true,\"bottomCenterPivot\":true,\"fullOriginalShaderParity\":false}";}
 std::cout<<"Original item box WARP depth / three MDNs and original images / native size / rotation / missing asset boundaries PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
