#include "water_effects_overlay.h"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt::stage;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
void save(const std::vector<uint32_t>& p,const std::filesystem::path& path){BITMAPFILEHEADER f{};BITMAPINFOHEADER h{};f.bfType=0x4d42;f.bfOffBits=sizeof(f)+sizeof(h);f.bfSize=f.bfOffBits+uint32_t(p.size()*4);h.biSize=sizeof(h);h.biWidth=1280;h.biHeight=-720;h.biPlanes=1;h.biBitCount=32;std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&f),sizeof(f));out.write(reinterpret_cast<const char*>(&h),sizeof(h));out.write(reinterpret_cast<const char*>(p.data()),p.size()*4);check(bool(out),"bitmap saved");}
int main(int argc,char**argv){try{
 auto floor=Collision::make({{-5000,0,-5000},{5000,0,-5000},{5000,0,5000},{-5000,0,5000}},{{{0,1,2}},{{0,2,3}}});WaterEffects effects;Vec3 feet{0,2,0};NavigationWaterState wet{100.f,100.f,.65f,WaterFoot::inWater};effects.update(1,1,feet,wet,true,.016f);feet[2]=200;effects.update(1,1,feet,wet,true,.016f);
 auto initial=effects.lines();check(std::count_if(initial.begin(),initial.end(),[](auto&l){return !l.splash;})==72,"three 24-edge rings per movement event");
 std::vector<uint32_t> pixels(1280*720);size_t previousLit=0;for(unsigned frame=0;frame<3;++frame){if(frame)effects.update(1,1,feet,wet,true,.20f);std::fill(pixels.begin(),pixels.end(),0);const auto lines=effects.lines();paint_water(pixels,1280,720,lines,{0,1300,-1800},{0,-.48f,.87726849f},floor,nullptr,{0,0,1280,720,16.f/9});size_t lit=0;for(auto p:pixels)if(p){++lit;check((p&255)==((p>>8)&255)&&((p>>8)&255)==((p>>16)&255),"neutral gray default");}check(lit>150&&lit<=12288,"bounded visible wave pixels");if(frame)check(lit!=previousLit,"rings visibly expand over time");previousLit=lit;if(argc>1){std::filesystem::create_directories(argv[1]);save(pixels,std::filesystem::path(argv[1])/("ripple_"+std::to_string(frame)+".bmp"));}}
 unsigned budget=10;std::fill(pixels.begin(),pixels.end(),0);paint_water(pixels,1280,720,effects.lines(),{0,1300,-1800},{0,-.48f,.87726849f},floor,nullptr,{0,0,1280,720,16.f/9},&budget);check(budget==0,"shared self/remote render budget consumed once");const auto limited=pixels;paint_water(pixels,1280,720,effects.lines(),{0,1300,-1800},{0,-.48f,.87726849f},floor,nullptr,{0,0,1280,720,16.f/9},&budget);check(pixels==limited,"later actors cannot exceed shared budget");
 effects.update(1,1,feet,{},true,.016f);check(effects.lines().empty(),"leaving water clears rendered wakes");std::fill(pixels.begin(),pixels.end(),0);paint_water(pixels,1280,720,effects.lines(),{0,1300,-1800},{0,-.48f,.87726849f},floor,nullptr,{0,0,1280,720,16.f/9});check(std::all_of(pixels.begin(),pixels.end(),[](auto p){return p==0;}),"dry frame has no stale pixel");
 std::cout<<"water_presentation_test PASS: three-ring geometry, expanding gray visible pixels, bounds and dry cleanup\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
