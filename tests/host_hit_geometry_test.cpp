#include "host_hit_geometry.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt::host_hit;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
bool close(float a,float b,float e=.02f){return std::abs(a-b)<=e;}
Vec3 center(const BoneTransform&b,const Vec3&o){auto p=b.origin;for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)p[j]+=o[i]*b.axes[i][j];return p;}
Vec3 rotate(const Vec3&v,float y){return {v[0]*std::cos(y)+v[2]*std::sin(y),v[1],-v[0]*std::sin(y)+v[2]*std::cos(y)};}
int main(int argc,char**argv){try{
 std::string error;
 if(argc>1)check(configure(argv[1],error)==ResourceStatus::local_resource,"local resource load");
 BoneTransform identity{0,{{{1,0,0},{0,1,0},{0,0,1}}},{0,0,0}};
 auto hit=intersect({0,0,-10},{0,0,1},8,identity,{0,0,0},{1,1,2});check(hit&&*hit==8,"inclusive end");
 check(!intersect({0,0,-10},{0,0,1},7.99f,identity,{0,0,0},{1,1,2}),"beyond range");
 check(intersect({1,0,-10},{0,0,1},8,identity,{0,0,0},{1,1,2}).has_value(),"parallel boundary");
 check(!intersect({1.01f,0,-10},{0,0,1},100,identity,{0,0,0},{1,1,2}),"outside parallel");
 auto inside=intersect({0,0,0},{0,0,1},100,identity,{0,0,0},{1,1,2});check(inside&&*inside==0,"inside");
 check(!intersect({0,0,10},{0,0,1},100,identity,{0,0,0},{1,1,2}),"behind");
 auto rotated=identity;rotated.axes={{{0,0,-1},{0,1,0},{1,0,0}}};rotated.origin={100,20,30};
 hit=intersect({100,20,20},{0,0,1},100,rotated,{0,0,0},{1,1,2});check(hit&&close(*hit,9),"rotated half extents");
 check(!intersect({0,0,0},{0,0,2},100,identity,{0,0,0},{1,1,2}),"unit direction");
 auto invalid=identity;invalid.axes[0]={2,0,0};check(!intersect({0,0,0},{0,0,1},100,invalid,{0,0,0},{1,1,2}),"scaled transform");
 check(!query({0,0,0},{0,0,1},100,{0,0,0},0,2,Stance::standing),"unknown gender");
 check(pose(0,Stance(3)).empty(),"unknown stance");
 check(!query({0,0,0},{0,0,1},100,{0,0,0},std::numeric_limits<float>::quiet_NaN(),0,Stance::standing),"nan yaw");
 const auto&boxes=mgo2mt::original_hit_regions::boxes;
 for(uint8_t gender=0;gender<2;++gender)for(unsigned stance=0;stance<3;++stance){auto bones=pose(gender,Stance(stance));check(bones.size()==21,"pose count");
  if(using_local_resource())check(bones[4].key==0xF5D387&&bones[14].key==0xF7F0C4,"local original bone indices");
  else check(bones[4].key==5&&bones[14].key==15,"authored proxy bone identifiers");
  for(const auto&box:boxes){auto b=bones[box.bone];auto p=center(b,box.offset);auto origin=p;for(unsigned j=0;j<3;++j)origin[j]-=b.axes[2][j]*2000;
   auto local=intersect(origin,b.axes[2],3000,b,box.offset,box.halfExtent);check(local&&close(*local,2000-box.halfExtent[2]),"all boxes full rotation");}
  auto p=center(bones[4],boxes[0].offset);Vec3 origin{p[0],p[1],p[2]-3000};auto base=query(origin,{0,0,1},6000,{0,0,0},0,gender,Stance(stance));check(base.has_value(),"pose head ray");
  const float yaw=1.17f;const Vec3 feet{15000,-700,4000};auto placed=rotate(origin,yaw);for(unsigned j=0;j<3;++j)placed[j]+=feet[j];
  auto world=query(placed,rotate({0,0,1},yaw),6000,feet,yaw,gender,Stance(stance));check(world&&world->bone==base->bone&&close(world->distance,base->distance),"world actor yaw translation");
  check(!query({0,4000,-3000},{0,0,1},6000,{0,0,0},0,gender,Stance(stance)),"above actor misses");
 }
 auto standing=pose(0,Stance::standing)[4].origin[1],crouching=pose(0,Stance::crouching)[4].origin[1],prone=pose(0,Stance::prone)[4].origin[1];
 check(standing>crouching+200&&crouching>prone+200,"stance head height");
 auto p=center(pose(0,Stance::standing)[4],boxes[0].offset);Vec3 origin{p[0],p[1],p[2]-3000};
 check(!query(origin,{0,0,1},6000,{0,0,0},0,0,Stance::crouching)&&!query(origin,{0,0,1},6000,{0,0,0},0,0,Stance::prone),"standing head ray misses low stance");
 std::cout<<"host hit geometry PASS: "<<(using_local_resource()?"local resource":"authored fallback")<<", 6 poses, 72 oriented boxes, boundary and actor transforms\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
