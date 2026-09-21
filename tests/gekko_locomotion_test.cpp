#include "gekko_locomotion.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt::gekko_locomotion;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){try{
 Scope scope{1,1,100,1,2,0};State s;Policy p;check(valid(p),"native policy valid");
 check(s.update(scope,{0,4224},0).rebaselined,"scope baseline no movement");s.update(scope,{0,4224},200);s.update(scope,{0,4224},400);check(s.speed()==4224,"run speed preserved after acceleration");
 auto reverse=s.update(scope,{3.14159265f,4224},450);check(reverse.phase==Phase::braking&&reverse.displacement[1]>0&&s.speed()<4224,"opposite input first brakes original travel");check(std::abs(reverse.yaw)<=p.yawRate*.05f+.001f,"finite yaw turn");
 auto stopped=s.update(scope,{3.14159265f,4224},576);check(stopped.phase==Phase::waiting&&s.speed()==0,"brake reaches rest");auto wait=s.update(scope,{3.14159265f,4224},700);check(wait.displacement[0]==0&&wait.displacement[1]==0&&wait.phase==Phase::waiting,"stationary reverse hold");auto resumed=s.update(scope,{3.14159265f,4224},800);check(resumed.displacement[1]<0&&s.speed()>0,"reverse accelerates after hold");
 State a,b;a.update(scope,{0,4224},0);b.update(scope,{0,4224},0);auto one=a.update(scope,{0,4224},200);float distance=0;for(unsigned t=1;t<=200;++t)distance+=b.update(scope,{0,4224},t).displacement[1];check(one.displacement[1]==distance&&a.speed()==b.speed(),"1ms partition exact");
 State c,d;c.update(scope,{0,1885},0);d.update(scope,{0,1885},0);c.update(scope,{0,1885},200);d.update(scope,{0,1885},200);check(c.speed()==1885,"walk target unchanged");auto coarse=c.update(scope,{3.14159265f,1885},450);std::array<float,2> split{};for(unsigned t=201;t<=450;++t){auto x=d.update(scope,{3.14159265f,1885},t);for(int k=0;k<2;++k)split[k]+=x.displacement[k];}check(coarse.displacement==split&&c.phase()==d.phase()&&c.speed()==d.speed(),"reverse brake and hold partition exact");
 State wrap;wrap.update(scope,{3.13f,1000},0);auto across=wrap.update(scope,{-3.13f,1000},10);check(across.phase==Phase::moving&&std::abs(std::remainder(across.yaw-3.13f,6.283185307f))<.03f,"yaw wrap follows short arc without false reversal");auto noTime=wrap.update(scope,{-3.13f,1000},10);check(noTime.displacement==std::array<float,2>{},"same timestamp no time consumed");
 a.update(scope,{3.14159265f,4224},250);auto cancel=a.update(scope,{0,4224},270);check(cancel.phase==Phase::moving,"change back cancels reversal");a.update(scope,{3.14159265f,4224},450);auto released=a.update(scope,{3.14159265f,0},650);check(released.phase==Phase::stopped&&a.speed()==0,"release removes hold and stops");
 auto previous=a.speed();check(!a.update(scope,{0,std::numeric_limits<float>::quiet_NaN()},651).accepted&&a.speed()==previous,"NaN rejected without mutation");check(!a.update(scope,{0,4225},651).accepted,"speed cannot exceed native cap");
 check(a.update(scope,{0,1000},649).rebaselined&&a.speed()==0,"clock rollback stops safely");check(a.update(scope,{0,1000},2000).rebaselined,"large gap no catchup teleport");scope.life++;check(a.update(scope,{0,1000},2001).rebaselined&&a.speed()==0,"new life clears motion");scope.epoch++;check(a.update(scope,{0,1000},2002).rebaselined,"epoch boundary");scope.instance++;check(a.update(scope,{0,1000},2003).rebaselined,"full identity boundary");
 p.reverseWaitMs=1001;check(!valid(p),"invalid wait policy");std::cout<<"Gekko native brake/hold/reaccelerate, bounded yaw, partition, release/scope/clock PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
