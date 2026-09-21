#include "world_depth.h"
#include "enemy_tag_target.h"
#include "stage_navigation.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){try{
 using namespace DirectX;
 auto floor=stage::Collision::make({{-10000,0,-10000},{-10000,0,10000},{10000,0,10000},{10000,0,-10000}},{{{0,1,2}},{{0,2,3}}});
 for(float yaw:{0.f,.7f,-2.1f}){
  stage::Navigation n;check(n.place(floor,{0,100,0}),"floor");n.facing(yaw);auto before=n.feet(),eye=n.eye(),direction=n.direction();
  n.advance(floor,{0,1,0,0,1000},.1f);auto after=n.feet();
  auto target=before;for(int i=0;i<3;++i)target[i]+=direction[i]*2000+(after[i]-before[i]);target[1]=eye[1];
  auto p=enemy_tag::project(target,eye,direction,0,0,1280,720,16.f/9);check(p&&p->x>640,"right stick moves toward screen right");
  auto view=XMMatrixLookToLH(XMVectorSet(eye[0],eye[1],eye[2],1),XMVectorSet(direction[0],direction[1],direction[2],0),XMVectorSet(0,1,0,0));
  auto clip=XMVector3TransformCoord(XMVectorSet(target[0],target[1],target[2],1),view*world_projection(16.f/9));
  check(std::abs((XMVectorGetX(clip)+1)*640-p->x)<1.01f,"GPU and CPU horizontal projection agree");
  n.rotate_view({0,0,1,0},.1f);check(n.yaw()<yaw,"right camera input turns to source negative yaw");
 }
 auto left=enemy_tag::project({100,0,1000},{0,0,0},{0,0,1},0,0,1280,720,16.f/9);
 check(left&&left->x<640,"source positive X is screen left when looking positive Z");
 auto nearPoint=XMVector3TransformCoord(XMVectorSet(0,0,10,1),world_projection(1));auto farPoint=XMVector3TransformCoord(XMVectorSet(0,0,500000,1),world_projection(1));
 check(std::abs(XMVectorGetZ(nearPoint)-1)<1e-5&&std::abs(XMVectorGetZ(farPoint))<1e-5,"reverse Z unchanged");
 for(float fov:{.15f,.4f,1.f,1.5f})for(float aspect:{1.f,16.f/9}){
  const combat::Vec3 point{20,30,1000};auto p=enemy_tag::project(point,{0,0,0},{0,0,1},0,0,1280,720,aspect,fov);check(p.has_value(),"zoom point remains visible");
  auto clip=XMVector3TransformCoord(XMVectorSet(20,30,1000,1),world_projection(aspect,fov));check(std::abs((XMVectorGetX(clip)+1)*640-p->x)<1.01f&&std::abs((1-XMVectorGetY(clip))*360-p->y)<1.01f,"GPU and CPU labels follow same lens on both axes");
  auto depth=XMVector3TransformCoord(XMVectorSet(0,0,1000,1),world_projection(aspect));check(XMVectorGetZ(clip)==XMVectorGetZ(depth),"lens does not alter reverse depth");
 }
 for(float fov:{0.f,-1.f,3.2f,std::numeric_limits<float>::quiet_NaN()})check(!enemy_tag::project({0,0,1000},{0,0,0},{0,0,1},0,0,1280,720,16.f/9,fov),"invalid lens cannot project a HUD target");
 combat::cover::State state;state.lean=1;check(combat::cover::eye_offset(state,0)[0]<0,"right lean follows source camera right");
 std::cout<<"Source camera/CPU projection/navigation/lean/depth PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
