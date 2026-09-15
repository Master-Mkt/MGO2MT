#include "motion_blend.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2win;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static bool close(double a,double b,double epsilon=1e-5){return std::abs(a-b)<=epsilon;}
static MotionPose sample(float y=0,std::array<float,4> q={0,0,0,1}){
 MotionPose p;p.rootBone=1;p.root={10,y,30};p.rotations={{1,q},{2,{0,0,0,1}}};return p;
}
static bool equal(const MotionPose& a,const MotionPose& b){return a.rootBone==b.rootBone&&a.root==b.root&&a.rotations==b.rotations;}
static void normalized(const MotionPose& p){for(const auto& [key,q]:p.rotations){double norm=0;for(float x:q){check(std::isfinite(x),"finite quaternion");norm+=double(x)*x;}check(close(norm,1),"unit quaternion");}}
template<class F>static void throws(F f){bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid initial sample throws");}
int main(){try{
 MotionBlend blend;check(!blend.pose()&&!blend.active(),"empty initial state");
 auto a=sample(),b=sample(100,{0,0,4,0});b.root={110,100,-70};
 check(equal(blend.update(1,a,100),a)&&blend.progress()==1&&blend.last_update_accepted(),"first target immediate");
 const auto previous=*blend.pose();check(equal(blend.update(2,b,100),previous),"key change preserves previous display and consumes no dt");
 check(blend.progress()==0&&blend.active(),"new transition starts at zero");
 auto half=blend.update(2,b,.1);check(close(blend.progress(),.5)&&half.root==std::array<float,3>{60,50,-20},"default 5 per second complete XYZ root midpoint");
 check(close(half.rotations.at(1)[2],std::sqrt(.5))&&close(half.rotations.at(1)[3],std::sqrt(.5)),"normalized 180-degree slerp midpoint");normalized(half);
 const auto held=*blend.pose();blend.update(2,b,0);check(equal(*blend.pose(),held)&&close(blend.progress(),.5),"zero dt same target preserves pose and progress");
 blend.update(2,b,.1);check(!blend.active()&&blend.progress()==1&&blend.pose()->root[1]==100,"same key completes in .2 seconds");
 auto moving=sample(125,{0,0,1,0});blend.update(2,moving,0);check(equal(*blend.pose(),moving),"completed same key adopts every current target immediately");

 // A->B->C interrupted while halfway: start C exactly at the last display.
 blend.reset();blend.update(1,a,0);blend.update(2,b,0);blend.update(2,b,.1);
 auto mid=*blend.pose(),c=sample(-100,{0,1,0,0});
 check(equal(blend.update(3,c,.15),mid)&&blend.progress()==0,"interruption freezes mixed display without a pop");
 auto interrupted=blend.update(3,c,.1);check(close(interrupted.root[1],-25),"interruption lerps from mixed root");normalized(interrupted);
 // Blend toward this frame's target, not a stale target captured at entry.
 auto changed=sample(200,{0,1,0,0});blend.update(3,changed,0);
 check(close(blend.progress(),.5)&&close(blend.pose()->root[1],125),"same key current target sampled at unchanged alpha");
 blend.update(3,changed,.01,10);check(close(blend.progress(),.6),"rate change advances existing progress");
 blend.update(3,changed,.04,10);check(!blend.active()&&blend.pose()->root[1]==200,"new rate completes without restart");

 blend.reset();blend.update(1,a,0);auto opposite=sample(0,{0,0,0,-1});blend.update(2,opposite,0);
 for(unsigned i=0;i<10;++i){const auto& pose=blend.update(2,opposite,.01);check(close(std::abs(pose.rotations.at(1)[3]),1),"antipodal signs use shortest zero-angle route");normalized(pose);}
 auto almost=a;almost.rotations[1]={0,0,1e-7f,1};blend.update(3,almost,0);blend.update(3,almost,.1);normalized(*blend.pose());
 blend.reset();blend.update(1,a,0);
 const double radians=350*3.14159265358979323846/180;
 auto wrap=sample(0,{0,0,static_cast<float>(std::sin(radians/2)),static_cast<float>(std::cos(radians/2))});
 blend.update(2,wrap,0);const auto wrapped=blend.update(2,wrap,.1).rotations.at(1);
 check(close(wrapped[2],std::sin(-2.5*3.14159265358979323846/180))&&wrapped[3]>0,"350 degree target follows the short negative arc");

 // Reject atomically during a transition, including a different invalid key.
 blend.reset();blend.update(1,a,0);blend.update(2,b,0);blend.update(2,b,.05);
 const auto stable=*blend.pose();const auto alpha=blend.progress();
 const float nan=std::numeric_limits<float>::quiet_NaN();
 auto reject=[&](uint64_t key,const MotionPose& pose,double dt,float rate){
  const auto& result=blend.update(key,pose,dt,rate);
  check(!blend.last_update_accepted()&&equal(result,stable)&&blend.progress()==alpha&&blend.active(),"invalid update preserves complete transition state");
 };
 reject(0,b,.1,5);reject(3,b,-.1,5);reject(3,b,std::numeric_limits<double>::infinity(),5);
 reject(3,b,0,0);reject(3,b,0,-1);reject(3,b,0,nan);
 auto bad=b;bad.rootBone=2;reject(3,bad,.1,5);
 bad=b;bad.rotations.erase(2);reject(3,bad,.1,5);
 bad=b;bad.rotations[3]={0,0,0,1};reject(3,bad,.1,5);
 bad=b;bad.rotations[1]={0,0,0,0};reject(3,bad,.1,5);
 bad=b;bad.rotations[1][0]=nan;reject(3,bad,.1,5);
 bad=b;bad.root[0]=nan;reject(3,bad,.1,5);
 bad=b;bad.root[0]=1000001;reject(3,bad,.1,5);
 blend.update(2,b,.05);check(close(blend.progress(),.5)&&close(blend.pose()->root[1],50),"rejected key did not replace active key or frozen source");
 MotionBlend initial;throws([&]{initial.update(0,a,0);});throws([&]{initial.update(1,MotionPose{},0);});
 bad=a;bad.rotations[0]={0,0,0,1};throws([&]{initial.update(1,bad,0);});
 bad=a;bad.rootBone=3;throws([&]{initial.update(1,bad,0);});
 bad=a;for(uint32_t bone=3;bone<=129;++bone)bad.rotations[bone]={0,0,0,1};throws([&]{initial.update(1,bad,0);});
 check(!initial.pose()&&!initial.last_update_accepted(),"invalid first sample leaves no partial initialization");
 initial.update(1,a,0);initial.reset();check(!initial.pose()&&!initial.active(),"reset clears skeleton and display");
 auto other=a;other.rootBone=7;other.rotations={{7,{0,0,0,1}}};initial.update(1,other,0);check(initial.pose()->rootBone==7,"reset permits another complete skeleton");

 // A huge finite dt/rate must saturate without intermediate multiplication overflow.
 blend.reset();blend.update(1,a,0);blend.update(2,b,0);
 blend.update(2,b,std::numeric_limits<double>::max(),std::numeric_limits<float>::max());
 check(!blend.active()&&blend.progress()==1&&blend.pose()->root[1]==100,"huge finite input saturates safely");normalized(*blend.pose());
 auto hugeQuaternion=a;hugeQuaternion.rotations[1]={std::numeric_limits<float>::max(),0,0,0};
 blend.reset();blend.update(1,hugeQuaternion,0);normalized(*blend.pose());
 auto tinyQuaternion=a;tinyQuaternion.rotations[1]={std::numeric_limits<float>::denorm_min(),0,0,0};
 blend.reset();blend.update(1,tinyQuaternion,0);normalized(*blend.pose());

 // Integrate equal elapsed times using 30/60/144 fps including the partial last
 // frame; all rates advance alpha, not a frame-count-dependent smoothing factor.
 auto atTime=[&](unsigned fps,double elapsed){MotionBlend f;f.update(1,a,0);f.update(2,b,0);double time=0;
  while(time<elapsed){const double delta=std::min(1./fps,elapsed-time);time+=delta;f.update(2,b,delta,2);}
  return std::pair{*f.pose(),f.progress()};};
 auto reference=atTime(30,.175);
 for(unsigned fps:{30u,60u,144u}){auto result=atTime(fps,.175);check(close(result.second,.35)&&close(result.first.root[1],reference.first.root[1]),"equal elapsed time gives equal root and progress");
  for(unsigned k=0;k<4;++k)check(close(result.first.rotations.at(1)[k],reference.first.rotations.at(1)[k]),"equal elapsed time gives equal quaternion");
  check(atTime(fps,.5).second==1,"partitioned exact duration completes");}
 std::cout<<"MotionBlend: shortest slerp/normalization, live targets, interruption, rate/clock, atomic rejection, 30/60/144fps PASS\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
