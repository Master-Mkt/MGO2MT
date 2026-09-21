#include "product_identity.h"
#include "stage_round.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <stdexcept>
namespace mgo2mt::stage {
Round Round::read(std::istream&in){
 auto check=[](bool ok){if(!ok)throw std::runtime_error("Invalid stage placements");};
 Round r;std::string magic;unsigned version,count;check(bool(in>>magic>>version>>count));check(magic==mgo2mt::brand::Format{"MGO2MT.STAGE_PLACEMENTS"}&&version==1&&count<=4096);std::set<uint32_t>keys;
 for(unsigned i=0;i<count;++i){Placement p;check(bool(in>>p.key>>p.model>>p.group>>p.position[0]>>p.position[1]>>p.position[2]>>p.yaw));check(p.key&&keys.insert(p.key).second&&p.model<6&&p.group<=1&&(!p.group||p.model<2));for(float x:p.position)check(std::isfinite(x)&&std::abs(x)<1000000);check(std::isfinite(p.yaw)&&std::abs(p.yaw)<=360);r.objects.push_back(p);}
 std::string tail;check(!(in>>tail));return r;
}
Round Round::reset(uint64_t newSeed)const{
 Round r=*this;r.seed=newSeed;r.transientLights.clear();std::vector<size_t>ids;std::vector<unsigned>models;
 for(size_t i=0;i<r.objects.size();++i)if(r.objects[i].group){ids.push_back(i);models.push_back(r.objects[i].model);}
 // Native diagnostic policy: exchange car variants only at existing car anchors.
 // Do not move doors/locker pieces or claim this is the original actor RNG.
 uint64_t state=newSeed;auto random=[&](){state+=0x9e3779b97f4a7c15ull;auto z=state;z=(z^(z>>30))*0xbf58476d1ce4e5b9ull;z=(z^(z>>27))*0x94d049bb133111ebull;return z^(z>>31);};
 for(size_t i=models.size();i>1;--i)std::swap(models[i-1],models[size_t(random()%i)]);
 for(size_t i=0;i<ids.size();++i)r.objects[ids[i]].model=models[i];return r;
}
}
