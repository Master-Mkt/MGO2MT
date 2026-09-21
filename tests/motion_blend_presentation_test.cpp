#include "motion_blend_presentation.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace mgo2mt;
using namespace mgo2mt::motion_blend;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static bool near(double a,double b,double tolerance=1e-4){return std::abs(a-b)<=tolerance;}
static MotionPose pose(float height=0){MotionPose p;p.rootBone=11;p.root={10,height,30};p.rotations={{11,{0,0,0,1}},{22,{0,0,0,1}}};return p;}
static bool same(const MotionPose& a,const MotionPose& b){return a.rootBone==b.rootBone&&a.root==b.root&&a.rotations==b.rotations;}
static std::array<float,3> rotate(std::array<float,3> v,float yaw){const float s=std::sin(yaw),c=std::cos(yaw);return {c*v[0]+s*v[2],v[1],-s*v[0]+c*v[2]};}
static std::array<float,3> world(const MotionPose& p,std::array<float,3> feet,float yaw,std::array<float,3> bind){for(unsigned i=0;i<3;++i)bind[i]+=p.root[i];auto result=rotate(bind,yaw);for(unsigned i=0;i<3;++i)result[i]+=feet[i];return result;}
static std::array<float,4> yaw_rotation(std::array<float,4> q,float yaw){const float s=std::sin(yaw*.5f),c=std::cos(yaw*.5f);return {c*q[0]+s*q[2],c*q[1]+s*q[3],c*q[2]-s*q[0],c*q[3]-s*q[1]};}
static void same_rotation(const std::array<float,4>& a,const std::array<float,4>& b){double dot=0;for(unsigned i=0;i<4;++i)dot+=double(a[i])*b[i];check(near(std::abs(dot),1.),"world orientation preserved up to quaternion sign");}

int main(){try{
 const Scope scope{123,4,567,8,99};const auto a=pose(0),b=pose(100),c=pose(-100);
 Lane lane;check(!lane.pose()&&!lane.active(),"empty lane");
 check(same(lane.sample(scope,1,0,a,0,5),a)&&lane.progress()==1&&lane.matches(scope),"first valid sample immediate and scope bound");
 check(same(lane.sample(scope,2,0,b,10,5),a)&&lane.progress()==0,"new source freezes previous picture without consuming dt");
 lane.sample(scope,2,.1,b,.1,5);check(near(lane.progress(),.5)&&near(lane.pose()->root[1],50),"same source progresses blend");
 const auto halfway=*lane.pose();lane.sample(scope,3,0,c,.1,5);
 check(same(*lane.pose(),halfway)&&lane.progress()==0,"retarget starts from last displayed blend");
 lane.sample(scope,3,.05,c,.05,5);check(near(lane.progress(),.25)&&near(lane.pose()->root[1],12.5),"retarget progresses from mixed pose");
 lane.sample(scope,3,.075,c,.025,10);check(near(lane.progress(),.5)&&near(lane.pose()->root[1],-25),"rate change retains progress and frozen source");
 lane.sample(scope,3,.125,c,.05,10);check(lane.progress()==1&&!lane.active()&&same(*lane.pose(),c),"new rate completes normally");

 lane.reset();check(!lane.pose()&&!lane.active()&&!lane.matches(scope),"explicit reset clears presentation and scope");
 lane.sample(scope,7,1,a,0,5);lane.sample(scope,8,2,b,0,5);lane.sample(scope,8,2.1,b,.1,5);
 const auto beforeRewind=*lane.pose();lane.sample(scope,8,0,c,.1,5);
 check(lane.progress()==0&&same(*lane.pose(),beforeRewind),"same-source clip rewind starts a fresh transition");
 lane.sample(scope,8,.1,c,.05,5);check(near(lane.progress(),.25),"rewound clip advances without per-frame restart");
 lane.sample(scope,8,.1-5e-7,c,.01,5);check(near(lane.progress(),.3),"sub-microsecond clip jitter does not retarget");

 // Every field participates: scene / actor slot / full identity / life / model generation.
 for(unsigned field=0;field<scope.size();++field){
  Lane identity;identity.sample(scope,1,0,a,0,5);identity.sample(scope,2,0,b,0,5);identity.sample(scope,2,.1,b,.1,5);
  auto changed=scope;++changed[field];auto newSkeleton=c;newSkeleton.rootBone=77;newSkeleton.rotations={{77,{0,0,0,1}}};
  check(same(identity.sample(changed,2,.2,newSkeleton,.1,5),newSkeleton)&&identity.progress()==1&&!identity.active(),"any scope field change drops old actor/life/model blend");
 }
 Lane other;other.sample(scope,1,0,c,0,5);const auto otherBefore=*other.pose();
 lane.sample(scope,9,0,a,0,5);lane.sample(scope,9,.1,a,.1,5);
 check(same(*other.pose(),otherBefore)&&other.progress()==1,"independent actor lanes never share progress or displayed pose");

 // Rejected samples must not poison Lane's scope/source/time/ticket metadata.
 const float nan=std::numeric_limits<float>::quiet_NaN();const double infinity=std::numeric_limits<double>::infinity();
 for(unsigned failure=0;failure<11;++failure){
  Lane actual;actual.sample(scope,1,0,a,0,5);actual.sample(scope,2,1,b,0,5);actual.sample(scope,2,1.1,b,.05,5);
  Lane expected=actual;auto bad=b;auto wrongScope=scope;auto proposedScope=scope;
  double time=10,dt=.1;float rate=5;uint64_t source=999;
  switch(failure){case 0:bad.root[0]=nan;break;case 1:dt=-1;break;case 2:dt=infinity;break;
   case 3:rate=0;break;case 4:rate=nan;break;case 5:time=-1;break;case 6:time=infinity;break;
   case 7:time=std::numeric_limits<double>::quiet_NaN();break;
   case 8:bad.rotations.erase(22);break;
   case 9:++wrongScope[3];proposedScope=wrongScope;bad.rotations[11]={0,0,0,0};break;case 10:source=0;break;}
  const auto before=*actual.pose();const auto progress=actual.progress();
  actual.sample(proposedScope,source,time,bad,dt,rate);
  check(same(*actual.pose(),before)&&actual.progress()==progress&&actual.matches(scope),"invalid sample keeps displayed state and progress even with a new scope/source");
  actual.sample(scope,2,1.2,b,.05,5);expected.sample(scope,2,1.2,b,.05,5);
  check(same(*actual.pose(),*expected.pose())&&actual.progress()==expected.progress(),"invalid sample cannot change clip-time, source, ticket or scope history");
 }
 for(unsigned failure=0;failure<4;++failure){
  Lane initial;auto bad=a;double time=0,dt=0;float rate=5;
  if(failure==0)bad.rotations.clear();if(failure==1)time=-1;if(failure==2)dt=infinity;if(failure==3)rate=0;
  bool rejected=false;try{initial.sample(scope,1,time,bad,dt,rate);}catch(const std::invalid_argument&){rejected=true;}
  check(rejected&&!initial.pose(),"invalid initial sample throws without partially initializing lane");
  check(same(initial.sample(scope,1,0,a,0,5),a),"valid initial sample works after rejection");
 }

 const std::array<float,3> from{200,300,-400},to{-250,50,600},bind{1,2,3};
 const float fromYaw=1.57079632679f,toYaw=-.78539816339f;
 auto original=pose(20);original.rotations[11]={.5f,.5f,.5f,.5f};
 Lane rebased;rebased.rebase(from,fromYaw,to,toYaw,bind);check(!rebased.pose(),"rebase before first pose is harmless");
 rebased.sample(scope,1,0,original,0,5);
 const auto oldWorld=world(*rebased.pose(),from,fromYaw,bind);const auto oldRotation=yaw_rotation(rebased.pose()->rotations.at(11),fromYaw);
 rebased.rebase(from,fromYaw,to,toYaw,bind);
 const auto newWorld=world(*rebased.pose(),to,toYaw,bind);
 for(unsigned i=0;i<3;++i)check(near(oldWorld[i],newWorld[i],.001),"coordinate rebase preserves world-space root position");
 same_rotation(oldRotation,yaw_rotation(rebased.pose()->rotations.at(11),toYaw));
 check(rebased.pose()->rotations.at(22)==original.rotations.at(22),"coordinate rebase changes root, not child local rotations");
 const auto beforeHandoff=*rebased.pose();rebased.sample(scope,2,0,b,.1,5);
 check(same(*rebased.pose(),beforeHandoff)&&rebased.progress()==0,"new animation handoff starts from rebased world picture");
 rebased.sample(scope,2,.05,b,.05,5);
 // Control emits an idle frame on evasion expiry. The held movement may turn
 // the body toward a rotated camera only on the following run frame.
 Lane evadeEnding;evadeEnding.sample(scope,19,.7,c,0,5);
 evadeEnding.sample(scope,1,0,a,.016,5);
 const auto endingWorld=world(*evadeEnding.pose(),from,0,bind);
 const auto endingRotation=yaw_rotation(evadeEnding.pose()->rotations.at(11),0);
 check(evadeEnding.same_source(1)&&!evadeEnding.same_source(3),"idle ending and resumed running are separate sources");
 if(!evadeEnding.same_source(3))evadeEnding.rebase(from,0,to,fromYaw,bind);
 evadeEnding.sample(scope,3,0,b,.016,5);
 const auto resumedWorld=world(*evadeEnding.pose(),to,fromYaw,bind);
 for(unsigned i=0;i<3;++i)check(near(endingWorld[i],resumedWorld[i],.001),"first run after evasion preserves the last world position despite a camera turn");
 same_rotation(endingRotation,yaw_rotation(evadeEnding.pose()->rotations.at(11),fromYaw));
 check(evadeEnding.progress()==0,"resumed movement starts at the rebased previous picture");
 evadeEnding.sample(scope,3,.05,b,.05,5);
 check(near(evadeEnding.progress(),.25),"resumed movement blend advances instead of restarting each frame");
 for(unsigned failure=0;failure<7;++failure){
  Lane actual=rebased,expected=rebased;auto badFrom=from,badTo=to,badBind=bind;float badFromYaw=fromYaw,badToYaw=toYaw;
  switch(failure){case 0:badFrom[0]=nan;break;case 1:badTo[1]=nan;break;case 2:badBind[2]=nan;break;
   case 3:badFromYaw=nan;break;case 4:badToYaw=std::numeric_limits<float>::infinity();break;
   case 5:badFrom[0]=1e8f;break;
   case 6:badFromYaw=(std::numeric_limits<float>::max)();badToYaw=-(std::numeric_limits<float>::max)();break;}
  const auto previous=*actual.pose();const auto progress=actual.progress();actual.rebase(badFrom,badFromYaw,badTo,badToYaw,badBind);
  check(actual.pose()&&same(*actual.pose(),previous)&&actual.progress()==progress&&actual.matches(scope),"NaN/oversized/overflowing rebase preserves existing display, progress and scope");
  actual.sample(scope,2,.1,b,.05,5);expected.sample(scope,2,.1,b,.05,5);
  check(same(*actual.pose(),*expected.pose())&&actual.progress()==expected.progress(),"invalid rebase preserves the frozen transition source for subsequent frames");
 }
 auto physical=local_physics_pose(original,fromYaw);
 same_rotation(yaw_rotation(physical.rotations.at(11),fromYaw),original.rotations.at(11));
 check(physical.root==original.root&&physical.rotations.at(22)==original.rotations.at(22),"physics yaw localization preserves translation and child rotations");
 std::cout<<"motion presentation scope/life/reset/rewind/retarget/rate/atomic rejection/rebase passed\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
