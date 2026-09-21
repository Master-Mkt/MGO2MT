#include "character_renderer.h"
#include "stage_lighting.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
using namespace mgo2mt;
namespace {
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
void ok(HRESULT value){check(SUCCEEDED(value),"Stage material D3D11 failure");}
std::vector<char> read(const std::filesystem::path& path){std::ifstream file(path,std::ios::binary);check(bool(file),"Stage material input missing");return {(std::istreambuf_iterator<char>(file)),{}};}
struct Frame {unsigned width=0,height=0;std::vector<unsigned char> rgba;};
Frame frame(ID3D11Device* device,ID3D11DeviceContext* context,const CharacterRenderer& renderer){
 ComPtr<ID3D11Resource> resource;renderer.view()->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;ok(resource.As(&texture));D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
 desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> copy;ok(device->CreateTexture2D(&desc,nullptr,&copy));context->CopyResource(copy.Get(),texture.Get());
 D3D11_MAPPED_SUBRESOURCE mapped{};ok(context->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped));Frame result{desc.Width,desc.Height,std::vector<unsigned char>(size_t(desc.Width)*desc.Height*4)};
 for(unsigned y=0;y<desc.Height;++y)std::copy_n(static_cast<unsigned char*>(mapped.pData)+y*mapped.RowPitch,desc.Width*4,result.rgba.data()+size_t(y)*desc.Width*4);
 context->Unmap(copy.Get(),0);return result;
}
void write(const std::filesystem::path& path,const Frame& image){
 auto pixels=image.rgba;for(size_t i=0;i<pixels.size();i+=4)std::swap(pixels[i],pixels[i+2]);
 BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=LONG(image.width);info.biHeight=-LONG(image.height);info.biPlanes=1;info.biBitCount=32;info.biSizeImage=DWORD(pixels.size());file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+info.biSizeImage;
 std::ofstream output(path,std::ios::binary);output.write(reinterpret_cast<const char*>(&file),sizeof(file));output.write(reinterpret_cast<const char*>(&info),sizeof(info));output.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());check(bool(output),"Stage material image write failed");
}
CharacterModel panel(uint32_t material=0x120000){
 CharacterModel m;m.bounds={-4000,-4000,2000,4000,4000,2000};
 for(auto xy:std::array<std::array<float,2>,4>{{{-4000,-4000},{4000,-4000},{4000,4000},{-4000,4000}}}){ModelVertex v{};v.x=xy[0];v.y=xy[1];v.z=2000;v.nz=-1;v.lr=v.lg=v.lb=.1f;v.lit=1;v.ar=.2f;v.ag=.1f;v.ab=.05f;v.aa=0;m.vertices.push_back(v);}
 m.indices={0,1,2,0,2,3};m.parts.push_back({0,6,0,0,material});m.textures.push_back({4,4,9,{255,255,255,255,0,0,0,0}});return m;
}
void word(std::vector<char>&b,uint32_t n){for(unsigned i=0;i<4;++i)b.push_back(char(n>>(8*i)));}
void number(std::vector<char>&b,float v){uint32_t n;std::memcpy(&n,&v,4);word(b,n);}
std::vector<char> encode(const CharacterModel&m,unsigned version){std::vector<char>b{'G','W','M','1'};
 for(auto n:{version,unsigned(m.vertices.size()),unsigned(m.indices.size()),unsigned(m.parts.size()),unsigned(m.textures.size())})word(b,n);
 for(auto v:m.bounds)number(b,v);
 for(auto&v:m.vertices){for(float n:{v.x,v.y,v.z,v.nx,v.ny,v.nz,v.u,v.v})number(b,n);if(version==2)for(float n:{v.ar,v.ag,v.ab,v.aa})number(b,n);}
 for(auto i:m.indices)word(b,i);for(auto&p:m.parts){for(auto i:{p.first,p.count,p.texture,p.flags})word(b,i);if(version==2){word(b,p.materialShader);for(float v:p.tint)number(b,v);}}
 for(auto&t:m.textures){for(auto i:{t.width,t.height,t.codec,unsigned(t.pixels.size())})word(b,i);b.insert(b.end(),t.pixels.begin(),t.pixels.end());}return b;
}
void invalid(const std::vector<char>&b){bool rejected=false;try{CharacterModel ignored(b);}catch(const std::runtime_error&){rejected=true;}check(rejected,"Invalid GWM version 2 accepted");}
void serialization(){auto fixture=panel();auto bytes=encode(fixture,2);CharacterModel restored(bytes);check(restored.vertices[0].ar==.2f&&restored.vertices[0].aa==0&&restored.parts[0].materialShader==0x120000,"Authored GWM data lost");
 for(size_t n=0;n<bytes.size();++n)invalid({bytes.begin(),bytes.begin()+n});
 auto damage=[&](size_t at,uint32_t value){auto bad=bytes;for(unsigned i=0;i<4;++i)bad[at+i]=char(value>>(8*i));invalid(bad);};
 damage(4,3);damage(80,0x7fc00000);damage(80,0xbf800000);damage(84,0x40000000);damage(92,0x40000000);damage(4,1);
 CharacterModel old(encode(fixture,1));check(old.vertices[0].ar==0&&old.vertices[0].aa==1&&old.parts[0].materialShader==0,"GWM version 1 defaults changed");
}
Frame draw(ID3D11Device*d,ID3D11DeviceContext*c,const CharacterModel&m){CharacterRenderer r(d,m);WorldView view{{0,0,0},{0,0,1}};r.render(c,0,false,&view);return frame(d,c,r);}
std::array<int,3> center(const Frame&f){size_t p=((f.height/2)*f.width+f.width/2)*4;return {f.rgba[p],f.rgba[p+1],f.rgba[p+2]};}
void check_rgb(const Frame&f,std::array<int,3> rgb){auto found=center(f);for(unsigned i=0;i<3;++i)check(std::abs(found[i]-rgb[i])<=1,"Authored prelight RGB arithmetic differs from original MAD");}
void synthetic(ID3D11Device*d,ID3D11DeviceContext*c){serialization();auto m=panel();auto lit=draw(d,c,m);check_rgb(lit,{128,77,51});
 for(auto&v:m.vertices)v.aa=1;check(draw(d,c,m).rgba==lit.rgba,"COLOR0 alpha incorrectly masks authored RGB");
 for(auto&v:m.vertices)v.ar=v.ag=v.ab=0;auto zero=draw(d,c,m);check_rgb(zero,{26,26,26});
 m=panel(0x110000);check(draw(d,c,m).rgba==zero.rgba,"Unaudited material changed");
 m=panel(0);check(draw(d,c,m).rgba==zero.rgba,"Legacy material changed");
 m=panel();for(auto&v:m.vertices)v.lit=0;auto preview=draw(d,c,m);for(auto&v:m.vertices)v.ar=v.ag=v.ab=0;check(draw(d,c,m).rgba==preview.rgba,"Unlit character preview changed");
 m=panel();CharacterRenderer r(d,m);WorldView view{{0,0,0},{0,0,1}};for(auto&v:m.vertices)v.lr=v.lg=v.lb=0;r.update_vertices(c,m.vertices);r.render(c,0,false,&view);check_rgb(frame(d,c,r),{102,51,26});
 for(auto&v:m.vertices)v.lr=v.lg=v.lb=.1f;r.update_vertices(c,m.vertices);r.render(c,0,false,&view);check(frame(d,c,r).rgba==lit.rgba,"Dynamic light on/off erased authored prelight");
 std::cout<<"GWM v1/v2, truncation/range rejection; WARP RGB MAD, alpha independence, zero COLOR0, unaffected materials and light refresh passed\n";
}
void original(ID3D11Device*d,ID3D11DeviceContext*c,char**argv){CharacterModel old(read(argv[1])),restored(read(argv[2]));check(old.vertices.size()==restored.vertices.size()&&old.indices==restored.indices&&old.parts.size()==restored.parts.size(),"Original stage topology changed");
 std::ifstream f(argv[3]);auto lighting=stage::Lighting::read(f);for(auto* m:{&old,&restored})for(auto&v:m->vertices){auto l=lighting.sample({v.x,v.y,v.z},{v.nx,v.ny,v.nz});v.lr=l.color[0];v.lg=l.color[1];v.lb=l.color[2];v.lit=1;}
 auto neutral=restored;for(auto&v:neutral.vertices)v.ar=v.ag=v.ab=0;
 std::filesystem::path output=argv[4];std::filesystem::create_directories(output);std::ofstream report(output/"comparison.json");report<<"{\n  \"cameras\": [\n";
 const std::array<WorldView,3> views{{{{-45300,2400,31500},{2421,-1250,-1718}},{{-40760,1900,32700},{-2118,-900,-2919}},{{-42100,5000,31000},{-779,-4000,-1218}}}};
 for(unsigned i=0;i<views.size();++i){auto capture=[&](const CharacterModel&m){CharacterRenderer r(d,m);r.render(c,0,false,&views[i]);return frame(d,c,r);};auto before=capture(old),after=capture(restored),zero=capture(neutral);check(before.rgba==zero.rgba,"Stage changed with authored COLOR0 disabled");size_t changed=0;unsigned long long a=0,b=0;for(size_t p=0;p<before.rgba.size();p+=4){if(!std::equal(before.rgba.begin()+p,before.rgba.begin()+p+3,after.rgba.begin()+p))++changed;for(unsigned k=0;k<3;++k){a+=before.rgba[p+k];b+=after.rgba[p+k];}}check(changed>1000&&b>a,"Original restored prelight not visible");write(output/("stage-"+std::to_string(i)+"-before.bmp"),before);write(output/("stage-"+std::to_string(i)+"-after.bmp"),after);if(i)report<<",\n";report<<"    {\"index\":"<<i<<",\"changed_pixels\":"<<changed<<",\"before_mean_rgb\":"<<double(a)/(before.width*before.height*3)<<",\"after_mean_rgb\":"<<double(b)/(after.width*after.height*3)<<",\"zero_color0_pixel_identical\":true}";std::cout<<"Camera "<<i<<" changed="<<changed<<" mean="<<double(a)/(before.width*before.height*3)<<" -> "<<double(b)/(after.width*after.height*3)<<'\n';}
 report<<"\n  ]\n}\n";
}
void objects(ID3D11Device*d,ID3D11DeviceContext*c,const std::filesystem::path& folder,const std::filesystem::path& output){
 std::filesystem::create_directories(output);size_t count=0;for(auto&file:std::filesystem::directory_iterator(folder)){if(file.path().extension()!=L".gwm")continue;CharacterModel m(read(file.path()));CharacterRenderer r(d,m);r.render(c,.5f,true);auto image=frame(d,c,r);size_t visible=0;for(size_t p=3;p<image.rgba.size();p+=4)if(image.rgba[p])++visible;check(visible>10,"Original object geometry not visible");write(output/(file.path().stem().string()+".bmp"),image);++count;std::cout<<file.path().stem().string()<<" visible="<<visible<<" parts="<<m.parts.size()<<'\n';}check(count>=20,"Missing restored object files");
}
}
int main(int argc,char**argv){try{ComPtr<ID3D11Device>d;ComPtr<ID3D11DeviceContext>c;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c));synthetic(d.Get(),c.Get());if(argc==4&&std::string_view(argv[1])=="--objects")objects(d.Get(),c.Get(),argv[2],argv[3]);else if(argc>1){check(argc==5,"Usage: stage_material_test [old.gwm new.gwm lighting.cfg output-dir] or --objects folder output-dir");original(d.Get(),c.Get(),argv);}return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
