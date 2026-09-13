#include "stage_navigation.h"
#include <algorithm>
#include <cmath>
namespace mgo2win::stage {
static Vec3 add(Vec3 a,Vec3 b){for(int i=0;i<3;++i)a[i]+=b[i];return a;}
static Vec3 mul(Vec3 a,float b){for(auto&v:a)v*=b;return a;}
static float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
bool Navigation::water(std::shared_ptr<const Water> value,float ratio){
 if(!std::isfinite(ratio)||ratio<=0||ratio>1)return false;
 water_=std::move(value);waterRatio_=ratio;waterState_={};return true;
}
void Navigation::sample_water(const Collision& world){
 waterState_={};if(!ready_||!water_)return;
 auto p=feet_;p[1]+=shape_.skin*2;
 auto floor=world.ray(p,{0,-1,0},shape_.height+shape_.skin*4);
 if(!floor||floor->normal[1]<.70710678f)return;
 auto level=water_->control_level(feet_,floor->position[1]);if(!level)return;
 waterState_.level=level;waterState_.depthAboveFloor=std::max(0.f,*level-floor->position[1]);
 waterState_.foot=Water::classify(*level,floor->position[1],feet_[1]);
 if(waterState_.foot==WaterFoot::inWater)waterState_.horizontalScale=waterRatio_;
}
bool Navigation::place(const Collision&world,Vec3 hint,float maximum){
 clear();auto ground=world.ray(hint,{0,-1,0},maximum);if(!ground||ground->normal[1]<.70710678f)return false;
 for(unsigned i=0;i<=10;++i){auto feet=ground->position;feet[1]+=shape_.skin*2+shape_.radius*i/10;
  if(!world.clear(feet,shape_))continue;auto floor=world.sweep(feet,{0,-shape_.radius-shape_.skin*4,0},shape_);
  if(!floor||floor->normal[1]<.70710678f)continue;feet[1]-=(shape_.radius+shape_.skin*4)*floor->fraction;
  feet_=anchor_=feet;ready_=grounded_=true;sample_water(world);return true;
 }return false;
}
Vec3 Navigation::slide(const Collision&world,Vec3 position,Vec3 delta,bool horizontal){
 for(unsigned i=0;i<6&&dot(delta,delta)>.0001f;++i){auto hit=world.sweep(position,delta,shape_);if(!hit){position=add(position,delta);break;}
  position=add(position,mul(delta,hit->fraction));delta=mul(delta,1-hit->fraction);auto normal=hit->normal;
  if(horizontal&&normal[1]>0&&normal[1]<.70710678f){normal[1]=0;float length=std::sqrt(dot(normal,normal));if(length>.001f)normal=mul(normal,1/length);}
  float into=dot(delta,normal);if(into<0)delta=add(delta,mul(normal,-into));else break;
  if(!horizontal){if(hit->normal[1]>=.70710678f&&vertical_<=0){grounded_=true;vertical_=0;}else if(hit->normal[1]<-.1f&&vertical_>0)vertical_=0;}
 }return position;
}
void Navigation::advance(const Collision&world,WalkInput input,float seconds){
 if(!ready_||!std::isfinite(seconds)||seconds<=0)return;
 for(auto v:{input.forward,input.right,input.turn,input.look,input.speed,input.yawRate,input.pitchRate})if(!std::isfinite(v))return;
 if(input.speed<0||input.speed>10000)return;
 if(input.yawRate<0||input.yawRate>4||input.pitchRate<0||input.pitchRate>3)return;
 seconds=std::min(seconds,.1f);unsigned steps=unsigned(std::ceil(seconds*120));float dt=seconds/steps;
 input.forward=std::clamp(input.forward,-1.f,1.f);input.right=std::clamp(input.right,-1.f,1.f);
 float magnitude=std::sqrt(input.forward*input.forward+input.right*input.right);if(magnitude>1){input.forward/=magnitude;input.right/=magnitude;}
 for(unsigned step=0;step<steps;++step){
  sample_water(world);
  yaw_=std::remainder(yaw_+std::clamp(input.turn,-1.f,1.f)*input.yawRate*dt,6.283185307f);pitch_=std::clamp(pitch_+std::clamp(input.look,-1.f,1.f)*input.pitchRate*dt,-1.4f,1.4f);
  float sine=std::sin(yaw_),cosine=std::cos(yaw_),travel=input.speed*dt*waterState_.horizontalScale;Vec3 horizontal{(sine*input.forward+cosine*input.right)*travel,0,(cosine*input.forward-sine*input.right)*travel};
  feet_=slide(world,feet_,horizontal,true);grounded_=false;vertical_=std::max(-15000.f,vertical_-9800*dt);feet_=slide(world,feet_,{0,vertical_*dt,0},false);
  if(vertical_<=0){auto floor=world.sweep(feet_,{0,-30,0},shape_);if(floor&&floor->normal[1]>=.70710678f){feet_[1]-=30*floor->fraction;grounded_=true;vertical_=0;}}
  // Unrecovered holes/geometry may have no floor. Return to the checked local
  // anchor instead of accumulating unbounded coordinates or claiming a respawn.
  if(feet_[1]<anchor_[1]-30000){feet_=anchor_;vertical_=0;grounded_=true;}
 }
 sample_water(world);
}
Vec3 Navigation::eye()const{auto p=feet_;p[1]+=shape_.height-150;return p;}
bool Navigation::shape(const Collision&w,Capsule value){if(!ready_||!w.clear(feet_,value))return false;shape_=value;return true;}
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
 if((shape.radius!=260&&shape.radius!=350)||shape.skin!=2||(shape.height!=1700&&shape.height!=1100&&shape.height!=560)||!world.clear(feet,shape))return false;
 feet_=feet;shape_=shape;yaw_=yaw;pitch_=pitch;vertical_=0;ready_=true;grounded_=false;sample_water(world);return true;
}

}
