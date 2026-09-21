#include "mounted_renderer.h"
#include "mounted_input.h"
#include "multi_ui_game_events.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <stdexcept>
using namespace mgo2mt;
namespace {
void check(bool value,const char* message){
 if(!value)throw std::runtime_error(message);
}
std::vector<char> read(const std::filesystem::path& path){
 std::ifstream file(path,std::ios::binary);
 check(bool(file),"fixture missing");
 return {std::istreambuf_iterator<char>(file),{}};
}
bool close_vec(std::array<float,3> a,std::array<float,3> b){
 for(unsigned axis=0;axis<3;++axis)if(std::abs(a[axis]-b[axis])>.02f)return false;
 return true;
}
void input(){
 mounted::Input input;input.scope(1,2,3);
 check(!input.step(true,true,0,1,0)&&!input.pending(),"held entering context cannot mount");
 input.step(false,true,0,1,1);
 check(input.step(true,true,0,1,2),"fresh edge mounts");
 auto request=input.intent(true);
 check(request.action==mounted::Action::mount&&request.instance==1,"mount intent");
 input.sent(5);input.acknowledge(4,true);
 check(input.pending(),"old ACK cannot complete");
 input.acknowledge(5,true);
 check(!input.pending()&&!input.step(true,true,1,1,3),"ACK and held button never dismount");
 input.step(false,true,1,1,4);
 check(input.step(true,true,1,1,5)&&input.intent(true).action==mounted::Action::dismount,"fresh edge dismounts");
 input.step(false,false,1,1,6);
 check(!input.pending()&&!input.step(true,true,1,1,7),"focus cancels and requires release");
 input.scope(2,2,4);
 check(!input.pending()&&!input.step(true,true,0,1,8),"life epoch transition requires release");
}
void routes(){
 using namespace multi_ui;
 ActionEvent event{"event","button","title.start"};Context context;context.state="idle";
 check(game_action(event,context,true)==GameAction::start,"sound-finished event routes START");
 check(game_action(event,context,false)==GameAction::none,"no unfocused action");
 context.flags.insert("modal");
 check(game_action(event,context,true)==GameAction::none,"modal blocks custom action");
 context.flags.clear();context.state="loading";
 check(game_action(event,context,true)==GameAction::none,"stale title action cannot repeat");
 event.name="menu.confirm";context.screen="hud";
 check(game_action(event,context,true)==GameAction::none,"menu action cannot shoot");
 context.screen="menu";
 check(game_action(event,context,true)==GameAction::confirm,"explicit menu confirm");
 event.kind="soundCompleted";
 check(game_action(event,context,true)==GameAction::none,"completion notice alone is not a game event");
 event.kind="event";event.name="unknown";
 check(game_action(event,context,true)==GameAction::none,"unknown event no implicit action");
}
void rig(const std::filesystem::path& root){
 CharacterModel model(read(root/"m2_browning.gwm"));
 auto bytes=read(root/"m2_browning.rig");
 auto rig=mounted::Rig::read(bytes,model.vertices.size());
 mounted::Type type;type.pivot=rig.pivots[2];
 auto out=model.vertices;
 mounted::articulate(model,rig,type,0,0,out);
 for(size_t vertex=0;vertex<out.size();++vertex){
  const auto& a=model.vertices[vertex];const auto& b=out[vertex];
  check(close_vec({a.x,a.y,a.z},{b.x,b.y,b.z}),"original bind geometry preserved");
 }
 mounted::articulate(model,rig,type,.7f,.3f,out);
 size_t stationary=0,turning=0;
 for(size_t vertex=0;vertex<out.size();++vertex){
  const auto& a=model.vertices[vertex];const auto& b=out[vertex];
  if(!rig.vertices[vertex]){
   ++stationary;
   check(close_vec({a.x,a.y,a.z},{b.x,b.y,b.z}),"tripod remains fixed");
  }else{
   ++turning;
   const auto pivot=rig.vertices[vertex]==1?rig.pivots[1]:type.pivot;
   auto expected=mounted::rotate({a.x-pivot[0],a.y-pivot[1],a.z-pivot[2]},.7f,rig.vertices[vertex]==2?.3f:0);
   for(unsigned axis=0;axis<3;++axis)expected[axis]+=pivot[axis];
   check(close_vec({b.x,b.y,b.z},expected),"bone pivot controls rotation");
  }
  check(a.u==b.u&&a.v==b.v,"source UV unchanged");
 }
 check(stationary&&turning,"both tripod and turret tested");
 bytes.back()=char(3);bool rejected=false;
 try{mounted::Rig::read(bytes,model.vertices.size());}catch(...){rejected=true;}
 check(rejected,"invalid bone index rejected");
}
void operator_grips(){
 // Synthetic skinning fixture: two fully weighted hands, a lower-body vertex,
 // and one vertex crossing the upper/lower skin boundary. No live renderer.
 std::vector<CatalogBone> bones{
  {0x100,-1,{},{}},{0x6c02b2,0,{},{}},
  {0x027a4c,1,{},{}},{0xfbfa42,2,{},{}},
  {0x62824c,1,{},{}},{0x5c0243,4,{},{}},
  {0x200,0,{},{}},{0x201,6,{},{}}
 };
 mounted::Type type;type.operatorOrbit=true;
 type.pivot={12,1129.5796f,-1.2114f};
 type.operatorOffset={-.7353515625f,0,-875};
 mounted::Instance instance{20,1,"test",{1300,2,-1700},.41f};
 const std::array<stage::Vec3,2> grips{{{-49,1205.7f,-447.4f},{49,1205.7f,-447.4f}}};
 auto transform=[&](stage::Vec3 point,float yaw,float pitch){
  for(unsigned axis=0;axis<3;++axis)point[axis]-=type.pivot[axis];
  point=mounted::rotate(point,yaw,pitch);
  const auto pivot=mounted::pivot_position(instance,type);
  for(unsigned axis=0;axis<3;++axis)point[axis]+=pivot[axis];
  return point;
 };
 for(float relativeYaw:{-1.57f,0.f,1.57f})for(float pitch:{-.6f,0.f,.6f}){
  PreparedCharacter body;
  for(auto point:grips){
   for(unsigned axis=0;axis<3;++axis)point[axis]-=type.operatorOffset[axis];
   body.model.vertices.push_back({point[0],point[1],point[2],0,1,0,0,0});
  }
  body.model.vertices.push_back({50,400,0,0,1,0,0,0});
  body.model.vertices.push_back({0,800,20,0,1,0,0,0});
  body.skin.resize(4);
  body.skin[0].bones[0]=3;body.skin[0].weights[0]=1;
  body.skin[1].bones[0]=5;body.skin[1].weights[0]=1;
  body.skin[2].bones[0]=7;body.skin[2].weights[0]=1;
  body.skin[3].bones[0]=3;body.skin[3].bones[1]=7;
  body.skin[3].weights[0]=body.skin[3].weights[1]=.5f;
  auto frame=[](stage::Vec3 at){return std::array<float,16>{1,0,0,0,0,1,0,0,0,0,1,0,at[0],at[1],at[2],1};};
  for(unsigned hand=0;hand<2;++hand){const auto&v=body.model.vertices[hand];auto key=hand?0x5c0243u:0xfbfa42u;body.boneFrames[key]=frame({v.x,v.y,v.z});body.bonePositions[key]={v.x,v.y,v.z};}
  body.boneFrames[0x201]=frame({50,400,0});body.bonePositions[0x201]={50,400,0};
  const auto legFrame=body.boneFrames[0x201];
  check(mounted::operator_pitch(body,bones,type,pitch),"operator pitch fixture accepted");
  const float yaw=instance.yaw+relativeYaw;
  const auto feet=mounted::operator_position(instance,type,yaw);
  for(unsigned hand=0;hand<2;++hand){
   const auto&v=body.model.vertices[hand];auto world=mounted::rotate({v.x,v.y,v.z},yaw);
   for(unsigned axis=0;axis<3;++axis)world[axis]+=feet[axis];
   check(close_vec(world,transform(grips[hand],yaw,pitch)),"hands remain on gun grips across full yaw/pitch range");
   const auto key=hand?0x5c0243u:0xfbfa42u;
   check(close_vec(body.bonePositions.at(key),{v.x,v.y,v.z}),"hand reference point agrees with skinned vertex");
   const auto&matrix=body.boneFrames.at(key);
   check(close_vec({matrix[12],matrix[13],matrix[14]},{v.x,v.y,v.z}),"hand bone frame translation agrees with skin");
   check(close_vec({v.nx,v.ny,v.nz},mounted::rotate({0,1,0},0,pitch)),"upper normal rotates with pitch");
  }
  const auto&leg=body.model.vertices[2];
  check(close_vec({leg.x,leg.y,leg.z},{50,400,0})&&body.boneFrames.at(0x201)==legFrame,"legs and lower bone frame remain unchanged");
  stage::Vec3 pivot=type.pivot;for(unsigned axis=0;axis<3;++axis)pivot[axis]-=type.operatorOffset[axis];
  auto mixed=mounted::rotate({-pivot[0],800-pivot[1],20-pivot[2]},0,pitch);
  const stage::Vec3 original{0,800,20};for(unsigned axis=0;axis<3;++axis)mixed[axis]=(mixed[axis]+pivot[axis]+original[axis])*.5f;
  const auto&v=body.model.vertices[3];
  check(close_vec({v.x,v.y,v.z},mixed),"mixed upper/lower skin weight retained");
 }
}
}
int main(int argc,char**argv){
 try{
  check(argc==2,"resource path required");
  input();routes();rig(argv[1]);operator_grips();
  std::cout<<"PASS original M2 articulation, operator grip orbit/pitch, input edges, UI event routes\n";
  return 0;
 }catch(const std::exception&error){std::cerr<<error.what()<<'\n';return 1;}
}
