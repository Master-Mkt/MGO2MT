#include "mounted_renderer.h"
#include "mounted_flight_presentation.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>

using namespace mgo2mt;
namespace {
size_t checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
std::vector<char> read(const std::filesystem::path& path){
 std::ifstream file(path,std::ios::binary);check(bool(file),"required original fixture missing");
 return {std::istreambuf_iterator<char>(file),{}};
}
uint32_t word(std::span<const char>b,size_t at){uint32_t v=0;for(unsigned j=0;j<4;++j)v|=uint32_t(uint8_t(b[at+j]))<<(8*j);return v;}
void word(std::vector<char>&b,size_t at,uint32_t value){for(unsigned j=0;j<4;++j)b[at+j]=char(value>>(8*j));}
bool close_value(float a,float b,float tolerance=.03f){return std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tolerance;}
bool close_value(mounted::Vec3 a,mounted::Vec3 b,float tolerance=.03f){return close_value(a[0],b[0],tolerance)&&close_value(a[1],b[1],tolerance)&&close_value(a[2],b[2],tolerance);}
mounted::Vec3 xyz(const ModelVertex&v){return {v.x,v.y,v.z};}
mounted::Vec3 sub(mounted::Vec3 a,mounted::Vec3 b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
mounted::Vec3 add(mounted::Vec3 a,mounted::Vec3 b){return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};}
float length(mounted::Vec3 p){return std::hypot(p[0],p[1],p[2]);}
mounted::Vec3 unit(mounted::Vec3 p){const auto n=length(p);check(n>0,"nonzero measured barrel axis");for(auto&x:p)x/=n;return p;}
bool same_vertices(std::span<const ModelVertex>a,std::span<const ModelVertex>b){return a.size()==b.size()&&!std::memcmp(a.data(),b.data(),a.size_bytes());}
void channels(const ModelVertex&a,const ModelVertex&b){
 // All three UV sets, baked light and authored COLOR0 are immutable when posing.
 check(!std::memcmp(&a.u,&b.u,sizeof(ModelVertex)-offsetof(ModelVertex,u)),"articulation altered original UV/light/color channels");
}
template<class Action>void refused(Action action,const char*why){bool denied=false;try{action();}catch(const std::exception&){denied=true;}check(denied,why);}
void invalid_rigs(const std::vector<char>&source,size_t vertices){
 auto test=[&](auto mutate,const char*why){auto bytes=source;mutate(bytes);refused([&]{mounted::Rig::read(bytes,vertices);},why);};
 test([](auto&b){word(b,12,0);},"zero bones accepted");
 test([](auto&b){word(b,12,65);},"unbounded bones accepted");
 test([](auto&b){word(b,36,word(b,16));},"duplicate bone identities accepted");
 test([](auto&b){word(b,40,2);},"forward parent/cycle accepted");
 test([](auto&b){word(b,20,0);},"root with parent accepted");
 test([](auto&b){word(b,24,0x7fc00000);},"NaN bind pivot accepted");
 test([](auto&b){b.back()=char(word(b,12));},"out of range vertex bone accepted");
 test([](auto&b){b.pop_back();},"truncated variable rig accepted");
 test([](auto&b){b.push_back(0);},"trailing rig bytes accepted");
 refused([&]{mounted::Rig::read(source,vertices+1);},"different model vertex extent accepted");
}
struct Fixture {CharacterModel model;mounted::Rig rig;std::vector<char> modelBytes,rigBytes;};
Fixture fixture(const std::filesystem::path&root,const char*name,size_t expectedBones,size_t expectedVertices){
 Fixture f;f.modelBytes=read(root/(std::string(name)+".gwm"));f.rigBytes=read(root/(std::string(name)+".rig"));
 f.model=CharacterModel(f.modelBytes);f.rig=mounted::Rig::read(f.rigBytes,f.model.vertices.size());
 check(f.model.vertices.size()==expectedVertices,"original model vertex count changed");
 check(f.rig.keys.size()==expectedBones&&f.rig.parents.size()==expectedBones&&f.rig.bonePivots.size()==expectedBones,"variable GMR1 lost original bones");
 std::vector<size_t> counts(expectedBones);
 for(size_t i=0;i<expectedBones;++i){
  const auto at=16+20*i;check(f.rig.keys[i]==word(f.rigBytes,at),"bone order/hash changed");
  check(f.rig.parents[i]==int32_t(word(f.rigBytes,at+4)),"bone hierarchy changed");
  for(unsigned axis=0;axis<3;++axis){auto bits=word(f.rigBytes,at+8+axis*4);float raw;std::memcpy(&raw,&bits,4);check(f.rig.bonePivots[i][axis]==raw,"source bone pivot changed");}
 }
 for(size_t i=0;i<expectedVertices;++i){check(f.rig.vertices[i]==uint8_t(f.rigBytes[16+20*expectedBones+i]),"source rigid assignment changed");++counts[f.rig.vertices[i]];}
 check(counts.back()>0,"highest original bone assignment was not exercised");
 invalid_rigs(f.rigBytes,expectedVertices);return f;
}
void original_models(const std::filesystem::path&root,const mounted::Registry&registry){
 const auto* mortar=registry.find("mortar");const auto* catapult=registry.find("catapult");
 check(mortar&&catapult&&mortar->kind==mounted::Kind::mortar&&catapult->kind==mounted::Kind::catapult,"configured launcher types missing");
 auto m=fixture(root,"mortar",5,3440),c=fixture(root,"catapult",14,5715);
 check(m.rig.keys==std::vector<uint32_t>{0x5ebe85,0x9f8893,0xe0640c,0xb32f58,0xe042c7},"original mortar bone identities changed");
 check(close_value(mortar->pivot,m.rig.bonePivots[1])&&close_value(mortar->bindPitch,1.57079632679f,.000001f),"mortar binding pivot/pitch differs from upright original resource");
 check(close_value(mortar->muzzle,{0,1071,4.2f})&&close_value(mortar->launchDirection,{0,1,0},.000001f),"mortar CNP muzzle/axis not applied");
 const auto originalM=m.model.vertices,originalC=c.model.vertices;const auto originalIndices=m.model.indices;
 auto posed=m.model.vertices;
 mounted::articulate(m.model,m.rig,*mortar,0,mortar->bindPitch,posed);
 for(size_t i=0;i<posed.size();++i){check(close_value(xyz(posed[i]),xyz(originalM[i])),"bind pitch must preserve upright geometry");channels(originalM[i],posed[i]);}
 // The tube is bone2. Its original lower/upper rings are tapered and differ
 // from the CNP axis by about0.20 degrees; do not invent coincident XZ edges.
 float lowY=std::numeric_limits<float>::max(),highY=-lowY;
 for(size_t i=0;i<originalM.size();++i)if(m.rig.vertices[i]==2){lowY=std::min(lowY,originalM[i].y);highY=std::max(highY,originalM[i].y);}
 const auto span=highY-lowY;check(span>400,"original barrel does not expose separated end rings");
 std::array<std::map<std::array<float,3>,size_t>,2> rings;
 for(size_t i=0;i<originalM.size();++i)if(m.rig.vertices[i]==2){const auto&v=originalM[i];if(v.y<lowY+1)rings[0].emplace(xyz(v),i);if(v.y>highY-1)rings[1].emplace(xyz(v),i);}
 check(rings[0].size()>=8&&rings[1].size()>=8,"source barrel rings not resolved");
 auto center=[&](size_t ring){mounted::Vec3 sum{};for(const auto&[point,index]:rings[ring])sum=add(sum,xyz(posed[index]));for(auto&v:sum)v/=float(rings[ring].size());return sum;};
 mounted::Instance instance{20,2,"mortar",{2300,120,-4100},.7f};
 for(float relativeYaw:{-.5f,0.f,.5f})for(float pitch:{mortar->pitchMin,mortar->initialPitch,mortar->pitchMax}){
  mounted::articulate(m.model,m.rig,*mortar,relativeYaw,pitch,posed);size_t moving=0;
  for(size_t i=0;i<posed.size();++i){channels(originalM[i],posed[i]);if(!m.rig.vertices[i])check(close_value(xyz(originalM[i]),xyz(posed[i])),"mortar base moved with barrel");else{moving+=!close_value(xyz(originalM[i]),xyz(posed[i]));check(close_value(length(sub(xyz(originalM[i]),mortar->pivot)),length(sub(xyz(posed[i]),mortar->pivot)),.05f),"rigid mortar shape stretched");}check(close_value(length({posed[i].nx,posed[i].ny,posed[i].nz}),length({originalM[i].nx,originalM[i].ny,originalM[i].nz}),.0001f),"normal length changed");}
  check(moving>1000,"pitched mortar stayed in bind pose");
  const float yaw=instance.yaw+relativeYaw;const auto launch=mounted::launch_direction(*mortar,yaw,pitch);
  const mounted::Vec3 expected{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)};
  check(close_value(launch,expected,.00001f),"launch direction has wrong bind-pitch or yaw sign");
  const auto barrel=unit(mounted::rotate(sub(center(1),center(0)),instance.yaw));
  check(barrel[0]*launch[0]+barrel[1]*launch[1]+barrel[2]*launch[2]>.99998f,"HOST launch direction disagrees with posed original barrel");
  // Apply the same skin assignment to the exact original CNP muzzle point.
  CharacterModel probe;const auto q=mortar->muzzle;probe.vertices={{q[0],q[1],q[2],0,1,0,0,0}};
  auto probeRig=m.rig;probeRig.vertices={1};auto output=probe.vertices;
  mounted::articulate(probe,probeRig,*mortar,relativeYaw,pitch,output);
  const auto renderedMuzzle=add(instance.origin,mounted::rotate(xyz(output[0]),instance.yaw));
  check(close_value(renderedMuzzle,mounted::muzzle_position(instance,*mortar,yaw,pitch)),"HOST muzzle does not follow rendered original CNP position");
 }
 auto cp=c.model.vertices;mounted::articulate(c.model,c.rig,*catapult,.8f,.6f,cp);
 for(size_t i=0;i<cp.size();++i)channels(originalC[i],cp[i]);
 check(cp.size()==originalC.size()&&c.model.indices.size()>0&&c.model.parts.size()>0,"catapult geometry discarded");
 check(same_vertices(m.model.vertices,originalM)&&same_vertices(c.model.vertices,originalC)&&m.model.indices==originalIndices,"posing modified immutable source model");
 check(read(root/"mortar.gwm")==m.modelBytes&&read(root/"mortar.rig")==m.rigBytes&&read(root/"catapult.gwm")==c.modelBytes&&read(root/"catapult.rig")==c.rigBytes,"presentation rewrote original asset bytes");
 std::cout<<"Original fixtures: mortar5 bones/3440 vertices; catapult14 bones/5715 vertices; barrel span="<<span<<"mm\n";
}
combat::Player player(){combat::Player p;p.identity={1,2,300};p.life=4;p.alive=true;p.flightId=3;p.flightElapsedMs=500;return p;}
void expect(const std::optional<mounted::FlightFrame>&f,uint16_t id,double seconds,bool landing,const char*why){check(f&&f->instance==id&&std::abs(f->seconds-seconds)<.000001&&f->landing==landing,why);}
void flight_clock(){
 mounted::FlightPresentation clock;auto p=player();
 expect(clock.update(9,&p,1000),3,.5,false,"HOST elapsed starts flight");
 expect(clock.update(9,&p,1100),3,.6,false,"interpolates within HOST quantum");
 expect(clock.update(9,&p,2000),3,.75,false,"missing packets cannot extrapolate beyond250ms");
 p.flightElapsedMs=750;expect(clock.update(9,&p,2050),3,.75,false,"new HOST age resets interpolation baseline");
 expect(clock.update(9,&p,2100),3,.8,false,"new baseline interpolation");
 expect(clock.update(9,&p,2075),3,.75,false,"clock rollback between packets must reset interpolation");
 expect(clock.update(9,&p,100),3,.75,false,"clock rollback before prior sample resets");
 clock.clear();expect(clock.update(9,&p,300),3,.75,false,"explicit clear resets clock");
 p.flightId=4;p.flightElapsedMs=0;expect(clock.update(9,&p,350),4,0,false,"another launch restarts instance clock");
}
void flight_lifecycle(){
 auto p=player();mounted::FlightPresentation clock;clock.update(9,&p,1000);p.flightId=0;p.flightElapsedMs=0;
 expect(clock.update(9,&p,1100),3,0,true,"HOST landing starts landing pose once");
 expect(clock.update(9,&p,1749),3,.649,true,"landing pose persists within650ms");
 check(!clock.update(9,&p,1750)&&!clock.update(9,&p,1800),"landing pose expires and never restarts");
 auto interrupted=[&](auto change,const char*why){auto q=player();mounted::FlightPresentation c;c.update(9,&q,1000);q.flightId=0;q.flightElapsedMs=0;c.update(9,&q,1100);change(q);check(!c.update(9,&q,1150),why);q=player();q.flightId=0;q.flightElapsedMs=0;check(!c.update(9,&q,1200),"cancelled landing reappeared");};
 interrupted([](auto&q){q.alive=false;},"death retains landing");interrupted([](auto&q){q.stunned=true;},"stun retains landing");
 interrupted([](auto&q){q.mountedId=2;},"new mount retains landing");interrupted([](auto&q){q.reloadUntil=1;},"reload retains landing");
 interrupted([](auto&q){q.aiming=true;},"aim retains landing");interrupted([](auto&q){q.pose.feet[0]+=100;},"movement retains landing");
 interrupted([](auto&q){++q.life;},"respawn retains old landing");interrupted([](auto&q){++q.identity.instance;},"identity replacement retains landing");
 for(unsigned condition=0;condition<3;++condition){auto q=player();mounted::FlightPresentation c;c.update(9,&q,1000);q.flightId=0;q.flightElapsedMs=0;check(!c.update(condition==0?10:condition==1?0:9,condition==2?nullptr:&q,1100),"epoch/null transition fabricates landing");}
 for(bool death:{false,true}){auto q=player();mounted::FlightPresentation c;c.update(9,&q,1000);q.alive=!death;q.stunned=!death;check(!c.update(9,&q,1100),"incapacitated player retains flight animation");q=player();q.flightId=0;q.flightElapsedMs=0;check(!c.update(9,&q,1200),"incapacitation fabricated later landing");}
}
void flight_position(){
 mounted::FlightPosition position;auto p=player();p.pose.feet={100,200,300};
 check(close_value(position.update(9,p,1000),p.pose.feet),"first flight position must snap to HOST");
 p.pose.feet={200,400,500};const auto authoritative=p;
 check(close_value(position.update(9,p,1050),{100,200,300}),"new packet begins from previous visible position");
 check(close_value(position.update(9,p,1075),{150,300,400}),"flight position interpolates linearly at25ms");
 check(close_value(position.update(9,p,1100),p.pose.feet),"flight interpolation reaches HOST within50ms");
 check(close_value(position.update(9,p,2000),p.pose.feet),"missing packets never extrapolate flight position");
 check(p==authoritative,"presentation changed HOST player data");
 p.pose.feet={300,500,600};position.update(9,p,2050);
 check(close_value(position.update(9,p,2075),{250,450,550}),"subsequent flight interpolation");
 check(close_value(position.update(9,p,2060),p.pose.feet),"flight position clock rollback must discard stale lerp");
 p.pose.feet={20000,20000,20000};check(close_value(position.update(9,p,2100),p.pose.feet),"large HOST correction must snap");
 auto scope=[&](auto change,const char*why){auto q=player();mounted::FlightPosition c;q.pose.feet={10,20,30};c.update(9,q,100);q.pose.feet={60,70,80};c.update(9,q,150);change(q);q.pose.feet={500,600,700};check(close_value(c.update(9,q,175),q.pose.feet),why);};
 scope([](auto&q){++q.life;},"new life inherited old flight position");
 scope([](auto&q){++q.identity.instance;},"new identity inherited old flight position");
 scope([](auto&q){++q.flightId;},"different launcher inherited old flight position");
 scope([](auto&q){q.flightId=0;q.flightElapsedMs=0;},"landing position remained delayed");
 scope([](auto&q){q.alive=false;q.flightId=0;q.flightElapsedMs=0;},"death position remained delayed");
 p=player();p.pose.feet={1,2,3};position.update(9,p,2200);p.pose.feet={4,5,6};
 check(close_value(position.update(10,p,2250),p.pose.feet),"epoch transition interpolated across worlds");
 p.flightId=0;p.flightElapsedMs=0;p.pose.feet={7,8,9};check(close_value(position.update(10,p,2270),p.pose.feet),"ordinary on-foot position delayed");
 p.pose.feet={10,11,12};check(close_value(position.update(10,p,2280),p.pose.feet),"subsequent on-foot movement delayed");
}
}
int main(int argc,char**argv){
 try{check(argc==3,"usage: presentation_test resources mounted_weapons.json");mounted::Registry registry;std::string error;check(registry.load(argv[2],error),error.c_str());
  original_models(argv[1],registry);flight_lifecycle();flight_clock();flight_position();
  std::cout<<"PASS "<<checks<<" checks: variable original GMR1, immutable channels/assets, muzzle/barrel agreement, authoritative flight clock and lifecycle\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}
}

