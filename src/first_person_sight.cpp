#include "first_person_sight.h"
#include "weapon_connection_points.h"
#include <algorithm>
#include <cmath>
namespace mgo2mt::first_person_sight {
namespace {
Vec3 sub(Vec3 a,Vec3 b){for(unsigned i=0;i<3;++i)a[i]-=b[i];return a;}
float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
bool finite(Vec3 p){return std::all_of(p.begin(),p.end(),[](float x){return std::isfinite(x);});}
std::optional<Vec3> normalized(Vec3 v){const float length=std::sqrt(dot(v,v));if(!finite(v)||!std::isfinite(length)||length<1e-5f)return {};for(auto&x:v)x/=length;return v;}
std::optional<std::array<Vec3,3>> basis(Vec3 direction){auto f=normalized(direction);if(!f)return {};auto r=normalized({(*f)[2],0,-(*f)[0]});if(!r)r=Vec3{1,0,0};Vec3 u{(*f)[1]*(*r)[2]-(*f)[2]*(*r)[1],(*f)[2]*(*r)[0]-(*f)[0]*(*r)[2],(*f)[0]*(*r)[1]-(*f)[1]*(*r)[0]};return std::array<Vec3,3>{*r,u,*f};}
}
std::optional<Axis> local_axis(uint32_t weapon,const CharacterModel&model,const weapon_hand::connections::Table& points){
 auto found=points.find(weapon,weapon_hand::connections::sight_line);if(!found||model.parts.empty())return {};
 for(const auto&p:model.parts)if(!p.original.present||p.original.mdnSha256!=found->mdnSha256)return {};
 return found->axis;
}
std::optional<Axis> world_axis(const Axis&a,const std::array<float,16>&m,float yaw,Vec3 origin){
 if(!finite(a.rear)||!finite(a.front)||!finite(origin)||!std::isfinite(yaw)||!std::all_of(m.begin(),m.end(),[](float x){return std::isfinite(x);}))return {};
 const float c=std::cos(yaw),s=std::sin(yaw);
 auto point=[&](Vec3 p){Vec3 q{};for(unsigned j=0;j<3;++j)q[j]=p[0]*m[j]+p[1]*m[4+j]+p[2]*m[8+j]+m[12+j];return Vec3{q[0]*c+q[2]*s+origin[0],q[1]+origin[1],-q[0]*s+q[2]*c+origin[2]};};
 Axis result{point(a.rear),point(a.front)};if(!finite(result.rear)||!finite(result.front)||!normalized(sub(result.front,result.rear)))return {};return result;
}
Result Controller::update(const WorldView&baseline,uint32_t weapon,std::optional<Axis>axis){
 Result result;result.view=baseline;if(weapon_!=weapon){reset();weapon_=weapon;}if(!weapon)return result;
 auto axes=basis(baseline.direction);if(!axes||!finite(baseline.eye))return result;
 bool captured=false;
 if(axis&&finite(axis->rear)&&finite(axis->front))if(auto forward=normalized(sub(axis->front,axis->rear))){
  // A non-aiming/corrupt pose cannot move the camera around the opposite side
  // of the actor. The native 1m bound is a guard, not an original camera value.
  const float cosine=dot(*forward,(*axes)[2]);
  const float vertical=std::tan(baseline.verticalFov*.5f),horizontal=vertical*baseline.aspect;
  // Collinearity alone is insufficient: with a mismatched source pose (the
  // current RPG/Javelin adapter) both sights could coincide off screen.
  // A 10% screen-edge margin is a native presentation guard, not aim assist.
  const bool inView=std::isfinite(vertical)&&vertical>0&&std::isfinite(horizontal)&&horizontal>0&&std::abs(dot(*forward,(*axes)[0]))<cosine*horizontal*.9f&&std::abs(dot(*forward,(*axes)[1]))<cosine*vertical*.9f;
  if(cosine>.5f&&inView){const float along=std::min(-100.f,dot(sub(baseline.eye,axis->rear),*forward));Vec3 eye=axis->rear;for(unsigned i=0;i<3;++i)eye[i]+=along*(*forward)[i];auto shift=sub(eye,baseline.eye);const float distance=std::sqrt(dot(shift,shift));
   if(std::isfinite(distance)&&distance<=1000){for(unsigned i=0;i<3;++i)offset_[i]=dot(shift,(*axes)[i]);angle_=std::acos(std::clamp(cosine,-1.f,1.f));ready_=captured=true;}
  }
 }
 // An explicitly supplied but unsuitable stable pose is not a reload hold.
 // Clear an old stance's offset rather than dragging it into this pose.
 if(axis&&!captured){ready_=false;offset_={};angle_=0;}
 if(ready_){for(unsigned j=0;j<3;++j)for(unsigned i=0;i<3;++i)result.view.eye[j]+=offset_[i]*(*axes)[i][j];result.calibrated=true;result.held=!captured;result.displacement=std::sqrt(dot(offset_,offset_));result.axisAngleRadians=angle_;}
 return result;
}
}
