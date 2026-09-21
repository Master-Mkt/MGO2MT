#include "stage_weather_surface.h"
#include <sstream>
#include <iostream>
#include <cmath>
using namespace mgo2mt;using namespace mgo2mt::stage;using namespace mgo2mt::stage::weather;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
std::shared_ptr<const Collision> scene(bool roof){std::vector<Vec3>v{{-10000,0,-10000},{10000,0,-10000},{10000,0,10000},{-10000,0,10000}};std::vector<CollisionTriangle>t{{{0,2,1}},{{0,3,2}}};if(roof){v.insert(v.end(),{{-2000,2000,-2000},{2000,2000,-2000},{2000,2000,2000},{-2000,2000,2000}});t.push_back({{4,6,5}});t.push_back({{4,7,6}});}return std::make_shared<const Collision>(Collision::make(v,t));}
int main(){try{
 check(Settings::defaults("n022a").preset==Preset::fog_sand&&Settings::defaults("n004a").preset==Preset::clear,"defaults do not invent rain/snow");
 std::istringstream cfg("MGO2MT.WEATHER 1 ENABLED 1 PRESET rain INTENSITY 1 WET_SECONDS 1 SNOW_SECONDS 1 DRY_SECONDS 1 MELT_SECONDS 1 END");auto s=Settings::read(cfg);check(s.preset==Preset::rain,"strict config");std::istringstream bad("MGO2MT.WEATHER 1 ENABLED 1 PRESET other");bool rejected=false;try{Settings::read(bad);}catch(...){rejected=true;}check(rejected,"bad preset");
 SurfaceController controller;auto world=scene(true);std::shared_ptr<const SurfaceGrid>g;for(int i=0;i<100;++i)g=controller.sample(s,i*.02,{0,1800,0},world,1);check(bool(g),"rain accumulates");
 check(surface_coverage(*g,{500,0,500},1)==0,"indoor floor stays dry");check(surface_coverage(*g,{500,2000,500},1)>.9f,"roof catches rain");check(surface_coverage(*g,{4000,0,4000},1)>.9f,"exposed ground wet");check(surface_coverage(*g,{4000,0,4000},0)==0,"vertical faces excluded");
 const auto previous=g;world=scene(false);g=controller.sample(s,2.02,{0,1800,0},world,2);check(!g||surface_coverage(*g,{500,0,500},1)<.1f,"newly exposed ground does not inherit old roof wetness");for(int i=0;i<100;++i)g=controller.sample(s,2.04+i*.02,{0,1800,0},world,2);check(g&&surface_coverage(*g,{500,0,500},1)>.9f,"destruction revision exposes lower floor");check(previous->collisionRevision==1,"published grid immutable");
 s.preset=Preset::snow;for(int i=0;i<100;++i)g=controller.sample(s,4.1+i*.02,{0,1800,0},world,2);auto center=g->cells[32*64+32];check(center.snow>.9f&&center.wetness<.01f,"snow accumulates independently of rain");
 s.preset=Preset::clear;for(int i=0;i<100;++i)g=controller.sample(s,6.2+i*.02,{0,1800,0},world,2);check(!g,"clear dries and melts");
 s.rainEnabled=true;s.snowEnabled=true;for(int i=0;i<100;++i)g=controller.sample(s,8.3+i*.02,{0,1800,0},world,2);center=g->cells[32*64+32];check(center.wetness>.9f&&center.snow>.9f,"HOST independent rain and snow accumulate simultaneously");
 s.preset=Preset::rain;s.rainEnabled=false;s.snowEnabled=false;for(int i=0;i<100;++i)g=controller.sample(s,10.4+i*.02,{0,1800,0},world,2);check(!g,"HOST OFF overrides legacy preset and gradually dries/melts");s.enabled=false;check(!controller.sample(s,13,{0,1800,0},world,2),"master off clears effect");std::cout<<"weather exposure, roof/destruction, timing, preset, independent HOST channels and disable passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
