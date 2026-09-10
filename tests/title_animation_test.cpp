#include "title_animation.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <cstring>
using mgo2win::TitleAnimation;
void require(bool value){if(!value)throw std::runtime_error("Animation contract check failed");}
int main(int argc,char**argv){try{
 require(argc==2);std::ifstream f(argv[1],std::ios::binary);require(bool(f));std::vector<char>b((std::istreambuf_iterator<char>(f)),{});
 require(TitleAnimation::interpolate(-2,1,.5f)==-1);require(TitleAnimation::interpolate(2,5,.5f)==4);
 TitleAnimation a(b);unsigned selected=0,notified=0;a.set_callbacks([&](uint32_t result){require(result==1);++selected;},[&](uint32_t result){require(result==1);++notified;});auto initial=a.geometry();require(initial.size()==662);
 for(unsigned tick=5;tick<=900;tick+=5)a.tick(5,(tick==100||tick==900)?8:0);
 require(a.state()==3&&a.accepted()==1&&a.rejected()==1&&a.accepted_tick()==900);
 for(unsigned i=0;i<1000&&a.state()!=4;++i)a.tick(5,0);
 require(a.state()==4&&a.callbacks()==1&&a.active_count()==0&&selected==1&&notified==1);
 auto completed=a.callback_tick();for(unsigned i=0;i<10;++i)a.tick(5,8);require(a.callbacks()==1&&a.accepted()==1);
 TitleAnimation gate(b);for(unsigned t=5;t<=625;t+=5)gate.tick(5,t==625?8:0);require(gate.accepted()==0&&gate.rejected()==1);gate.tick(5,8);require(gate.accepted()==1);
 TitleAnimation timeout(b);unsigned timeoutCallbacks=0;timeout.set_callbacks({},[&](uint32_t result){require(result==2);++timeoutCallbacks;});
 for(unsigned t=5;t<=18005;t+=5)timeout.tick(5,t==18005?8:0);
 require(timeout.state()==3&&timeout.result()==2&&timeout.accepted()==0);
 for(unsigned i=0;i<100&&timeout.state()!=4;++i)timeout.tick(5,0);
 require(timeout.state()==4&&timeoutCallbacks==1);
 TitleAnimation fine(b),coarse(b);for(int i=0;i<60;++i)fine.tick(5,0);coarse.tick(300,0);
 // Segment snapshots must not drift when multiple segment boundaries are crossed.
 for(unsigned n=1;n<=655;++n)require(fine.node_values(n)==coarse.node_values(n));
 bool moved=false;auto middle=fine.geometry();for(size_t i=0;i<middle.size();++i)if(middle[i].vertices[0].x!=initial[i].vertices[0].x)moved=true;require(moved);
 for(int which=0;which<5;++which){auto bad=b;if(which==0)bad.resize(10);if(which==1)bad.pop_back();if(which==2){uint32_t parent=1;std::memcpy(bad.data()+24,&parent,4);}if(which==3){uint32_t nan=0x7fc00000;std::memcpy(bad.data()+36,&nan,4);}if(which==4){float blend=2;std::memcpy(bad.data()+36+29*4,&blend,4);}bool rejected=false;try{TitleAnimation invalid(bad);}catch(const std::exception&){rejected=true;}require(rejected);}
 std::cout<<"normal actor, gate at 620, one callback at tick "<<completed<<", timeout priority/result 2, all-node snapshots, motion, and 5 malformed assets: passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
