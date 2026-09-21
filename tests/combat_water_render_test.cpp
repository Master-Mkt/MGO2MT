#include "combat_initial_profile.h"
#include "water_effects_overlay.h"
#include <iostream>
using namespace mgo2mt;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main(){try{
 std::vector<stage::Vec3> vertices{{-10000,0,-10000},{10000,0,-10000},{10000,0,10000},{-10000,0,10000},{-10000,1000,-10000},{10000,1000,-10000},{10000,1000,10000},{-10000,1000,10000}};
 std::vector<stage::CollisionTriangle> triangles{{{0,1,2},stage::attribute::native_solid},{{0,2,3},stage::attribute::native_solid},{{4,5,6},0x40048000},{{4,6,7},0x40048000}};
 auto raw=std::make_shared<const stage::Collision>(stage::Collision::make(vertices,triangles));auto water=std::make_shared<const stage::Water>(stage::Water::make({{{0,0,0},{10000,1000,10000}}}));
 combat::Authority host;auto weapons=combat::initial_profiles(21,0,0);host.begin(1,raw,weapons);host.water(water);combat::Identity self{1,10,100};combat::Pose p;p.feet={0,2,0};check(host.join(self,0,p,1000,1000,std::array<uint16_t,1>{25},0),"HOST standing capsule crosses water surface without solid obstruction");host.active(true);
 auto moved=p;moved.feet[0]=350;check(host.pose(self,1,1,moved,100)==combat::Reject::none,"host accepts reduced movement");auto tooFast=moved;tooFast.feet[0]+=500;check(host.pose(self,1,2,tooFast,200)==combat::Reject::too_fast,"host rejects dry speed inside water");
 check(raw->ray({0,3000,0},{0,-1,0},10000)->position[1]==1000,"original bullet/render world still has surface");
 auto moving=stage::movement_collision(raw);stage::WaterEffects effects;stage::NavigationWaterState dry,wet{1000.f,1000.f,.65f,stage::WaterFoot::inWater};effects.update(1,1,p.feet,dry,true,.016f);effects.update(1,1,p.feet,wet,true,.016f);auto lines=effects.lines();check(!lines.empty(),"native entry strokes");
 std::vector<uint32_t> pixels(1280*720);const stage::Vec3 eye{0,2000,-3000},direction{0,-.3f,1};stage::paint_water(pixels,1280,720,lines,eye,direction,*moving);check(std::count_if(pixels.begin(),pixels.end(),[](auto c){return c!=0;})>20,"water strokes visible under actual projection");
 for(int y=0;y<720;++y)for(int x=0;x<1280;++x)if(x<620||x>=1236||y<120||y>=512)check(!pixels[size_t(y)*1280+x],"water respects viewport");
 auto wall=stage::Collision::make({{-10000,0,-1500},{10000,0,-1500},{10000,10000,-1500},{-10000,10000,-1500}},{{{0,1,2},stage::attribute::native_solid},{{0,2,3},stage::attribute::native_solid}});std::fill(pixels.begin(),pixels.end(),0);stage::paint_water(pixels,1280,720,lines,eye,direction,*moving,&wall);check(std::all_of(pixels.begin(),pixels.end(),[](auto p){return p==0;}),"wall occludes water effects");
 std::cout<<"HOST water body/speed and original bullet separation / projected native strokes / viewport and occlusion PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
