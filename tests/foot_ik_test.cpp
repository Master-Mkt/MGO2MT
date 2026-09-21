#include "foot_ik.h"
#include "weapon_hand.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cmath>
using namespace mgo2mt;using namespace physics;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
std::vector<char> read(const char*p){std::ifstream f(p,std::ios::binary);check(bool(f),"original input missing");return {std::istreambuf_iterator<char>(f),{}};}
constexpr uint64_t footSurface=stage::attribute::floor|stage::attribute::player|stage::attribute::ik;
stage::Collision plane(float slope=0){return stage::Collision::make({{-3000,-3000*slope,-3000},{3000,3000*slope,-3000},{3000,3000*slope,3000},{-3000,-3000*slope,3000}},{{{0,2,1},footSurface},{{0,3,2},footSurface}});}
bool shoe(const CharacterCatalog& catalog,const PreparedCharacter& body,size_t i){float foot=0;for(unsigned j=0;j<4;++j){auto key=catalog.skeleton(body.gender)[body.skin[i].bones[j]].key;if(key==0xfb4232||key==0xf81206||key==0x5b4a33||key==0xfb1246)foot+=body.skin[i].weights[j];}return foot>.5f;}
bool same(const MotionPose&a,const MotionPose&b){return a.root==b.root&&a.rootBone==b.rootBone&&a.rotations==b.rotations;}
void lengths(const CharacterCatalog& catalog,const PreparedCharacter& body){auto bones=catalog.skeleton(body.gender);for(const auto&b:bones)if(b.parent>=0){auto a=body.bone_position(b.key),p=body.bone_position(bones[b.parent].key);check(a&&p,"full skeleton exposed");check(std::abs(length(sub(*a,*p))-length(sub(b.position,bones[b.parent].position)))<.02f,"original bone lengths preserved");}}
stage::Collision step(){return stage::Collision::make({{-3000,0,-3000},{0,0,-3000},{0,0,3000},{-3000,0,3000},{0,150,-3000},{3000,150,-3000},{3000,150,3000},{0,150,3000}},{{{0,2,1},footSurface},{{0,3,2},footSurface},{{4,6,5},footSurface},{{4,7,6},footSurface},{{1,2,7},footSurface},{{1,7,4},footSurface}});}
int main(int argc,char**argv){try{
 check(argc==4,"catalog, motion and weapon pose paths");CharacterCatalog catalog(read(argv[1]));PlayerMotionBank bank(read(argv[2]));weapon_hand::Bank hands(read(argv[3]));auto ground=plane();
 check(foot_ik::grounded(&ground,{0,2,0},{}),"remote planted capsule admitted");check(!foot_ik::grounded(&ground,{0,80,0},{}),"remote near-ground airborne capsule excluded");
 for(unsigned gender=0;gender<2;++gender){std::array<uint8_t,28>a{};a[0]=uint8_t(gender);a[2]=11;a[3]=22;a[15]=46;a[17]=57;auto body=catalog.assemble(a);check(body.ready(),"original character assembled");
  for(auto b:catalog.skeleton(gender))if(b.key==0xfb4232||b.key==0x5b4a33)std::cout<<"bone "<<gender<<' '<<std::hex<<b.key<<std::dec<<" bind="<<b.position[0]<<','<<b.position[1]<<','<<b.position[2]<<'\n';
  for(auto action:{PlayerMotion::Idle,PlayerMotion::Walk,PlayerMotion::Run,PlayerMotion::CrouchIdle,PlayerMotion::CrouchWalk,PlayerMotion::Aim,PlayerMotion::Reload}){
   foot_ik::Solver solver;float worstError=0,worstUnder=0;unsigned active=0;float lowBefore=1e9f;
   for(unsigned n=0;n<60;++n){auto input=*bank.sample(action,double(n)/60);catalog.pose(body,input);for(const auto&v:body.model.vertices)lowBefore=std::min(lowBefore,v.y);
    auto fixed=solver.solve(catalog,body,input,&ground,{0,2,0},0,1.f/60,true,{1,1,1,1,1});check(solver.result().rig,"MDN chains and boot samples mapped");catalog.pose(body,fixed);
    for(const auto&f:solver.result().feet)if(f.supported){++active;worstError=std::max(worstError,f.error);}
    for(size_t vi=0;vi<body.model.vertices.size();++vi)if(shoe(catalog,body,vi)){const auto&v=body.model.vertices[vi];worstUnder=std::max(worstUnder,-v.y-2);}
    lengths(catalog,body);
    for(const auto&[key,q]:fixed.rotations)check(finite(q),"finite normalized rotations");
   }
   std::cout<<"gender="<<gender<<" action="<<unsigned(action)<<" before="<<lowBefore<<" under="<<worstUnder<<" error="<<worstError<<" active="<<active<<'\n';
   check(active>0,"ground support active");check(worstError<1,"leg endpoint reachable");check(worstUnder<1,"flat ground sole penetration removed");
  }
  for(float slope:{-.6f,-.25f,.25f,.6f})for(float yaw:{0.f,1.13f})for(auto action:{PlayerMotion::Idle,PlayerMotion::Walk,PlayerMotion::Run,PlayerMotion::CrouchIdle}){
   auto world=plane(slope);Vec3 origin{500,500*slope+2,-800};Quat turn{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};foot_ik::Solver solver;float under=0,error=0;
   for(unsigned n=0;n<60;++n){auto pose=*bank.sample(action,double(n)/60);auto original=pose;auto fixed=solver.solve(catalog,body,pose,&world,origin,yaw,1.f/60,true,{1,1,2,1,1});check(same(pose,original),"input pose immutable");catalog.pose(body,fixed);lengths(catalog,body);
    for(const auto&f:solver.result().feet)if(f.supported)error=std::max(error,f.error);
    for(size_t i=0;i<body.model.vertices.size();++i)if(shoe(catalog,body,i)){auto&v=body.model.vertices[i];auto p=add(origin,rotate(turn,{v.x,v.y,v.z}));under=std::max(under,p[0]*slope-p[1]);}
   }
   std::cout<<"slope="<<slope<<" yaw="<<yaw<<" gender="<<gender<<" action="<<unsigned(action)<<" under="<<under<<" error="<<error<<'\n';check(error<1,"slope targets reachable");check(under<1,"slope shoe vertices clear terrain");
  }
  for(float yaw:{0.f,1.13f}){auto world=step();Vec3 origin{-100,2,0};Quat turn{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};foot_ik::Solver solver;float under=0;
   for(unsigned n=0;n<60;++n){auto input=*bank.sample(PlayerMotion::Idle,double(n)/60);auto fixed=solver.solve(catalog,body,input,&world,origin,yaw,1.f/60,true,{1,1,3,1,1});catalog.pose(body,fixed);lengths(catalog,body);
    for(size_t i=0;i<body.model.vertices.size();++i)if(shoe(catalog,body,i)){const auto&v=body.model.vertices[i];auto p=add(origin,rotate(turn,{v.x,v.y,v.z}));under=std::max(under,(p[0]>=0?150.f:0.f)-p[1]);}
   }std::cout<<"step gender="<<gender<<" yaw="<<yaw<<" under="<<under<<'\n';check(under<1,"step sole penetration removed");
  }
  foot_ik::Solver solver;auto input=*bank.sample(PlayerMotion::Idle,0);auto corrected=solver.solve(catalog,body,input,&ground,{0,2,0},0,.016f,true,{1,1,1,1,1});
  auto noIk=ground;for(auto& t:noIk.triangles)t.attribute=stage::attribute::floor|stage::attribute::player;foot_ik::Solver noIkSolver;
  check(same(input,noIkSolver.solve(catalog,body,input,&noIk,{0,2,0},0,.016f,true,{1,1,1,1,1}))&&!noIkSolver.result().active,"player floor without IK bit leaves original foot pose unchanged");
  auto ikOnly=ground;for(auto& t:ikOnly.triangles)t.attribute=stage::attribute::ik;foot_ik::Solver ikOnlySolver;
  check(same(input,ikOnlySolver.solve(catalog,body,input,&ikOnly,{0,2,0},0,.016f,true,{1,1,1,1,1})),"IK-only plane cannot pretend to support the actor body");
  check(same(input,solver.solve(catalog,body,input,&ground,{0,2,0},0,.016f,false,{1,1,1,1,1})),"disabled pose exact");
  check(same(input,solver.solve(catalog,body,input,&ground,{0,1000,0},0,.016f,true,{1,1,1,1,1})),"airborne pose exact");
  check(same(input,solver.solve(catalog,body,input,nullptr,{0,2,0},0,.016f,true,{1,1,1,1,1})),"missing collision exact");
  for(auto a:{PlayerMotion::ProneIdle,PlayerMotion::SupineIdle,PlayerMotion::Roll,PlayerMotion::Backstep,PlayerMotion::PlayDeadProne,PlayerMotion::SelectionSalute})check(!foot_ik::eligible(a),"special poses excluded");
  auto newLife=solver.solve(catalog,body,input,&ground,{0,2,0},0,.016f,true,{1,1,1,2,1});foot_ik::Solver fresh;auto first=fresh.solve(catalog,body,input,&ground,{0,2,0},0,.016f,true,{1,1,1,2,1});check(same(newLife,first),"life change clears contact history");
  unsigned weapons=0;for(auto id:hands.weapons())if(auto hand=hands.select(id,PlayerMotion::Aim,0,true)){
   auto armed=*bank.sample(PlayerMotion::Aim,0);weapon_hand::upper_body(armed,*hand,catalog.skeleton(gender));foot_ik::Solver held;auto fixed=held.solve(catalog,body,armed,&ground,{0,2,0},0,.016f,true,{1,1,id,1,1});catalog.pose(body,fixed);lengths(catalog,body);
   for(size_t i=0;i<body.model.vertices.size();++i)if(shoe(catalog,body,i))check(body.model.vertices[i].y+2>=-1,"weapon stance shoe clearance");++weapons;
  }std::cout<<"weapon aim poses gender="<<gender<<" checked="<<weapons<<'\n';check(weapons>=39,"current weapon catalog covered");
 }
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
