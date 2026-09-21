#include "stage_navigation.h"
#include "water_gameplay.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt;
namespace {
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
bool near(float a,float b,float eps=.02f){return std::abs(a-b)<=eps;}
stage::Collision make_floor(float y=0){return stage::Collision::make({{-20000,y,-20000},{20000,y,-20000},{20000,y,20000},{-20000,y,20000}},{{{0,1,2}},{{0,2,3}}});}
}
int main(int argc,char**argv){try{
 namespace wg=water_gameplay;using stage::Vec3;using stage::Capsule;
 const Capsule standing{350,1700,2},crouching{350,1100,2},prone{260,560,2};
 const auto wet=std::make_shared<stage::Water>(stage::Water::make({{{0,0,0},{10000,1000,10000}}}));auto world=make_floor();
 check(wg::valid_policy({.65f,true})&&wg::valid_policy({1,false}),"native policy bounds");for(float f:{0.f,-1.f,1.01f,NAN,INFINITY})check(!wg::valid_policy({f,true}),"invalid ratio rejected");
 for(auto shape:{standing,crouching,prone})check(wg::valid_body(shape),"supported stance capsule");check(!wg::valid_body({350,560,2})&&!wg::valid_body({350,1101,2})&&!wg::valid_body({350,1700,NAN}),"invalid body rejected");
 auto s=wg::sample(wet.get(),world,{0,2,0},standing),c=wg::sample(wet.get(),world,{0,2,0},crouching),p=wg::sample(wet.get(),world,{0,2,0},prone);
 check(s.level==1000&&s.foot==stage::WaterFoot::inWater&&near(s.horizontalScale,.65f)&&!s.faceSubmerged&&near(s.faceY,1552),"standing is slowed with face above surface");
 check(c.faceSubmerged&&near(c.faceY,952)&&near(c.horizontalScale,.65f)&&!c.proneBlocked,"crouching can move with face submerged");
 check(p.faceSubmerged&&near(p.faceY,412)&&p.proneBlocked&&p.horizontalScale==0,"prone and supine proxy stops horizontal translation");
 auto edge=wg::evaluate(*wet,{10000,2,10000},0,crouching);check(edge.faceSubmerged,"inclusive horizontal FIELD edge");check(!wg::evaluate(*wet,{10000.01f,2,0},0,crouching).level,"outside horizontal FIELD is dry");
 check(wg::evaluate(*wet,{0,50,0},0,crouching).faceSubmerged&&!wg::evaluate(*wet,{0,std::nextafter(1000.f,INFINITY)-950.f,0},0,crouching).faceSubmerged,"face boundary at surface is inclusive and next representable face height is not submerged");
 check(wg::evaluate(*wet,{0,1000,0},0,standing).foot==stage::WaterFoot::inWater&&wg::evaluate(*wet,{0,1000.01f,0},0,standing).foot==stage::WaterFoot::aboveSurface,"body above surface is not slowed");
 check(wg::evaluate(*wet,{0,1002,0},1000,prone).foot==stage::WaterFoot::dry,"floor on water surface is dry");
 check(!wg::evaluate(*wet,{0,2,0},100,standing).level,"floor above body rejected");check(!wg::evaluate(*wet,{0,2,0},-1001,standing).level,"floor below FIELD bottom cannot invent depth");
 check(wg::evaluate(*wet,{0,-1001,0},-1000,prone).foot==stage::WaterFoot::dry,"body outside lower FIELD bound cannot become submerged from support tolerance");
 auto empty=stage::Collision::make({},{});check(!wg::sample(wet.get(),empty,{0,2,0},prone).level&&!wg::sample(nullptr,world,{0,2,0},prone).level,"no floor or no GWW means no water effects");
 check(!wg::sample(wet.get(),world,{NAN,2,0},prone).faceSubmerged&&!wg::sample(wet.get(),world,{0,2,0},{350,560,2}).faceSubmerged,"invalid state never drains oxygen");
 auto overlap=stage::Water::make({{{0,0,0},{10000,1000,10000}},{{0,1,0},{10000,1000,10000}}});check(!wg::sample(&overlap,world,{0,2,0},prone).level,"ambiguous overlapping FIELD levels fail closed");
 auto raised=make_floor(1200);check(!wg::sample(wet.get(),raised,{0,1202,0},standing).faceSubmerged&&wg::sample(wet.get(),raised,{0,1202,0},standing).horizontalScale==1,"bridge floor over same XZ stays dry");
 for(auto shape:{standing,crouching,prone})for(const auto direction:std::array<std::array<float,2>,4>{{{1,0},{-1,0},{0,1},{0,-1}}}){
  stage::Navigation nav(shape);check(nav.water(wet)&&nav.place(world,{0,500,0}),"navigation supported floor");const auto start=nav.feet();
  for(unsigned i=0;i<60;++i)nav.advance(world,{direction[0],direction[1],0,0,1000},1.f/60);
  const float distance=std::hypot(nav.feet()[0]-start[0],nav.feet()[2]-start[2]);check(near(distance,shape.height==560?0.f:650.f,.1f)&&nav.grounded(),"standing/crouch slowed; all prone axes blocked without losing support");
 }
 {stage::Navigation nav(prone);nav.water(wet);check(nav.place(world,{0,500,0}),"turn fixture");const auto start=nav.feet();nav.advance(world,{1,1,1,1,3500},.1f);check(nav.feet()==start&&near(nav.yaw(),-.2f,.0001f)&&near(nav.pitch(),.15f,.0001f),"prone blockage permits camera rotation without translating");
  check(nav.shape(world,standing),"stand up under water allowed");nav.advance(world,{1,0,0,0,1000},.1f);check(nav.feet()==start,"prone-to-standing same tick cannot bypass blocked translation");nav.advance(world,{1,0,0,0,1000},.1f);check(near(std::hypot(nav.feet()[0]-start[0],nav.feet()[2]-start[2]),65,.02f),"next standing step resumes slowed movement");
  check(nav.shape(world,prone),"return prone");auto stopped=nav.feet();nav.advance(world,{1,0,0,0,1000},.1f);check(nav.feet()==stopped,"standing-to-prone cannot move underwater");
  check(!nav.water_policy({NAN,true})&&nav.water_state().proneBlocked,"invalid config preserves active state");check(nav.water_policy({.8f,false}),"explicit policy override");nav.advance(world,{1,0,0,0,1000},.1f);check(near(std::hypot(nav.feet()[0]-stopped[0],nav.feet()[2]-stopped[2]),80,.02f),"native configurable restriction can be explicitly disabled");
 }
 {stage::Navigation dry(prone);check(dry.place(world,{0,500,0}),"dry prone fixture");dry.advance(world,{1,0,0,0,1000},.1f);check(near(dry.feet()[2],100,.02f),"dry prone stays mobile");
  auto small=std::make_shared<stage::Water>(stage::Water::make({{{0,0,100},{1000,1000,50}}}));stage::Navigation boundary(prone);boundary.water(small);check(boundary.place(world,{0,500,0}),"entry boundary fixture");for(unsigned i=0;i<20;++i)boundary.advance(world,{1,0,0,0,1000},1.f/60);check(boundary.feet()[2]<50&&!boundary.water_state().proneBlocked,"prone endpoint inside water is rejected before entering");
 }
 if(argc==3){std::ifstream waterFile(argv[1],std::ios::binary);auto actual=stage::Water::read(waterFile);std::ifstream collisionFile(argv[2]);auto raw=std::make_shared<stage::Collision>(stage::Collision::read(collisionFile));auto physical=stage::movement_collision(raw);stage::Navigation nav;nav.water(std::make_shared<stage::Water>(actual));check(nav.place(*physical,{-73400,3000,-192200}),"actual JJ sampled position has original underwater solid floor");check(nav.water_state().level==1000&&nav.water_state().foot==stage::WaterFoot::inWater&&near(nav.water_state().horizontalScale,.65f),"actual GWW FIELD and original floor feed movement policy");check(nav.shape(*physical,prone),"actual JJ allows prone capsule");auto before=nav.feet();auto neutral=nav;neutral.advance(*physical,{0,0,0,0,1000},.1f);nav.advance(*physical,{1,0,0,0,1000},.1f);check(nav.feet()==neutral.feet()&&nav.feet()[0]==before[0]&&nav.feet()[2]==before[2]&&nav.water_state().proneBlocked&&physical->clear(nav.feet(),nav.capsule()),"actual JJ water prone keeps exact XZ even during passive floor settling");check(nav.shape(*physical,standing),"actual JJ standing escape is clear");before=nav.feet();nav.advance(*physical,{1,0,0,0,1000},.1f);check(nav.feet()[0]==before[0]&&nav.feet()[2]==before[2]&&physical->clear(nav.feet(),nav.capsule()),"actual JJ stand-up latch keeps exact XZ and collision clearance");}
 std::cout<<"water movement: FIELD/body/floor/face bounds, standing/crouch/prone, dry and native policy, transition and entry guard PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
