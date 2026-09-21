#include "original_sop.h"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace mgo2mt::original_sop;
namespace { void check(bool ok) { if(!ok) { std::cerr<<"SOP check failed\n"; std::exit(1); } } }
int main() {
    for(auto b:{Branch::status22,Branch::status140}) {
        const unsigned half=b==Branch::status22?8192:5461;
        for(unsigned angle=0;angle<65536;++angle) {
            const int signedAngle=angle<=32767?static_cast<int>(angle):static_cast<int>(angle)-65536;
            check(yaw_window(static_cast<std::uint16_t>(angle),0,b)==(std::abs(signedAngle)<=static_cast<int>(half)));
            check(yaw_window(static_cast<std::uint16_t>(angle+12345),12345,b)==yaw_window(static_cast<std::uint16_t>(angle),0,b));
        }
        const float r=range(b);
        check(geometry({}, {0,2000,r},0,b));
        check(!geometry({}, {0,std::nextafter(2000.f,3000.f),r},0,b));
        check(!geometry({}, {0,0,std::nextafter(r,r*2)},0,b));
        check(geometry({}, {0,-2000,-2000},0,b)); // inclusive near-radius facing exemption
        check(!geometry({}, {0,0,-2001},0,b));
        check(geometry({100,200,300},{100,200,300+r},0,b));
        check(!geometry({}, {std::numeric_limits<float>::quiet_NaN(),0,0},0,b));
        check(!geometry({}, {std::numeric_limits<float>::max(),0,std::numeric_limits<float>::max()},0,b));
    }
    check(bearing({0,0,3000}) && std::abs(int(*bearing({0,0,3000})))<=1);
    check(bearing({3000,0,0}) && std::abs(int(*bearing({3000,0,0}))-16384)<=1);
    check(bearing({0,0,-3000}) && std::abs(int(*bearing({0,0,-3000}))-32768)<=1);
    check(bearing({-3000,0,0}) && std::abs(int(*bearing({-3000,0,0}))-49152)<=1);
    check(!bearing({}));
    Peer s{0,2,0,true,false,false,true,false},t{1,2,1,true};
    check(eligible(s,t,1,2));
    check(!eligible(s,t,1,1));
    for(int state:{1,3,6}) { auto q=t;q.state=state;check(!eligible(s,q,1,2)); }
    t.state=0;check(eligible(s,t,1,2));check(!eligible(s,t,4,2));t.state=2;
    for(int flag=0;flag<4;++flag) {auto a=s,b=t;if(flag==0)a.status136=true;if(flag==1)a.status160=true;if(flag==2)b.status136=true;if(flag==3)b.status160=true;check(!eligible(a,b,1,2));}
    s.status22=false;check(!eligible(s,t,1,2));s.status140=true;check(eligible(s,t,1,2));
    check(suppressed_level(3,2)==1 && suppressed_level(1,3)==0);
    Groups g;check(g.group(0)==-1);check(g.merge(0,1));check(g.group(0)==24 && g.group(1)==24);
    check(g.merge(2,3));check(g.group(2)==25);check(g.merge(0,2));
    for(int i=0;i<4;++i)check(g.group(i)==25); // full old group unions into target group
    check(g.merge(4,0));check(g.group(4)==25);check(g.merge(0,5));check(g.group(5)==25);
    check(!g.merge(0,1) && !g.merge(-1,2) && !g.merge(2,24));
    for(int i=0;i<5;++i)g.clear(i);g.dissolve_singletons();check(g.group(5)==-1);
    g.reset();for(int i=0;i<24;i+=2)check(g.merge(i,i+1));
    for(int i=2;i<24;i+=2)check(g.merge(0,i));for(int i=0;i<24;++i)check(g.group(i)==g.group(0));
    g.reset();for(int i=0;i<24;++i)check(g.group(i)==-1);
    std::cout<<"original SOP geometry, 262144 yaw comparisons, membership and status gates PASS\n";
}
