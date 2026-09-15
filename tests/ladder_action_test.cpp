#include "ladder_action.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2win;
namespace {
void check(bool b,const char*why){if(!b)throw std::runtime_error(why);}
stage::Collision geometry(bool ceiling=false){std::vector<stage::Vec3>v;std::vector<stage::CollisionTriangle>t;
 auto quad=[&](stage::Vec3 a,stage::Vec3 b,stage::Vec3 c,stage::Vec3 d){unsigned n=unsigned(v.size());v.insert(v.end(),{a,b,c,d});t.push_back({{n,n+1,n+2}});t.push_back({{n,n+2,n+3}});};
 quad({-4000,0,-4000},{-4000,0,4000},{4000,0,4000},{4000,0,-4000});
 quad({500,3000,-4000},{500,3000,4000},{4000,3000,4000},{4000,3000,-4000});
 if(ceiling)quad({-1000,2500,-1000},{1000,2500,-1000},{1000,2500,1000},{-1000,2500,1000});return stage::Collision::make(v,t);
}
}
int main(int argc,char**argv){try{
 auto w=geometry();ladder::Anchor a{1,{0,4,0},{0,3004,0},{-400,4,0},{1100,3004,0},1.57079633f};stage::Capsule c{350,1700,2};
 check(ladder::valid(a),"anchor valid");auto s=ladder::enter(a,a.bottomExit,c,w);check(bool(s),"bottom enter");check(!ladder::leave(*s,w)||!s->active,"bottom exit");s=ladder::enter(a,a.bottomExit,c,w);
 check(!ladder::enter(a,{0,1500,0},c,w),"no midair enter");check(!ladder::enter(a,{-1500,4,0},c,w),"no distant snap");check(!ladder::enter(a,a.bottomExit,{800,4200,2},w),"Gekko does not shrink into human ladder");
 for(int i=0;i<20;++i)check(ladder::advance(*s,1,.1,w),"climb step");check(s->feet[1]>1100&&s->feet[1]<1300,"bounded exact climb");check(!ladder::leave(*s,w),"no midair exit");
 auto feet=s->feet;check(!ladder::advance(*s,1,.251,w)&&s->feet==feet,"unbounded dt rejected atomically");check(!ladder::advance(*s,std::numeric_limits<float>::quiet_NaN(),.1,w)&&s->feet==feet,"NaN rejected");
 for(int i=0;i<40;++i)check(ladder::advance(*s,1,.1,w),"top clamp");check(s->feet==a.top&&ladder::leave(*s,w)&&!s->active&&s->feet==a.topExit,"safe supported top exit");
 s=ladder::enter(a,a.topExit,c,w);check(bool(s),"top enter");for(int i=0;i<60;++i)check(ladder::advance(*s,-1,.1,w),"descent");check(ladder::leave(*s,w)&&s->feet==a.bottomExit,"bottom full roundtrip");
 auto low=geometry(true);s=ladder::enter(a,a.bottomExit,c,low);check(bool(s),"low ceiling initial");for(int i=0;i<100;++i)check(ladder::advance(*s,1,.1,low),"ceiling accepted stop");check(s->feet[1]<805&&s->feet[1]>700,"ceiling no tunnel");
 s=ladder::enter(a,a.bottomExit,c,w);std::array<ladder::Body,1>peer{{{{0,2100,0},c}}};for(int i=0;i<100;++i)check(ladder::advance(*s,1,.1,w,peer),"peer accepted stop");check(s->feet[1]<410,"peer continuous capsule stop");
 auto bad=a;bad.top[0]=1;check(!ladder::valid(bad),"diagonal anchor rejected");bad=a;bad.topExit={1100,4500,0};s=ladder::enter(bad,bad.bottomExit,c,w);check(bool(s),"unsupported exit fixture");for(int i=0;i<100;++i)ladder::advance(*s,1,.1,w);check(!ladder::leave(*s,w)&&s->active,"unsupported exit retains attachment");
 if(argc==2){std::ifstream f(argv[1]);check(bool(f),"AA collision input");auto real=stage::Collision::read(f);ladder::Anchor aa{1,{-42000,6,24500},{-42000,7454,24500},{-42400,6,24500},{-40500,7004,25500},1.57079633f};s=ladder::enter(aa,aa.bottomExit,c,real);check(bool(s),"AA native offset entrance");for(int i=0;i<130;++i)check(ladder::advance(*s,1,.1,real),"AA ascent");check(s->feet==aa.top&&!ladder::leave(*s,real)&&s->active,"AA real railing rejects unverified top exit");for(int i=0;i<130;++i)check(ladder::advance(*s,-1,.1,real),"AA descent");check(ladder::leave(*s,real)&&s->feet==aa.bottomExit,"AA real lower return");std::cout<<"AA original GEOM ascent / railing stop / lower return PASS\n";}
 std::cout<<"Ladder finite entry/exit/GEOM/peer/time/invalid boundaries PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}








