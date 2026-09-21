#include "gekko_traversal_motion.h"
#include "character_catalog.h"
#include "motion_blend_presentation.h"
#include <fstream>
#include <iostream>
#include <limits>
using namespace mgo2mt;
using namespace mgo2mt::special_pc;
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
std::vector<char> read(const char* p){std::ifstream f(p,std::ios::binary);check(bool(f),"asset missing");return {(std::istreambuf_iterator<char>(f)),{}};}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"invalid bank must reject");}
float length(std::array<float,3>a,std::array<float,3>b){float n=0;for(size_t i=0;i<3;++i)n+=(a[i]-b[i])*(a[i]-b[i]);return std::sqrt(n);}
}
int main(int argc,char**argv){try{
 check(argc==4,"gekko_traversal_motion_test GKT1 GKG1 GWC");auto bytes=read(argv[1]);GekkoTraversalMotionBank traversal(bytes);GekkoMotionBank base(read(argv[2]));CharacterCatalog catalog(read(argv[3]));auto body=catalog.assemble({});check(traversal.size()==5&&body.ready(),"five reviewed candidates and original model");size_t samples=0;
 auto verify=[&](const GekkoSample& s){
  check(s.pose.root==std::array<float,3>{0,3160.f-gekko_model_feet_offset,0}&&s.authoredAirHeight==0,"physical world motion never applied twice");
  check(s.pose.rotations.size()==56,"source track set preserved");auto p=catalog.complete_pose(0,s.pose);check(p.rotations.size()==59,"complete original rig");
  const auto q=p.rotations.at(p.rootBone);const double yaw=std::atan2(2.*(double(q[3])*q[1]+double(q[0])*q[2]),1.-2.*(double(q[0])*q[0]+double(q[1])*q[1]));check(std::abs(yaw)<1e-5,"HOST body yaw only");
  catalog.pose(body,p);const auto bones=catalog.skeleton(0);
  for(const auto& b:bones){auto v=body.bone_position(b.key);check(bool(v),"posed original bone");if(b.parent>=0)check(std::abs(length(b.position,bones[b.parent].position)-length(*v,*body.bone_position(bones[b.parent].key)))<.5f,"bone lengths invariant");}
  for(const auto& v:body.model.vertices)check(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::abs(v.x)<20000&&std::abs(v.y)<20000&&std::abs(v.z)<20000,"all visible skin finite and bounded");++samples;
 };
 for(uint32_t i=0;i<5;++i){const auto& d=gekko_traversal_clips[i];for(uint32_t f=0;f<=d.frames;++f){auto p=traversal.sample_candidate(i,double(f)/60);check(p&&p->sourceIndex==d.sourceIndex&&p->sourceKey==d.sourceKey,"original candidate identity");verify(*p);}}
 for(uint32_t ms=0;ms<=2600;ms+=10){auto p=traversal.sample_climb(base,double(ms)/1000);check(p&&p->phase==(ms<1600?0u:ms<2400?1u:2u)&&p->sourceIndex==(ms<1600?7u:63u),"HOST-aligned rise/cross/settle");verify(*p);}
 auto cross=traversal.sample_climb(base,1.6),settle=traversal.sample_climb(base,2.4),end=traversal.sample_climb(base,2.6),late=traversal.sample_climb(base,1e300);
 check(traversal.sample_climb(base,std::nextafter(1.6,0.))->sourceIndex==7,"last representable rise sample cannot select landing");
 check(cross->phaseSeconds==0&&std::abs(settle->phaseSeconds-.7)<1e-12&&std::abs(end->phaseSeconds-.9)<1e-12,"native retiming samples exact source frame ranges");check(end->pose.rotations==late->pose.rotations,"huge finite time clamps");
 for(double bad:{-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})check(!traversal.sample_climb(base,bad)&&!traversal.sample_candidate(0,bad),"invalid time rejected");check(!traversal.sample_candidate(5,0),"unknown slot rejected");
 motion_blend::Lane lane;motion_blend::Scope scope{1,2,3,4,5};auto idle=base.sample_detail(GekkoMotion::idle,0);lane.sample(scope,idle->sourceKey,0,catalog.complete_pose(0,idle->pose),0,5);
 for(double t:{0.,1.6,2.4}){auto old=*lane.pose();auto p=*traversal.sample_climb(base,t);const uint64_t key=(uint64_t(p.sourceKey)<<8)|p.phase;auto target=catalog.complete_pose(0,p.pose);auto shown=lane.sample(scope,key,p.phaseSeconds,target,.1,5);check(shown.root==old.root&&shown.rotations==old.rotations&&lane.progress()==0,"all phases preserve preceding displayed pose");lane.sample(scope,key,p.phaseSeconds+.1,target,.1,5);check(lane.progress()==.5f,"F12 blend rate applies");lane.sample(scope,key,p.phaseSeconds+.2,target,.025,10);check(lane.progress()==.75f,"rate change progresses without resetting");}
 for(size_t i:{size_t(0),size_t(4),size_t(8),size_t(12),size_t(16),size_t(36)}){auto bad=bytes;bad[i]^=1;rejects([&]{GekkoTraversalMotionBank x(bad);});}auto bad=bytes;bad.pop_back();rejects([&]{GekkoTraversalMotionBank x(bad);});bad=bytes;bad.push_back(0);rejects([&]{GekkoTraversalMotionBank x(bad);});
 std::cout<<"GKT1 PASS: "<<samples<<" original poses, root/yaw removal, 59-bone skin, HOST phase clock, three blends, malformed banks\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
