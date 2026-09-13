#include "character_renderer.h"
#include "stage_lighting.h"
#include "stage_assets.h"
#include "stage_navigation.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
using Microsoft::WRL::ComPtr;
int main(int argc,char**argv){try{
 if(argc<2)throw std::runtime_error("GWM path required");mgo2win::CharacterModel model;bool directory=std::filesystem::is_directory(argv[1]);
 if(directory){mgo2win::stage::Assets a(argv[1]);a.select(mgo2win::host::LoadRequest{1,1,0,0,{20,1,2},mgo2win::host::MatchTransition::initial});auto wait=[&]{auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);while(a.result().status==mgo2win::stage::Status::loading&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));};wait();if(argc>3){a.reset();wait();}auto r=a.result();if(!r.debugModel||r.round.objects.size()!=18)throw std::runtime_error("Debug placement model unavailable");model=*r.debugModel;std::cout<<"debug generation="<<r.generation<<" instances="<<r.round.objects.size()<<"\n";}
 else {std::ifstream in(argv[1],std::ios::binary);std::vector<char>b((std::istreambuf_iterator<char>(in)),{});model=mgo2win::CharacterModel(b);}
 if(argc>3&&!directory){std::ifstream input(argv[3]);auto lighting=mgo2win::stage::Lighting::read(input);unsigned hit=0;for(auto&v:model.vertices){auto s=lighting.sample({v.x,v.y,v.z},{v.nx,v.ny,v.nz});v.lr=s.color[0];v.lg=s.color[1];v.lb=s.color[2];v.lit=1;if(s.volumes)++hit;}model.overviewBounds=lighting.cameraBounds;model.hasOverviewBounds=true;if(!hit)throw std::runtime_error("Stage vertices never intersect hemisphere field");std::cout<<"hemisphere affected vertices="<<hit<<'\n';}
 ComPtr<ID3D11Device>d;ComPtr<ID3D11DeviceContext>c;D3D_FEATURE_LEVEL level;auto ok=[](HRESULT h){if(FAILED(h))throw std::runtime_error("Stage render D3D failure");};ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c));
 bool walking=argc>4&&std::string_view(argv[4])=="--walk";std::optional<mgo2win::WorldView> camera;
 if(walking){std::ifstream input(std::filesystem::path(argv[1]).parent_path()/"n022a.collision.cfg");auto world=mgo2win::stage::Collision::read(input);mgo2win::stage::Navigation navigation;if(!navigation.place(world,{-42878.8359f,3000,29781.7969f}))throw std::runtime_error("Walk camera anchor unavailable");navigation.facing(2.2f);for(unsigned i=0;i<120;++i)navigation.advance(world,{1,0,0,0},1.f/120);camera=mgo2win::WorldView{navigation.eye(),navigation.direction()};}
 mgo2win::CharacterRenderer renderer(d.Get(),model);renderer.render(c.Get(),.35f,true,camera?&*camera:nullptr);ComPtr<ID3D11Resource>resource;renderer.view()->GetResource(&resource);ComPtr<ID3D11Texture2D>texture;ok(resource.As(&texture));D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D>copy;ok(d->CreateTexture2D(&desc,nullptr,&copy));c->CopyResource(copy.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE mapped{};ok(c->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped));
 std::vector<unsigned char>pixels(size_t(desc.Width)*desc.Height*4);size_t visible=0;for(unsigned y=0;y<desc.Height;++y)for(unsigned x=0;x<desc.Width;++x){auto src=static_cast<unsigned char*>(mapped.pData)+y*mapped.RowPitch+x*4;auto dst=pixels.data()+(size_t(y)*desc.Width+x)*4;dst[0]=src[2];dst[1]=src[1];dst[2]=src[0];dst[3]=src[3];if(src[3])++visible;}c->Unmap(copy.Get(),0);if(visible<1000||(!walking&&visible>=size_t(desc.Width)*desc.Height))throw std::runtime_error("Stage shape not visible or fully clipped");
 if(argc>2){BITMAPFILEHEADER f{};BITMAPINFOHEADER h{};h.biSize=sizeof(h);h.biWidth=LONG(desc.Width);h.biHeight=-LONG(desc.Height);h.biPlanes=1;h.biBitCount=32;h.biSizeImage=DWORD(pixels.size());f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(h);f.bfSize=f.bfOffBits+h.biSizeImage;std::ofstream out(argv[2],std::ios::binary);out.write(reinterpret_cast<char*>(&f),sizeof(f));out.write(reinterpret_cast<char*>(&h),sizeof(h));out.write(reinterpret_cast<char*>(pixels.data()),pixels.size());if(!out)throw std::runtime_error("Stage capture write");}
 std::cout<<"stage shape rendered; visible pixels="<<visible<<" triangles="<<model.indices.size()/3<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
