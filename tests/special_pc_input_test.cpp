#include "special_pc_input.h"
#include "special_pc_clock.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){try{
 special_pc::Input input;combat::Identity id{1,4,99};input.scope(10,id,1,special_pc::Kind::gekko);std::array<float,24> v{};
 v[5]=1;check(!input.step(v,true,special_pc::Action::none,1),"Spawn requires neutral");v={};input.step(v,true,special_pc::Action::none,10);v[5]=1;check(input.step(v,true,special_pc::Action::none,20),"A requests jump");auto first=input.intent(true);check(first.action==special_pc::Action::jump&&first.request==1,"Jump serial");check(!input.step(v,true,special_pc::Action::none,30),"Held A cannot repeat");
 combat::SopView ack;ack.recipient=id;ack.life=2;ack.specialPcRequest=1;input.acknowledge(ack,40);check(input.pending(),"Other life ACK ignored");ack.life=1;input.acknowledge(ack,50);check(!input.pending(),"Rejection/success consumes request");
 v={};input.step(v,true,special_pc::Action::none,60);v[7]=1;check(input.step(v,true,special_pc::Action::none,70)&&input.intent(true).action==special_pc::Action::kick,"Y requests kick");input.step(v,false,special_pc::Action::none,80);check(!input.pending()&&input.intent(false)==special_pc::Intent{},"Menu/focus loss clears pending");
 input.step(v,true,special_pc::Action::none,90);check(!input.pending(),"Return with held Y cannot replay");v={};input.step(v,true,special_pc::Action::none,100);v[7]=1;check(input.step(v,true,special_pc::Action::none,110)&&input.intent(true).request==3,"Focus never reuses serial");
 input.acknowledge(ack,1610);check(!input.pending(),"Missing ACK bounded");input.scope(10,id,1,special_pc::Kind::human);v={};input.step(v,true,special_pc::Action::none,1700);v[5]=1;check(!input.step(v,true,special_pc::Action::none,1710),"Human cannot request special PC actions");
 special_pc::Clock clock;special_pc::State state;state.kind=special_pc::Kind::gekko;state.action=special_pc::Action::kick;state.serial=1;state.elapsedMs=100;
 check(clock.sample(10,id,1,state,2000)==.1,"Use HOST action age");check(clock.sample(10,id,1,state,2050)>.14,"Interpolate stale age monotonically");state.elapsedMs=10;check(clock.sample(10,id,1,state,2100)>.19,"Delayed packet cannot rewind same action");state.serial=2;check(clock.sample(10,id,1,state,2110)==.01,"New action resets phase");check(clock.sample(10,id,1,{},2120)==0,"End clears phase");
 special_pc::Input greeting;greeting.scope(15,id,2,special_pc::Kind::gekko);v={};greeting.step(v,true,special_pc::Action::none,1);v[4]=1;check(greeting.step(v,true,special_pc::Action::none,2)&&greeting.intent(true).action==special_pc::Action::salute,"B requests native greeting");check(!greeting.step(v,true,special_pc::Action::none,3),"held greeting does not repeat");greeting.cancel();check(!greeting.step(v,true,special_pc::Action::none,4)&&!greeting.pending(),"focus return cannot replay greeting");
 std::cout<<"special PC input/ACK/scope/phase clock PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
