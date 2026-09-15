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
 for(uint8_t map:{uint8_t(1),uint8_t(4),uint8_t(20),uint8_t(21)}){
  for(uint32_t h:{0x45BCB4u,0x48C4B8u,0x48C4B9u,0x48C4BAu,0x48C4BBu,0x48C4BCu,
    0x65C426u,0x7818B1u,0x7818B2u,0x7818B3u,0x7ADCCAu,0x7ADCCBu,0xA1DCCBu,0xB920C5u,0xB920C6u})assert(verified_kind(map,h)==Kind::metal);
  for(uint32_t h:{0x189CD4u,0x189CD5u,0x189CD6u,0x189CD7u,0x989CB2u})assert(verified_kind(map,h)==Kind::wood);
  // Neighboring hashes have different original callbacks; neither audio group
  // nor a legacy material-name family is sufficient to classify them.
  for(uint32_t h:{0u,0x48C4B7u,0x48C4BDu,0x48C4CEu,0x189CD3u,0x189CD8u,0x189CEAu,
    0x4598AAu,0x3920C5u,0xE1D8C5u,0x6690BCu,0x885BE5u,0x0248C4B8u,0xFFFFFFFFu})assert(verified_kind(map,h)==Kind::unknown);
 }
 for(unsigned map=0;map<=255;++map)if(map!=1&&map!=4&&map!=20&&map!=21){assert(verified_kind(uint8_t(map),0x48C4B8)==Kind::unknown);assert(verified_kind(uint8_t(map),0x189CD4)==Kind::unknown);}
 {Pool classified;Impact i{{8,9},2,{0,0,0},{0,1,0},0x48C4B8,decals::Surface::static_solid};
  classified.synchronize(i.scope,1,0);classified.synchronize(i.scope,2,1);
  assert(classified.emit(i,verified_kind(20,i.material),1));assert(classified.lines(2).front().kind==Kind::metal);
  i.eventId=3;i.material=0x189CD4;classified.synchronize(i.scope,3,2);
  assert(classified.emit(i,verified_kind(20,i.material),2));assert(classified.lines(3).back().kind==Kind::wood);
  i.eventId=4;i.material=0x48C4CE;classified.synchronize(i.scope,4,3);
  assert(!classified.emit(i,verified_kind(20,i.material),3));
  i.material=0x48C4B8;assert(!classified.emit(i,verified_kind(20,i.material),3)); // rejected ID consumed
 }
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
