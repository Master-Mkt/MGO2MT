#include "cover_policy.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include "stage_water.h"
using namespace mgo2mt;
namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
stage::Collision scene(bool floor=true,bool corner=false){
 std::vector<stage::Vec3> v;std::vector<stage::CollisionTriangle> t;
 auto quad=[&](stage::Vec3 a,stage::Vec3 b,stage::Vec3 c,stage::Vec3 d){unsigned i=unsigned(v.size());v.insert(v.end(),{a,b,c,d});t.push_back({{i,i+1,i+2}});t.push_back({{i,i+2,i+3}});};
 if(floor)quad({-10000,0,-10000},{-10000,0,10000},{10000,0,10000},{10000,0,-10000});
 quad({-2000,0,0},{0,0,0},{0,2500,0},{-2000,2500,0});
 if(corner)quad({50,0,-1000},{50,0,1000},{50,2500,1000},{50,2500,-1000});
 return stage::Collision::make(v,t);
}
}
int main(int argc,char**argv){try{
 using namespace combat::cover;auto world=scene();stage::Capsule body{260,1700,2};stage::Vec3 feet{-1000,0,-300};
 auto c=acquire(world,feet,body,0);check(bool(c)&&c->normal[2]<-.99f,"real vertical wall admitted");State state{true,0,std::atan2(c->normal[0],c->normal[2])};
 check(!acquire(world,feet,body,3.14159265f),"wall behind player cannot attach");check(!acquire(scene(false),feet,body,0),"unsupported floor rejected");
 check(!acquire(world,{-1000,500,-300},body,0),"floating wall rejected");check(!acquire(world,feet,{260,560,2},0),"prone rejected");
 auto projected=projected_destination(world,feet,{-700,0,-200},body,state);check(std::abs(projected[0]+700)<.01f&&std::abs(projected[2]+300)<.01f,"parallel projection preserves wall separation");
 auto end=projected_destination(world,feet,{1000,0,-300},body,state);check(end[0]<=.01f&&end[0]>-420,"finite wall endpoint clamps");
 check(evaluate(world,feet,body,0,state,1,false).lean==0&&!fire_allowed(world,feet,body,0,state),"middle of wall cannot popout or shoot");
 feet={-100,0,-300};auto peek=evaluate(world,feet,body,0,state,-1,false);check(peek.lean==-1&&fire_allowed(world,feet,body,0,peek),"source +X wall end permits screen-left popout");check(evaluate(world,feet,body,0,state,1,false).lean==0,"opposite wall side still covered");
 check(eye(feet,body,peek,0)[0]>300,"admitted eye follows exposed upper body");check(evaluate(scene(true,true),feet,body,0,state,-1,false).lean==0,"corner blocker disallows eye/body crossing");
 check(evaluate(world,{-1000,0,-2000},body,0,{},1,false).lean==0,"free lean needs first-person intent");auto free=evaluate(world,{-1000,0,-2000},body,0,{},1,true);check(free.lean==1&&!free.attached&&eye_offset(free,0)[0]==-125,"bounded free first-person lean");
 check(!valid(State{false,1,1})&&!valid(Intent{0,Action::attach})&&!valid(Intent{1,Action::none}),"canonical states and request edge");
 for(int index=1;index<argc;++index){std::ifstream file(argv[index]);check(bool(file),"original stage collision open");auto original=stage::movement_collision(std::make_shared<stage::Collision>(stage::Collision::read(file)));bool found=false;
  for(const auto&t:original->triangles){auto a=original->vertices[t.vertices[0]],b=original->vertices[t.vertices[1]],c0=original->vertices[t.vertices[2]];stage::Vec3 u{},v{},normal{},center{};for(unsigned j=0;j<3;++j){u[j]=b[j]-a[j];v[j]=c0[j]-a[j];center[j]=(a[j]+b[j]+c0[j])/3;}
   normal={u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]};float len=std::sqrt(normal[0]*normal[0]+normal[1]*normal[1]+normal[2]*normal[2]);if(len<1||std::abs(normal[1]/len)>.15f)continue;for(auto& n:normal)n/=len;
   for(float sign:{-1.f,1.f}){auto p=center;for(unsigned j=0;j<3;++j)p[j]+=normal[j]*sign*320;p[1]+=850;auto floorHit=original->ray(p,{0,-1,0},3500);if(!floorHit)continue;p[1]=floorHit->position[1]+2;auto contact=acquire(*original,p,body,std::atan2(-normal[0]*sign,-normal[2]*sign));if(contact){found=true;break;}}
   if(found)break;
  }check(found,"original stage contains admitted real wall contact");std::cout<<"original_stage_cover="<<argv[index]<<" PASS\n";
 }
 std::cout<<"Cover policy real triangles, floor, finite edge, corner, bounded body/eye exposure PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
