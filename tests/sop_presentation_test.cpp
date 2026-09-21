#include "sop_presentation.h"
#include <stdexcept>
#include <iostream>
using namespace mgo2mt;
void check(bool ok){if(!ok)throw std::runtime_error("SOP presentation assertion");}
int main(){
 combat::Snapshot s;s.epoch=9;s.revision=1;
 combat::Identity self{0,2,10},ally{1,3,20};
 combat::Player a;a.identity=self;a.life=1;a.team=1;a.alive=true;a.hp=a.maxHp=a.stamina=a.maxStamina=100;s.players[0]=a;
 a.identity=ally;s.players[1]=a;
 combat::SopView v;v.recipient=self;v.life=1;
 sop::Presentation p;check(!p.update(1,s,self,v,true,1));
 v.activation=1;v.visibleMask=2;v.origin={1,2,3};check(p.update(1,s,self,v,true,2));
 check(p.visible(ally,1,s)&&!p.visible(self,1,s));check(p.pulse(2)->origin==v.origin);check(p.pulse(602)->radius==24000);
 check(!p.update(1,s,self,v,true,3));check(!p.pulse(1202));
 auto enemy=s;enemy.players[1]->team=2;check(!p.visible(ally,1,enemy));
 auto reused=s;reused.players[1]->identity.instance++;check(!p.visible(reused.players[1]->identity,1,reused));
 reused=s;reused.players[1]->life++;check(!p.visible(ally,1,reused));
 v.jammed=true;v.visibleMask=0;check(!p.update(1,s,self,v,true,4));check(!p.pulse(5));
 v.jammed=false;v.activation=2;v.visibleMask=2;check(p.update(1,s,self,v,true,6));
 check(!p.update(1,s,self,v,false,7));check(!p.visible(ally,1,s));
 check(!p.update(1,s,self,v,true,8));check(!p.pulse(8)); // Late join does not replay activation.
 check(!p.update(2,s,self,v,true,9));v.activation=3;check(p.update(2,s,self,v,true,10));
 s.players[0]->life=2;v.life=2;check(!p.update(2,s,self,v,true,11));check(!p.pulse(11));
 v.recipient.instance++;check(!p.update(2,s,self,v,true,12));
 sop::SpecialInput input;input.press(100);check(input.pending()&&input.edge());input.sent(0xffffffffu);check(!input.edge()&&input.pending());
 combat::SopView ack;ack.inputSequenced=true;ack.inputSequence=0xfffffffeu;input.acknowledge(ack,101);check(input.pending());
 ack.inputSequence=0;input.acknowledge(ack,102);check(!input.pending());
 input.press(200);input.sent(5);ack.inputSequenced=false;input.acknowledge(ack,1699);check(input.pending());input.acknowledge(ack,1700);check(!input.pending());
 sop::PhaseClock clock;a.specialPhase=combat::SpecialPhase::start;check(clock.update(9,a,100)==0);check(clock.update(9,a,300)==.2);a.specialPhase=combat::SpecialPhase::hold;check(clock.update(9,a,400)==0);a.life++;check(clock.update(9,a,500)==0);
 std::cout<<"SOP presentation/reset/ack/phase PASS\n";
}
