#include "gekko_motion.h"
#include "character_catalog.h"
#include "motion_blend_presentation.h"
#include <fstream>
#include <iostream>
#include <limits>
using namespace mgo2mt;
using namespace mgo2mt::special_pc;
namespace {
void check(bool x,const char*s){if(!x)throw std::runtime_error(s);}
std::vector<char> read(const char*p){std::ifstream in(p,std::ios::binary);check(bool(in),"asset missing");return {(std::istreambuf_iterator<char>(in)),{}};}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"corrupt bank rejected");}
float length(std::array<float,3>a,std::array<float,3>b){float sum=0;for(unsigned k=0;k<3;++k)sum+=(a[k]-b[k])*(a[k]-b[k]);return std::sqrt(sum);}
}
int main(int argc,char**argv){try{
 check(argc==4,"gekko_motion_test gekko.gwmot gekko.gwc gekko_jump.gwjc");std::string curveError;check(configure_gekko_jump(argv[3],curveError)&&using_local_gekko_jump(),"local root compensation loaded");auto bytes=read(argv[1]);GekkoMotionBank bank(bytes);CharacterCatalog catalog(read(argv[2]));auto body=catalog.assemble({});check(body.ready()&&catalog.skeleton(0).size()==59&&bank.size()==7,"dedicated original Gekko rig and clips");
 check(duration(GekkoMotion::jump)==3.15&&duration(GekkoMotion::kick)==125./60,"native clock follows source sample count");
 size_t samples=0;
 for(auto action:{GekkoMotion::idle,GekkoMotion::walk,GekkoMotion::run,GekkoMotion::jump,GekkoMotion::kick}){
  const auto count=uint32_t(std::round(duration(action)*60));auto start=bank.sample_detail(action,0);check(start&&start->pose.rotations.size()==56,"56 original tracks retained");
  for(uint32_t f=0;f<=count;++f){double t=double(f)/60;auto p=bank.sample_detail(action,t);check(p&&p->pose.root[0]==0&&p->pose.root[2]==0,"source XZ does not move physical owner");
   auto complete=catalog.complete_pose(0,p->pose);check(complete.rotations.size()==59,"three absent tracks use canonical identity");catalog.pose(body,complete);
   for(auto [key,q]:complete.rotations){check(key!=0,"valid bone key");float n=0;for(float v:q){check(std::isfinite(v),"finite quaternion");n+=v*v;}check(std::abs(n-1)<1e-5,"normalized quaternion");}
   const auto bones=catalog.skeleton(0);for(const auto& b:bones){auto v=body.bone_position(b.key);check(bool(v),"all original bones present");for(float x:*v)check(std::isfinite(x)&&std::abs(x)<15000,"finite bounded posed bone");if(b.parent>=0)check(std::abs(length(b.position,bones[b.parent].position)-length(*v,*body.bone_position(bones[b.parent].key)))<.5f,"original bone lengths preserved");}
   for(const auto& v:body.model.vertices)check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::abs(v.x)<15000&&std::abs(v.y)<15000&&std::abs(v.z)<15000,"all skinned vertices remain bounded");
   if(action==GekkoMotion::jump)check(p->authoredAirHeight==gekko_jump_height_seconds(t),"same HOST jump trajectory at identical clock");++samples;
  }
  auto end=bank.sample(action,duration(action)),late=bank.sample(action,1e300);check(end&&late,"huge time safely clamped/wrapped");if(action==GekkoMotion::jump||action==GekkoMotion::kick)check(end->root==late->root&&end->rotations==late->rotations,"one-shot holds final pose");else check(end->root==start->pose.root&&end->rotations==start->pose.rotations,"loop restarts consistently");
  check(!bank.sample(action,-1)&&!bank.sample(action,std::numeric_limits<double>::quiet_NaN())&&!bank.sample(action,std::numeric_limits<double>::infinity()),"invalid clock rejected");
 }
 check(!bank.sample(GekkoMotion(5),0),"unknown native action rejected");
 for(const auto& [game,source]:std::array<std::array<double,2>,6>{{{0,0},{.6,.6},{2.029,(36.+29.5)/60},{3.458,95./60},{4.458,155./60},{5.024,95./60+1.566}}}){
  const auto a=bank.sample_gameplay(GekkoMotion::jump,game),b=bank.sample_detail(GekkoMotion::jump,source);
  if(a&&b&&(a->sourceKey!=b->sourceKey||std::abs(a->pose.root[1]-b->pose.root[1])>=.01f))std::cerr<<"retime game="<<game<<" source="<<source<<" keys="<<a->sourceKey<<','<<b->sourceKey<<" rootY="<<a->pose.root[1]<<','<<b->pose.root[1]<<" phase="<<a->phaseSeconds<<','<<b->phaseSeconds<<'\n';
  check(a&&b&&a->sourceKey==b->sourceKey&&std::abs(a->pose.root[1]-b->pose.root[1])<.01f,"native ten metre timing uses corresponding original root compensation");
  check(std::abs(a->phaseSeconds-b->phaseSeconds)<.001,"preparation and landing preserve original fps, air stretched");
 }
 check(bank.sample_gameplay(GekkoMotion::jump,3.457)->sourceIndex==7&&bank.sample_gameplay(GekkoMotion::jump,3.458)->sourceIndex==8,"physical landing and animation phase agree");
 auto before=bank.sample_detail(GekkoMotion::jump,35./60),air=bank.sample_detail(GekkoMotion::jump,36./60),land=bank.sample_detail(GekkoMotion::jump,95./60);
 check(before->sourceIndex==6&&air->sourceIndex==7&&land->sourceIndex==8&&air->phaseSeconds==0&&land->phaseSeconds==0,"three real source phase identities");
 check(gekko_jump_height_ms(0)==0&&gekko_jump_height_ms(3150)==0&&gekko_jump_height_ms(UINT32_MAX)==0&&gekko_jump_height_seconds(-1)==0&&gekko_jump_height_seconds(std::numeric_limits<double>::quiet_NaN())==0,"curve endpoints and invalid time");
 float high=0;for(uint32_t ms=0;ms<=3150;++ms){auto h=gekko_jump_height_ms(ms);check(h>=0&&h<=1100,"bounded native collider trajectory");high=(std::max)(high,h);}check(high>1000,"actual source airborne excursion retained");
 motion_blend::Lane lane;motion_blend::Scope scope{1,2,3,4,5};auto p=bank.sample_detail(GekkoMotion::idle,0);lane.sample(scope,p->sourceKey,0,catalog.complete_pose(0,p->pose),0,5);
 for(double seconds:{0.,.6,95./60}){auto old=*lane.pose();auto next=*bank.sample_detail(GekkoMotion::jump,seconds);auto shown=lane.sample(scope,next.sourceKey,next.phaseSeconds,catalog.complete_pose(0,next.pose),.1,5);check(shown.root==old.root&&shown.rotations==old.rotations&&lane.progress()==0,"phase switch preserves displayed pose at blend alpha zero");auto frozen=lane.sample(scope,next.sourceKey,next.phaseSeconds,catalog.complete_pose(0,next.pose),0,5);check(frozen.root==old.root&&frozen.rotations==old.rotations,"zero dt freeze");lane.sample(scope,next.sourceKey,next.phaseSeconds+.1,catalog.complete_pose(0,next.pose),.1,5);check(lane.progress()==.5f,"all phase transitions blend");}
 scope[3]++;p=bank.sample_detail(GekkoMotion::idle,0);lane.sample(scope,p->sourceKey,0,catalog.complete_pose(0,p->pose),0,5);check(lane.matches(scope)&&!lane.active(),"new life resets old trajectory/pose");
 for(size_t index:{size_t(0),size_t(4),size_t(8),size_t(12),size_t(36)}){auto bad=bytes;bad[index]^=1;rejects([&]{GekkoMotionBank x(bad);});}auto bad=bytes;bad.pop_back();rejects([&]{GekkoMotionBank x(bad);});bad=bytes;bad.push_back(0);rejects([&]{GekkoMotionBank x(bad);});
 std::cout<<"Gekko motion PASS: 7 original clips, "<<samples<<" poses x59 bones/17539 skin vertices, phase blend, HOST trajectory, malformed input\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
