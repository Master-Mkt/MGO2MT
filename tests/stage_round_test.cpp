#include "stage_round.h"
#include <sstream>
#include <iostream>
#include <set>
using namespace mgo2win::stage;
static void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(){try{
 std::istringstream in("MGO2WIN.STAGE_PLACEMENTS 1 4\n1 0 1 10 20 30 0\n2 1 1 40 50 60 90\n3 0 1 70 80 90 180\n4 3 0 100 110 120 -90\n");auto original=Round::read(in);std::set<std::vector<unsigned>>arrangements;
 PointLight flash;flash.range=flash.extendedRange=10;flash.color={1,1,1};check(original.transientLights.add(flash,0,1),"round temporary light");
 for(unsigned seed=0;seed<64;++seed){auto r=original.reset(seed),same=original.reset(seed);check(r.transientLights.entries.empty(),"round reset clears temporary lights");unsigned cars=0;std::vector<unsigned>order;for(size_t i=0;i<r.objects.size();++i){auto&a=r.objects[i],&b=original.objects[i];check(a.position==b.position&&a.yaw==b.yaw&&a.key==b.key,"anchors and orientation retained");check(a.model==same.objects[i].model,"reproducible seed");if(a.group)cars+=a.model==0;else check(a.model==b.model,"doors remain paired and fixed");order.push_back(a.model);}check(cars==2,"variant count preserved");arrangements.insert(order);}check(arrangements.size()>1,"reset changes dynamic assignment");
 for(auto s:{"MGO2WIN.STAGE_PLACEMENTS 1 1 1 5 1 0 0 0 0","MGO2WIN.STAGE_PLACEMENTS 1 2 1 0 1 0 0 0 0 1 1 1 0 0 0 0","MGO2WIN.STAGE_PLACEMENTS 1 4097"}){std::istringstream bad(s);bool caught=false;try{Round::read(bad);}catch(...){caught=true;}check(caught,"bad group, duplicate anchor, oversized rejected");}
 std::cout<<"seeded car assignment, fixed anchors, preserved counts and invalid placement bounds passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
