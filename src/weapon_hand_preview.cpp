#include "character_renderer.h"
#include "weapon_hand_renderer.h"
#include "character_catalog.h"
#include "player_motion.h"
#include "stage_lighting.h"
#include "stage_navigation.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
using namespace mgo2win;
namespace {
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
void ok(HRESULT value){check(SUCCEEDED(value),"Player render D3D11 failure");}
std::vector<char> read(const std::filesystem::path& path){std::ifstream file(path,std::ios::binary);check(bool(file),"Player render input missing");return {(std::istreambuf_iterator<char>(file)),{}};}
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
 std::ofstream output(path,std::ios::binary);output.write(reinterpret_cast<const char*>(&file),sizeof(file));output.write(reinterpret_cast<const char*>(&info),sizeof(info));output.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());check(bool(output),"Player render image write failed");
}
}
namespace mgo2win {
int run_weapon_hand_preview(const std::filesystem::path&data,const std::filesystem::path&output){try{
 std::filesystem::create_directories(output);
 CharacterCatalog catalog(read(data/"character/appearance.gwc"));PlayerMotionBank motions(read(data/"character/player.gwmot"));
 auto bytes=read(data/"weapons/hands.gwh");weapon_hand::Bank bank(bytes);weapon_hand::Models models(data/"weapons");
 check(bank.size()==15,"All original hand clips required");bool rejected=false;try{weapon_hand::Bank bad(std::span<const char>(bytes.data(),bytes.size()-1));}catch(...){rejected=true;}check(rejected,"Truncated hand bank rejected");
 ComPtr<ID3D11Device>d;ComPtr<ID3D11DeviceContext>c;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c));
 unsigned frames=0;double totalChanged=0,maxPalmError=0;
 for(unsigned gender=0;gender<2;++gender){std::array<uint8_t,28>a{};a[0]=uint8_t(gender);a[2]=11;a[3]=22;a[15]=46;a[17]=57;auto body=catalog.assemble(a);check(body.ready()&&!body.missingModels&&!body.missingColors,"Original body assets required");CharacterRenderer renderer(d.Get(),body.model);renderer.resize_target(d.Get(),1280,720);
  if(!gender)for(auto weapon:{25u,3u})for(unsigned index=0;index<(weapon==25?12u:3u);++index){auto clip=bank.sample(weapon,index,.5,false);check(bool(clip),"Audit clip");catalog.pose(body,clip->pose);renderer.update_vertices(c.Get(),body.model.vertices);WorldView camera{{-1600,1450,2300},{1600,-250,-2100},16.f/9.f};std::array<float,3>origin{};renderer.render(c.Get(),0,false,&camera,nullptr,&origin);weapon_hand::Actor held;held.update(d.Get(),c.Get(),models,body,*clip,.1);held.draw(c.Get(),0,camera,&renderer,origin);write(output/("clip-"+std::to_string(weapon)+"-"+std::to_string(clip->index)+".bmp"),frame(d.Get(),c.Get(),renderer));}
  for(auto weapon:{25u,3u}){weapon_hand::Actor held;check(models.find(weapon)&&models.magazine(weapon),"Original weapon and magazine models required");
   for(unsigned action=0;action<4;++action){auto motion=action==0?PlayerMotion::Aim:action==1?PlayerMotion::Walk:action==2?PlayerMotion::CrouchIdle:PlayerMotion::Reload;
    for(unsigned step=0;step<=30;++step){double time=step*.07;auto base=*motions.sample(motion,time);const auto previous=base;auto sample=bank.select(weapon,motion,time);check(bool(sample),"Weapon-specific MTP sample");weapon_hand::upper_body(base,*sample,catalog.skeleton(gender));check(base.root==previous.root&&base.rotations.at(0xf5d387)==previous.rotations.at(0xf5d387),"Hand masks preserve head and movement");catalog.pose(body,base);
     auto f=weapon_hand::frame(body,sample->point);check(bool(f),"Hand bone frame present");
     // The attachment origin must equal transforming MTP translation through
     // the SAME world bone used by skinning, including root, exactly once.
     auto&b=body.boneFrames.at(sample->point.bone);for(unsigned axis=0;axis<3;++axis){double expected=b[12+axis];for(unsigned j=0;j<3;++j)expected+=sample->point.position[j]*b[j*4+axis];maxPalmError=std::max(maxPalmError,std::abs(expected-(*f)[12+axis]));}check(maxPalmError<.02,"Weapon origin/skin bone mismatch");
     check(held.update(d.Get(),c.Get(),models,body,*sample,.07),"Original weapon attached");++frames;
     if(step==0||step==15||step==30){renderer.update_vertices(c.Get(),body.model.vertices);WorldView camera{{-1600,1450,2300},{1600,-250,-2100},16.f/9.f};std::array<float,3>origin{};renderer.render(c.Get(),0,false,&camera,nullptr,&origin);auto before=frame(d.Get(),c.Get(),renderer);held.draw(c.Get(),0,camera,&renderer,origin);auto after=frame(d.Get(),c.Get(),renderer);size_t changed=0;for(size_t j=0;j<after.rgba.size();j+=4)if(!std::equal(after.rgba.begin()+j,after.rgba.begin()+j+3,before.rgba.begin()+j))++changed;check(changed>20,"Weapon must contribute visible pixels");totalChanged+=changed;
      write(output/(std::string(gender?"female":"male")+"-"+std::to_string(weapon)+"-"+std::to_string(action)+"-"+std::to_string(step)+".bmp"),after);
     }
    }
   }
   held.clear();check(!held.visible(),"Weapon change clears stale renderer");auto unsupported=*bank.select(weapon,PlayerMotion::Aim,0);unsupported.weapon=127;check(!held.update(d.Get(),c.Get(),models,body,unsupported,.1)&&!held.visible(),"Unknown weapon never reuses previous model");
  }
 }
 std::ofstream report(output/"capture.json");report<<"{\"passed\":true,\"offlineRenderFixture\":true,\"runtimeFrames\":"<<frames<<",\"maxPalmErrorMm\":"<<maxPalmError<<",\"weaponChangedPixels\":"<<totalChanged<<",\"originalModels\":[25,3],\"originalMtpClips\":15}\n";
 std::cout<<"Weapon hand runtime rendering, 2 rigs, 2 weapons, 4 actions, "<<frames<<" frames PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
}
