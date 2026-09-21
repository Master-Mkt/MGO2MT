#include "stage_weather.h"
#include <cmath>
#include <iostream>
#include <limits>
using namespace mgo2mt::stage;
void require(bool v,const char*m){if(!v)throw std::runtime_error(m);}
int main(){try{
 weather::Controller c;auto empty=c.sample("n001a",12,{0,1800,0});require(!empty.active&&empty.dust.empty(),"QQ-only preset");
 require(!c.sample("n022a",-1,{0,0,0}).active,"invalid elapsed time");
 auto start=c.sample("n022a",0,{0,1800,0}),a=c.sample("n022a",12,{0,1800,0}),b=c.sample("n022a",13,{0,1800,0});
 require(start.active&&start.strength==0&&start.dust.empty(),"stage reset begins clear");require(a.strength>.6f&&!a.dust.empty()&&a.dust.size()<=128,"visible bounded storm");
 require(a.dust[0].position!=b.dust[0].position,"wind moves dust in world");
 require(weather::fog_amount(5000,a)==0,"near combat stays clear");require(weather::fog_amount(20000,a)>0,"distance fog visible");require(weather::fog_amount(50000,a)>weather::fog_amount(20000,a),"fog increases with range");require(weather::fog_amount(500000,a)<.8f,"distant visibility bounded");
 require(std::abs(weather::reverse_depth_distance(1)-10)<.001f&&std::abs(weather::reverse_depth_distance(0)-500000)<.1f,"reverse depth endpoints");
 const float z=40000,d=(10.f*500000.f/z-10.f)/(500000.f-10.f);require(std::abs(weather::reverse_depth_distance(d)-z)<.02f,"far depth reconstruction");
 auto ceiling=Collision::make({{-30000,3000,-30000},{30000,3000,-30000},{30000,3000,30000},{-30000,3000,30000}},{{{0,1,2}},{{0,2,3}}});auto indoor=c.sample("n022a",12,{0,1800,0},&ceiling);require(!indoor.outdoors&&indoor.dust.empty(),"ceiling suppresses indoor dust");
 for(int t=0;t<10000;++t){auto s=weather::storm_strength(t);require(s>=0&&s<=1,"strength bounded over long session");}
 std::cout<<"QQ fog, gradual storm, wind, indoor suppression and reverse depth PASS\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
