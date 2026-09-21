#include "stage_navigation.h"
#include "camera_settings.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
using namespace mgo2mt::stage;
static void check(bool v,const char*s){if(!v)throw std::runtime_error(s);}
static bool near(float a,float b,float tolerance=1){return std::abs(a-b)<=tolerance;}
struct Mesh {
 std::vector<Vec3> v;std::vector<std::array<unsigned,3>> t;
 void quad(Vec3 a,Vec3 b,Vec3 c,Vec3 d){unsigned i=unsigned(v.size());v.insert(v.end(),{a,b,c,d});t.push_back({i,i+1,i+2});t.push_back({i,i+2,i+3});}
 Collision world(){std::ostringstream out;out<<"MGO2MT.STAGE_COLLISION 1 "<<v.size()<<' '<<t.size()<<'\n';for(auto&p:v)out<<p[0]<<' '<<p[1]<<' '<<p[2]<<'\n';for(auto&p:t)out<<p[0]<<' '<<p[1]<<' '<<p[2]<<" 11282940 7\n";std::istringstream in(out.str());return Collision::read(in);}
};
int main(int argc,char**argv){try{
 Mesh box;box.quad({-10000,0,-10000},{10000,0,-10000},{10000,0,10000},{-10000,0,10000});
 box.quad({2000,0,-10000},{2000,5000,-10000},{2000,5000,10000},{2000,0,10000});
 box.quad({-10000,0,4000},{10000,0,4000},{10000,5000,4000},{-10000,5000,4000});
 box.quad({-10000,2500,-10000},{10000,2500,-10000},{10000,2500,10000},{-10000,2500,10000});auto world=box.world();Capsule body;
 check(world.clear({0,2,0},body)&&!world.clear({1900,2,0},body),"initial overlap detected");
 auto hit=world.sweep({0,2,0},{10000,0,0},body);check(hit&&near(hit->fraction,.1648f,.0001f)&&hit->normal[0]<-.99f,"continuous capsule sweep must not cross thin wall");
 auto roof=world.sweep({0,2,0},{0,10000,0},body);check(roof&&near(roof->fraction,.0796f,.0001f)&&roof->normal[1]<-.99f,"capsule head hits ceiling");
 check(!world.sweep({0,2,0},{-1000,0,0},body),"motion parallel to floor remains free");
 Navigation walk;check(walk.place(world,{0,1000,0}),"place on verified floor");
 // Looking during an evasion must not curve its world travel or bypass sweeps.
 for(float direction:{-1.f,1.f}){
  Navigation maneuver;check(maneuver.place(world,{0,1000,0}),"maneuver floor");
  WalkInput input{direction,0,1,.5f,2000,2,1.5f,0.f};
  for(unsigned i=0;i<60;++i)maneuver.advance(world,input,1.f/60);
  check(std::abs(maneuver.feet()[0])<.01f&&near(maneuver.feet()[2],direction*2000,.2f),"evasion keeps initial world heading despite camera rotation");
  check(near(maneuver.yaw(),-2.f,.001f)&&near(maneuver.pitch(),.75f,.001f)&&maneuver.grounded(),"locked travel preserves camera controls and floor");
  auto p=maneuver.feet();auto yaw=maneuver.yaw();input.movementYaw=NAN;maneuver.advance(world,input,.1f);
  check(p==maneuver.feet()&&yaw==maneuver.yaw(),"invalid maneuver heading rejects whole step");
 }
 {Navigation maneuver;check(maneuver.place(world,{0,1000,0}),"wall maneuver floor");
  for(unsigned i=0;i<120;++i)maneuver.advance(world,{1,0,-1,0,6000,2,1.5f,1.570796327f},1.f/60);
  check(maneuver.feet()[0]>1600&&maneuver.feet()[0]<=1649&&std::abs(maneuver.feet()[2])<.01f&&world.clear(maneuver.feet(),body),"locked high-speed maneuver stops at wall and keeps capsule clear");}
 for(int i=0;i<240;++i)walk.advance(world,{1,-1,0,0},1.f/120);
 check(walk.ready()&&walk.grounded()&&walk.feet()[0]<=1649&&walk.feet()[2]<=3649&&walk.feet()[0]>1600&&walk.feet()[2]>3600,"wall sliding then corner stops both axes");
 check(near(walk.feet()[1],2),"floor retains capsule foot height");auto before=walk.feet();walk.advance(world,{1,0,0,0},-1);walk.advance(world,{std::numeric_limits<float>::quiet_NaN(),0,0,0},1);check(walk.feet()==before,"invalid input cannot alter position");
 Navigation falling;check(falling.place(world,{0,1000,0}),"second place");for(int i=0;i<60;++i)falling.advance(world,{0,1,0,0},1.f/60);check(near(falling.feet()[0],-3500,3),"speed is per second without diagonal boost");
 Navigation analog;check(analog.place(world,{0,1000,0}),"analog place");for(int i=0;i<60;++i)analog.advance(world,{.5f,0,0,0,1000},1.f/60);check(near(analog.feet()[2],500,2),"half stick preserves half speed");
 auto waterField=std::make_shared<Water>(Water::make({{{0,0,0},{10000,1000,10000}}}));
 Navigation wet,dry;check(wet.water(waterField,.65f)&&wet.place(world,{0,500,0})&&dry.place(world,{0,500,0}),"water navigation setup");
 check(wet.water_state().level==1000&&wet.water_state().foot==WaterFoot::inWater&&near(wet.water_state().depthAboveFloor,1000,.01f),"original level/floor classification reaches native controller");
 for(unsigned i=0;i<60;++i){wet.advance(world,{.5f,0,0,0,1000},1.f/60);dry.advance(world,{.5f,0,0,0,1000},1.f/60);}
 check(near(wet.feet()[2],325,.1f)&&near(dry.feet()[2],500,.1f)&&wet.feet()[1]==dry.feet()[1],"explicit native .65 reduces horizontal speed only; surface is not solid");
 auto wetPos=wet.feet();for(float bad:{0.f,-1.f,1.1f,NAN})check(!wet.water(waterField,bad),"invalid prototype ratio rejected");
 check(wet.water_state().horizontalScale==.65f&&wet.feet()==wetPos,"invalid setting keeps active water state");
 check(wet.water({},1)&&!wet.water_state().level,"clearing map water clears state");wet.advance(world,{1,0,0,0,1000},.1f);check(near(wet.feet()[2]-wetPos[2],100,.1f),"unloaded water restores full speed");
 Navigation edge;auto smallWater=std::make_shared<Water>(Water::make({{{0,0,0},{10000,1000,50}}}));check(edge.water(smallWater,.5f)&&edge.place(world,{0,500,0}),"water edge setup");
 for(unsigned i=0;i<60;++i)edge.advance(world,{1,0,0,0,1000},1.f/60);
 check(edge.feet()[2]>940&&edge.feet()[2]<951&&!edge.water_state().level&&edge.water_state().horizontalScale==1,"leaving verified bounds restores speed during next substep");
 wet.water(waterField,.65f);wet.clear();check(!wet.water_state().level&&!wet.ready(),"clearing navigation drops water state");
 for(unsigned mode=0;mode<3;++mode)for(unsigned axis=0;axis<2;++axis){
  Navigation normal,reversed;check(normal.place(world,{0,1000,0})&&reversed.place(world,{0,1000,0}),"camera direction fixture");
  mgo2mt::camera::Settings settings;settings.reversed[mode*2+axis]=true;
  auto motion=settings.motion(mode==2,mode!=0,.25f,.5f);
  for(unsigned i=0;i<30;++i){normal.advance(world,{0,0,.25f,.5f},1.f/60);reversed.advance(world,{0,0,motion[0],motion[1]},1.f/60);}
  check(near(reversed.yaw(),axis?-normal.yaw():normal.yaw(),.000001f)&&near(reversed.pitch(),axis?normal.pitch():-normal.pitch(),.000001f),"camera inversion reaches navigation without altering angular magnitude");
  check(reversed.feet()==normal.feet(),"camera direction setting does not translate player");
 }
 for(unsigned mode=0;mode<3;++mode)for(unsigned value:{1u,5u,10u}){
  Navigation camera;check(camera.place(world,{0,1000,0}),"camera rate fixture");mgo2mt::camera::Settings settings;settings.speed[mode]=value;
  const auto rates=settings.rates(mode==2,mode!=0);auto origin=camera.feet();
  const float expectedScale=mode==0?(value==1?.5f:value==5?1.f:1.625f):(value==1?.2f:value==5?1.f:2.f);
  check(rates==std::array<float,2>{2.f*expectedScale,1.5f*expectedScale},"original mode-mapped setting curve preserves native default rates");
  camera.advance(world,{0,0,.5f,.25f,3500,rates[0],rates[1]},.1f);
  check(near(camera.yaw(),-.05f*rates[0],.000001f)&&near(camera.pitch(),.025f*rates[1],.000001f),"selected camera rates reach angular integration without clamp saturation");
  check(near(camera.yaw(),-.1f*expectedScale,.000001f)&&near(camera.pitch(),.0375f*expectedScale,.000001f),"golden original relative settings drive actual navigation angles");
  check(camera.feet()==origin,"camera speed does not change player position");auto yaw=camera.yaw(),pitch=camera.pitch();
  for(float bad:{-1.f,5.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})camera.advance(world,{0,0,1,1,3500,bad,rates[1]},.1f);
  for(float bad:{-1.f,4.f,std::numeric_limits<float>::quiet_NaN()})camera.advance(world,{0,0,1,1,3500,rates[0],bad},.1f);
  check(camera.yaw()==yaw&&camera.pitch()==pitch&&camera.feet()==origin,"invalid angular rates reject the complete step");
 }
 Navigation tracking;check(tracking.place(world,{0,1000,0}),"tracking place");auto trackFeet=tracking.feet(),trackEye=tracking.eye();Vec3 target{10000,trackEye[1]+1000,10000};
 check(tracking.track_view(target,.1f)&&near(tracking.yaw(),.2f,.00001f)&&std::abs(tracking.pitch())<=.150001f&&tracking.feet()==trackFeet,"tracking uses manual angular limits without translating body");
 for(unsigned i=0;i<60;++i)check(tracking.track_view(target,1.f/60),"tracking converges");check(near(tracking.yaw(),.7853982f,.00001f),"tracking reaches target yaw");auto heading=tracking.direction();
 check(tracking.track_view(target,0)&&tracking.direction()==heading,"same millisecond keeps valid lock and exact direction");
 check(!tracking.track_view({NAN,0,0},.1f)&&!tracking.track_view(target,-1)&&tracking.direction()==heading,"invalid tracking input has no effect");
 tracking.facing(3.13f);check(tracking.track_view({-100,trackEye[1],-10000},.1f)&&std::abs(std::remainder(tracking.yaw()-3.13f,6.283185307f))<.03f,"tracking wraps yaw through shortest arc");
 Mesh low;low.quad({-10000,0,-10000},{10000,0,-10000},{10000,0,10000},{-10000,0,10000});low.quad({-10000,1250,-10000},{10000,1250,-10000},{10000,1250,10000},{-10000,1250,10000});auto lowWorld=low.world();Navigation crouched({350,1100,2});check(crouched.place(lowWorld,{0,800,0}),"crouched clearance");check(!crouched.shape(lowWorld,{350,1700,2}),"low ceiling prevents standing");check(crouched.shape(lowWorld,{260,560,2}),"prone clearance shrink");check(crouched.shape(lowWorld,{350,1100,2}),"prone can return to crouch");
 Mesh ramp;ramp.quad({-10000,-3000,-10000},{10000,3000,-10000},{10000,3000,10000},{-10000,-3000,10000});auto slope=ramp.world();Navigation uphill;check(uphill.place(slope,{0,1000,0}),"capsule can be placed on walkable slope");uphill.facing(1.5707963f);for(int i=0;i<120;++i)uphill.advance(slope,{1,0,0,0},1.f/120);check(uphill.feet()[0]>3000&&uphill.feet()[1]>900&&uphill.grounded(),"walkable slope raises body without penetrating");
 Mesh emptyMesh;auto empty=emptyMesh.world();check(!walk.place(empty,{0,1000,0})&&!walk.ready(),"missing floor refuses inspection start");
 Mesh grid;for(int x=0;x<32;++x)for(int z=0;z<32;++z)grid.quad({float(x*100),12,float(z*100)},{float((x+1)*100),12,float(z*100)},{float((x+1)*100),12,float((z+1)*100)},{float(x*100),12,float((z+1)*100)});auto many=grid.world();std::mt19937 rng(77);for(int i=0;i<500;++i){float x=float(int(rng()%5200)-1000)+.5f,z=float(int(rng()%5200)-1000)+.5f;auto h=many.ray({x,100,z},{0,-3,0},1000);bool inside=x>=0&&x<=3200&&z>=0&&z<=3200;check(bool(h)==inside,"BVH ray agrees with analytic grid extent");if(h)check(near(h->distance,88,.001f)&&h->normal[1]>.99f,"BVH nearest plane distance and normal");}
 if(argc>1){std::ifstream input(argv[1]);auto real=Collision::read(input);Navigation actual;check(actual.place(real,{-42878.8359f,3000,29781.7969f}),"decoded BASE_HOME vicinity has capsule clearance (inspection anchor, not spawn)");auto began=std::chrono::steady_clock::now();for(int i=0;i<600;++i)actual.advance(real,{1,.2f,.15f,0},1.f/120);auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count();check(actual.ready()&&real.clear(actual.feet(),body),"real map walk ends outside geometry");std::cout<<"world_triangles="<<real.triangles.size()<<" steps=600 seconds="<<elapsed<<" feet="<<actual.feet()[0]<<','<<actual.feet()[1]<<','<<actual.feet()[2]<<'\n';}
 std::cout<<"capsule floor/wall/ceiling, corner sliding, slope, BVH and invalid inputs passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
