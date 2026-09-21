#include "source_coordinates.h"
#include "stage_navigation.h"
#include "special_pc.h"
#include <algorithm>
#include <cmath>
namespace mgo2mt::stage {
static Vec3 add(Vec3 a,Vec3 b){for(int i=0;i<3;++i)a[i]+=b[i];return a;}
static Vec3 mul(Vec3 a,float b){for(auto&v:a)v*=b;return a;}
static float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
bool Navigation::water(std::shared_ptr<const Water> value,float ratio){
 auto policy=waterPolicy_;policy.horizontalScale=ratio;if(!water_gameplay::valid_policy(policy))return false;
 water_=std::move(value);waterPolicy_=policy;waterState_={};waterTransitionBlocked_=false;return true;
}
bool Navigation::water_policy(water_gameplay::Policy value){
 if(!water_gameplay::valid_policy(value))return false;waterPolicy_=value;waterState_={};return true;
}
void Navigation::sample_water(const Collision& world){
 waterState_=ready_?water_gameplay::sample(water_.get(),world,feet_,shape_,waterPolicy_):NavigationWaterState{};
}
bool Navigation::place(const Collision&world,Vec3 hint,float maximum){
 clear();auto ground=world.ray(hint,{0,-1,0},maximum,query::floor);if(!ground||ground->normal[1]<.70710678f)return false;
 for(unsigned i=0;i<=10;++i){auto feet=ground->position;feet[1]+=shape_.skin*2+shape_.radius*i/10;
  if(!world.clear(feet,shape_))continue;auto floor=world.sweep(feet,{0,-shape_.radius-shape_.skin*4,0},shape_);
  if(!floor||floor->normal[1]<.70710678f||!attribute::has(world.triangles[floor->triangle].attribute,attribute::floor))continue;feet[1]-=(shape_.radius+shape_.skin*4)*floor->fraction;
  feet_=anchor_=feet;ready_=grounded_=true;sample_water(world);return true;
 }return false;
}
Vec3 Navigation::slide(const Collision&world,Vec3 position,Vec3 delta,bool horizontal){
 for(unsigned i=0;i<6&&dot(delta,delta)>.0001f;++i){auto hit=world.sweep(position,delta,shape_);if(!hit){position=add(position,delta);break;}
  position=add(position,mul(delta,hit->fraction));delta=mul(delta,1-hit->fraction);auto normal=hit->normal;
  // Gravity ends at walkable support. Projecting its remaining displacement
  // along that slope would move an idle character sideways on every tick.
  if(!horizontal&&normal[1]>=.70710678f&&vertical_<=0&&attribute::has(world.triangles[hit->triangle].attribute,attribute::floor)){grounded_=true;vertical_=0;break;}
  if(horizontal&&normal[1]>0&&normal[1]<.70710678f){normal[1]=0;float length=std::sqrt(dot(normal,normal));if(length>.001f)normal=mul(normal,1/length);}
  float into=dot(delta,normal);if(into<0)delta=add(delta,mul(normal,-into));else break;
  if(!horizontal&&hit->normal[1]<-.1f&&vertical_>0)vertical_=0;
 }return position;
}
void Navigation::advance(const Collision&world,WalkInput input,float seconds){
 if(!ready_||!std::isfinite(seconds)||seconds<=0)return;
 for(auto v:{input.forward,input.right,input.turn,input.look,input.speed,input.yawRate,input.pitchRate})if(!std::isfinite(v))return;
 if(input.movementYaw&&!std::isfinite(*input.movementYaw))return;
 if(input.speed<0||input.speed>10000)return;
 if(input.yawRate<0||input.yawRate>4||input.pitchRate<0||input.pitchRate>3)return;
 seconds=std::min(seconds,.1f);unsigned steps=unsigned(std::ceil(seconds*120));float dt=seconds/steps;
 input.forward=std::clamp(input.forward,-1.f,1.f);input.right=std::clamp(input.right,-1.f,1.f);
 float magnitude=std::sqrt(input.forward*input.forward+input.right*input.right);if(magnitude>1){input.forward/=magnitude;input.right/=magnitude;}
 for(unsigned step=0;step<steps;++step){
  sample_water(world);
  const auto beforeStep=feet_;const bool wasGrounded=grounded_;
  const bool fixedHorizontal=waterState_.proneBlocked||waterTransitionBlocked_;
  yaw_=std::remainder(yaw_+source_screen_x*std::clamp(input.turn,-1.f,1.f)*input.yawRate*dt,6.283185307f);pitch_=std::clamp(pitch_+std::clamp(input.look,-1.f,1.f)*input.pitchRate*dt,-1.4f,1.4f);
  const float movementYaw=input.movementYaw.value_or(yaw_);
  float sine=std::sin(movementYaw),cosine=std::cos(movementYaw),travel=waterTransitionBlocked_?0:input.speed*dt*waterState_.horizontalScale;Vec3 horizontal{(sine*input.forward+source_screen_x*cosine*input.right)*travel,0,(cosine*input.forward-source_screen_x*sine*input.right)*travel};
  if(shape_.height==560&&water_gameplay::sample(water_.get(),world,add(feet_,horizontal),shape_,waterPolicy_).proneBlocked)horizontal={};
  horizontal=mul(horizontal,world.fall_prevention_fraction(feet_,horizontal,shape_));
  feet_=slide(world,feet_,horizontal,true);
  grounded_=false;vertical_=std::max(-15000.f,vertical_-9800*dt);feet_=slide(world,feet_,{0,vertical_*dt,0},false);
  if(vertical_<=0){auto floor=world.sweep(feet_,{0,-30,0},shape_);if(floor&&floor->normal[1]>=.70710678f&&attribute::has(world.triangles[floor->triangle].attribute,attribute::floor)){feet_[1]-=30*floor->fraction;grounded_=true;vertical_=0;}}
  // A sloping triangle can give gravity/settling a horizontal component even
  // with no input. Keep the strict HOST prone contract without relaxing its
  // speed limit: retain vertical settling only when the fixed-XZ body is clear.
  if(fixedHorizontal||water_gameplay::sample(water_.get(),world,feet_,shape_,waterPolicy_).proneBlocked){
   auto fixed=feet_;fixed[0]=beforeStep[0];fixed[2]=beforeStep[2];
   if(world.clear(fixed,shape_))feet_=fixed;
   else{feet_=beforeStep;vertical_=0;grounded_=wasGrounded;}
  }
  // Unrecovered holes/geometry may have no floor. Return to the checked local
  // anchor instead of accumulating unbounded coordinates or claiming a respawn.
  if(feet_[1]<anchor_[1]-30000){feet_=anchor_;vertical_=0;grounded_=true;}
 }
 waterTransitionBlocked_=false;sample_water(world);
}
Vec3 Navigation::eye()const{auto p=feet_;p[1]+=shape_.height-150;return p;}
void Navigation::rotate_view(WalkInput input,float seconds){
 if(!ready_||!std::isfinite(seconds)||seconds<0)return;
 for(float v:{input.turn,input.look,input.yawRate,input.pitchRate})if(!std::isfinite(v))return;
 if(input.yawRate<0||input.yawRate>4||input.pitchRate<0||input.pitchRate>3)return;seconds=std::min(seconds,.1f);
 yaw_=std::remainder(yaw_+source_screen_x*std::clamp(input.turn,-1.f,1.f)*input.yawRate*seconds,6.283185307f);pitch_=std::clamp(pitch_+std::clamp(input.look,-1.f,1.f)*input.pitchRate*seconds,-1.4f,1.4f);
}
void Navigation::advance_cover(const Collision& world,WalkInput input,float seconds,const combat::cover::State& cover){
 if(!cover.attached){advance(world,input,seconds);return;}
 const auto before=feet_;const bool wasGrounded=grounded_;
 input.forward=0;input.speed=std::min(input.speed,combat::cover::native_policy.slideSpeed);input.movementYaw=cover.normalYaw+3.14159265359f;
 advance(world,input,seconds);
 feet_=combat::cover::projected_destination(world,before,feet_,shape_,cover);
 grounded_=wasGrounded;vertical_=0;sample_water(world);
}
bool Navigation::shape(const Collision&w,Capsule value){if(!ready_||!w.clear(feet_,value))return false;sample_water(w);if(waterState_.proneBlocked&&value.height!=shape_.height)waterTransitionBlocked_=true;shape_=value;sample_water(w);return true;}
Vec3 Navigation::direction()const{float cp=std::cos(pitch_);return {std::sin(yaw_)*cp,std::sin(pitch_),std::cos(yaw_)*cp};}
bool Navigation::track_view(Vec3 target,float seconds){
 if(!ready_||!std::isfinite(seconds)||seconds<0)return false;
 for(float v:target)if(!std::isfinite(v)||std::abs(v)>=1e6f)return false;
 auto origin=eye();Vec3 delta{};for(unsigned i=0;i<3;++i)delta[i]=target[i]-origin[i];
 float horizontal=std::hypot(delta[0],delta[2]);if(horizontal<1e-4f)return false;
 const float yaw=std::atan2(delta[0],delta[2]),pitch=std::atan2(delta[1],horizontal),dt=std::min(seconds,.1f);
 if(std::abs(pitch)>1.4f)return false;
 if(seconds==0)return true;
 yaw_=std::remainder(yaw_+std::clamp(std::remainder(yaw-yaw_,6.283185307f),-2.f*dt,2.f*dt),6.283185307f);
 pitch_=std::clamp(pitch_+std::clamp(pitch-pitch_,-1.5f*dt,1.5f*dt),-1.4f,1.4f);return true;
}
bool Navigation::authoritative(const Collision&world,Vec3 feet,float yaw,float pitch,Capsule shape){
 if(!std::isfinite(yaw)||!std::isfinite(pitch)||std::abs(yaw)>3.14159274f||std::abs(pitch)>1.4f)return false;for(auto v:feet)if(!std::isfinite(v)||std::abs(v)>=1000000)return false;
 const bool human=(shape.radius==260||shape.radius==350)&&(shape.height==1700||shape.height==1100||shape.height==560);
 const bool gekko=shape.radius==special_pc::native_gekko.capsule.radius&&shape.height==special_pc::native_gekko.capsule.height;
 if((!human&&!gekko)||shape.skin!=2||!world.clear(feet,shape))return false;
 feet_=feet;shape_=shape;yaw_=yaw;pitch_=pitch;vertical_=0;ready_=true;grounded_=false;waterTransitionBlocked_=false;sample_water(world);return true;
}

}
