#include "character_catalog.h"
#include "character_renderer.h"
#include "motion_blend_presentation.h"
#include "cover_motion.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2win;
using Microsoft::WRL::ComPtr;
namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void ok(HRESULT value){check(SUCCEEDED(value),"Cover WARP resource failure");}
std::vector<char> read(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);check(bool(in),"Cover render asset missing");return {(std::istreambuf_iterator<char>(in)),{}};}
bool same(const MotionPose& a,const MotionPose& b){return a.rootBone==b.rootBone&&a.root==b.root&&a.rotations==b.rotations;}
float distance(std::array<float,3>a,std::array<float,3>b){float sum=0;for(unsigned i=0;i<3;++i)sum+=(a[i]-b[i])*(a[i]-b[i]);return std::sqrt(sum);}
void healthy(const CharacterCatalog& catalog,const PreparedCharacter& body){
 const auto bones=catalog.skeleton(body.gender);check(body.bonePositions.size()==bones.size(),"Complete gender-specific rig retained");
 for(const auto& bone:bones){auto point=body.bone_position(bone.key);check(bool(point),"Required bone missing");for(float v:*point)check(std::isfinite(v)&&std::abs(v)<15000,"Bone finite and inside native inspection bound");
  if(bone.parent>=0){const float original=distance(bone.position,bones[bone.parent].position),posed=distance(*point,*body.bone_position(bones[bone.parent].key));check(std::abs(original-posed)<.2f,"Original bone length preserved across cover skinning");}}
 for(const auto& v:body.model.vertices){check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::abs(v.x)<15000&&std::abs(v.y)<15000&&std::abs(v.z)<15000,"Every skinned vertex finite and within 15000 units");const float length=v.nx*v.nx+v.ny*v.ny+v.nz*v.nz;check(std::isfinite(length)&&std::abs(length-1)<.001f,"Skinned normals stay normalized");}
}
struct Image{unsigned width=0,height=0;std::vector<unsigned char> pixels;};
Image capture(ID3D11Device* device,ID3D11DeviceContext* context,const CharacterRenderer& renderer){
 ComPtr<ID3D11Resource> resource;renderer.view()->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;ok(resource.As(&texture));D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);check(desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM,"RGBA texture expected");
 desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> copy;ok(device->CreateTexture2D(&desc,nullptr,&copy));context->CopyResource(copy.Get(),texture.Get());
 D3D11_MAPPED_SUBRESOURCE mapped{};ok(context->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped));Image image{desc.Width,desc.Height,std::vector<unsigned char>(size_t(desc.Width)*desc.Height*4)};
 for(unsigned y=0;y<image.height;++y)std::copy_n(static_cast<const unsigned char*>(mapped.pData)+size_t(y)*mapped.RowPitch,size_t(image.width)*4,image.pixels.data()+size_t(y)*image.width*4);context->Unmap(copy.Get(),0);return image;
}
void write(const std::filesystem::path& path,const Image& image){
 auto pixels=image.pixels;for(size_t i=0;i<pixels.size();i+=4)std::swap(pixels[i],pixels[i+2]);BITMAPFILEHEADER file{};BITMAPINFOHEADER bitmap{};bitmap.biSize=sizeof(bitmap);bitmap.biWidth=LONG(image.width);bitmap.biHeight=-LONG(image.height);bitmap.biPlanes=1;bitmap.biBitCount=32;bitmap.biSizeImage=DWORD(pixels.size());file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(bitmap);file.bfSize=file.bfOffBits+bitmap.biSizeImage;
 std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&file),sizeof(file));out.write(reinterpret_cast<const char*>(&bitmap),sizeof(bitmap));out.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(pixels.size()));check(bool(out),"Cover capture write failed");
}
size_t visible(const Image& image){size_t count=0;for(unsigned y=0;y<image.height;++y)for(unsigned x=0;x<image.width;++x)if(image.pixels[(size_t(y)*image.width+x)*4+3]){++count;check(x&&y&&x+1<image.width&&y+1<image.height,"Body clipped at fixed-camera capture edge");}return count;}
void series(ID3D11Device* device,ID3D11DeviceContext* context,const CharacterCatalog& catalog,PreparedCharacter body,
            PlayerMotion fromKey,const MotionPose& from,PlayerMotion toKey,const MotionPose& to,const std::filesystem::path& output,const std::string& name){
 motion_blend::Lane lane;const motion_blend::Scope scope{1,2,body.gender+1,1,1};auto a=catalog.complete_pose(body.gender,from),b=catalog.complete_pose(body.gender,to);
 catalog.pose(body,lane.sample(scope,uint64_t(fromKey)+1,0,a,0,5));CharacterRenderer renderer(device,body.model);
 // Fixed model-space camera: no per-frame fit, preview lift, or root re-scaling.
 const WorldView camera{{-2300,1600,3500},{2300,-750,-3500}};
 auto draw=[&](){healthy(catalog,body);renderer.update_vertices(context,body.model.vertices);renderer.render(context,0,false,&camera);return capture(device,context,renderer);};
 const auto original=draw();catalog.pose(body,lane.sample(scope,uint64_t(toKey)+1,0,b,100,5));
 check(lane.progress()==0&&draw().pixels==original.pixels,"Phase boundary alpha0 freezes exact previously rendered pixels");
 Image last;
 for(unsigned step=0;step<=4;++step){
  if(step)catalog.pose(body,lane.sample(scope,uint64_t(toKey)+1,step*.05,b,.05,5));
  const auto shown=draw();check(std::abs(lane.progress()-step*.25f)<1e-5,"Expected 0/25/50/75/100 percent transition progress");
  check(visible(shown)>1000,"Gender body is visible in cover capture");write(output/(name+"-"+std::to_string(step*25)+".bmp"),shown);
  const auto held=*lane.pose();catalog.pose(body,lane.sample(scope,uint64_t(toKey)+1,step*.05,b,0,5));
  check(same(*lane.pose(),held)&&draw().pixels==shown.pixels,"Paused zero-dt blend preserves exact pose and pixels");last=shown;
 }
 motion_blend::Lane endpoint;catalog.pose(body,endpoint.sample(scope,100,0,b,0,5));check(draw().pixels==last.pixels,"100 percent equals independently normalized target skin");
 std::cout<<name<<" 0/25/50/75/100 captures PASS\n";
}
}
int main(int argc,char** argv){try{
 check(argc==5,"cover_motion_render_test appearance.gwc player.gwmot cover.gwmot outputdir");CharacterCatalog catalog(read(argv[1]));PlayerMotionBank player(read(argv[2]));cover::CoverMotionBank bank(read(argv[3]));std::filesystem::path output=argv[4];std::filesystem::create_directories(output);
 ComPtr<ID3D11Device>device;ComPtr<ID3D11DeviceContext>context;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context));
 size_t poses=0,captures=0;
 for(unsigned gender=0;gender<2;++gender){std::array<uint8_t,28> appearance{};appearance[0]=uint8_t(gender);appearance[2]=11;appearance[3]=22;appearance[15]=46;appearance[17]=57;auto body=catalog.assemble(appearance);check(body.ready()&&!body.missingModels&&!body.missingColors,"Original gender model assembles");std::string prefix=gender?"female":"male";
  for(uint32_t id=0;id<uint32_t(cover::Action::count);++id){auto action=cover::Action(id);for(double t:{0.,cover::duration(action)*.5,cover::duration(action)}){catalog.pose(body,catalog.complete_pose(gender,bank.sample(action,t)->pose));healthy(catalog,body);++poses;}}
  CharacterRenderer renderer(device.Get(),body.model);const WorldView camera{{-2300,1600,3500},{2300,-750,-3500}};
  auto draw=[&](const MotionPose& pose,const std::string& name){catalog.pose(body,catalog.complete_pose(gender,pose));healthy(catalog,body);renderer.update_vertices(context.Get(),body.model.vertices);renderer.render(context.Get(),0,false,&camera);auto image=capture(device.Get(),context.Get(),renderer);check(visible(image)>1000,"Body visible in fixed camera");write(output/(prefix+"-"+name+".bmp"),image);++captures;};
  for(auto [action,name]:{std::pair{cover::Action::stand_right,"wall-right"},std::pair{cover::Action::stand_left,"wall-left"},std::pair{cover::Action::move_right,"slide-right"},std::pair{cover::Action::move_left,"slide-left"},std::pair{cover::Action::peek_right_hold,"peek-right"},std::pair{cover::Action::peek_left_hold,"peek-left"},std::pair{cover::Action::crouch_right,"crouch-wall"},std::pair{cover::Action::crouch_peek_left_hold,"crouch-peek-left"}})draw(bank.sample(action,.3)->pose,name);
  auto aim=catalog.complete_pose(gender,*player.sample(PlayerMotion::Aim,.3));catalog.pose(body,aim);auto base=body;const auto headKey=catalog.skeleton(gender)[4].key;const auto baseHead=*base.bone_position(headKey);
  std::array<float,2> headX{};
  for(int side:{-1,1}){auto p=cover::native_side_lean(aim,side,1);check(bool(p),"Native lean handles both base rigs");draw(*p,side<0?"native-lean-left":"native-lean-right");headX[side>0]=(*body.bone_position(headKey))[0];
   for(unsigned i:{13u,14u,15u,16u,17u,18u,19u,20u}){const auto key=catalog.skeleton(gender)[i].key;check(distance(*body.bone_position(key),*base.bone_position(key))<.01f,"Native upper-spine lean keeps all leg and foot joints fixed");}
  }
  check(headX[0]>baseHead[0]&&headX[1]<baseHead[0],"Native lean moves head to actual corresponding side");std::cout<<prefix<<" native head delta "<<headX[0]-baseHead[0]<<" / "<<headX[1]-baseHead[0]<<'\n';
  auto idle=*player.sample(PlayerMotion::Idle,.1),wall=bank.sample(cover::Action::stand_right,.1)->pose,peek=bank.sample(cover::Action::peek_right_hold,.1)->pose;
  series(device.Get(),context.Get(),catalog,body,PlayerMotion(0),idle,PlayerMotion(1),wall,output,prefix+"-attach");
  series(device.Get(),context.Get(),catalog,body,PlayerMotion(1),wall,PlayerMotion(2),peek,output,prefix+"-peek");
  series(device.Get(),context.Get(),catalog,body,PlayerMotion(2),peek,PlayerMotion(0),idle,output,prefix+"-detach");captures+=15;
 }
 std::cout<<"cover WARP PASS: "<<poses<<" rig poses; "<<captures<<" real images; full bone lengths, foot anchoring, native left/right, blend boundaries\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

