#include "evade_travel_curve.h"
#include "player_motion.h"
#include <bit>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt;
using namespace mgo2mt::combat::evade_runtime;
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}}
int main(int argc,char**argv){try{
 check(argc==3,"evade_travel_curve_test evade.gwmot evade_travel.gwet");std::string error;check(configure(argv[2],error)==ResourceStatus::local_resource,"local travel curve present");
 auto profile=source_profile();const auto&roll_source_z=profile->roll;const auto&recover_source_z=profile->recover;const auto&travel_distances=profile->distances;
 std::ifstream f(argv[1],std::ios::binary);check(bool(f),"source bank present");std::vector<char> bytes{std::istreambuf_iterator<char>(f),{}};PlayerMotionBank bank(bytes);
 const auto* roll=bank.find(PlayerMotion::Roll);const auto* recover=bank.find(PlayerMotion::RollRecover);check(roll&&recover&&roll->sourceIndex==56&&roll->sourceKey==0x57bb63&&roll->frames==40&&recover->sourceIndex==57&&recover->sourceKey==0x52de74&&recover->frames==45,"reviewed original source identities");
 for(size_t i=0;i<roll_source_z.size();++i)check(std::bit_cast<uint32_t>(roll_source_z[i])==std::bit_cast<uint32_t>(roll->roots[i][2]),"roll raw float bits exact");
 for(size_t i=0;i<recover_source_z.size();++i)check(std::bit_cast<uint32_t>(recover_source_z[i])==std::bit_cast<uint32_t>(recover->roots[i][2]),"recover raw float bits exact");
 check(roll_source_z.back()-roll_source_z.front()==4038&&recover_source_z.back()-recover_source_z.front()==854,"source displacement distinct from native cap");
 float total=0;size_t n=1;for(const auto* c:{roll,recover})for(size_t i=1;i<c->roots.size();++i){float raw=c->roots[i][2]-c->roots[i-1][2];float delta=c==recover&&i>35?0:(std::min)(100.f,(std::max)(0.f,raw));total+=delta;check(travel_distances[n++]==total,"every native step equals bounded source delta");}
 check(total==3521.875f&&distance_seconds(40./60)==2667.875f,"exact capped displacement without catch-up");
 for(size_t i=1;i<travel_distances.size();++i)check(travel_distances[i]>=travel_distances[i-1]&&travel_distances[i]-travel_distances[i-1]<=100,"frame monotonic and speed cap");
 for(unsigned i=0;i<=10000;++i){const double t=1.25+double(i)/10000;check(distance_seconds(t)==total,"recovery last ten frames completely stationary");}
 check(distance_seconds(0)==0&&distance_seconds(-1)==0&&distance_seconds(std::numeric_limits<double>::quiet_NaN())==0&&distance_seconds(std::numeric_limits<double>::infinity())==0,"invalid time contract");check(distance_seconds(1e300)==total,"large finite clamps without overflow");
 for(unsigned fps:{30u,60u,144u,1000u}){double sum=0,previous=0;float at=0;for(unsigned i=1;previous<1.417;++i){const double next=(std::min)(1.417,double(i)/fps);float value=distance_seconds(next);check(value>=at&&double(value-at)<=6000*(next-previous)+.001,"subframe monotonic speed cap");sum+=double(value)-at;previous=next;at=value;}check(sum==total,"partition-independent cumulative distance");}
 const auto a=distance_seconds(.3123),b=distance_seconds(.8999),c=distance_seconds(1.417);check((double(b)-a)+(double(c)-b)==double(c)-a,"arbitrary partition telescopes");
 check(distance_seconds(std::nextafter(40./60,0.))<=distance_seconds(40./60)&&distance_seconds(std::nextafter(40./60,1.))>=distance_seconds(40./60),"phase boundary continuous");
 std::cout<<"Evade travel PASS: 87 raw root floats exact, capped monotone curve3521.875, stationary recovery, partition30/60/144/1000Hz\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
