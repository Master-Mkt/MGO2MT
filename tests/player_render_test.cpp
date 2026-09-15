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
CharacterModel quad(float left,float right,float bottom,float top,float z,uint16_t color){
 CharacterModel model;model.bounds={left,bottom,z,right,top,z};
 for(auto p:std::array<std::array<float,3>,4>{{{left,bottom,z},{right,bottom,z},{right,top,z},{left,top,z}}}){ModelVertex v{};v.x=p[0];v.y=p[1];v.z=p[2];v.nz=-1;v.lr=v.lg=v.lb=v.lit=1;model.vertices.push_back(v);}
 model.indices={0,1,2,0,2,3};model.parts.push_back({0,6,0,0});
 model.textures.push_back({4,4,9,{uint8_t(color),uint8_t(color>>8),uint8_t(color),uint8_t(color>>8),0,0,0,0}});return model;
}
size_t colored(const Frame& image,unsigned channel){size_t count=0;for(size_t i=0;i<image.rgba.size();i+=4)if(image.rgba[i+channel]>240&&image.rgba[i+(channel+1)%3]<10&&image.rgba[i+(channel+2)%3]<10)++count;return count;}
float red_center(const Frame& image){double sum=0;size_t count=0;for(size_t i=0;i<image.rgba.size();i+=4)if(image.rgba[i]>240&&image.rgba[i+1]<10&&image.rgba[i+2]<10){sum+=double((i/4)%image.width);++count;}check(count>100,"Red avatar fixture not visible");return float(sum/count);}
size_t changed(const Frame& a,const Frame& b){check(a.rgba.size()==b.rgba.size(),"Capture dimensions changed");size_t n=0;for(size_t i=0;i<a.rgba.size();i+=4)if(!std::equal(a.rgba.begin()+i,a.rgba.begin()+i+4,b.rgba.begin()+i))++n;return n;}
void synthetic(ID3D11Device* device,ID3D11DeviceContext* context){
 CharacterRenderer wall(device,quad(-3000,3000,-2000,2000,3000,0x001f));
 CharacterRenderer red(device,quad(-300,300,-300,300,0,0xf800)),green(device,quad(-300,300,-300,300,0,0x07e0));
 WorldView camera{{0,0,0},{0,0,1}};std::array<float,3> origin{0,0,2000};
 wall.render(context,0,false,&camera);auto background=frame(device,context,wall);check(colored(background,2)>100000,"Wall fixture not rendered");
 red.render(context,0,false,&camera,&wall,&origin);auto foreground=frame(device,context,wall);check(colored(foreground,0)>1000,"Foreground avatar absent from shared surface");check(colored(foreground,2)>100000,"Avatar pass cleared the stage");check(changed(background,foreground)==colored(foreground,0),"Avatar pass changed unrelated background pixels");
 origin[2]=2500;green.render(context,0,false,&camera,&wall,&origin);check(frame(device,context,wall).rgba==foreground.rgba,"Avatar did not write shared scene depth");
 wall.render(context,0,false,&camera);origin[2]=4000;red.render(context,0,false,&camera,&wall,&origin);check(frame(device,context,wall).rgba==background.rgba,"Avatar behind wall ignored existing depth");
 wall.render(context,0,false,&camera);origin={900,0,2000};red.render(context,0,false,&camera,&wall,&origin);auto translated=frame(device,context,wall);check(red_center(translated)<red_center(foreground)-100,"Avatar world origin translation not applied");
 CharacterRenderer asymmetric(device,quad(0,600,-250,250,0,0xf800));origin={0,0,2200};
 wall.render(context,0,false,&camera);asymmetric.render(context,0,false,&camera,&wall,&origin);auto right=frame(device,context,wall);
 wall.render(context,0,false,&camera);asymmetric.render(context,3.14159265358979323846f,false,&camera,&wall,&origin);auto left=frame(device,context,wall);check(red_center(right)<background.width*.5f-25&&red_center(left)>background.width*.5f+25,"Avatar world yaw did not rotate geometry");
 std::cout<<"Synthetic WARP shared surface, retained background, wall occlusion, shared depth write, origin translation and yaw verified\n";
}
void original(ID3D11Device* device,ID3D11DeviceContext* context,char** argv,int argc){
 std::filesystem::path stagePath=argv[1],output=argv[4];std::filesystem::create_directories(output);
 CharacterModel world(read(stagePath));if(argc>5){std::ifstream input(argv[5]);auto lighting=stage::Lighting::read(input);for(auto&v:world.vertices){auto light=lighting.sample({v.x,v.y,v.z},{v.nx,v.ny,v.nz});v.lr=light.color[0];v.lg=light.color[1];v.lb=light.color[2];v.lit=1;}}
 std::ifstream collisionFile(stagePath.parent_path()/"n022a.collision.cfg");auto collision=stage::Collision::read(collisionFile);stage::Navigation navigation;check(navigation.place(collision,{-42878.8359f,3000,29781.7969f}),"Original avatar inspection anchor unavailable");auto origin=navigation.feet();
 CharacterCatalog catalog(read(argv[2]));PlayerMotionBank motions(read(argv[3]));std::array<uint8_t,28> appearance{};appearance[2]=11;appearance[3]=22;appearance[15]=46;appearance[17]=57;auto body=catalog.assemble(appearance);check(body.ready()&&!body.missingModels,"Original avatar fixture missing parts");
 CharacterRenderer stageRenderer(device,world),avatar(device,body.model);
 struct Choice {PlayerMotion action;const char* name;};const Choice choices[]={{PlayerMotion::Walk,"walk"},{PlayerMotion::Run,"run"},{PlayerMotion::ProneForward,"prone"},{PlayerMotion::SupineIdle,"supine"}};
 constexpr float yaw=2.2f;for(const auto&choice:choices){auto pose=motions.sample(choice.action,.23);check(bool(pose),"Restored avatar motion unavailable");catalog.pose(body,*pose);avatar.update_vertices(context,body.model.vertices);
  float low=1e9f,high=-1e9f;for(const auto&v:body.model.vertices){low=std::min(low,v.y);high=std::max(high,v.y);}std::cout<<"motion="<<choice.name<<" local_y="<<low<<","<<high<<" floor_y="<<origin[1]<<'\n';
  for(unsigned side=0;side<2;++side){float angle=yaw+(side?1.57079632679f:0.f);float targetHeight=choice.action==PlayerMotion::Walk||choice.action==PlayerMotion::Run?1000.f:450.f;WorldView camera;
   camera.eye={origin[0]-std::sin(angle)*3600,origin[1]+targetHeight+900,origin[2]-std::cos(angle)*3600};camera.direction={origin[0]-camera.eye[0],origin[1]+targetHeight-camera.eye[1],origin[2]-camera.eye[2]};
   stageRenderer.render(context,0,false,&camera);auto background=frame(device,context,stageRenderer);avatar.render(context,yaw,false,&camera,&stageRenderer,&origin);auto shown=frame(device,context,stageRenderer);auto visible=changed(background,shown);
   auto name=std::string(choice.name)+(side?"-side.bmp":"-rear.bmp");write(output/name,shown);std::cout<<"capture="<<name<<" avatar_pixels="<<visible<<'\n';check(visible>50,"Original avatar fully hidden at inspected camera");
  }
 }
}
}
int main(int argc,char**argv){try{
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context));synthetic(device.Get(),context.Get());
 if(argc>1){check(argc>=5,"Usage: player_render_test [stage.gwm catalog.gwc player.gwmot output-dir [lighting.cfg]]");original(device.Get(),context.Get(),argv,argc);}return 0;
}catch(const std::exception&error){std::cerr<<error.what()<<'\n';return 1;}}
