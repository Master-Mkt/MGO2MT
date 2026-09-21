#include "stage_precipitation.h"
#include <algorithm>
#include <cmath>
namespace mgo2mt::stage::weather {
namespace {
float random(unsigned x){x^=x>>16;x*=0x7feb352d;x^=x>>15;x*=0x846ca68b;x^=x>>16;return float(x&0xffffff)/16777216.f;}
float wrap(float x,float extent){return x-std::floor(x/extent)*extent;}
}
std::vector<combat::tracers::Segment> precipitation(const Settings&s,double seconds,Vec3 eye,const Collision*collision){
 std::vector<combat::tracers::Segment> result;if(!s.enabled||!std::isfinite(seconds)||seconds<0||!std::isfinite(s.intensity)||s.intensity<=0||s.intensity>1)return result;for(float v:eye)if(!std::isfinite(v)||std::abs(v)>1000000)return result;
 const bool rain=s.rainEnabled.value_or(s.preset==Preset::rain),snow=s.snowEnabled.value_or(s.preset==Preset::snow);if(!rain&&!snow)return result;if(collision&&collision->ray(eye,{0,1,0},30000))return result;
 const float t=float(std::fmod(seconds,600.));result.reserve(256);constexpr float width=12000,height=5000;
 const Vec3 anchor{std::floor(eye[0]/2000)*2000,std::floor(eye[1]/2000)*2000,std::floor(eye[2]/2000)*2000};
 for(unsigned kind=0;kind<2;++kind){if(kind?!snow:!rain)continue;for(unsigned i=0;i<128;++i){unsigned key=i+kind*991;const float drift=kind?750.f:350.f,speed=kind?950.f:12500.f;
  Vec3 p{anchor[0]+wrap(random(key*7+1)*width+t*drift-anchor[0],width)-width/2,anchor[1]+wrap(random(key*7+2)*height-t*speed-anchor[1],height)-height/2,anchor[2]+wrap(random(key*7+3)*width+t*drift*.35f-anchor[2],width)-width/2};
  if(kind)p[0]+=std::sin(t*1.2f+key)*100;Vec3 delta{p[0]-eye[0],p[1]-eye[1],p[2]-eye[2]};const float d=std::hypot(delta[0],delta[1],delta[2]);if(d<350||d>9000)continue;
  if(collision&&(collision->ray(p,{0,1,0},30000)||collision->ray(eye,{delta[0]/d,delta[1]/d,delta[2]/d},d)))continue;
  combat::tracers::Segment line;line.to=p;line.from={p[0]-(kind?7.f:15.f),p[1]+(kind?16.f:420.f),p[2]};line.opacity=s.intensity*(kind?.85f:.65f)*std::clamp((9000-d)/4000,0.f,1.f);line.widthPixels=kind?2.f:.85f;line.color=kind?Vec3{.94f,.96f,1.f}:Vec3{.65f,.76f,.87f};if(line.opacity>.001f)result.push_back(line);
 }}return result;
}
}
