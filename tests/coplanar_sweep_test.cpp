#include "stage_collision.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace mgo2win::stage;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
Collision plane(float size,float y=0){return Collision::make({{-size,y,-size},{-size,y,size},{size,y,size},{size,y,-size}},{{{0,1,2}},{{0,2,3}}});}
int main(){try{
 for(float size:{2000.f,20000.f,100000.f})for(Capsule shape:{Capsule{350,1700,2},Capsule{350,700,2},Capsule{800,4200,2}}){auto floor=plane(size);
  for(Vec3 delta:{Vec3{150,0,0},Vec3{-150,0,0},Vec3{0,0,150}})for(float margin:{0.f,.01f}){
   Vec3 start=delta[0]>0?Vec3{257.875f,2+margin,380}:delta[0]<0?Vec3{407.875f,2+margin,380}:Vec3{380,2+margin,257.875f};
   check(floor.clear(start,shape)&&!floor.sweep(start,delta,shape),"sphere/capsule tangent traversal crosses coplanar edge freely");delta[1]=.000012f;check(!floor.sweep(start,delta,shape),"tiny upward motion cannot collide with plane below");
  }
  check(bool(floor.sweep({257.875f,1.99f,380},{150,0,0},shape)),"inside inflated skin still reaches conservative contact");
  check(bool(floor.sweep({257.875f,2,380},{150,-.000012f,0},shape)),"tiny downward motion still meets floor");
  const auto down=floor.sweep({0,1000,0},{0,-1200,0},shape);check(down&&down->fraction>0&&down->fraction<1&&down->normal[1]>.9f,"vertical falling floor hit retained");
  auto roof=plane(size,shape.height+502);auto up=roof.sweep({0,2,0},{0,1000,0},shape);check(up&&up->fraction>0&&up->fraction<1&&up->normal[1]<-.9f,"head/ceiling collision retained");
  auto wall=Collision::make({{1000,-10000,-size},{1000,10000,-size},{1000,10000,size},{1000,-10000,size}},{{{0,1,2}},{{0,2,3}}});
  check(bool(wall.sweep({0,2,0},{1000,0,0},shape))&&!wall.sweep({0,2,0},{-1000,0,0},shape),"approaching wall blocked and away movement free");
 }
 auto slope=Collision::make({{-20000,-10000,-20000},{-20000,-10000,20000},{20000,10000,20000},{20000,10000,-20000}},{{{0,1,2}},{{0,2,3}}});
 Capsule shape{350,1700,2};const float y=352*std::sqrt(1.25f)-350+.01f;
 check(!slope.sweep({0,y,0},{150,75,0},shape),"tangent movement outside sloped plane remains free");check(bool(slope.sweep({0,y,0},{150,0,0},shape)),"movement into slope still blocked");
 // Generic non-upright segment API must consider both endpoints, not just the first.
 auto floor=plane(20000);check(bool(floor.sweep_segment({0,500,0},{500,200,0},{0,-100,0},250,2)),"tilted capsule nearer endpoint cannot be culled");
 check(!floor.sweep_segment({0,-352,0},{0,-1352,0},{150,-30,0},350,2)&&bool(floor.sweep_segment({0,-352,0},{0,-1352,0},{150,30,0},350,2)),"same-side guard is symmetric below the plane");
 auto degenerate=Collision::make({{0,0,0},{1000,0,0}},{{{0,1,1}}});check(bool(degenerate.sweep_segment({500,500,0},{500,500,0},{0,-500,0},100,2)),"degenerate triangle retains edge narrowphase");
 std::cout<<"Coplanar guard: strict skin sides,3directions,3scales,sphere/upright/tilted capsules,wall/floor/ceiling/slope/degenerate PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
