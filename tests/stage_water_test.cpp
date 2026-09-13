#ifdef NDEBUG
#undef NDEBUG
#endif
#include "stage_water.h"
#include "stage_collision.h"
#include <cassert>
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
using namespace mgo2win::stage;
int main(int argc,char** argv){
 const WaterField jj{{-49500,0,-176000},{27500,1000,28000}};
 auto water=Water::make({jj,jj});assert(water.fields().size()==1);
 assert(water.level(jj.center)==1000);
 for(unsigned axis=0;axis<3;++axis)for(float sign:{-1.f,1.f}){
  auto p=jj.center;p[axis]+=sign*jj.halfSize[axis];assert(water.level(p)==1000);
  p[axis]=std::nextafter(p[axis],sign*std::numeric_limits<float>::infinity());assert(!water.level(p));
 }
 assert(water.control_level({-49500,4000,-176000},0)==1000);
 assert(!water.level({-49500,4000,-176000})); // query Y distinction is deliberate
 assert(!water.control_level(jj.center,-1001));
 assert(water.on_foot(jj.center,0)->foot==WaterFoot::inWater);
 assert(Water::classify(1000,1000,0)==WaterFoot::dry);
 assert(Water::classify(1000,999,1000)==WaterFoot::inWater);
 assert(Water::classify(1000,999,1001)==WaterFoot::aboveSurface);
 assert(water.on_foot(jj.center,1001)->depthAboveFloor==0);
 assert(!Water{}.level(jj.center));
 auto moved=jj;moved.center[1]=1;assert(!Water::make({jj,moved}).level(jj.center));
 auto nan=std::numeric_limits<float>::quiet_NaN();assert(!water.level({nan,0,0}));
 assert(!water.control_level({nan,0,0},0));assert(!water.on_foot(jj.center,nan));
 assert(Water::classify(nan,0,0)==WaterFoot::dry);
 auto rejects=[](auto fn){bool fail=false;try{fn();}catch(const std::exception&){fail=true;}assert(fail);};
 auto bad=jj;bad.halfSize[0]=0;rejects([&]{Water::make({bad});});bad=jj;bad.center[2]=nan;rejects([&]{Water::make({bad});});
 auto solid=std::make_shared<Collision>(Collision::make({{0,0,0},{0,0,1000},{1000,0,0}},{{{0,1,2},0x20A002834ULL,0}}));
 assert(movement_collision(solid)==solid&&!movement_collision({}));
 auto combined=std::make_shared<Collision>(Collision::make(solid->vertices,{{{0,1,2},0x40048000ULL,0},{{0,1,2},0x20A002834ULL,0}}));
 auto movement=movement_collision(combined);assert(movement!=combined&&movement->triangles.size()==1&&combined->triangles.size()==2);
 assert(movement->triangles[0].attribute==0x20A002834ULL); // normal control query accepts authored bit0x10
 auto decorative=std::make_shared<Collision>(Collision::make(solid->vertices,{{{0,1,2},0x10000000ULL,0}}));assert(movement_collision(decorative)->triangles.empty());
 auto unknown=std::make_shared<Collision>(Collision::make(solid->vertices,{{{0,1,2},0,0}}));assert(movement_collision(unknown)==unknown); // native unknown remains solid
 auto add=[](std::string& b,unsigned x){for(unsigned s=0;s<32;s+=8)b.push_back(char(x>>s));};
 std::string bytes="GWW1";add(bytes,1);add(bytes,1);for(float x:jj.center)add(bytes,std::bit_cast<unsigned>(x));for(float x:jj.halfSize)add(bytes,std::bit_cast<unsigned>(x));
 std::istringstream input(bytes);assert(Water::read(input).level(jj.center)==1000);
 for(size_t n=0;n<bytes.size();++n)rejects([&]{std::istringstream in(bytes.substr(0,n));Water::read(in);});
 rejects([&]{std::istringstream in(bytes+"x");Water::read(in);});
 auto corrupt=bytes;corrupt[4]=2;rejects([&]{std::istringstream in(corrupt);Water::read(in);});
 if(argc>1){std::ifstream real(argv[1],std::ios::binary);auto fromAsset=Water::read(real);assert(fromAsset.fields().size()==1);assert(fromAsset.level(jj.center)==1000);}
 std::cout<<"stage_water: exact JJ field, inclusive bounds, floor query, classification, malformed data PASS\n";
}
