#include "original_first_person_lens.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace mgo2mt::original_first_person;
void require(bool b,const char* why){if(!b){std::cerr<<why<<'\n';std::exit(1);}}
int main(){
 require(weapon_class(0)==0&&weapon_class(25)==4&&weapon_class(37)==5,"original class table");
 require(unscoped_aim_zoom(3,0)==1.f&&unscoped_aim_zoom(18,0)==1.2f&&unscoped_aim_zoom(25,0)==1.5f,"weapon base zoom");
 require(unscoped_aim_zoom(52,3)==1.f&&unscoped_aim_zoom(128,3)==1.f,"non firearm does not inherit rifle zoom");
 for(unsigned level=0;level<4;++level){
  require(std::abs(unscoped_aim_zoom(25,level)-1.5f*hawkeye_zoom[level])<0.00001f,"HAWKEYE float table");
  require(aim_zoom(41,level,true)==3.f&&aim_zoom(41,level,true,true)==10.f,"scope excludes HAWKEYE");
  if(level)require(vertical_fov_degrees(25,level,16.f/9.f)<vertical_fov_degrees(25,level-1,16.f/9.f),"zoom narrows lens");
 }
 require(unscoped_aim_zoom(25,255)==unscoped_aim_zoom(25,0),"invalid level cannot amplify skill");
 require(std::abs(horizontal_fov_degrees(25,0)-41.927846f)<0.0001f,"AK horizontal lens inverse");
 require(std::abs(vertical_fov_degrees(25,0,16.f/9.f)-24.324407f)<0.0001f,"native displayed aspect adapter");
 const float a=16.f/9.f,rad=vertical_fov_degrees(25,3,a)/57.29577951308232f;
 require(std::abs(1.f/std::tan(rad*.5f)-horizontal_lens(25,3)*a)<0.00001f,"projection roundtrip");
 require(std::isfinite(vertical_fov_degrees(25,0,0)),"invalid aspect safety");
 std::cout<<"Original weapon lens and HAWKEYE coefficients PASS\n";
}
