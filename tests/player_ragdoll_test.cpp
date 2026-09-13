#include "player_ragdoll.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2win;using namespace physics;
static void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
static std::vector<char> read(const char*p){std::ifstream f(p,std::ios::binary);check(bool(f),"open original fixture");return {(std::istreambuf_iterator<char>(f)),{}};}
int main(int argc,char**argv){try{
 player::Ragdoll inactive;check(!inactive.active()&&inactive.bodies().empty()&&inactive.pose().rotations.empty(),"uninitialized ragdoll exposes no fabricated body");
 if(argc<3){std::cout<<"No private original fixtures supplied; inactive-state check passed\n";return 0;}
 PlayerMotionBank motions(read(argv[1]));CharacterCatalog catalog(read(argv[2]));auto floor=stage::Collision::make({{-20000,0,-20000},{20000,0,-20000},{20000,0,20000},{-20000,0,20000}},{{{0,1,2}},{{0,2,3}}});
 for(unsigned gender=0;gender<2;++gender){std::array<uint8_t,28>a{};a[0]=uint8_t(gender);a[2]=11;a[3]=22;a[15]=46;a[17]=57;auto body=catalog.assemble(a);check(body.ready(),"assembled player body");
  for(auto action:{PlayerMotion::Idle,PlayerMotion::ProneIdle,PlayerMotion::SupineIdle})for(float yaw:{0.f,1.13f}){auto input=*motions.sample(action,.137);catalog.pose(body,input);auto original=body.model.vertices;Vec3 actor{400,20,-300};player::Ragdoll rag;check(rag.start(catalog,gender,input,actor,yaw)&&rag.bodies().size()==13&&rag.maximum_joint_error()<.01f,"13 mapped bodies start with 12 connected anchors");catalog.pose(body,rag.pose());Quat turn{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};float error=0;for(size_t i=0;i<original.size();++i){const auto&p=original[i];const auto&v=body.model.vertices[i];auto expected=add(actor,rotate(turn,{p.x,p.y,p.z}));auto actual=add(rag.origin(),{v.x,v.y,v.z});error=std::max(error,length(sub(actual,expected)));}check(error<.02f,"original skin pose remains continuous at native ragdoll handoff");
   if(action==PlayerMotion::ProneIdle)check(!rag.supine(),"prone root classification");if(action==PlayerMotion::SupineIdle)check(rag.supine(),"supine root classification");
  }
  player::Ragdoll rag;check(rag.start(catalog,gender,*motions.sample(PlayerMotion::Idle,0),{0,800,0},.3f),"start falling rig");auto pelvis=rag.root_position();rag.impulse({9000,1500,13000},add(pelvis,{0,400,0}));float maxError=0,maxPenetration=0;
  for(int i=0;i<360;++i){rag.step(floor,1.f/120);check(rag.active(),"falling native rig stays valid");maxError=std::max(maxError,rag.maximum_joint_error());for(const auto&b:rag.bodies()){check(b.valid()&&std::abs(b.position[0])<15000&&b.position[1]>-1000,"finite bounded articulated body");auto[x,z]=b.segment();maxPenetration=std::max(maxPenetration,b.radius-std::min(x[1],z[1]));}auto pose=rag.pose();catalog.pose(body,pose);for(const auto&v:body.model.vertices)check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z),"ragdoll produces finite skinned model");}
  std::cout<<"gender="<<gender<<" max_joint_mm="<<maxError<<" max_floor_penetration_mm="<<maxPenetration<<" root_y="<<rag.root_position()[1]<<'\n';check(maxError<55&&maxPenetration<8,"articulated contacts and links stay bounded during impact");check(rag.root_position()[1]<500,"unpowered articulated player collapses to ground after impact");
  auto root=rag.root_position();rag.step(floor,std::numeric_limits<float>::quiet_NaN());check(rag.root_position()==root,"invalid ragdoll time ignored");rag.stop();check(!rag.active()&&rag.pose().rotations.empty(),"stopped rig relinquishes pose");auto bad=*motions.sample(PlayerMotion::Idle,0);bad.rootBone=7;check(!rag.start(catalog,gender,bad,{},0),"unmapped root rejected");check(!rag.start(catalog,2,*motions.sample(PlayerMotion::Idle,0),{},0),"unmapped gender rejected");
 }
 if(argc>=4){std::ifstream file(argv[3]);auto world=stage::Collision::read(file);auto hit=world.ray({-42878.8359f,3000,29781.7969f},{0,-1,0},6000);check(bool(hit),"original terrain has checked inspection floor");player::Ragdoll rag;auto actor=hit->position;actor[1]+=500;check(rag.start(catalog,0,*motions.sample(PlayerMotion::Idle,0),actor,.3f),"original terrain ragdoll starts");rag.impulse({0,30000,70000},add(rag.root_position(),{0,500,0}));auto begin=std::chrono::steady_clock::now();float maximum=0;for(int i=0;i<180;++i){rag.step(world,1.f/120);check(rag.active(),"original terrain keeps physical body valid");maximum=std::max(maximum,rag.maximum_joint_error());}auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();std::cout<<"original_triangles="<<world.triangles.size()<<" steps=180 elapsed_seconds="<<seconds<<" max_joint_mm="<<maximum<<'\n';check(maximum<80,"original terrain contacts preserve articulated links");}
 std::cout<<"Ragdoll original topology, male/female continuity, yaw, impact, contacts and rejection passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
