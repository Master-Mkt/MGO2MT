#include "character_catalog.h"
#include "character_renderer.h"
#include "motion_blend_presentation.h"
#include "evade_motion.h"
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
void ok(HRESULT value){check(SUCCEEDED(value),"Evade WARP resource failure");}
std::vector<char> read(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);check(bool(in),"Evade render asset missing");return {(std::istreambuf_iterator<char>(in)),{}};}
bool same(const MotionPose& a,const MotionPose& b){return a.rootBone==b.rootBone&&a.root==b.root&&a.rotations==b.rotations;}
float distance(std::array<float,3>a,std::array<float,3>b){float sum=0;for(unsigned i=0;i<3;++i)sum+=(a[i]-b[i])*(a[i]-b[i]);return std::sqrt(sum);}
void healthy(const CharacterCatalog& catalog,const PreparedCharacter& body){
 const auto bones=catalog.skeleton(body.gender);check(body.bonePositions.size()==bones.size(),"Complete gender-specific rig retained");
 for(const auto& bone:bones){auto point=body.bone_position(bone.key);check(bool(point),"Required bone missing");for(float v:*point)check(std::isfinite(v)&&std::abs(v)<15000,"Bone finite and inside native inspection bound");
  if(bone.parent>=0){const float original=distance(bone.position,bones[bone.parent].position),posed=distance(*point,*body.bone_position(bones[bone.parent].key));check(std::abs(original-posed)<.2f,"Original bone length preserved across evasion skinning");}}
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
 std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&file),sizeof(file));out.write(reinterpret_cast<const char*>(&bitmap),sizeof(bitmap));out.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(pixels.size()));check(bool(out),"Evade capture write failed");
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
  check(visible(shown)>1000,"Gender body is visible in evasion capture");write(output/(name+"-"+std::to_string(step*25)+".bmp"),shown);
  const auto held=*lane.pose();catalog.pose(body,lane.sample(scope,uint64_t(toKey)+1,step*.05,b,0,5));
  check(same(*lane.pose(),held)&&draw().pixels==shown.pixels,"Paused zero-dt blend preserves exact pose and pixels");last=shown;
 }
 motion_blend::Lane endpoint;catalog.pose(body,endpoint.sample(scope,100,0,b,0,5));check(draw().pixels==last.pixels,"100 percent equals independently normalized target skin");
 std::cout<<name<<" 0/25/50/75/100 captures PASS\n";
}
void phase_images(ID3D11Device* device,ID3D11DeviceContext* context,const CharacterCatalog& catalog,PreparedCharacter body,const PlayerMotionBank& raw,const std::filesystem::path& output,const std::string& prefix){
 CharacterRenderer renderer(device,body.model);const WorldView camera{{-2300,1600,3500},{2300,-750,-3500}};
 for(auto [phase,name]:{std::pair{PlayerMotion::Roll,"roll"},std::pair{PlayerMotion::RollRecover,"recover"},std::pair{PlayerMotion::Backstep,"backstep"}}){
  const auto* clip=raw.find(phase);
  for(unsigned part=0;part<=4;++part){const double seconds=double(clip->frames)*part/4./60.;catalog.pose(body,catalog.complete_pose(body.gender,*raw.sample(phase,seconds)));healthy(catalog,body);
   renderer.update_vertices(context,body.model.vertices);renderer.render(context,0,false,&camera);const auto image=capture(device,context,renderer);check(visible(image)>1000,"Actual clip pose visible without camera fitting");
   write(output/(prefix+"-clip-"+name+"-"+std::to_string(part*25)+".bmp"),image);
  }
 }
}
void timeline(const CharacterCatalog& catalog,PreparedCharacter body,const PlayerMotionBank& normal,const player::EvadeMotionBank& evade){
 motion_blend::Lane lane;const motion_blend::Scope scope{1,2,body.gender+1,1,1};
 auto sample=[&](PlayerMotion phase,double clipTime,const MotionPose& pose,double dt){auto complete=catalog.complete_pose(body.gender,pose);catalog.pose(body,lane.sample(scope,uint64_t(phase)+1,clipTime,complete,dt,5));healthy(catalog,body);};
 sample(PlayerMotion::Run,.2,*normal.sample(PlayerMotion::Run,.2),0);
 auto start=evade.sample(combat::EvadeKind::roll,0);auto before=*lane.pose();sample(start->phase,0,start->pose,1./60.);check(lane.progress()==0&&same(before,*lane.pose()),"Run-to-roll enters with no pop");
 for(unsigned frame=1;frame<40;++frame){auto value=evade.sample(combat::EvadeKind::roll,double(frame)/60);sample(value->phase,double(frame)/60,value->pose,1./60.);}
 before=*lane.pose();auto recover=evade.sample(combat::EvadeKind::roll,40./60.);sample(recover->phase,0,recover->pose,1./60.);check(lane.progress()==0&&same(before,*lane.pose()),"Roll-to-recovery boundary blends original independent clip roots");
 for(unsigned frame=1;frame<=45;++frame){auto value=evade.sample(combat::EvadeKind::roll,40./60.+double(frame)/60);sample(value->phase,double(frame)/60,value->pose,1./60.);}
 before=*lane.pose();sample(PlayerMotion::Idle,0,*normal.sample(PlayerMotion::Idle,0),1./60.);check(lane.progress()==0&&same(before,*lane.pose()),"Recovery-to-idle starts from displayed endpoint");
 for(unsigned frame=1;frame<=12;++frame)sample(PlayerMotion::Idle,double(frame)/60,*normal.sample(PlayerMotion::Idle,double(frame)/60),1./60.);
 check(lane.progress()==1,"Idle completes roll handoff");
 before=*lane.pose();auto back=evade.sample(combat::EvadeKind::backstep,0);sample(back->phase,0,back->pose,1./60.);check(lane.progress()==0&&same(before,*lane.pose()),"Idle-to-backstep starts with blend");
 for(unsigned frame=1;frame<=45;++frame){auto value=evade.sample(combat::EvadeKind::backstep,double(frame)/60);sample(value->phase,double(frame)/60,value->pose,1./60.);}
 before=*lane.pose();sample(PlayerMotion::Idle,0,*normal.sample(PlayerMotion::Idle,0),1./60.);check(lane.progress()==0&&same(before,*lane.pose()),"Backstep-to-idle uses blend");
}
Image directional(ID3D11Device* device,ID3D11DeviceContext* context,const CharacterCatalog& catalog,PreparedCharacter body,const PlayerMotionBank& normal,const player::EvadeMotionBank& evade,combat::EvadeKind kind,const std::filesystem::path& output,const std::string& prefix){
 // Input uses a camera-relative quarter turn once, then the HOST packet carries
 // that travel yaw unchanged through the remote renderer.
 const float facing=kind==combat::EvadeKind::rollLeft?-1.57079633f:1.57079633f;
 motion_blend::Lane lane;const motion_blend::Scope scope{1,2,body.gender+1,1,1};float yaw=0;uint64_t source=1;
 auto start=catalog.complete_pose(body.gender,*normal.sample(PlayerMotion::Run,.2));catalog.pose(body,lane.sample(scope,source,0,start,0,5));CharacterRenderer renderer(device,body.model);const WorldView camera{{-2300,1600,3500},{2300,-750,-3500}};
 auto worldBones=[&]{std::vector<std::array<float,3>> result;for(const auto& bone:catalog.skeleton(body.gender)){const auto p=*body.bone_position(bone.key);result.push_back({std::cos(yaw)*p[0]+std::sin(yaw)*p[2],p[1],-std::sin(yaw)*p[0]+std::cos(yaw)*p[2]});}return result;};
 auto draw=[&]{healthy(catalog,body);renderer.update_vertices(context,body.model.vertices);const std::array<float,3> origin{};renderer.render(context,yaw,false,&camera,nullptr,&origin);return capture(device,context,renderer);};
 const auto roll=evade.sample(kind,.3),recover=evade.sample(kind,.9);
 Image rollImage;std::array<MotionPose,3> poses{roll->pose,recover->pose,*normal.sample(PlayerMotion::Idle,.1)};
 for(unsigned phase=0;phase<poses.size();++phase){const auto previous=worldBones();const float targetYaw=phase==2?0:facing;const auto target=catalog.complete_pose(body.gender,poses[phase]);
  lane.rebase({},yaw,{},targetYaw,catalog.skeleton(body.gender).front().position);yaw=targetYaw;++source;catalog.pose(body,lane.sample(scope,source,0,target,.1,5));check(lane.progress()==0,"Side heading change starts blend at zero");auto preserved=worldBones();for(size_t i=0;i<previous.size();++i)check(distance(previous[i],preserved[i])<.05f,"Every bone world position preserved at side-roll transition including return");
  for(unsigned step=0;step<=4;++step){if(step)catalog.pose(body,lane.sample(scope,source,.05*step,target,.05,5));auto rendered=draw();if(phase==0&&step==4)rollImage=rendered;check(visible(rendered)>1000,"Side rolling body visible");check(std::abs(lane.progress()-step*.25f)<1e-5,"Side transition uses existing configured blend rate");write(output/(prefix+"-phase"+std::to_string(phase)+"-"+std::to_string(step*25)+".bmp"),rendered);}
 }
 std::cout<<prefix<<" direction / all bone world continuity / 3 phase blends PASS\n";return rollImage;
}
}
int main(int argc,char** argv){try{
 check(argc==5,"usage: evade_motion_render_test appearance.gwc player.gwmot evade.gwmot output-directory");
 CharacterCatalog catalog(read(argv[1]));PlayerMotionBank normal(read(argv[2]));const auto data=read(argv[3]);player::EvadeMotionBank evade(data);PlayerMotionBank raw(data);
 const std::filesystem::path output=argv[4];std::filesystem::create_directories(output);
 for(auto kind:{combat::EvadeKind::none,static_cast<combat::EvadeKind>(255)})check(!evade.sample(kind,0),"Invalid action kind has no pose");
 for(double time:{-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})check(!evade.sample(combat::EvadeKind::roll,time),"Invalid evasion clock has no pose");
 const double boundary=40./60.;check(evade.sample(combat::EvadeKind::roll,0)->phase==PlayerMotion::Roll&&evade.sample(combat::EvadeKind::roll,boundary-1e-7)->phase==PlayerMotion::Roll&&evade.sample(combat::EvadeKind::roll,boundary)->phase==PlayerMotion::RollRecover,"Exact 40-frame phase boundary");
 check(same(evade.sample(combat::EvadeKind::roll,boundary)->pose,*raw.sample(PlayerMotion::RollRecover,0)),"Recovery starts at its own zero, not the roll's absolute time");
 check(same(evade.sample(combat::EvadeKind::roll,85./60.+100)->pose,*raw.sample(PlayerMotion::RollRecover,100)),"Non-loop recovery safely clamps after endpoint");
 check(same(evade.sample(combat::EvadeKind::backstep,100)->pose,*raw.sample(PlayerMotion::Backstep,100)),"Non-loop backstep safely clamps after endpoint");
 for(auto side:{combat::EvadeKind::rollLeft,combat::EvadeKind::rollRight})for(double time:{0.,.3,40./60.,.9,85./60.,100.}){auto a=evade.sample(side,time),b=evade.sample(combat::EvadeKind::roll,time);check(a&&b&&a->phase==b->phase&&same(a->pose,b->pose),"Side roll preserves original verified forward/recovery sample byte values");}
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context));
 size_t checkedFrames=0;
 for(unsigned gender=0;gender<2;++gender){std::array<uint8_t,28> appearance{};appearance[0]=uint8_t(gender);appearance[2]=11;appearance[3]=22;appearance[15]=46;appearance[17]=57;auto body=catalog.assemble(appearance);check(body.ready()&&!body.missingModels&&!body.missingColors,"Both original gender models assemble");const std::string prefix=gender?"female":"male";
  for(auto phase:{PlayerMotion::Roll,PlayerMotion::RollRecover,PlayerMotion::Backstep}){auto clip=raw.find(phase);check(clip,"Required evasion clip exists");for(unsigned frame=0;frame<=clip->frames;++frame){catalog.pose(body,catalog.complete_pose(gender,*raw.sample(phase,double(frame)/60)));healthy(catalog,body);++checkedFrames;}}
  timeline(catalog,body,normal,evade);
  const auto left=directional(device.Get(),context.Get(),catalog,body,normal,evade,combat::EvadeKind::rollLeft,output,prefix+"-left");
  const auto right=directional(device.Get(),context.Get(),catalog,body,normal,evade,combat::EvadeKind::rollRight,output,prefix+"-right");check(left.pixels!=right.pixels,"Actual WARP left/right travel headings produce different images");
  phase_images(device.Get(),context.Get(),catalog,body,raw,output,prefix);
  const auto idle=*normal.sample(PlayerMotion::Idle,.1),run=*normal.sample(PlayerMotion::Run,.2),rollStart=*raw.sample(PlayerMotion::Roll,0),rollEnd=*raw.sample(PlayerMotion::Roll,40./60.),recoverStart=*raw.sample(PlayerMotion::RollRecover,0),recoverEnd=*raw.sample(PlayerMotion::RollRecover,45./60.),backStart=*raw.sample(PlayerMotion::Backstep,0),backEnd=*raw.sample(PlayerMotion::Backstep,45./60.);
  series(device.Get(),context.Get(),catalog,body,PlayerMotion::Run,run,PlayerMotion::Roll,rollStart,output,prefix+"-run-roll");
  series(device.Get(),context.Get(),catalog,body,PlayerMotion::Roll,rollEnd,PlayerMotion::RollRecover,recoverStart,output,prefix+"-roll-recover");
  series(device.Get(),context.Get(),catalog,body,PlayerMotion::RollRecover,recoverEnd,PlayerMotion::Idle,idle,output,prefix+"-recover-idle");
  series(device.Get(),context.Get(),catalog,body,PlayerMotion::Idle,idle,PlayerMotion::Backstep,backStart,output,prefix+"-idle-backstep");
  series(device.Get(),context.Get(),catalog,body,PlayerMotion::Backstep,backEnd,PlayerMotion::Idle,idle,output,prefix+"-backstep-idle");
 }
 std::cout<<"Evade native clip adapter: 140 WARP captures (110 blends + 30 clip poses), "<<checkedFrames<<" gender/frame poses, complete skin and every phase blend PASS; original dispatcher and side clips not asserted\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
