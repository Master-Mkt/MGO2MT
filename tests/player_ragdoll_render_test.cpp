#include "player_ragdoll.h"
#include "character_renderer.h"
#include "stage_navigation.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;using namespace physics;using Microsoft::WRL::ComPtr;
namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void ok(HRESULT value){check(SUCCEEDED(value),"Ragdoll WARP failure");}
std::vector<char> read(const char*p){std::ifstream f(p,std::ios::binary);check(bool(f),"Ragdoll render fixture missing");return {(std::istreambuf_iterator<char>(f)),{}};}
struct Frame {unsigned width,height;std::vector<unsigned char>rgba;};
Frame capture(ID3D11Device*d,ID3D11DeviceContext*c,CharacterRenderer&r){ComPtr<ID3D11Resource> resource;r.view()->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;ok(resource.As(&texture));D3D11_TEXTURE2D_DESC desc;texture->GetDesc(&desc);desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> copy;ok(d->CreateTexture2D(&desc,nullptr,&copy));c->CopyResource(copy.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE map;ok(c->Map(copy.Get(),0,D3D11_MAP_READ,0,&map));Frame f{desc.Width,desc.Height,std::vector<unsigned char>(size_t(desc.Width)*desc.Height*4)};for(unsigned y=0;y<desc.Height;++y)std::copy_n(static_cast<unsigned char*>(map.pData)+y*map.RowPitch,desc.Width*4,f.rgba.data()+size_t(y)*desc.Width*4);c->Unmap(copy.Get(),0);return f;}
void save(const std::filesystem::path&p,Frame f){for(size_t i=0;i<f.rgba.size();i+=4)std::swap(f.rgba[i],f.rgba[i+2]);BITMAPFILEHEADER b{};BITMAPINFOHEADER h{};h.biSize=sizeof(h);h.biWidth=LONG(f.width);h.biHeight=-LONG(f.height);h.biPlanes=1;h.biBitCount=32;h.biSizeImage=DWORD(f.rgba.size());b.bfType=0x4d42;b.bfOffBits=sizeof(b)+sizeof(h);b.bfSize=b.bfOffBits+h.biSizeImage;std::ofstream file(p,std::ios::binary);file.write(reinterpret_cast<char*>(&b),sizeof(b));file.write(reinterpret_cast<char*>(&h),sizeof(h));file.write(reinterpret_cast<char*>(f.rgba.data()),f.rgba.size());check(bool(file),"Ragdoll render write failed");}
}
int main(int argc,char**argv){try{
 check(argc==7,"Usage: player_ragdoll_render_test stage.gwm catalog.gwc player.gwmot collision.cfg lighting.cfg output-dir");
 CharacterModel stageModel(read(argv[1]));CharacterCatalog catalog(read(argv[2]));PlayerMotionBank motions(read(argv[3]));std::ifstream cf(argv[4]),lf(argv[5]);auto world=stage::Collision::read(cf);auto light=stage::Lighting::read(lf);for(auto&v:stageModel.vertices){auto l=light.sample({v.x,v.y,v.z},{v.nx,v.ny,v.nz});v.lr=l.color[0];v.lg=l.color[1];v.lb=l.color[2];v.lit=1;}
 stage::Navigation navigation;check(navigation.place(world,{-42878.8359f,3000,29781.7969f}),"Ragdoll render inspection floor missing");auto actor=navigation.feet();
 std::array<uint8_t,28> appearance{};appearance[2]=11;appearance[3]=22;appearance[15]=46;appearance[17]=57;auto body=catalog.assemble(appearance);check(body.ready()&&!body.missingModels,"Ragdoll render body incomplete");auto idle=*motions.sample(PlayerMotion::Idle,0);constexpr float yaw=2.2f;player::Ragdoll rag;check(rag.start(catalog,0,idle,actor,yaw),"Ragdoll render handoff failed");
 ComPtr<ID3D11Device>d;ComPtr<ID3D11DeviceContext>c;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c));CharacterRenderer stageRenderer(d.Get(),stageModel),avatar(d.Get(),body.model);
 std::filesystem::path output=argv[6];std::filesystem::create_directories(output);std::ofstream report(output/"validation.json");report<<"{\n  \"synthetic_input\":true,\n  \"device\":\"D3D11 WARP\",\n  \"frames\":[\n";
 const float times[]={0,.2f,.8f,1.5f};float elapsed=0;for(unsigned frame=0;frame<4;++frame){if(frame==1)rag.impulse({std::sin(yaw)*70000,30000,std::cos(yaw)*70000},add(rag.root_position(),{0,500,0}));while(elapsed<times[frame]-.00001f){float dt=std::min(1.f/120,times[frame]-elapsed);rag.step(world,dt);elapsed+=dt;}check(rag.active(),"Ragdoll inactive during render sequence");catalog.pose(body,rag.pose());avatar.update_vertices(c.Get(),body.model.vertices);auto root=rag.root_position(),origin=rag.origin();float angle=yaw+1.57079632679f;WorldView camera;camera.eye={root[0]-std::sin(angle)*3400,actor[1]+1900,root[2]-std::cos(angle)*3400};camera.direction=sub({root[0],actor[1]+750,root[2]},camera.eye);
  stageRenderer.render(c.Get(),0,false,&camera);auto background=capture(d.Get(),c.Get(),stageRenderer);avatar.render(c.Get(),0,false,&camera,&stageRenderer,&origin);auto shown=capture(d.Get(),c.Get(),stageRenderer);size_t changed=0;for(size_t i=0;i<shown.rgba.size();i+=4)if(!std::equal(shown.rgba.begin()+i,shown.rgba.begin()+i+4,background.rgba.begin()+i))++changed;check(changed>100,"Ragdoll body fully hidden or clipped");auto name="ragdoll-"+std::to_string(frame)+".bmp";save(output/name,shown);if(frame)report<<",\n";report<<"    {\"seconds\":"<<elapsed<<",\"path\":\""<<name<<"\",\"avatar_pixels\":"<<changed<<",\"root_y_mm\":"<<root[1]<<",\"max_joint_error_mm\":"<<rag.maximum_joint_error()<<"}";std::cout<<"t="<<elapsed<<" pixels="<<changed<<" root="<<root[0]<<','<<root[1]<<','<<root[2]<<" joint_mm="<<rag.maximum_joint_error()<<'\n';
 }
 report<<"\n  ]\n}\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
