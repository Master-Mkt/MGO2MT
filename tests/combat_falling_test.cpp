#include "combat_falling.h"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace mgo2mt::combat::falling;
void check(bool value,const char* m){if(!value)throw std::runtime_error(m);}
int main(){try{
 check(damage(3000,1000)==0u&&damage(5000,1000)==450u&&damage(7000,1000)==900u&&damage(8500,1000)==950u&&damage(10000,1000)==1000u&&damage(12000,1000)==1000u,"user native thresholds");check(damage(7000,100)==90u&&damage(9999,1000).value()<1000,"maxHP scaling and below fatal survival");check(!damage(-1,1000)&&!damage(1,0)&&!damage(std::numeric_limits<float>::quiet_NaN(),1000),"invalid policy inputs");
 Scope key{1,0,1,100,1};Tracker t;Input in{key,10000,0,true,true,false};check(!t.update(in,1000)&&t.primed(),"ground primes");in.nowMs=10;in.grounded=false;in.feetY=9500;check(!t.update(in,1000)&&t.airborne(),"step off tracks former ground height");in.nowMs=20;in.feetY=3000;check(!t.update(in,1000),"no damage without validated floor");in.nowMs=30;in.grounded=true;auto land=t.update(in,1000);check(land&&land->height==7000&&land->damage==900&&!land->fatal,"seven metre landing leaves100 of1000");check(!t.update(in,1000),"same tick no duplicate");in.nowMs=40;check(!t.update(in,1000),"continued ground no duplicate");auto serial=land->serial;
 // A jump rising above takeoff must use the apex, not only the previous tick.
 in.nowMs=50;in.grounded=false;in.feetY=8000;t.update(in,1000);in.nowMs=60;in.feetY=13000;t.update(in,1000);in.nowMs=70;in.feetY=5000;t.update(in,1000);in.nowMs=80;in.feetY=3000;in.grounded=true;land=t.update(in,1000);check(land&&land->height==10000&&land->damage==1000&&land->fatal&&land->serial>serial,"apex ten metre fatal once");
 auto begin=[&]{++key.life;t.clear();in={key,12000,100,true,true,false};t.update(in,1000);in.nowMs=110;in.grounded=false;in.feetY=11000;t.update(in,1000);};
 begin();in.nowMs=120;++in.scope.life;in.feetY=0;in.grounded=true;check(!t.update(in,1000),"respawn suppresses old fall");
 begin();in.nowMs=120;in.scope.character=999;in.feetY=0;in.grounded=true;check(!t.update(in,1000),"character reuse suppresses old fall");
 for(int scopePart=0;scopePart<2;++scopePart){begin();in.nowMs=120;if(scopePart==0)++in.scope.epoch;else ++in.scope.instance;in.feetY=0;in.grounded=true;check(!t.update(in,1000),"round full identity reset");}
 begin();in.nowMs=120;in.ladder=true;t.update(in,1000);in.nowMs=130;in.ladder=false;in.feetY=0;in.grounded=true;check(!t.update(in,1000),"ladder transport reset");
 begin();in.nowMs=120;in.eligible=false;t.update(in,1000);in.nowMs=130;in.eligible=true;in.feetY=0;in.grounded=true;check(!t.update(in,1000),"dead disabled teleport reset");
 begin();in.nowMs=109;in.feetY=0;in.grounded=true;check(!t.update(in,1000)&&t.airborne(),"clock rollback preserves evidence");in.nowMs=120;check(t.update(in,1000)->fatal,"later landing still fatal after rollback");begin();in.nowMs=2111;in.feetY=0;in.grounded=true;check(t.update(in,1000)->fatal,"long gap preserves fall evidence");
 begin();in.nowMs=120;in.feetY=std::numeric_limits<float>::infinity();t.update(in,1000);in.nowMs=130;in.feetY=0;in.grounded=true;check(t.update(in,1000)->fatal,"invalid height cannot erase evidence");
 ++key.life;t.clear();in={key,12000,0,false,true,false};t.update(in,1000);in.nowMs=10;in.feetY=0;in.grounded=true;check(t.update(in,1000)->fatal,"initial airborne peak retained");
 ++key.life;t.clear();in={key,12000,0,false,true,false};t.update(in,1000);in.nowMs=10;in.feetY=2000;land=t.update(in,1000);check(land&&land->fatal,"deep hole fatal before ground or navigation rescue");in.nowMs=20;in.grounded=true;check(!t.update(in,1000),"fatal not repeated on ground");t.clear();in.nowMs=30;check(!t.update(in,1000),"same-life fatal cannot rearm through clear");in.eligible=false;t.update(in,1000);in.eligible=true;in.nowMs=40;check(!t.update(in,1000),"death eligibility toggle cannot rearm");
 ++key.life;t.clear();in={key,12000,100,true,true,false};t.update(in,1000);in.grounded=false;in.feetY=11000;check(!t.update(in,1000)&&t.airborne(),"same-tick accepted pose starts falling");in.feetY=2000;in.grounded=true;land=t.update(in,1000);check(land&&land->fatal,"same-tick landing evaluated before shooting");check(!t.update(in,1000),"identical repeated landing stays once");
 bool rejected=false;try{Tracker bad({3000,3000,10000,900});}catch(...){rejected=true;}check(rejected,"invalid policy rejected");std::cout<<"fall thresholds apex validated support once reset scope clock ladder PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
