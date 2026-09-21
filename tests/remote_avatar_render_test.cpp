#include "remote_avatar.h"
#include "character_renderer.h"
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
using namespace mgo2mt;
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
CharacterModel quad(float left,float right,float bottom,float top,float z,uint16_t color){
 CharacterModel model;model.bounds={left,bottom,z,right,top,z};
 for(auto p:std::array<std::array<float,3>,4>{{{left,bottom,z},{right,bottom,z},{right,top,z},{left,top,z}}}){ModelVertex v{};v.x=p[0];v.y=p[1];v.z=p[2];v.nz=-1;v.lr=v.lg=v.lb=v.lit=1;model.vertices.push_back(v);}
 model.indices={0,1,2,0,2,3};model.parts.push_back({0,6,0,0});
 model.textures.push_back({4,4,9,{uint8_t(color),uint8_t(color>>8),uint8_t(color),uint8_t(color>>8),0,0,0,0}});return model;
}
size_t colored(const Frame& image,unsigned channel){size_t count=0;for(size_t i=0;i<image.rgba.size();i+=4)if(image.rgba[i+channel]>240&&image.rgba[i+(channel+1)%3]<10&&image.rgba[i+(channel+2)%3]<10)++count;return count;}
float red_center(const Frame& image){double sum=0;size_t count=0;for(size_t i=0;i<image.rgba.size();i+=4)if(image.rgba[i]>240&&image.rgba[i+1]<10&&image.rgba[i+2]<10){sum+=double((i/4)%image.width);++count;}check(count>100,"Red avatar fixture not visible");return float(sum/count);}
size_t changed(const Frame& a,const Frame& b){check(a.rgba.size()==b.rgba.size(),"Capture dimensions changed");size_t n=0;for(size_t i=0;i<a.rgba.size();i+=4)if(!std::equal(a.rgba.begin()+i,a.rgba.begin()+i+4,b.rgba.begin()+i))++n;return n;}
}
int main(int argc,char**argv){try{
 check(argc==4,"Usage: remote_avatar_render_test catalog.gwc player.gwmot output-dir");std::filesystem::path output=argv[3];std::filesystem::create_directories(output);
 CharacterCatalog catalog(read(argv[1]));PlayerMotionBank motions(read(argv[2]));
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context));
 CharacterRenderer wall(device.Get(),quad(-10000,10000,-5000,10000,6000,0x4208));WorldView camera{{0,1200,-3500},{0,-.04f,1}};
 combat::Snapshot state{1,1};host::Roster roster;roster.complete=true;
 std::array<PreparedCharacter,3> models;std::array<std::unique_ptr<CharacterRenderer>,3> renderers;
 for(unsigned i=0;i<3;++i){combat::Player p;p.identity={uint8_t(i),uint16_t(i+256),i+100};p.maxHp=p.hp=p.stamina=p.maxStamina=1000;p.alive=true;p.weapon=25;p.pose.feet={i==1?-600.f:600.f,0,0};state.players[i]=p;
  host::Player h{uint8_t(i),uint16_t(i+256),i+100,"Fixture",""};h.appearance=std::array<uint8_t,28>{};auto&a=*h.appearance;a[0]=i==2?1:0;a[2]=11;a[3]=22;a[15]=46;a[17]=57;if(i==2){a[5]=14;a[6]=5;}roster.slots[i]=h;
  models[i]=catalog.assemble(a);check(models[i].ready()&&!models[i].missingModels&&!models[i].missingColors,"male/female original catalog fixture complete");renderers[i]=std::make_unique<CharacterRenderer>(device.Get(),models[i].model);}
 remote::Scene scene;auto self=state.players[0]->identity;
 auto draw=[&](uint64_t now,const char*name){check(scene.update(state,roster,self,now),"valid remote frame");auto avatars=scene.sample(now+100);wall.render(context.Get(),0,false,&camera);auto before=frame(device.Get(),context.Get(),wall);size_t count=0;
  for(const auto&a:avatars){check(a.identity.slot!=0,"self never submitted");auto i=a.identity.slot;auto pose=motions.sample(a.motion,a.seconds);check(bool(pose),"remote original motion present");catalog.pose(models[i],*pose);renderers[i]->update_vertices(context.Get(),models[i].model.vertices);renderers[i]->render(context.Get(),a.yaw,false,&camera,&wall,&a.origin);auto next=frame(device.Get(),context.Get(),wall);check(changed(before,next)>100,"each remote original avatar visible on shared surface");before=next;++count;}
  write(output/name,before);std::cout<<name<<" avatars="<<count<<'\n';return std::pair{before,count};};
 auto standing=draw(0,"remote-standing.bmp");check(standing.second==2,"male and female visible together");
 state.players[1]->pose.capsule={260,1100,2};state.players[2]->pose.capsule={260,560,2};++state.revision;auto posture=draw(300,"remote-crouch-prone.bmp");check(changed(standing.first,posture.first)>1000,"host capsule changed actual rendered posture");
 state.players[1]->hp=0;state.players[1]->alive=false;++state.revision;auto death=draw(600,"remote-dead.bmp");check(changed(posture.first,death.first)>1000,"dead posture updates actual mesh");
 state.players[1]->life=2;state.players[1]->hp=1000;state.players[1]->alive=true;state.players[1]->pose.capsule={260,1700,2};state.players[1]->pose.feet[0]=-900;++state.revision;auto respawn=draw(900,"remote-respawn.bmp");check(changed(death.first,respawn.first)>1000,"new life restores standing pose at new origin");
 roster.slots[1].reset();auto removal=draw(1200,"remote-removed.bmp");check(removal.second==1&&changed(respawn.first,removal.first)>1000,"departed actor vanishes from surface");
 scene.clear();check(scene.sample(1500).empty(),"scene clear leaves no stale render submissions");std::cout<<"WARP original male/female remote pose, downed state, life and departure passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
