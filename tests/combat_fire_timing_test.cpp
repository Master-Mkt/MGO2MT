#include "combat_service.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt;
using namespace mgo2mt::combat;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Match {
 Identity id{1,1,100};Service host{1};uint32_t sequence=0;
 explicit Match(bool automatic=true){
  auto floor=std::make_shared<const stage::Collision>(stage::Collision::make({{-20000,0,-20000},{20000,0,-20000},{20000,0,20000},{-20000,0,20000}},{{{0,1,2}},{{0,2,3}}}));
  // Only the fire clock is original; all other values are synthetic.
  Weapon gun{23,1,0,0,300,1000,40,10000,1,0,automatic};gun.fireIntervalTicks=original::ak102_fire_ticks;
  host.configure(floor,std::array{gun});check(host.admit(id),"admit");
  check(host.authority().join(id,1,{{0,2,0}},100,100,std::array<uint16_t,1>{23},0),"grant");
  host.authority().active(true);check(host.receive(id,wire::encode(wire::Accept{1}),0),"accept");host.deliveries();
 }
 void input(uint64_t ns,bool held=true,bool pressed=false){check(host.receive(id,wire::encode(wire::Input{1,++sequence,{{0,2,0}},23,held,false,pressed}),ns/1000000),"input");}
 void tick(uint64_t ns){host.poll(ns/1000000,uint32_t(ns%1000000));host.deliveries();}
 uint16_t ammo(){return host.authority().snapshot().players[1]->ammo;}
};
}
int main(){try{
 check(original::fire_interval_ns(30)==100100000,"30 original ticks are exactly 100.1 ms under nominal policy");
 check(original::fire_interval_ns(1)==3336667&&!original::fire_interval_ns(0)&&!original::fire_interval_ns(17983),"bounded upward ns rounding");
 constexpr uint64_t step=100100000,start=123456;
 Match m;
 for(unsigned i=0;i<400;++i){const auto time=start+i*step;m.input(time);
  if(i){m.tick(time-1);check(m.ammo()==1000-i,"one nanosecond early is rejected");}
  m.tick(time);check(m.ammo()==999-i,"fractional millisecond boundary fires without accumulated ms rounding");
  m.tick(time);check(m.ammo()==999-i,"same timestamp cannot repeat a shot");
 }
 Match delayed;delayed.input(0);delayed.tick(0);delayed.input(490000000);delayed.tick(490000000);check(delayed.ammo()==998,"stall produces only one shot");
 delayed.tick(490000001);delayed.tick(590099999);check(delayed.ammo()==998,"next interval anchors to actual delayed shot");delayed.tick(590100000);check(delayed.ammo()==997,"full interval follows delayed shot");
 delayed.tick(990000000);check(delayed.ammo()==997,"stale input stops automatic fire");
 Match semi(false);semi.input(0,true,true);semi.tick(0);semi.input(50000000,false,true);semi.tick(50000000);semi.tick(step-1);check(semi.ammo()==999,"semi short tap held through fractional interval");semi.tick(step);semi.tick(step*2);check(semi.ammo()==998,"semi short tap consumes exactly once");
 Match precision;precision.input(500000);precision.tick(500000);
 auto before=precision.host.authority().snapshot();
 check(precision.host.authority().fire(precision.id,{1,2,23,{0,0,1}},0,499999).reject==Reject::clock,"sub-ms clock regression rejected");
 check(precision.host.authority().fire(precision.id,{1,3,23,{0,0,1}},1,1000000).reject==Reject::clock&&precision.host.authority().snapshot()==before,"invalid fractional time is atomic");
 for(uint32_t ticks:{0u,17983u,std::numeric_limits<uint32_t>::max()}){
  Weapon bad{23,1,0,0,300,10,10,10000};bad.fireIntervalTicks=ticks;
  bool rejected=false;try{Authority a;auto f=std::make_shared<const stage::Collision>(stage::Collision::make({{0,0,0},{1,0,0},{0,0,1}},{{{0,1,2}}}));a.begin(1,f,std::array{bad});}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid original interval rejected");
 }
 std::cout<<"Original fire clock: 400 fractional intervals, exact boundary, no catch-up, semi tap and invalid clocks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
