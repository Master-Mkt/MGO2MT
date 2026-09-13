#ifdef NDEBUG
#undef NDEBUG
#endif
#include "water_effects.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace mgo2win::stage;
int main(){
 WaterEffects effects;NavigationWaterState dry,wet{1000.f,1000.f,.65f,WaterFoot::inWater};Vec3 p{0,2,0};
 effects.update(1,1,p,wet,true,.016f);assert(effects.size()==0); // late join is not entry
 effects.update(1,1,p,wet,true,.016f);assert(effects.lines().empty());
 effects.reset();effects.update(1,1,p,dry,true,.016f);effects.update(1,1,p,wet,true,.016f);
 assert(effects.size()==4&&effects.lines().size()==15);
 for(const auto& line:effects.lines()){assert(line.alpha==1);if(!line.splash)assert(line.from[1]==1003&&line.to[1]==1003);}
 for(unsigned i=0;i<100;++i)effects.update(1,1,p,wet,true,.016f);assert(effects.size()==0); // stationary no loop splash
 p[2]=299;effects.update(1,1,p,wet,true,.016f);assert(effects.size()==0);
 p[2]=300;effects.update(1,1,p,wet,true,.016f);assert(effects.size()==4);
 effects.update(1,2,p,wet,true,.016f);assert(effects.size()==0);
 p[2]+=300;effects.update(1,2,p,wet,true,.2f);assert(effects.size()==4);
 effects.update(2,2,p,wet,true,.016f);assert(effects.size()==0);
 p[2]+=300;effects.update(2,2,p,wet,true,.2f);assert(effects.size()==4);
 effects.update(2,2,p,wet,false,.016f);assert(effects.size()==0);
 effects.update(2,2,p,wet,true,.016f);assert(effects.size()==0);
 p[2]+=3000;effects.update(2,2,p,wet,true,.016f);assert(effects.size()==0);
 auto raised=wet;raised.level=1100.f;effects.update(2,2,p,raised,true,.016f);assert(effects.size()==0);
 effects.update(2,2,p,wet,true,1);assert(effects.size()==0);
 effects.update(2,2,p,wet,true,.016f);assert(effects.size()==0);
 auto invalid=wet;invalid.depthAboveFloor=0;
 for(unsigned i=0;i<30;++i){p[2]+=300;effects.update(2,2,p,invalid,true,.2f);}assert(effects.size()==0);
 effects.update(2,2,p,dry,true,.016f);auto airborne=p;airborne[1]=1001;effects.update(2,2,airborne,wet,true,.016f);assert(effects.size()==0);
 effects.update(2,2,p,dry,true,.016f);effects.update(2,2,p,wet,true,.016f);assert(effects.size()==4);
 for(unsigned i=0;i<1000;++i){p[2]+=300;effects.update(2,2,p,wet,true,.02f);assert(effects.size()<=WaterEffects::maximumEffects);assert(effects.lines().size()<=WaterEffects::maximumEffects*12);}
 effects.update(2,2,{NAN,0,0},wet,true,.016f);assert(effects.size()==0);
 std::cout<<"water_effects: entry/movement, level geometry, bounded particles, stationary/latejoin/reset/pause/teleport suppression PASS\n";
}
