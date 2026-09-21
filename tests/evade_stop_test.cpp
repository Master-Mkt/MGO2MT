#include "character_catalog.h"
#include "character_renderer.h"
#include "motion_blend_presentation.h"
#include "evade_motion.h"
#include "evade_travel_curve.h"
#include "player_control.h"
#include "stage_navigation.h"
#include "stage_water.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
using namespace mgo2mt;
using Microsoft::WRL::ComPtr;
namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void ok(HRESULT value){check(SUCCEEDED(value),"Evade WARP resource failure");}
std::vector<char> read(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);check(bool(in),"Evade render asset missing");return {(std::istreambuf_iterator<char>(in)),{}};}
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

using E=player::Evade;
using V=stage::Vec3;
combat::EvadeKind kind(E e){return e==E::roll?combat::EvadeKind::roll:e==E::rollLeft?combat::EvadeKind::rollLeft:e==E::rollRight?combat::EvadeKind::rollRight:e==E::backstep?combat::EvadeKind::backstep:combat::EvadeKind::none;}
float horizontal(V a,V b){return std::hypot(a[0]-b[0],a[2]-b[2]);}
stage::Collision floor_world(bool wall=false){
 std::ostringstream data;data<<"MGO2MT.STAGE_COLLISION 1 "<<(wall?8:4)<<' '<<(wall?4:2)<<'\n';
 data<<"-20000 0 -20000\n20000 0 -20000\n20000 0 20000\n-20000 0 20000\n";
 if(wall)data<<"-20000 0 900\n20000 0 900\n20000 5000 900\n-20000 5000 900\n";
 data<<"0 1 2 11282940 7\n0 2 3 11282940 7\n";if(wall)data<<"4 5 6 11282940 7\n4 6 7 11282940 7\n";
 std::istringstream in(data.str());return stage::Collision::read(in);
}
struct Runtime {
 player::Control control;stage::Navigation nav;const stage::Collision& world;E action;
 std::array<float,24> input{};double seconds=0;float facing=0;V start{};
 Runtime(const stage::Collision&w,E e,V hint={0,1000,0},bool checkRequestPosition=true):world(w),action(e){
  check(nav.place(world,hint),"Real Navigation placement on finite floor");start=nav.feet();nav.facing(0);
  control.step({},.01f,true,false,nav.grounded());
  input[e==E::backstep?17:e==E::rollLeft?18:e==E::rollRight?19:16]=1;input[5]=1;
  control.step(input,1.f/60,true,true,nav.grounded());check(control.evadeRequested==e,"Actual directional A edge requests expected maneuver");
  facing=e==E::rollLeft?1.57079632679f:e==E::rollRight?-1.57079632679f:0;
  check(control.begin_reviewed_evade(e,facing),"New reviewed entry API accepts request");
  if(player::is_roll(e))check(control.speed==0,"Request frame must not advance reviewed travel twice");
  advance(1.f/60);if(player::is_roll(e)&&checkRequestPosition)check(horizontal(nav.feet(),start)<.001f,"Actual request-frame Navigation displacement is zero");input={};
 }
 void advance(float dt){stage::WalkInput m{control.forward,control.right,0,0,control.speed,2,1.5f,0.f};nav.advance(world,m,dt);check(world.clear(nav.feet(),nav.capsule()),"Every actual controller movement retains capsule clearance");}
 void step(float dt,bool hold=false,bool run=false){input={};if(hold)input[16]=1;control.step(input,dt,true,run,nav.grounded());advance(dt);seconds+=dt;}
};
float exercise(const stage::Collision&world,E e,const std::vector<float>& steps){
 Runtime run(world,e);const double duration=combat::evade_runtime::profile(kind(e)).durationMs/1000.;V stopped{};bool saved=false;unsigned n=0;float peak=0;
 while(run.seconds<duration+2.05){const auto beforeTime=run.seconds;const auto before=run.nav.feet();const auto dt=steps[n++%steps.size()];run.step(dt);peak=std::max(peak,run.control.speed);
  if(player::is_roll(e)&&beforeTime>=1.251){check(horizontal(before,run.nav.feet())<.001f,"Last original ten frames and two seconds after release have no XZ travel");}
  if(run.seconds>=duration+.1&&!saved){stopped=run.nav.feet();saved=true;}
  if(saved)check(horizontal(stopped,run.nav.feet())<.001f,"Released controller stays at exact final horizontal position");
 }
 check(run.control.evade_active()==E::none,"Reviewed maneuver expires");check(peak<=6000.01f,"Original travel adapter respects HOST 6000 speed cap");
 const auto p=run.nav.feet();const float travel=horizontal(p,run.start);
 if(player::is_roll(e)){check(std::abs(travel-float(combat::evade_runtime::distance_seconds(2.)))<.15f,"Actual Navigation distance equals shared bounded cumulative curve");
  if(e==E::roll)check(std::abs(p[0])<.01f&&p[2]>0,"Forward roll direction");else check(std::abs(p[2])<.01f&&(e==E::rollLeft?p[0]>0:p[0]<0),"Lateral travel is rotated exactly once");
 }else{
  // The preserved backstep contract advances its request frame, then omits
  // the complete dt that crosses its expiry. Float accumulation can place an
  // exactly aligned final step on either side of that boundary.
  const auto profile=combat::evade_runtime::backstep;
  const double requestFrame=1./60.,maximumStep=*std::max_element(steps.begin(),steps.end());
  const double lower=profile.speed*(duration+requestFrame-maximumStep),upper=profile.speed*(duration+requestFrame);
  check(p[2]<0&&std::abs(p[0])<.01f&&travel>=lower-.2&&travel<=upper+.2,"Backstep retains request-frame travel and discards at most one expiry step");
 }
 return travel;
}
void held_and_cancel(const stage::Collision&world){
 for(bool running:{false,true}){Runtime run(world,E::roll);while(run.control.evade_active()!=E::none)run.step(1.f/120,true,running);const auto stopped=run.nav.feet();
  check(run.control.speed==0,"Natural terminal frame suppresses held movement");run.step(1.f/120,true,running);
  check(run.control.motion()==(running?player::Motion::run:player::Motion::walk)&&run.nav.feet()[2]>stopped[2],"Held stick resumes ordinary walk/run only after terminal frame");
 }
 for(bool held:{false,true}){Runtime run(world,E::roll);for(unsigned i=0;i<30;++i)run.step(1.f/120);const auto stopped=run.nav.feet();run.control.cancel_evade();
  check(run.control.speed==0&&run.control.evade_active()==E::none,"HOST terminal ACK cancel retires local predicted maneuver before step");run.step(1.f/120,held,false);
  if(held)check(run.control.motion()==player::Motion::walk&&run.nav.feet()[2]>stopped[2],"ACK-before-step permits ordinary held walk, not stale roll speed");
  else{for(unsigned i=0;i<240;++i)run.step(1.f/120);check(horizontal(stopped,run.nav.feet())<.001f,"ACK-before-step plus released input remains still for two seconds");}
 }
 auto wall=floor_world(true);Runtime blocked(wall,E::roll);for(unsigned i=0;i<220;++i)blocked.step(1.f/60);check(blocked.nav.feet()[2]>400&&blocked.nav.feet()[2]<551,"New travel curve remains stopped by real finite wall capsule sweep");
}
void slope_stop(){
 std::istringstream input("MGO2MT.STAGE_COLLISION 1 4 2\n-20000 -4000 -20000\n20000 -4000 -20000\n20000 4000 20000\n-20000 4000 20000\n0 1 2 11282940 7\n0 2 3 11282940 7\n");
 const auto slope=stage::Collision::read(input);stage::Navigation nav;check(nav.place(slope,{0,1000,0}),"Synthetic walkable twenty-percent slope placement");
 const auto initial=nav.feet();for(unsigned i=0;i<240;++i)nav.advance(slope,{},1.f/120);check(horizontal(initial,nav.feet())<.001f&&nav.grounded(),"Walkable slope gravity alone must not accumulate tangential XZ drift");
 auto move=[&](float direction){const auto before=nav.feet();for(unsigned i=0;i<240;++i){nav.advance(slope,{direction,0,0,0,1000},1.f/120);check(slope.clear(nav.feet(),nav.capsule()),"Slope walking retains real capsule clearance");}
  check((nav.feet()[2]-before[2])*direction>1500&&(nav.feet()[1]-before[1])*direction>250,"Input-driven uphill and downhill walking still advances along slope");
  const auto stopped=nav.feet();for(unsigned i=0;i<240;++i)nav.advance(slope,{},1.f/120);check(horizontal(stopped,nav.feet())<.001f,"Releasing input after slope walking stops horizontal gravity sliding");};
 move(1);move(-1);
 Runtime roll(slope,E::roll);while(roll.seconds<1.5)roll.step(1.f/120);const auto end=roll.nav.feet();for(unsigned i=0;i<240;++i)roll.step(1.f/120);check(horizontal(end,roll.nav.feet())<.001f,"Actual roll release remains stopped on walkable slope for two seconds");
}
V world_point(V p,V o,float yaw){const auto s=std::sin(yaw),c=std::cos(yaw);return {o[0]+c*p[0]+s*p[2],o[1]+p[1],o[2]-s*p[0]+c*p[2]};}
void render_case(ID3D11Device*d,ID3D11DeviceContext*c,const stage::Collision&world,const CharacterCatalog&catalog,const PlayerMotionBank&normal,const player::EvadeMotionBank&evade,unsigned gender,E e,const std::filesystem::path&output,unsigned& transitions){
 std::array<uint8_t,28> appearance{};appearance[0]=uint8_t(gender);appearance[2]=11;appearance[3]=22;appearance[15]=46;appearance[17]=57;auto body=catalog.assemble(appearance);check(body.ready()&&!body.missingModels&&!body.missingColors,"Real male/female model complete");
 Runtime run(world,e);motion_blend::Lane lane;const motion_blend::Scope scope{1,2,gender+1,1,1};V drawOrigin=run.nav.feet();float drawYaw=0;
 auto initial=catalog.complete_pose(gender,*normal.sample(PlayerMotion::Idle,0));catalog.pose(body,lane.sample(scope,1,0,initial,0,5));
 CharacterRenderer renderer(d,body.model);renderer.resize_target(d,640,480);
 const float finalDistance=float(combat::evade_runtime::distance_seconds(2.));V finalOrigin=run.start;finalOrigin[0]+=std::sin(run.facing)*finalDistance;finalOrigin[2]+=std::cos(run.facing)*finalDistance;
 const WorldView camera{{finalOrigin[0]-2300,finalOrigin[1]+1600,finalOrigin[2]+3500},{2300,-750,-3500},640.f/480};
 const auto prefix=std::string(gender?"female-":"male-")+(e==E::roll?"forward":e==E::rollLeft?"left":"right");
 auto draw=[&](const MotionPose&pose,V origin,float yaw){catalog.pose(body,pose);healthy(catalog,body);renderer.update_vertices(c,body.model.vertices);renderer.render(c,yaw,false,&camera,nullptr,&origin);return capture(d,c,renderer);};
 bool terminalSaved=false,stableSaved=false;Image stable;V stableOrigin{};MotionPose stablePose;unsigned phaseChanges=0;
 while(run.seconds<3.5){run.step(1.f/60);const bool active=run.control.evade_active()!=E::none;const auto sample=evade.sample(kind(run.control.evade_active()),run.control.evade_elapsed());
  auto target=catalog.complete_pose(gender,sample?sample->pose:*normal.sample(PlayerMotion::Idle,run.seconds));const uint64_t source=sample?100+unsigned(sample->phase):1;const float yaw=active?run.facing:0;const V origin=run.nav.feet();
  const bool changed=!lane.same_source(source);std::map<uint32_t,V> before;
  if(changed){for(auto [bone,p]:body.bonePositions)before[bone]=world_point(p,drawOrigin,drawYaw);lane.rebase(drawOrigin,drawYaw,origin,yaw,catalog.skeleton(gender).front().position);}
  const auto&pose=lane.sample(scope,source,active?run.control.evade_elapsed():run.seconds,target,1./60,5);catalog.pose(body,pose);healthy(catalog,body);
  if(changed){++phaseChanges;++transitions;check(lane.progress()==0,"Every runtime source transition starts at zero blend progress");for(auto [bone,p]:body.bonePositions)check(distance(world_point(p,origin,yaw),before.at(bone))<.15f,"Real moved world origins and side-yaw return preserve every bone at transition");}
  drawOrigin=origin;drawYaw=yaw;
  if(!terminalSaved&&run.seconds>=1.27){auto im=draw(pose,origin,yaw);check(visible(im)>300,"Real terminal recovery visible at fixed camera");write(output/(prefix+"-terminal-recovery.bmp"),im);terminalSaved=true;}
  if(!stableSaved&&run.seconds>=1.5){stableOrigin=origin;stablePose=catalog.complete_pose(gender,*normal.sample(PlayerMotion::Idle,0));stable=draw(stablePose,stableOrigin,0);check(visible(stable)>300,"Canonical stopped pose visible");write(output/(prefix+"-stopped.bmp"),stable);stableSaved=true;catalog.pose(body,pose);}
 }
 check(phaseChanges==3,"Actual runtime traverses idle-roll-recover-idle with all three blends");check(horizontal(stableOrigin,run.nav.feet())<.001f,"Actual two-second stopped world origin invariant");
 // Pixel equality isolates translation. Original idle breathing is deliberately
 // sampled at the same phase for these two placement-witness images.
 const auto after=draw(stablePose,run.nav.feet(),0);check(stable.pixels==after.pixels,"Same original idle phase two seconds later has identical full image, no residual root translation");write(output/(prefix+"-stopped-plus2s.bmp"),after);
}
}
int main(int argc,char**argv){try{
 check(argc==5||argc==6,"usage: evade_stop_test appearance.gwc player.gwmot evade.gwmot output-directory [n022a.collision.cfg]");
 const auto world=floor_world();std::vector<std::vector<float>> timings{{1.f/60},{1.f/120},{.007f,.019f,.011f,.033f,.005f,.025f}};
 unsigned cases=0;for(E e:{E::roll,E::rollLeft,E::rollRight,E::backstep}){float first=0;for(const auto&t:timings){const auto travel=exercise(world,e,t);if(first&&player::is_roll(e))check(std::abs(first-travel)<.15f,"60Hz/120Hz/jitter integration matches cumulative travel");first=travel;++cases;}}
 held_and_cancel(world);
 slope_stop();
 if(argc==6){std::ifstream input(argv[5]);check(bool(input),"Original n022a collision file required");auto original=stage::movement_collision(std::make_shared<const stage::Collision>(stage::Collision::read(input)));check(bool(original),"Original movement collision filter");
  for(E e:{E::roll,E::rollLeft,E::rollRight}){Runtime run(*original,e,{-42878.8359f,3000,29781.7969f},false);while(run.seconds<1.5)run.step(1.f/120);const auto stopped=run.nav.feet();float maximumDrift=0;
   for(unsigned i=0;i<240;++i){run.step(1.f/120);maximumDrift=std::max(maximumDrift,horizontal(stopped,run.nav.feet()));}
   std::cout<<"n022a released stop kind="<<unsigned(e)<<" feet="<<stopped[0]<<','<<stopped[1]<<','<<stopped[2]<<" two-second drift="<<maximumDrift<<'\n';
   check(maximumDrift<.1f,"Actual n022a terminal floor gravity must not continue horizontal sliding without input");
  }
 }
 CharacterCatalog catalog(read(argv[1]));PlayerMotionBank normal(read(argv[2]));const auto bank=read(argv[3]);player::EvadeMotionBank evade(bank);const std::filesystem::path output=argv[4];std::filesystem::create_directories(output);
 ComPtr<ID3D11Device>d;ComPtr<ID3D11DeviceContext>c;D3D_FEATURE_LEVEL level;ok(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c));unsigned transitions=0;
 for(unsigned gender=0;gender<2;++gender)for(E e:{E::roll,E::rollLeft,E::rollRight})render_case(d.Get(),c.Get(),world,catalog,normal,evade,gender,e,output,transitions);
 std::ofstream report(output/"measurements.json");report<<"{\"offlineWARP\":true,\"controlNavigationCases\":"<<cases<<",\"worldBoneRebaseChecks\":"<<transitions<<",\"images\":18,\"lastTenFramesTranslationZero\":true,\"releasedTwoSecondsStill\":true,\"normalHeldMovementResumes\":true,\"canonicalIdleImagesEqual\":true,\"canonicalIdleComparisonExcludesBreathing\":true,\"nativeFiniteFloorFixture\":true}\n";check(bool(report),"Stop regression evidence write");std::cout<<"Reviewed evade stop: real Control/Navigation, finite wall, clocks, ACK, both gender world-rebase and WARP PASS\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
