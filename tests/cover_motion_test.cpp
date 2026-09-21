#include "cover_motion.h"
#include "motion_blend_presentation.h"
#include <fstream>
#include <iostream>
#include <limits>
using namespace mgo2mt;
void check(bool x,const char* s){if(!x)throw std::runtime_error(s);}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"bad bank rejected");}
int main(int argc,char** argv){try{
 check(argc==2,"cover_motion_test cover.gwmot");std::ifstream in(argv[1],std::ios::binary);std::vector<char>b{std::istreambuf_iterator<char>(in),{}};cover::CoverMotionBank bank(b);check(bank.size()==23,"23 real source clips");
 size_t frames=0;
 for(uint32_t i=0;i<uint32_t(cover::Action::count);++i){auto action=cover::Action(i);auto d=cover::descriptor(action);auto start=bank.sample(action,0);check(start&&start->sourceIndex==d->sourceIndex&&start->sourceKey==d->sourceKey&&start->pose.rotations.size()==53,"verified clip identity");check(start->rootTravel==std::array<float,3>{},"relative root starts zero");
  for(uint32_t n=0;n<=d->frames;++n){auto p=bank.sample(action,double(n)/60);check(p&&p->pose.root[0]==0&&p->pose.root[2]==0&&p->rootTravel[1]==0,"physical XZ ownership retained");for(auto [key,q]:p->pose.rotations){float norm=0;for(auto v:q){check(std::isfinite(v),"finite quaternion");norm+=v*v;}check(std::abs(norm-1)<1e-5f,"normalized sampled quaternion");}++frames;}
  auto end=bank.sample(action,cover::duration(action)),late=bank.sample(action,1e300);check(end&&late,"large finite time stays bounded");if(d->loop)check(end->pose.root==start->pose.root,"loop boundary returns start");else check(end->pose.rotations==late->pose.rotations&&end->pose.root==late->pose.root,"nonloop clamps at source end");
  check(!bank.sample(action,-1)&&!bank.sample(action,std::numeric_limits<double>::quiet_NaN()),"invalid clock rejected");
 }
 check(!bank.sample(cover::Action::count,0)&&!cover::descriptor(cover::Action(999)),"unknown action unavailable");
 auto r=bank.sample(cover::Action::move_right,.89),l=bank.sample(cover::Action::move_left,.89);check(r->rootTravel[0]>500&&l->rootTravel[0]<-490,"true opposite slide directions before wrap");
 check(cover::duration(cover::Action::lean_enter)==.3&&cover::duration(cover::Action::lean_exit)==.3,"separate original18frame lean phases");
 auto aim=bank.sample(cover::Action::stand_right,0)->pose;auto left=cover::native_side_lean(aim,-1,1),right=cover::native_side_lean(aim,1,1),neutral=cover::native_side_lean(aim,0,1);check(left&&right&&neutral&&left->rotations!=right->rotations&&neutral->rotations==aim.rotations,"native digital side lean and neutral");
 for(auto [key,q]:aim.rotations)if(key!=0x6C02B2)check(left->rotations.at(key)==q&&right->rotations.at(key)==q,"native lean changes upper spine only; legs and root unchanged");
 check(!cover::native_side_lean(aim,2,1)&&!cover::native_side_lean(aim,1,-1)&&!cover::native_side_lean(aim,1,std::numeric_limits<float>::infinity()),"native lean invalid input rejected");
 motion_blend::Lane lane;motion_blend::Scope scope{1,2,3,4,5};auto p=*bank.sample(cover::Action::stand_right,0);auto base=lane.sample(scope,1,0,p.pose,0,5);
 for(auto action:{cover::Action::move_right,cover::Action::peek_right_enter,cover::Action::peek_right_hold,cover::Action::peek_right_exit,cover::Action::stand_right,cover::Action::lean_enter,cover::Action::lean_hold,cover::Action::lean_exit}){
  auto next=*bank.sample(action,0);auto shown=lane.sample(scope,uint32_t(action)+1,0,next.pose,.01,5);check(shown.root==base.root&&shown.rotations==base.rotations&&lane.progress()==0,"each phase begins from displayed pose without pop");auto half=lane.sample(scope,uint32_t(action)+1,.1,bank.sample(action,.1)->pose,.1,5);check(std::abs(lane.progress()-.5)<1e-6,"phase blends at configured rate");base=half;
 }
 auto stale=*lane.pose();scope[3]++;auto fresh=bank.sample(cover::Action::crouch_right,0)->pose;lane.sample(scope,11,0,fresh,0,5);check(lane.matches(scope)&&!lane.active(),"full identity/life scope resets pose lane");
 auto damaged=b;damaged[0]='X';rejects([&]{cover::CoverMotionBank x(damaged);});damaged=b;damaged[8]=1;rejects([&]{cover::CoverMotionBank x(damaged);});damaged=b;damaged[12]=2;rejects([&]{cover::CoverMotionBank x(damaged);});damaged=b;damaged[16]=char(127);rejects([&]{cover::CoverMotionBank x(damaged);});damaged=b;damaged[36]^=1;rejects([&]{cover::CoverMotionBank x(damaged);});damaged=b;damaged.pop_back();rejects([&]{cover::CoverMotionBank x(damaged);});damaged=b;damaged.push_back(0);rejects([&]{cover::CoverMotionBank x(damaged);});
 std::cout<<"cover motion PASS:23 source identities, "<<frames<<" samples x53 bones, loop/clamp/travel, phase blending, scope, corrupt bank\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

