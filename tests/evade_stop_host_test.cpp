#include "combat_service.h"
#include "evade_input.h"
#include "evade_runtime_profile.h"
#include "evade_travel_curve.h"
#include "player_control.h"
#include "stage_navigation.h"
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace mgo2mt;
namespace {
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
constexpr combat::Identity self{0,1,100};
constexpr uint64_t epoch=31,start=200;
constexpr float pi=3.14159265359f;
float horizontal(stage::Vec3 a,stage::Vec3 b){return std::hypot(a[0]-b[0],a[2]-b[2]);}
// Cross the internal diagonal of two coplanar triangles in all three roll directions.
std::shared_ptr<const stage::Collision> floor(){return std::make_shared<const stage::Collision>(stage::Collision::make({{-20000,0,-20000},{-20000,0,20000},{20000,0,20000},{20000,0,-20000}},{{{0,1,2}},{{0,2,3}}}));}
combat::Weapon gun(){combat::Weapon w;w.id=23;w.damage=100;w.intervalMs=100;w.reloadMs=500;w.magazine=30;w.reserve=60;w.range=20000;return w;}
struct Fixture {
 std::shared_ptr<const stage::Collision> world=floor();
 std::unique_ptr<combat::Service> host=std::make_unique<combat::Service>(epoch);
 stage::Navigation nav;player::Control control;player::EvadeInput request;
 std::array<float,24> values{};uint32_t sequence=0;combat::wire::Frame latest{};
 uint64_t latestAt=0;float maximumSpeed=0;unsigned resends=0;
 explicit Fixture(float startX=0){check(nav.place(*world,{startX,500,0}),"real navigation starts supported");host->configure(world,std::array{gun()});auto p=pose(0);check(host->authority().join(self,1,p,1000,1000,std::array<uint16_t,1>{23},0),"HOST spawn");host->authority().active(true);check(host->admit(self)&&host->receive(self,combat::wire::encode(combat::wire::Accept{epoch}),0),"native handshake");host->deliveries();request.scope(epoch,self,1);control.step(values,.005f,true,false,nav.grounded());
  values[16]=1;control.step(values,.1f,true,true,nav.grounded());nav.advance(*world,{control.forward,control.right,0,0,control.speed},.1f);send(100,0); // Real recent running evidence for forward roll.
 }
 combat::Pose pose(float yaw)const{return {nav.feet(),yaw,nav.pitch(),nav.capsule()};}
 void collect(uint64_t now){for(const auto&d:host->deliveries()){check(d.recipient==self&&d.payload.size()<=2000,"bounded self delivery");auto record=combat::wire::decode(d.payload);if(auto*f=std::get_if<combat::wire::Frame>(&record)){check(f->sop.recipient==self&&f->sop.life==1,"recipient full-scope ACK");latest=*f;latestAt=now;}}}
 void send(uint64_t now,float yaw){combat::wire::Input input;input.epoch=epoch;input.life=1;input.sequence=++sequence;input.pose=pose(yaw);input.weapon=23;request.apply(input);if(input.evadeRequest)++resends;
  check(host->receive(self,combat::wire::encode(input),now),"actual wire request received");host->poll(now);collect(now);auto snapshot=host->authority().snapshot();if(!(snapshot.players[0]&&snapshot.players[0]->pose==input.pose&&host->authority().sop_view(self)->inputSequence==sequence)){
   const auto&actual=*snapshot.players[0];auto movement=stage::movement_collision(world);auto delta=input.pose.feet;for(unsigned k=0;k<3;++k)delta[k]-=actual.pose.feet[k];auto hit=movement->sweep(actual.pose.feet,delta,input.pose.capsule);std::cerr<<"clear="<<movement->clear(input.pose.feet,input.pose.capsule)<<" sweep="<<(hit?hit->fraction:-1)<<" dy="<<delta[1]<<'\n';std::cerr<<"pose failure now="<<now<<" sequence="<<sequence<<" accepted="<<host->authority().sop_view(self)->inputSequence<<" requested=("<<input.pose.feet[0]<<','<<input.pose.feet[1]<<','<<input.pose.feet[2]<<") actual=("<<actual.pose.feet[0]<<','<<actual.pose.feet[1]<<','<<actual.pose.feet[2]<<") direct_reject="<<unsigned(host->authority().pose(self,epoch,sequence,input.pose,now))<<'\n';
   check(false,"every real navigation pose accepted without correction or silent speed rejection");
  }
 }
};
void run(combat::EvadeKind kind,unsigned interval,bool keepStick,bool lateAck){
 Fixture f(kind==combat::EvadeKind::rollRight?0.f:500.f);const auto local=kind==combat::EvadeKind::roll?player::Evade::roll:kind==combat::EvadeKind::rollLeft?player::Evade::rollLeft:player::Evade::rollRight;
 const float heading=kind==combat::EvadeKind::rollLeft?-pi/2:kind==combat::EvadeKind::rollRight?pi/2:0;
 f.values={};f.values[5]=1;f.values[kind==combat::EvadeKind::rollLeft?18:kind==combat::EvadeKind::rollRight?19:16]=1;
 f.control.step(f.values,.005f,true,true,f.nav.grounded());check(f.control.evadeRequested==local&&f.request.press(kind,start),"one actual A edge chooses requested direction");check(f.control.begin_reviewed_evade(local,heading),"runtime reviewed curve begins");check(f.control.speed==0,"begin frame adds no old constant-speed displacement");
 const auto initial=f.nav.feet();f.send(start,heading);check(f.host->authority().snapshot().players[0]->evadeKind==kind,"HOST admits reviewed action");
 f.values[5]=0;if(!keepStick)f.values={}; // User reproducer: movement fully released throughout roll and two seconds afterward.
 bool acknowledged=false,ackHandled=false;stage::Vec3 stopped{};bool stoppedSet=false;const unsigned ackAge=lateAck?1420:1415;const uint32_t serial=f.request.request();
 const auto total=combat::evade_runtime::distance_seconds(2.);
 for(unsigned age=5;age<=3520;age+=5){const uint64_t now=start+age;
  f.control.step(f.values,.005f,true,keepStick,f.nav.grounded());f.maximumSpeed=(std::max)(f.maximumSpeed,f.control.speed);check(f.control.speed<=6000.01f,"instantaneous native curve speed stays within unchanged HOST cap");
  if(f.control.evade_active()!=player::Evade::none)check(std::abs(f.control.bodyYaw-heading)<.0001f,"forward and side heading stays fixed");
  const auto before=f.nav.feet();f.nav.advance(*f.world,{f.control.forward,f.control.right,0,0,f.control.speed,2,1.5f,0.f},.005f);
  check(horizontal(before,f.nav.feet())<=30.02f,"every 5ms displacement respects 6000 units/s");
  if(age%interval==0||age==ackAge)f.send(now,f.control.evade_active()!=player::Evade::none?heading:f.nav.yaw());else{f.host->poll(now);f.collect(now);}
  const auto state=*f.host->authority().snapshot().players[0];
  if(age<1417){check(state.evadeKind==kind&&state.evadeSerial==serial&&state.evadeElapsedMs==age,"retries never restart or extend HOST action time");}
  else check(state.evadeKind==combat::EvadeKind::none&&!state.evadeSerial&&!state.evadeElapsedMs,"HOST expiry clears all optional action fields");
  if(age==ackAge){check(f.latestAt==now,"ACK tested on a newly decoded response");const auto ack=f.request.acknowledge(f.latest.sop,*f.latest.snapshot.players[0],epoch,now);
   if(lateAck){check(ack==player::EvadeInput::Ack::rejected,"completed footer ACK cannot restart prediction");f.control.cancel_evade();}
   else{check(ack==player::EvadeInput::Ack::accepted,"ACK two milliseconds before expiry accepts exact active serial");acknowledged=true;}
   ackHandled=true;check(!f.request.pending(),"one response drains pending request");
  }
  if(acknowledged&&state.evadeKind==combat::EvadeKind::none){f.control.cancel_evade();acknowledged=false;}
  if(!keepStick&&age>=1250){if(!stoppedSet){stopped=f.nav.feet();stoppedSet=true;}check(horizontal(stopped,f.nav.feet())<.03f,"released input: original recover terminal plus two seconds after expiry stays stationary");}
  if(age>=1450&&keepStick)check(f.control.evade_active()==player::Evade::none&&f.control.running&&f.control.speed==3800,"held movement returns to ordinary run after action");
  if(age>=1450&&!keepStick)check(f.control.evade_active()==player::Evade::none&&f.control.forward==0&&f.control.right==0,"released movement never remains latched");
 }
 check(ackHandled&&f.resends>2,"delayed ACK exercises repeated same-serial wire requests");
 if(!keepStick){const auto final=f.nav.feet();check(std::abs(horizontal(initial,final)-total)<.2f,"actual Control and Navigation retain exact capped curve distance");check(kind==combat::EvadeKind::roll?std::abs(final[0]-initial[0])<.02f:std::abs(final[2]-initial[2])<.02f,"side travel remains on fixed world axis");}
 else check(horizontal(initial,f.nav.feet())>total+6000,"held stick continues normal locomotion instead of being permanently blocked");
 // Stale queued request after completion must be ACKed only as the old serial,
 // with no re-admission, movement or new action clock.
 auto before=f.host->authority().snapshot().players[0]->pose;combat::wire::Input stale;stale.epoch=epoch;stale.life=1;stale.sequence=++f.sequence;stale.pose=before;stale.weapon=23;stale.evadeKind=kind;stale.evadeRequest=serial;
 check(f.host->receive(self,combat::wire::encode(stale),start+3600),"old same request delivered after completion");f.host->poll(start+3600);f.collect(start+3600);check(f.host->authority().snapshot().players[0]->evadeKind==combat::EvadeKind::none&&f.host->authority().snapshot().players[0]->pose==before,"late same serial does not move or rearm HOST");
 std::cout<<"kind="<<unsigned(kind)<<" send_ms="<<interval<<" held="<<keepStick<<" late_ack="<<lateAck<<" max_speed="<<f.maximumSpeed<<" curve_distance="<<total<<" PASS\n";
}
}
int main(){try{for(auto kind:{combat::EvadeKind::roll,combat::EvadeKind::rollLeft,combat::EvadeKind::rollRight})for(unsigned interval:{25u,50u,100u}){run(kind,interval,false,false);run(kind,interval,false,true);run(kind,interval,true,false);}std::cout<<"Reviewed roll Control->Navigation->GWCB->HOST stop/cap/ACK tests PASS\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
