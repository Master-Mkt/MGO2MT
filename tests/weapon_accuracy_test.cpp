#include "weapon_accuracy.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2win::weapon_accuracy;
namespace {void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}}
int main(){try{
 check(valid(native_ak)&&!valid({4,3,1,1})&&!valid({0,100001,1,1})&&!valid({0,3,0,1})&&!valid({0,3,1,0}),"bounded native policy");check(mix(0)==0xe220a8397b1dcdafull,"fixed HOST mixer golden");
 State s;check(s.milliradians(native_ak,0)==3&&s.shots()==0,"initial 3mrad native baseline");auto first=s.direction(native_ak,0,{0,0,1},123);check(first.has_value(),"valid first cone sample");check(s.accepted(native_ak,0)&&s.milliradians(native_ak,0)==7&&s.milliradians(native_ak,100)==6,"only acceptance adds spread; linear 1.2mrad recovery per100ms");check(s.direction(native_ak,0,{0,0,1},123)!=first,"accepted shot advances deterministic sample");check(s.milliradians(native_ak,334)==3,"complete time recovery");
 check(s.accepted(native_ak,100),"second acceptance");auto frozen=s;check(!s.accepted(native_ak,99)&&!s.radians(native_ak,99)&&s.shots()==frozen.shots()&&s.direction(native_ak,100,{0,0,1},123)==frozen.direction(native_ak,100,{0,0,1},123),"backward time cannot change cone or RNG");
 for(unsigned i=0;i<100;++i)check(s.accepted(native_ak,100),"bounded accumulated shot");check(s.milliradians(native_ak,100)==30&&s.milliradians(native_ak,UINT64_MAX)==3,"30mrad ceiling and huge time recovery without overflow");check(!s.direction(native_ak,100,{0,0,0},1)&&!s.direction(native_ak,100,{NAN,0,1},1),"invalid direction rejected");
 double x=0,y=0,radius2=0;for(uint64_t i=0;i<10000;++i){auto ray=s.direction(native_ak,100,{0,0,1},i);check(ray.has_value(),"cone sample");const double length=double((*ray)[0])*(*ray)[0]+double((*ray)[1])*(*ray)[1]+double((*ray)[2])*(*ray)[2];check(std::abs(length-1)<2e-7&&(*ray)[2]>=std::cos(.03)-1e-7,"unit ray never escapes HOST cone");x+=(*ray)[0];y+=(*ray)[1];radius2+=double((*ray)[0])*(*ray)[0]+double((*ray)[1])*(*ray)[1];}check(std::abs(x/10000)<.0005&&std::abs(y/10000)<.0005&&radius2/10000>.00042&&radius2/10000<.00048,"cone covers disk without directional bias");
 for(auto aim:{Vec3{0,1,0},Vec3{0,-1,0},Vec3{1,0,0},Vec3{0,0,-1}})check(s.direction(native_ak,100,aim,1).has_value(),"vertical/reverse bases valid");s.reset();check(s.shots()==0&&s.direction(native_ak,0,{0,0,1},123)==first,"explicit scope reset returns first deterministic state");
 std::cout<<"Weapon accuracy PASS: native bounds, fixed RNG, accepted-only bloom, fractional recovery, cap, clock, scoped reset, unit cone distribution\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
