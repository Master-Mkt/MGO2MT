#include "original_lock_policy.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt::original_lock;
static void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){try{
 const auto p=ak102_parameters(25,0,0,1);check(bool(p),"AK102 current parameter with explicit Surveyor1");
 check(ak102_parameters(25,0,0,0)->acquire.range==7200,"online absent Surveyor still multiplies by 0.9");
 check(ak102_parameters(25,0,0,2)->acquire.range==9600&&ak102_parameters(25,0,0,3)->acquire.range==11200,"Surveyor2 and3 original multipliers");
 check(!ak102_parameters(25,0,0,4),"unknown skill level rejected");
 check(!ak102_parameters(25,0,std::numeric_limits<float>::max(),0)&&
  !ak102_parameters(25,0,-std::numeric_limits<float>::max(),0),"finite modifier intermediate overflow rejected before clamp");
 check(p->acquire.range==8000&&p->retain.range==9000&&p->crosshair.range==8000,"three separate ranges");
 check(p->acquire.width==500&&p->retain.width==500,"width retained");
 check(std::bit_cast<uint32_t>(p->acquire.yaw)==0x3db2b8c4u&&
  p->retain.yaw==2*p->acquire.yaw&&p->retain.pitch==p->acquire.pitch,"current angle bits and retention");
 check(ak102_parameters(25,range_bonus_actor_flag,0,1)->acquire.range==10000,"original actor flag range multiplier");
 check(ak102_parameters(25,range_bonus_actor_flag,1,1)->acquire.range==5000,"bonus before environment factor");
 check(ak102_parameters(25,0,1,1)->acquire.range==4000,"modifier one");
 check(ak102_parameters(25,0,2,1)->acquire.range==3000,"minimum range");
 check(ak102_parameters(25,0,.5f,1)->acquire.range==6000,"fractional modifier");
 check(ak102_parameters(25,0,1,1)->crosshair.range==8000,"crosshair ignores acquisition modifier");
 check(!ak102_parameters(24,0,0,1)&&!ak102_parameters(0,0,0,1),"no other weapon fallback");
 check(!ak102_parameters(25,0,std::numeric_limits<float>::quiet_NaN(),1)&&
  !ak102_parameters(25,0,std::numeric_limits<float>::infinity(),1),"unknown modifier rejected");
 const auto& a=p->acquire;const int yaw=angular_units(a.yaw),pitch=angular_units(a.pitch);
 check(accepts_quantized(a,8000,500,0,{pitch,pitch+1,yaw+1}),"range width inclusive and inner yaw bypass");
 check(!accepts_quantized(a,8001,0,8001,{}),"range beyond rejected");
 check(!accepts_quantized(a,100,0,-1,{}),"behind rejected");
 check(!accepts_quantized(a,0,0,0,{}),"zero distance rejected");
 check(accepts_quantized(a,1000,501,100,{-pitch-1,-pitch,-yaw}),"outer negative bounds inclusive");
 check(accepts_quantized(a,1000,501,100,{pitch+1,pitch,yaw}),"outer positive bounds inclusive");
 check(!accepts_quantized(a,1000,501,100,{0,0,yaw+1}),"outer yaw one quantum rejected");
 check(!accepts_quantized(a,1000,-501,100,{0,-pitch-1,0}),"outer pitch one quantum rejected");
 check(!accepts_quantized(a,1000,500,100,{pitch+1,0,0}),"inner pitch required");
 check(evaluate_local(a,{500,0,1}).accepted,"half-width slab allows close lateral target");
 check(!evaluate_local(a,{600,0,1}).accepted,"outside width tests adjusted yaw");
 check(evaluate_local(a,{550,0,1000}).accepted,"width-adjusted yaw accepts");
 check(!evaluate_local(a,{700,0,1000}).accepted,"width-adjusted yaw rejects");
 check(evaluate_local(p->retain,{700,0,2000}).accepted,"retention angle survives acquisition edge");
 check(!evaluate_local(a,{500,2000,1}).accepted,"inner slab still enforces vertical angle");
 check(evaluate_local(a,{0,1000,1000}).accepted&&evaluate_local(a,{0,-1000,1000}).accepted,"up/down symmetry");
 check(!evaluate_local(a,{0,3000,1000}).accepted,"vertical cap rejects");
 check(!evaluate_local(a,{0,0,-1}).accepted&&!evaluate_local(a,{0,0,0}).accepted,"invalid facing/separation");
 check(!evaluate_local(a,{std::numeric_limits<float>::infinity(),0,1}).accepted,"nonfinite coordinate");
 check(prefer_mode0(2000,{})&&prefer_mode0(1000,2000.f)&&!prefer_mode0(2000,2000.f)&&
  !prefer_mode0(3000,2000.f),"nearest distance and stable equal score");
 std::cout<<"PASS original AK102 parameters, range modifiers, width/angle boundaries, acquisition/retention, mode0 ranking\n";
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
