#ifdef NDEBUG
#undef NDEBUG
#endif
#include "stage_navigation.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
using namespace mgo2win::stage;
int main(int argc,char** argv){
 if(argc!=3){std::cerr<<"stage_water_real_test <n023a.collision.cfg> <n023a.gww>\n";return 2;}
 std::ifstream ci(argv[1]);auto source=std::make_shared<Collision>(Collision::read(ci));
 std::ifstream wi(argv[2],std::ios::binary);auto water=std::make_shared<Water>(Water::read(wi));
 auto moving=movement_collision(source);assert(moving!=source&&moving->triangles.size()<source->triangles.size());
 const Vec3 surfacePoint{-73400,3000,-192200};
 auto originalHit=source->ray(surfacePoint,{0,-1,0},20000),solidHit=moving->ray(surfacePoint,{0,-1,0},20000);
 assert(originalHit&&solidHit&&std::abs(originalHit->position[1]-1000)<.02f&&std::abs(solidHit->position[1]-645)<.02f);
 assert(source->triangles[originalHit->triangle].attribute==0x40048000ULL);
 assert(moving->triangles[solidHit->triangle].attribute==0x20A002834ULL);
 Navigation wet,neutral;assert(wet.water(water,.65f)&&neutral.water(water,1));
 const Vec3 hint{-57800,3000,-176000};assert(wet.place(*moving,hint)&&neutral.place(*moving,hint));
 auto state=wet.water_state();assert(state.level==1000&&state.foot==WaterFoot::inWater&&std::abs(state.depthAboveFloor-1000)<.02f);
 const auto wetStart=wet.feet(),neutralStart=neutral.feet();
 for(unsigned i=0;i<24;++i){wet.advance(*moving,{1,0,0,0,3500},1.f/120);neutral.advance(*moving,{1,0,0,0,3500},1.f/120);}
 float wetDistance=wet.feet()[2]-wetStart[2],neutralDistance=neutral.feet()[2]-neutralStart[2];
 assert(std::abs(wetDistance-455)<1&&std::abs(neutralDistance-700)<1);
 assert(wet.water_state().foot==WaterFoot::inWater&&wet.water_state().horizontalScale==.65f&&moving->clear(wet.feet(),wet.capsule()));
 assert(source->ray(surfacePoint,{0,-1,0},20000)->position[1]==originalHit->position[1]);
 std::cout<<"actual JJ: original surface="<<originalHit->position[1]<<" movement floor="<<solidHit->position[1]<<" sample feet="<<wetStart[0]<<','<<wetStart[1]<<','<<wetStart[2]<<" wetDistance="<<wetDistance<<" neutralDistance="<<neutralDistance<<" original/source preserved PASS\n";
}
