#include "combat_service.h"
#include "special_pc_input.h"
#include "special_pc_clock.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2win;using namespace mgo2win::combat;
namespace {void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}template<class F>bool rejects(F f){try{f();}catch(const wire::Invalid&){return true;}return false;}}
int main(){try{
 Snapshot snapshot;snapshot.epoch=9;snapshot.revision=1;snapshot.eventWatermark=1;
 for(unsigned i=0;i<24;++i){Player p;p.identity={uint8_t(i),uint16_t(i+1),i+10};p.life=1;p.alive=true;p.hp=p.maxHp=1000;p.stamina=p.maxStamina=5000;p.pose.feet={float(i)*2200,2,0};p.pose.capsule=special_pc::native_gekko.capsule;p.specialPc.kind=special_pc::Kind::gekko;snapshot.players[i]=p;}
 wire::Frame frame;frame.snapshot=snapshot;frame.status=wire::Status::active;
 Event event;event.epoch=9;event.id=1;event.source=snapshot.players[0]->identity;event.sourceLife=1;event.weapon=25;event.kind=EventKind::shot;event.normal={0,0,1};event.shotDistance=12000;frame.events={event};
 size_t maximum=0;
 for(auto action:{special_pc::Action::jump,special_pc::Action::climb,special_pc::Action::salute,special_pc::Action::kick})for(bool name:{false,true}){
  for(auto&p:frame.snapshot.players)p->specialPc={special_pc::Kind::gekko,name,action,UINT32_MAX,uint16_t(special_pc::duration(action)-1)};
  auto bytes=wire::encode(frame);maximum=(std::max)(maximum,bytes.size());check(bytes[5]==19&&bytes.size()<=2000,"24 action states plus shot stay within GWCB19 bound");
  check(std::get<wire::Frame>(wire::decode(bytes))==frame,"action4 and name bit remain independent and elapsed13 preserves5023ms");
  auto old=bytes;old[5]=16;check(!wire::recognized(old)&&rejects([&]{wire::decode(old);}),"old version rejected");
  bytes.pop_back();check(rejects([&]{wire::decode(bytes);}),"truncated action frame rejected");
 }
 auto invalid=frame;invalid.snapshot.players[0]->specialPc.action=special_pc::Action(5);check(rejects([&]{wire::encode(invalid);}),"unused action rejected");
 invalid=frame;invalid.snapshot.players[0]->specialPc.elapsedMs=special_pc::duration(invalid.snapshot.players[0]->specialPc.action);check(rejects([&]{wire::encode(invalid);}),"finished action cannot remain on wire");
 wire::Input first;first.epoch=9;first.specialPc={special_pc::Action::climb,12};check(std::get<wire::Input>(wire::decode(wire::encode(first)))==first,"explicit climb input roundtrip");
 auto second=first;second.sequence=2;second.specialPc={};check(wire::coalesce_input(first,second).specialPc==first.specialPc,"pending climb retained through input coalescing");second.suspended=true;check(wire::coalesce_input(first,second).specialPc==special_pc::Intent{},"focus loss cancels climb edge");
 special_pc::Input input;const auto id=snapshot.players[0]->identity;input.scope(9,id,1,special_pc::Kind::gekko);std::array<float,24>values{};input.step(values,true,special_pc::Action::none,0);values[16]=values[5]=1;
 check(input.step(values,true,special_pc::Action::none,10,true)&&input.intent(true).action==special_pc::Action::climb,"eligible forward+A routes climb");
 SopView ack;ack.recipient=id;ack.life=1;ack.specialPcRequest=input.intent(true).request;input.acknowledge(ack,20);check(!input.step(values,true,special_pc::Action::none,30,false)&&!input.pending(),"rejected climb never falls back to jump from held A");
 values={};input.step(values,true,special_pc::Action::none,40);values[5]=1;check(input.step(values,true,special_pc::Action::none,50,false)&&input.intent(true).action==special_pc::Action::jump,"new A with no eligible wall remains jump");
 special_pc::Clock clock;special_pc::State state{special_pc::Kind::gekko,true,special_pc::Action::jump,1,5000};check(clock.sample(9,id,1,state,100)==5.,"long jump clock accepts landing phase");check(clock.sample(9,id,1,state,200)==5.024,"jump extrapolation bounded to new duration");
 state={special_pc::Kind::gekko,true,special_pc::Action::climb,2,1599};check(clock.sample(9,id,1,state,210)==1.599,"new climb resets local clock");
 std::cout<<"Gekko GWCB19 climb/long jump/name visibility/input/clock PASS maximum="<<maximum<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
