#include "player_motion.h"
#include "original_reload_timing.h"
#include "character_catalog.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2win;
namespace {
void check(bool v,const char*why){if(!v)throw std::runtime_error(why);}
void u(std::vector<char>&b,uint32_t v){for(unsigned i=0;i<4;++i)b.push_back(char(v>>(8*i)));}
void f(std::vector<char>&b,float v){uint32_t bits;std::memcpy(&bits,&v,4);u(b,bits);}
void put(std::vector<char>&b,size_t at,uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=char(v>>(8*i));}
std::vector<char> read(const char*p){std::ifstream in(p,std::ios::binary);check(bool(in),"open motion fixture");return {(std::istreambuf_iterator<char>(in)),{}};}
std::vector<char> fixture(bool loop=true){
 std::vector<char>b{'G','W','T','1'};u(b,1);u(b,1);
 for(auto n:{0u,42u,9u,2u,60u,loop?1u:0u,1u,7u})u(b,n);
 for(unsigned t=0;t<3;++t){f(b,float(t)*500);f(b,100.f+t*10);f(b,float(t)*-300);}
 u(b,7);for(unsigned t=0;t<3;++t){f(b,0);f(b,0);f(b,0);f(b,t==1?-1.f:1.f);}return b;
}
void reject(const std::vector<char>&b){bool threw=false;try{PlayerMotionBank bank(b);}catch(const std::runtime_error&){threw=true;}check(threw,"malformed bank accepted");}
}
int main(int argc,char**argv){try{
 auto b=fixture();PlayerMotionBank bank(b);check(bank.size()==1&&bank.has(PlayerMotion::Idle)&&!bank.has(PlayerMotion::Walk),"bank action presence");check(!bank.sample(PlayerMotion::Walk,0),"missing action is explicit");
 auto pose=*bank.sample(PlayerMotion::Idle,.5/60);check(pose.root==std::array<float,3>{0,105,0}&&pose.rootBone==7,"vertical interpolation and no doubled world travel");check(std::abs(pose.rotations.at(7)[3]-1)<.00001f,"antipodal quaternion interpolation");
 check(bank.sample(PlayerMotion::Idle,2./60)->root[1]==100,"loop wraps");PlayerMotionBank once(fixture(false));check(once.sample(PlayerMotion::Idle,1000)->root[1]==120,"one-shot clamps");
 for(double time:{-1.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()})check(bank.sample(PlayerMotion::Idle,time)->root[1]==100,"invalid time is neutral");
 check(std::isfinite(bank.sample(PlayerMotion::Idle,std::numeric_limits<double>::max())->root[1]),"large time stays finite");
 for(size_t size=0;size<b.size();++size)reject({b.begin(),b.begin()+size});auto mutate=[&](size_t at,uint32_t value){auto bad=b;put(bad,at,value);reject(bad);};
 mutate(0,0);mutate(4,2);mutate(8,0);mutate(8,uint32_t(PlayerMotion::Count)+1);mutate(12,uint32_t(PlayerMotion::Count));mutate(16,0);mutate(20,4096);mutate(24,3601);mutate(28,120);mutate(32,2);mutate(36,129);mutate(40,8);mutate(44,0x7fc00000);mutate(80,0);mutate(96,0);
 auto trailing=b;trailing.push_back(0);reject(trailing);auto duplicate=b;put(duplicate,8,2);duplicate.insert(duplicate.end(),b.begin()+12,b.end());reject(duplicate);
 if(argc>=2){auto original=read(argv[1]);PlayerMotionBank restored(original);check(restored.size()==15,"reviewed motion coverage");check(restored.find(PlayerMotion::Walk)->sourceKey==0x4c078d&&restored.find(PlayerMotion::Run)->sourceKey==0x460c5,"source clip identity");check(restored.find(PlayerMotion::Reload)->frames==210,"reload duration");
  auto reloadClip=restored.find(PlayerMotion::Reload);check(reloadClip->sourceKey==0x7c56c7&&reloadClip->sourceIndex==3,"reviewed reload archive selector");
  auto endPose=restored.sample(PlayerMotion::Reload,100);check(endPose->root[1]==reloadClip->roots[209][1],"reload holds original end interval instead of stored trailing sample");
  auto halfPose=restored.sample(PlayerMotion::Reload,100.5/original::nominal_motion_fps);check(std::abs(halfPose->root[1]-(reloadClip->roots[100][1]+reloadClip->roots[101][1])*.5f)<.001f,"reload samples at original nominal clock");
  for(unsigned i=0;i<15;++i){auto action=PlayerMotion(i);for(double t:{0.,.137,.62,8.}){auto sampled=restored.sample(action,t);check(sampled&&sampled->rotations.size()==restored.find(action)->tracks.size(),"complete restored pose");for(const auto&[key,q]:sampled->rotations){float sum=0;for(float v:q){check(std::isfinite(v),"finite sampled quaternion");sum+=v*v;}check(std::abs(sum-1)<.0001f,"normalized sampled quaternion");}}}
  if(argc>=3){CharacterCatalog catalog(read(argv[2]));for(unsigned gender=0;gender<2;++gender){std::array<uint8_t,28>a{};a[0]=uint8_t(gender);a[2]=11;a[3]=22;a[15]=46;a[17]=57;auto body=catalog.assemble(a);check(body.ready(),"assembled motion body");auto indices=body.model.indices;auto imageCount=body.model.textures.size();float standingHeight=0,proneHeight=0;
    for(unsigned i=0;i<15;++i)for(double t:{0.,.137,.62,8.}){catalog.pose(body,*restored.sample(PlayerMotion(i),t));float top=-1e9;for(const auto&v:body.model.vertices){check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::abs(v.x)<4000&&std::abs(v.y)<4000&&std::abs(v.z)<4000,"finite bounded full-body pose");top=std::max(top,v.y);}if(i==0&&t==0)standingHeight=top;if(i==unsigned(PlayerMotion::ProneIdle)&&t==0)proneHeight=top;check(indices==body.model.indices&&imageCount==body.model.textures.size(),"pose preserves geometry and textures");}
    check(standingHeight>1500&&proneHeight<standingHeight*.5,"original prone body height");
   }}
 }
 std::cout<<"Player motion interpolation, in-place root, clips, skinning and rejection passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
