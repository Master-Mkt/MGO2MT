#include "stage_normals.h"
#include "source_coordinates.h"
#include "gekko_preview.h"
#include "build_version.h"
#include "character_catalog.h"
#include "character_renderer.h"
#include "controller_input.h"
#include "motion_blend_presentation.h"
#include "gekko_motion.h"
#include "gekko_greeting.h"
#include "audio_control.h"
#include "special_pc.h"
#include "gekko_jump.h"
#include "gekko_climb.h"
#include "gekko_locomotion.h"
#include "gekko_traversal_motion.h"
#include "stage_water.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <thread>

int run_audio_probe(int,wchar_t**,const std::atomic_bool*,const mgo2win::AudioControl*);
namespace mgo2win { namespace {
struct StepAudio {std::thread thread;std::atomic_bool stop{false},busy{false};AudioControl control;~StepAudio(){stop=true;if(thread.joinable())thread.join();}void play(const std::filesystem::path&p){if(busy||!std::filesystem::is_regular_file(p))return;if(thread.joinable())thread.join();stop=false;busy=true;control.gain=.65f;control.stream="gekko_native_footstep";thread=std::thread([this,p]{std::wstring a=L"audio",b=p.wstring(),c=L"2";wchar_t* args[]{a.data(),b.data(),c.data()};run_audio_probe(3,args,&stop,&control);busy=false;});}};
using stage::Vec3;using Microsoft::WRL::ComPtr;using Action=special_pc::Action;
void require(bool v,const char*m){if(!v)throw std::runtime_error(m);}void checked(HRESULT h){require(SUCCEEDED(h),"Gekko D3D operation failed");}
std::vector<char> bytes(const std::filesystem::path&p,size_t maximum){std::ifstream f(p,std::ios::binary|std::ios::ate);require(bool(f),"Gekko preview asset missing");auto size=f.tellg();require(size>0&&uint64_t(size)<=maximum,"Gekko preview asset extent");std::vector<char>b(static_cast<size_t>(size));f.seekg(0);require(bool(f.read(b.data(),size)),"Gekko preview asset read");return b;}
Vec3 add(Vec3 a,Vec3 b){for(unsigned i=0;i<3;++i)a[i]+=b[i];return a;}Vec3 mul(Vec3 a,float n){for(auto&v:a)v*=n;return a;}float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vec3 normalized(Vec3 a){float n=std::sqrt(dot(a,a));return n>1e-5f?mul(a,1/n):Vec3{0,0,1};}
struct Window {
 HWND handle=nullptr;bool quit=false,resized=false,showSky=false;unsigned width=1280,height=720;
 static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
  auto*self=reinterpret_cast<Window*>(GetWindowLongPtrW(h,GWLP_USERDATA));
  if(m==WM_NCCREATE){self=static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));self->handle=h;}
  if(self){if(m==WM_CLOSE){self->quit=true;return 0;}if(m==WM_SIZE){self->width=LOWORD(l);self->height=HIWORD(l);self->resized=true;return 0;}if(m==WM_KEYDOWN&&w==VK_F12&&!(l&(1LL<<30))){self->showSky=!self->showSky;return 0;}if(m==WM_KEYDOWN&&w==VK_ESCAPE){self->quit=true;return 0;}if(m==WM_ERASEBKGND)return 1;}
  return DefWindowProcW(h,m,w,l);
 }
 Window(){WNDCLASSW c{};c.lpfnWndProc=proc;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"MGO2WIN.Gekko.LocalPreview";c.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));if(!RegisterClassW(&c)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("Gekko window class");RECT r{0,0,1280,720};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);auto title=versioned_title(L"月光ローカル操作テスト | LS/WASD 移動・RS/IJKL 視点・A/Space ジャンプ・Y/X 蹴り・B/Z 挨拶・F12 天球診断・Esc 終了");handle=CreateWindowExW(0,c.lpszClassName,title.c_str(),WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,c.hInstance,this);require(handle,"Gekko window");ShowWindow(handle,SW_SHOW);}
 ~Window(){if(handle)DestroyWindow(handle);}
 void messages(){MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){if(m.message==WM_QUIT)quit=true;else{TranslateMessage(&m);DispatchMessageW(&m);}}}
};
struct Input {float forward=0,right=0,turn=0,look=0;bool run=false,jump=false,kick=false,salute=false;};
struct LocalInput {
 ControllerInput controller{std::filesystem::path{}};std::set<unsigned> previous;bool armed=false;
 LocalInput(){controller.config.device=1;}
 Input poll(bool active){
  auto p=controller.poll(active);Input in;std::set<unsigned> held;
  for(unsigned key:{unsigned('W'),unsigned('A'),unsigned('S'),unsigned('D'),unsigned('I'),unsigned('J'),unsigned('K'),unsigned('L'),unsigned('X'),unsigned('Z'),unsigned(VK_SPACE),unsigned(VK_SHIFT)})if(GetAsyncKeyState(int(key))&0x8000)held.insert(key);
  if(!active){armed=false;previous.clear();return in;}
  if(!armed){if(held.empty())armed=true;previous=held;}else{
   auto down=[&](unsigned k){return held.contains(k)?1.f:0.f;};in.forward=down('W')-down('S');in.right=down('D')-down('A');in.turn=down('L')-down('J');in.look=down('I')-down('K');in.run=held.contains(VK_SHIFT);in.jump=held.contains(VK_SPACE)&&!previous.contains(VK_SPACE);in.kick=held.contains('X')&&!previous.contains('X');in.salute=held.contains('Z')&&!previous.contains('Z');previous=held;
  }
  if(p.armed){if(std::abs(p.left_y)>std::abs(in.forward))in.forward=p.left_y;if(std::abs(p.left_x)>std::abs(in.right))in.right=p.left_x;if(std::abs(p.right_x)>std::abs(in.turn))in.turn=p.right_x;if(std::abs(p.right_y)>std::abs(in.look))in.look=p.right_y;in.run=in.run||std::hypot(p.left_x,p.left_y)>.65f;in.jump=in.jump||(p.pressed&(1u<<4));in.kick=in.kick||(p.pressed&(1u<<7));in.salute=in.salute||(p.pressed&(1u<<5));}
  return in;
 }
};
struct Actor {
 Vec3 feet{},anchor{},velocity{},jumpStart{};float yaw=0,cameraYaw=0,cameraPitch=-.3f,vertical=0;bool grounded=true;Action action=Action::none;double actionSeconds=0,motionSeconds=0;uint64_t serial=0;std::optional<special_pc::Jump> jump;bool moving=false,running=false;size_t jumps=0,kicks=0,salutes=0,climbs=0,climbsCompleted=0;float maxJump=0,jumpTravel=0,jumpSpeed=0,climbHeight=0;std::optional<special_pc::Climb> climb;gekko_locomotion::State locomotion;double clockMs=0;uint32_t previewLife=1;uint64_t reverseWaitObservedMs=0;float maximumYawRate=0;
 static constexpr auto profile=special_pc::native_gekko;
 void place(const stage::Collision&w,const stage::Collision&cameraWorld){
  for(int radius=0;radius<=16;++radius)for(int z=-radius;z<=radius;++z)for(int x=-radius;x<=radius;++x){if(radius&&std::abs(x)!=radius&&std::abs(z)!=radius)continue;auto ground=w.ray({-42878.8359f+x*2000,9000,29781.7969f+z*2000},{0,-1,0},20000);if(!ground||ground->normal[1]<.707f)continue;auto p=ground->position;p[1]+=8;if(!w.clear(p,profile.capsule)||w.sweep(p,{0,profile.jumpHeight+100,0},profile.capsule))continue;if(w.sweep(p,{0,0,12600},profile.capsule))continue;
   bool floorPath=true;for(int step=1;step<=14;++step){auto q=p;q[1]+=200;q[2]+=900*step;auto support=w.ray(q,{0,-1,0},400);if(!support||support->normal[1]<.707f||std::abs(support->position[1]-p[1])>100){floorPath=false;break;}}
   if(!floorPath)continue;bool cameraPath=true;for(int step=0;step<=14;step+=2){auto target=p;target[1]+=2700;target[2]+=900*step;Vec3 delta{0,-12500*std::sin(cameraPitch),-12500*std::cos(cameraPitch)};auto hit=cameraWorld.sweep_segment(target,target,delta,100,2);if(hit&&hit->fraction<.95f){cameraPath=false;break;}}
   if(!cameraPath)continue;feet=anchor=p;return;}
  throw std::runtime_error("No checked open floor for Gekko capsule in n022a");
 }
 Vec3 slide(const stage::Collision&w,Vec3 delta){auto p=feet;for(unsigned i=0;i<5&&dot(delta,delta)>.001f;++i){auto hit=w.sweep(p,delta,profile.capsule);if(!hit){p=add(p,delta);break;}p=add(p,mul(delta,hit->fraction));delta=mul(delta,1-hit->fraction);float into=dot(delta,hit->normal);if(into<0)delta=add(delta,mul(hit->normal,-into));else break;}return p;}
 void relocate(Vec3 p){feet=anchor=p;velocity={};vertical=0;grounded=true;action=Action::none;actionSeconds=0;jump.reset();climb.reset();locomotion.reset();yaw=cameraYaw=0;++previewLife;}
 void update(const stage::Collision&w,Input input,float dt){
  clockMs+=double(dt)*1000;const auto now=uint64_t(std::llround(clockMs));
  cameraYaw=std::remainder(cameraYaw+source_screen_x*input.turn*2*dt,6.283185307f);cameraPitch=std::clamp(cameraPitch+input.look*1.5f*dt,-.8f,.6f);
  if(action==Action::none&&grounded&&(input.jump||input.kick||input.salute)){
   if(input.jump&&input.forward>.12f)climb=special_pc::begin_climb(feet,cameraYaw,w);
   action=climb?Action::climb:input.jump?Action::jump:input.kick?Action::kick:Action::salute;actionSeconds=0;++serial;locomotion.reset();
   if(action==Action::jump){jump=special_pc::begin_jump(feet,velocity);require(bool(jump),"Gekko preview jump velocity");jumpStart=feet;jumpSpeed=std::hypot(velocity[0],velocity[2]);++jumps;}
   else if(action==Action::climb){yaw=cameraYaw;++climbs;climbHeight=climb->landing[1]-climb->start[1];}
   else if(action==Action::kick)++kicks;else ++salutes;
  }
  moving=running=false;motionSeconds+=dt;
  if(action!=Action::none){
   actionSeconds+=dt;const auto duration=special_pc::duration(action);
   if(action==Action::jump){require(jump&&special_pc::advance_jump(*jump,uint32_t(std::min(actionSeconds*1000.,double(duration))),w),"Gekko shared jump step");feet=jump->feet;maxJump=std::max(maxJump,feet[1]-jumpStart[1]);jumpTravel=std::max(jumpTravel,std::hypot(feet[0]-jumpStart[0],feet[2]-jumpStart[2]));grounded=false;}
   if(action==Action::climb){require(bool(climb),"Gekko climb state");if(!special_pc::advance_climb(*climb,uint32_t(std::min(actionSeconds*1000.,double(duration))),w)){feet=climb->feet;action=Action::none;climb.reset();vertical=0;}else{feet=climb->feet;grounded=climb->finished;}}
   if(actionSeconds*1000>=duration){if(action==Action::climb&&climb&&climb->finished)++climbsCompleted;action=Action::none;actionSeconds=0;jump.reset();climb.reset();velocity={};vertical=0;locomotion.reset();}
  }else{
   float magnitude=std::hypot(input.forward,input.right);if(magnitude>1){input.forward/=magnitude;input.right/=magnitude;magnitude=1;}
   const float targetYaw=magnitude>.01f?cameraYaw+source_movement_angle(input.right,input.forward):yaw;
   const auto beforeYaw=yaw;auto eased=locomotion.update({1,1,1,previewLife,1,0},{targetYaw,magnitude>.01f?(input.run?profile.runSpeed:profile.walkSpeed)*magnitude:0.f},now);
   velocity={};if(eased.accepted){if(eased.rebaselined){locomotion.reset();eased=locomotion.update({1,1,1,previewLife,1,0},{beforeYaw,0},now);}yaw=eased.yaw;auto before=feet;feet=slide(w,{eased.displacement[0],0,eased.displacement[1]});if(dt>0){velocity={(feet[0]-before[0])/dt,0,(feet[2]-before[2])/dt};const auto speed=std::hypot(velocity[0],velocity[2]);if(speed>profile.runSpeed)velocity=mul(velocity,profile.runSpeed/speed);if(!eased.rebaselined)maximumYawRate=std::max(maximumYawRate,std::abs(std::remainder(yaw-beforeYaw,6.283185307f))/dt);}if(eased.phase==gekko_locomotion::Phase::waiting)reverseWaitObservedMs+=uint64_t(std::llround(dt*1000));moving=locomotion.speed()>.01f;running=moving&&locomotion.speed()>profile.walkSpeed+1;}
  }
  if(action!=Action::jump&&action!=Action::climb){vertical=std::max(-15000.f,vertical-9800*dt);auto delta=Vec3{0,vertical*dt,0};auto hit=w.sweep(feet,delta,profile.capsule);feet=add(feet,mul(delta,hit?hit->fraction:1.f));grounded=hit&&hit->normal[1]>.707f;if(grounded)vertical=0;auto floor=w.sweep(feet,{0,-30,0},profile.capsule);if(vertical<=0&&floor&&floor->normal[1]>.707f){feet[1]-=30*floor->fraction;grounded=true;vertical=0;}}
  if(feet[1]<anchor[1]-30000){if(jump)special_pc::cancel_jump(*jump,w);relocate(anchor);}
 }
 WorldView camera(const stage::Collision&w,float aspect)const{Vec3 target=feet;target[1]+=2000;float cp=std::cos(cameraPitch);Vec3 direction{std::sin(cameraYaw)*cp,std::sin(cameraPitch),std::cos(cameraYaw)*cp};Vec3 delta=mul(direction,-8500);auto hit=w.sweep_segment(target,target,delta,100,2);auto eye=add(target,mul(delta,hit?std::max(0.f,hit->fraction-.01f):1.f));return {eye,normalized(add(target,mul(eye,-1))),aspect};}
};
// Capture-only diagnostic floor and mantle wall. These triangles are generated
// locally and are never written into original stage assets or sent to a HOST.
std::shared_ptr<const stage::Collision> capture_fixture(CharacterModel& model,const stage::Collision& base,Vec3 origin){
 std::vector<Vec3> vertices;std::vector<stage::CollisionTriangle> triangles;
 auto quad=[&](Vec3 a,Vec3 b,Vec3 c,Vec3 d){const auto n=unsigned(vertices.size());for(auto p:{a,b,c,d})vertices.push_back(add(p,origin));triangles.push_back({{n,n+1,n+2}});triangles.push_back({{n,n+2,n+3}});};
 quad({-40000,0,-40000},{-40000,0,40000},{40000,0,40000},{40000,0,-40000});
 quad({19000,0,1500},{29000,0,1500},{29000,7000,1500},{19000,7000,1500});
 quad({19000,7000,1500},{19000,7000,6500},{29000,7000,6500},{29000,7000,1500});
 const auto vbase=unsigned(model.vertices.size()),ibase=unsigned(model.indices.size()),texture=unsigned(model.textures.size());
 model.textures.push_back({4,4,9,{255,255,255,255,0,0,0,0}});
 for(size_t i=0;i<vertices.size();++i){const auto p=vertices[i];ModelVertex v{};v.x=p[0];v.y=p[1];v.z=p[2];v.ny=(i<4||i>=8)?1.f:0;v.nz=i>=4&&i<8?-1.f:0;v.lr=i<4?.18f:.48f;v.lg=i<4?.23f:.35f;v.lb=i<4?.28f:.14f;v.lit=1;model.vertices.push_back(v);}
 for(const auto&t:triangles)for(auto i:t.vertices)model.indices.push_back(vbase+i);model.parts.push_back({ibase,unsigned(triangles.size()*3),texture,0});
 auto fixture=std::make_shared<const stage::Collision>(stage::Collision::make(std::move(vertices),std::move(triangles)));std::array instances{stage::CollisionInstance{0xf0000001u,fixture,{},{}}};
 return std::make_shared<const stage::Collision>(stage::Collision::combine(base,instances));
}
void screenshot(ID3D11Device*d,ID3D11DeviceContext*c,ID3D11Texture2D*back,const std::filesystem::path&p){
 D3D11_TEXTURE2D_DESC desc{};back->GetDesc(&desc);auto w=desc.Width,h=desc.Height;desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;ComPtr<ID3D11Texture2D> copy;checked(d->CreateTexture2D(&desc,nullptr,&copy));c->CopyResource(copy.Get(),back);D3D11_MAPPED_SUBRESOURCE m{};checked(c->Map(copy.Get(),0,D3D11_MAP_READ,0,&m));std::vector<uint8_t>b(size_t(w)*h*4);for(unsigned y=0;y<h;++y){std::copy_n(static_cast<const uint8_t*>(m.pData)+size_t(y)*m.RowPitch,size_t(w)*4,b.data()+size_t(y)*w*4);}c->Unmap(copy.Get(),0);for(size_t i=0;i<b.size();i+=4)std::swap(b[i],b[i+2]);BITMAPFILEHEADER f{};BITMAPINFOHEADER bi{};bi.biSize=sizeof(bi);bi.biWidth=LONG(w);bi.biHeight=-LONG(h);bi.biPlanes=1;bi.biBitCount=32;bi.biSizeImage=DWORD(b.size());f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(bi);f.bfSize=f.bfOffBits+bi.biSizeImage;std::ofstream out(p,std::ios::binary);out.write(reinterpret_cast<const char*>(&f),sizeof(f));out.write(reinterpret_cast<const char*>(&bi),sizeof(bi));out.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));require(bool(out),"Gekko screenshot write");
}
} // namespace

int run_gekko_preview(const std::filesystem::path&data,bool capture,const std::filesystem::path&output){
 try{
  require(!capture||!output.empty(),"Gekko capture output required");
  CharacterCatalog catalog(bytes(data/"special/gekko.gwc",32*1024*1024));std::array<uint8_t,28> appearance{};auto body=catalog.assemble(appearance);require(body.ready()&&catalog.skeleton(0).size()==59,"Gekko rig candidate");
  special_pc::GekkoMotionBank motion(bytes(data/"special/gekko.gwmot",8*1024*1024));require(motion.size()==7,"Gekko original motion phases required");
  special_pc::GekkoGreeting greeting(bytes(data/"special/gekko_salute.gwmot",1024*1024));
  std::unique_ptr<special_pc::GekkoTraversalMotionBank> traversal; // API provided by the dedicated original-motion sampler.
  if(std::filesystem::exists(data/"special/gekko_traversal.gwmot"))traversal=std::make_unique<special_pc::GekkoTraversalMotionBank>(bytes(data/"special/gekko_traversal.gwmot",4*1024*1024));
  auto stageBytes=bytes(data/"stage/n022a.gwm",64*1024*1024);CharacterModel worldModel(stageBytes);stage::load_original_normals(worldModel,stageBytes,data/"stage/n022a.gwn");std::ifstream col(data/"stage/n022a.collision.cfg");require(bool(col),"Gekko stage collision");auto rawWorld=std::make_shared<const stage::Collision>(stage::Collision::read(col));auto world=stage::movement_collision(rawWorld);require(bool(world),"Gekko movement collision");
  std::ifstream light(data/"stage/n022a.lighting.cfg");if(light){auto lighting=stage::Lighting::read(light);for(auto&v:worldModel.vertices){auto s=lighting.sample({v.x,v.y,v.z},{v.nx,v.ny,v.nz});v.lr=s.color[0];v.lg=s.color[1];v.lb=s.color[2];v.lit=1;}}
  std::unique_ptr<CharacterModel> skyModel;if(std::filesystem::exists(data/"stage/n022a.sky.gwm"))skyModel=std::make_unique<CharacterModel>(bytes(data/"stage/n022a.sky.gwm",8*1024*1024),ModelExtent::sky);
  special_pc::GekkoFootsteps footsteps;StepAudio footAudio;unsigned footCount=0;
  Actor originalPlacement;
  try{originalPlacement.place(*world,*rawWorld);}catch(...){if(capture&&!output.empty()){std::filesystem::create_directories(output);std::ofstream f(output/"original-placement.json");f<<"{\"originalStagePlacementChecked\":true,\"passed\":false,\"nativeFixtureNotUsedForPlacementCheck\":true}\n";}throw;}
  require(world->clear(originalPlacement.feet,Actor::profile.capsule),"Original stage normal-start capsule clear");
  if(capture){std::filesystem::create_directories(output);std::ofstream f(output/"original-placement.json");f<<"{\"originalStagePlacementChecked\":true,\"passed\":true,\"feet\":["<<originalPlacement.feet[0]<<','<<originalPlacement.feet[1]<<','<<originalPlacement.feet[2]<<"],\"nativeFixtureNotUsedForPlacementCheck\":true}\n";require(bool(f),"Original stage placement evidence write");}
  Actor actor=originalPlacement;Vec3 fixtureOrigin{};
  if(capture){float highest=0;for(auto p:world->vertices)highest=std::max(highest,p[1]);fixtureOrigin={0,highest+20000,0};world=capture_fixture(worldModel,*world,fixtureOrigin);rawWorld=world;actor.relocate(add(fixtureOrigin,{0,2,0}));require(world->clear(actor.feet,Actor::profile.capsule),"Native fixture initial capsule");}
  
  LocalInput input;motion_blend::Lane blend;Vec3 drawOrigin{};float drawYaw=0;bool hasDrawFrame=false;Window window;if(capture)SetWindowTextW(window.handle,L"月光ローカル描画試験 / native追加床・7m壁（原ステージ配置ではありません）");
  ComPtr<ID3D11Device>d;ComPtr<ID3D11DeviceContext>c;ComPtr<IDXGISwapChain>swap;DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=window.width;desc.BufferDesc.Height=window.height;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.OutputWindow=window.handle;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;D3D_FEATURE_LEVEL level;
  HRESULT hr=E_FAIL;if(!capture)hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&d,&level,&c);if(FAILED(hr))checked(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&d,&level,&c));
  CharacterRenderer scene(d.Get(),worldModel),model(d.Get(),body.model);std::unique_ptr<CharacterRenderer> sky;if(skyModel)sky=std::make_unique<CharacterRenderer>(d.Get(),*skyModel,true);scene.resize_target(d.Get(),window.width,window.height);ComPtr<ID3D11Texture2D>back;checked(swap->GetBuffer(0,IID_PPV_ARGS(&back)));window.resized=false;
  std::ofstream trace;if(capture){require(bool(traversal),"Gekko traversal capture asset required");require(!output.empty(),"Gekko capture output required");std::filesystem::create_directories(output);trace.open(output/"frames.csv");trace<<"image,time,source_index,source_key,phase,feet_x,feet_y,feet_z,blend_progress,jump_horizontal,locomotion_phase,speed,body_yaw,climb_active\n";}
  const auto captureStarted=std::chrono::steady_clock::now();auto last=captureStarted;double elapsed=0;unsigned frame=0,collisionChecks=0,blendTransitions=0,worldRebaseChecks=0;float maximumWorldRebaseError=0;uint64_t previousBlendKey=0;std::set<unsigned>saved;float travel=0;auto start=actor.feet;
  const double jumpBegin=2.8,jumpEnd=jumpBegin+special_pc::native_gekko.jumpMs/1000.,kickBegin=jumpEnd+.7,kickEnd=kickBegin+special_pc::native_gekko.kickMs/1000.,saluteBegin=kickEnd+.5,saluteEnd=saluteBegin+special_pc::salute_ms/1000.,reverseBegin=saluteEnd+.5,reverseFlip=reverseBegin+.7,climbSetup=reverseFlip+1.1,climbBegin=climbSetup+.3,climbEnd=climbBegin+special_pc::climb_ms/1000.,finish=climbEnd+.5;bool reversedScene=false,climbScene=false;
  while(!window.quit){window.messages();if(window.quit)break;auto now=std::chrono::steady_clock::now();if(capture)require(now-captureStarted<std::chrono::seconds(180)&&frame<18000,"Gekko capture deadline");float dt=capture?1.f/60.f:std::clamp(std::chrono::duration<float>(now-last).count(),0.f,.05f);last=now;
   
   if(window.resized&&window.width&&window.height){c->ClearState();back.Reset();checked(swap->ResizeBuffers(0,window.width,window.height,DXGI_FORMAT_UNKNOWN,0));checked(swap->GetBuffer(0,IID_PPV_ARGS(&back)));scene.resize_target(d.Get(),window.width,window.height);window.resized=false;}
   const bool active=capture||GetForegroundWindow()==window.handle;Input in;if(capture){in.forward=elapsed>=.7&&elapsed<jumpBegin?1.f:0.f;in.run=elapsed>=1.8;in.jump=elapsed>=jumpBegin&&elapsed<jumpBegin+dt;in.kick=elapsed>=kickBegin&&elapsed<kickBegin+dt;in.salute=elapsed>=saluteBegin&&elapsed<saluteBegin+dt;
    if(elapsed>=reverseBegin&&!reversedScene){actor.relocate(add(fixtureOrigin,{0,2,0}));blend.reset();reversedScene=true;}
    if(elapsed>=reverseBegin&&elapsed<climbSetup){in.forward=elapsed<reverseFlip?1.f:-1.f;in.run=true;}
    if(elapsed>=climbSetup&&!climbScene){actor.relocate(add(fixtureOrigin,{24000,2,0}));blend.reset();climbScene=true;require(bool(special_pc::begin_climb(actor.feet,0,*world)),"Native fixture valid capsule mantle route");}
    if(elapsed>=climbBegin&&elapsed<climbBegin+dt){in.forward=1;in.jump=true;}}else in=input.poll(active);if(!active){actor.locomotion.reset();actor.velocity={};}
   const bool simulate=active||actor.action!=Action::none||!actor.grounded;
   if(simulate){unsigned steps=std::max(1u,unsigned(std::ceil(dt*120)));for(unsigned s=0;s<steps;++s){auto step=active?in:Input{};if(s)step.jump=step.kick=step.salute=false;actor.update(*world,step,dt/steps);if(capture){require(world->clear(actor.feet,Actor::profile.capsule),"Gekko every capture step retains collision clearance");++collisionChecks;}}elapsed+=dt;}
   if(!window.width||!window.height){std::this_thread::sleep_for(std::chrono::milliseconds(10));continue;}
   using Motion=special_pc::GekkoMotion;const auto action=actor.action==Action::jump?Motion::jump:actor.action==Action::kick?Motion::kick:actor.running?Motion::run:actor.moving?Motion::walk:Motion::idle;
   double motionTime=actor.action==Action::none?actor.motionSeconds:actor.actionSeconds;
   auto sample=actor.action==Action::climb?(traversal?traversal->sample_climb(motion,motionTime):motion.sample_detail(Motion::jump,.6+std::fmod(motionTime,.95))):actor.action==Action::salute?greeting.sample(motionTime):motion.sample_gameplay(action,motionTime);require(bool(sample),"Gekko motion sample unavailable");uint64_t key=sample->sourceKey|(uint64_t(sample->phase)<<24);if(actor.action!=Action::none)key|=actor.serial<<32;
   if(key!=previousBlendKey){++blendTransitions;previousBlendKey=key;}
   auto origin=actor.feet;origin[1]+=special_pc::gekko_model_feet_offset;const motion_blend::Scope scope{1,capture?2u:1u,1,actor.previewLife,0};
   const bool rebase=hasDrawFrame&&blend.matches(scope)&&!blend.same_source(key);
   auto worldPoint=[](Vec3 p,Vec3 o,float yaw){const float a=std::sin(yaw),b=std::cos(yaw);return Vec3{o[0]+b*p[0]+a*p[2],o[1]+p[1],o[2]-a*p[0]+b*p[2]};};
   std::map<uint32_t,Vec3> priorWorld;if(capture&&rebase)for(auto [bone,p]:body.bonePositions)priorWorld.emplace(bone,worldPoint(p,drawOrigin,drawYaw));
   if(rebase)blend.rebase(drawOrigin,drawYaw,origin,actor.yaw,catalog.skeleton(0).front().position);
   const auto&pose=blend.sample(scope,key,sample->phaseSeconds,catalog.complete_pose(0,sample->pose),simulate?dt:0,5);catalog.pose(body,pose);
   if(capture&&rebase){++worldRebaseChecks;for(auto [bone,p]:body.bonePositions){const auto current=worldPoint(p,origin,actor.yaw),before=priorWorld.at(bone);const auto delta=add(current,mul(before,-1));maximumWorldRebaseError=std::max(maximumWorldRebaseError,std::sqrt(dot(delta,delta)));}require(maximumWorldRebaseError<.15f,"All phase changes preserve world bone positions at blend alpha zero");}
   drawOrigin=origin;drawYaw=actor.yaw;hasDrawFrame=true;model.update_vertices(c.Get(),body.model.vertices);
   if(footsteps.update(1,1,sample->sourceKey,sample->phaseSeconds,active&&actor.grounded&&actor.action==Action::none)){++footCount;if(!capture)footAudio.play(data/"special/gekko_step.wav");}
   auto camera=actor.camera(*rawWorld,float(window.width)/window.height);scene.render(c.Get(),0,false,&camera);if(window.showSky&&sky)sky->render(c.Get(),0,false,&camera,&scene);model.render(c.Get(),actor.yaw,false,&camera,&scene,&origin);
   ComPtr<ID3D11Resource>surface;scene.view()->GetResource(&surface);c->OMSetRenderTargets(0,nullptr,nullptr);c->CopyResource(back.Get(),surface.Get());
   if(capture){std::array<double,16>times{.35,1.35,2.35,jumpBegin+2.029,kickBegin+special_pc::native_gekko.kickMs*.00045,jumpBegin+.3,jumpEnd-.35,saluteEnd+.1,saluteBegin+1.15,reverseFlip+.05,reverseFlip+.25,reverseFlip+.5,climbBegin+.8,climbBegin+1.9,climbBegin+2.5,finish-.05};const char*names[]={"idle","walk","run","jump-10m-peak","kick","jump-start","jump-land","return","salute","reverse-brake","reverse-wait","reverse-accelerate","climb-rise","climb-cross","climb-settle","climb-landed"};for(unsigned i=0;i<times.size();++i)if(elapsed>=times[i]&&!saved.contains(i)){screenshot(d.Get(),c.Get(),back.Get(),output/(std::string(names[i])+".bmp"));trace<<names[i]<<','<<elapsed<<','<<sample->sourceIndex<<','<<sample->sourceKey<<','<<sample->phase<<','<<actor.feet[0]<<','<<actor.feet[1]<<','<<actor.feet[2]<<','<<blend.progress()<<','<<actor.jumpTravel<<','<<unsigned(actor.locomotion.phase())<<','<<actor.locomotion.speed()<<','<<actor.yaw<<','<<(actor.action==Action::climb)<<'\n';saved.insert(i);}}
   checked(swap->Present(capture?0:1,0));++frame;travel=std::max(travel,std::hypot(actor.feet[0]-start[0],actor.feet[2]-start[2]));if(capture&&elapsed>=finish)break;
  }
  if(capture){std::cout<<"Gekko capture measurements images="<<saved.size()<<" jumps="<<actor.jumps<<" kicks="<<actor.kicks<<" travel="<<travel<<" peak="<<actor.maxJump<<'\n';require(saved.size()==16&&actor.jumps==1&&actor.kicks==1&&actor.salutes==1&&actor.climbs==1&&actor.climbsCompleted==1&&actor.maxJump>9990&&actor.jumpTravel>1000&&actor.jumpSpeed>3000&&actor.climbHeight>6900&&actor.reverseWaitObservedMs>=150&&actor.maximumYawRate<6.8f&&blendTransitions>=12&&worldRebaseChecks>=8&&collisionChecks>1000,"Gekko capture did not exercise complete runtime");trace.flush();require(bool(trace),"Gekko capture trace");std::ofstream report(output/"capture.json");report<<"{\"offline\":true,\"frames\":"<<frame<<",\"images\":16,\"travel\":"<<travel<<",\"jumpHeight\":"<<actor.maxJump<<",\"jumpHorizontalDistance\":"<<actor.jumpTravel<<",\"jumpInitialSpeed\":"<<actor.jumpSpeed<<",\"originalStagePlacementChecked\":true,\"originalStagePlacement\":["<<originalPlacement.feet[0]<<','<<originalPlacement.feet[1]<<','<<originalPlacement.feet[2]<<"],\"worldRebaseChecks\":"<<worldRebaseChecks<<",\"maximumWorldRebaseError\":"<<maximumWorldRebaseError<<",\"inactivePhysicsContinues\":true,\"nativeFixture\":true,\"fixtureOrigin\":["<<fixtureOrigin[0]<<','<<fixtureOrigin[1]<<','<<fixtureOrigin[2]<<"],\"climbCount\":"<<actor.climbs<<",\"climbsCompleted\":"<<actor.climbsCompleted<<",\"climbHeight\":"<<actor.climbHeight<<",\"reverseWaitPolicyMs\":180,\"reverseWaitObservedMs\":"<<actor.reverseWaitObservedMs<<",\"maximumYawRate\":"<<actor.maximumYawRate<<",\"collisionClearChecks\":"<<collisionChecks<<",\"blendTransitions\":"<<blendTransitions<<",\"sharedLocomotionHelper\":true,\"sharedClimbHelper\":true,\"sharedJumpHelper\":true,\"jumpCount\":"<<actor.jumps<<",\"kickCount\":"<<actor.kicks<<",\"nativeFootstepCount\":"<<footCount<<",\"saluteCount\":1,\"originalMotionConnected\":true,\"nativePhysics\":true}";require(bool(report),"Gekko capture report");}
  c->ClearState();std::cout<<"Gekko local preview completed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
} // namespace mgo2win

