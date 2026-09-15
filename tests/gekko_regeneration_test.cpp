#include "gekko_regeneration.h"
#include "special_pc.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2win::special_pc;
using namespace mgo2win::special_pc::regeneration;
namespace {void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}}
int main(){try{
 static_assert(native_gekko.hp==1000&&native_gekko.stamina==5000);
 Scope scope{8,3,22,1234,9};Policy policy;
 for(uint64_t step:{1,7,16,50,100,1000,30000}){
  regeneration::State state;uint32_t hp=state.advance(scope,policy,0,1,true);uint64_t now=0;
  while(now<30000){now=(std::min)(uint64_t(30000),now+step);hp=state.advance(scope,policy,now,hp,true);check(hp==((std::min)(1000u,1u+uint32_t(now/30))),"frequency-independent exact rational healing");}
  check(hp==1000&&!state.fraction(),"remaining one reaches full within thirty seconds");
 }
 regeneration::State edge;check(edge.advance(scope,policy,10,1,true)==1,"initial sample never instant heals");check(edge.advance(scope,policy,29979,1,true)==999,"one millisecond before full");check(edge.advance(scope,policy,29980,999,true)==1000,"exact final fractional HP");
 regeneration::State hits;uint32_t hp=hits.advance(scope,policy,0,1000,true);hp=hits.advance(scope,policy,100000,hp,true);hp-=1;
 check(hits.advance(scope,policy,100000,hp,true)==999,"time spent full cannot bank an instant heal");
 hp=hits.advance(scope,policy,100015,hp,true);check(hp==999&&hits.fraction()==15000,"half HP retained");hp-=499;
 hp=hits.advance(scope,policy,100029,hp,true);check(hp==500&&hits.fraction()==29000,"additional hit preserves accumulated fraction");hp=hits.advance(scope,policy,100030,hp,true);check(hp==501,"second hit does not restart regeneration");
 auto remainder=hits.fraction();check(hits.advance(scope,policy,99999,hp,true)==hp&&hits.fraction()==remainder,"clock rollback ignored without mutation");check(hits.advance(scope,policy,100060,hp,true)==502,"rollback cannot credit interval twice");
 hp=hits.advance(scope,policy,130015,501,true);check(hp==1000,"full within thirty seconds of subsequent hit");
 for(unsigned field=0;field<5;++field){regeneration::State state;state.advance(scope,policy,0,1,true);state.advance(scope,policy,29,1,true);auto next=scope;if(field==0)++next.epoch;if(field==1)++next.slot;if(field==2)++next.instance;if(field==3)++next.character;if(field==4)++next.life;
  check(state.advance(next,policy,30000,1,true)==1&&state.fraction()==0,"full scope change clears old time and fraction");}
 regeneration::State reset;reset.advance(scope,policy,0,1,true);check(reset.advance(scope,policy,1000,0,true)==0&&!reset.armed(),"dead cannot regenerate");check(reset.advance(scope,policy,2000,1,true)==1,"new living baseline cannot inherit dead time");
 check(reset.advance(scope,policy,3000,1,false)==1&&!reset.armed(),"human or inactive round clears regeneration");
 check(reset.advance({},policy,4000,1,true)==1&&!reset.armed(),"invalid identity resets");check(reset.advance(scope,{0},4000,1,true)==1&&!reset.armed(),"zero period rejected");check(reset.advance(scope,{30001},4000,1,true)==1&&!reset.armed(),"longer-than-thirty policy rejected");check(reset.advance(scope,policy,4000,1001,true)==1001&&!reset.armed(),"foreign maximum is not silently clamped");
 reset.advance(scope,policy,0,1,true);check(reset.advance(scope,policy,std::numeric_limits<uint64_t>::max(),1,true)==1000,"large host gap has bounded arithmetic");
 regeneration::State changed;changed.advance(scope,policy,0,1,true);changed.advance(scope,policy,15,1,true);check(changed.advance(scope,{15000},15,1,true)==1&&changed.fraction()==7500,"policy conversion retains fraction without instant heal");check(changed.advance(scope,{15000},23,1,true)==2,"new period applies prospectively");
 std::cout<<"Gekko HP1000 / thirty-second rational regeneration / damage, scope, death and clock boundaries PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
