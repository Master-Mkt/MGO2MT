#ifdef NDEBUG
#undef NDEBUG
#endif
#include "material_impact_effects.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <iostream>
using namespace mgo2win::combat;
using namespace material_effects;
int main(){Pool p;Scope scope{1,2};Impact hit{scope,1,{0,0,0},{0,0,1},123,decals::Surface::static_solid};
 assert(verified_kind(0x48c4b8)==Kind::unknown);assert(verified_kind(0x189cd4)==Kind::unknown);
 assert(!p.emit(hit,Kind::metal,0));p.synchronize(scope,1,0);assert(!p.emit(hit,Kind::metal,0));
 p.synchronize(scope,2,1);hit.eventId=2;assert(p.emit(hit,Kind::metal,1));assert(p.size()==8);assert(!p.emit(hit,Kind::wood,1));
 auto lines=p.lines(11);assert(lines.size()==8);for(auto l:lines){assert(l.to[2]>2);assert(l.rgba[0]==1);assert(l.rgba[3]>0&&l.rgba[3]<1);for(float f:l.to)assert(std::isfinite(f));}
 Pool twin;twin.synchronize(scope,1,0);twin.synchronize(scope,2,1);assert(twin.emit(hit,Kind::metal,1));auto same=twin.lines(11);assert(same[0].to==lines[0].to);
 p.synchronize(scope,3,12);hit.eventId=3;assert(!p.emit(hit,Kind::unknown,12));assert(!p.emit(hit,Kind::wood,12));
 p.synchronize(scope,4,13);hit.eventId=4;hit.surface=decals::Surface::water;assert(!p.emit(hit,Kind::glass,13));
 hit.surface=decals::Surface::static_solid;p.synchronize(scope,5,14);hit.eventId=5;hit.normal={0,0,0};assert(!p.emit(hit,Kind::wood,14));
 hit.normal={0,1,0};p.synchronize(scope,6,15);hit.eventId=6;hit.position[0]=std::numeric_limits<float>::quiet_NaN();assert(!p.emit(hit,Kind::stone,15));hit.position={};
 p.synchronize(scope,7,16);hit.eventId=8;assert(!p.emit(hit,Kind::wood,16));hit.eventId=7;assert(p.emit(hit,Kind::wood,16));assert(p.lines(17).back().kind==Kind::wood);
 assert(p.lines(0).empty());assert(!p.emit(hit,Kind::wood,1)); // clock rewind never permits replay
 p.synchronize({2,2},10,100);hit.scope={2,2};hit.eventId=10;assert(!p.emit(hit,Kind::metal,100));assert(p.size()==0);
 p.synchronize({2,2},11,101);hit.eventId=11;assert(p.emit(hit,Kind::glass,101));assert(!p.lines(650).empty());assert(p.lines(651).empty());
 p.synchronize({2,2},100,700);for(uint64_t e=12;e<=100;++e){hit.eventId=e;assert(p.emit(hit,Kind::stone,700));}assert(p.size()==256);
 p.synchronize({},0,701);assert(p.size()==0);assert(!p.emit(hit,Kind::metal,702));
 auto policy=Policy{};policy.capacity=257;assert(!Pool::valid(policy));policy={};policy.speed=std::numeric_limits<float>::infinity();assert(!Pool::valid(policy));
 std::cout<<"material impact particles identity, bounds, deterministic geometry PASS\n";
}
