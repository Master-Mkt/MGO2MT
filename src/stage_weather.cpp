#include "stage_weather.h"
#include <algorithm>
#include <cmath>
namespace mgo2mt::stage::weather {
namespace {
float smooth(float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
uint32_t mix(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
float random(uint32_t x){return float(mix(x)&0xffffff)/16777216.f;}
float wrap(float x,float width){return x-std::floor(x/width)*width;}
bool finite(Vec3 p){return std::all_of(p.begin(),p.end(),[](float v){return std::isfinite(v)&&std::abs(v)<1000000;});}
}
float storm_strength(double seconds) noexcept {
 if(!std::isfinite(seconds)||seconds<0)return 0;
 const float t=float(std::fmod(seconds,86400.));
 return smooth(float(std::min(seconds/8.,1.)))*(.80f+.13f*std::sin(t*.21f)+.07f*std::sin(t*.57f));
}
float fog_amount(float distance,const Frame&f) noexcept {
 if(!f.active||!std::isfinite(distance)||distance<=f.nearDistance||f.farDistance<=f.nearDistance)return 0;
 return std::clamp((distance-f.nearDistance)/(f.farDistance-f.nearDistance),0.f,1.f)*std::clamp(f.maximum*f.strength,0.f,1.f);
}
float reverse_depth_distance(float depth) noexcept {
 if(!std::isfinite(depth))return 500000.f;
 return (10.f*500000.f)/(10.f+std::clamp(depth,0.f,1.f)*(500000.f-10.f));
}
Frame Controller::sample(std::string_view stage,double seconds,Vec3 eye,const Collision* collision)const {
 return sample(stage=="n022a",stage=="n022a",seconds,eye,collision);
}
Frame Controller::sample(bool fog,bool sandstorm,double seconds,Vec3 eye,const Collision* collision)const {
 Frame f;if((!fog&&!sandstorm)||!std::isfinite(seconds)||seconds<0||!finite(eye))return f;
 f.active=true;f.strength=sandstorm?storm_strength(seconds):smooth(float(std::min(seconds/8.,1.)));if(!fog){f.maximum=0;f.skyAmount=0;}if(f.strength<=.0001f)return f;
 // Original QQ normal/dust RGB8. A native blend softens the requested continuous storm.
 const std::array<float,3> normal{189.f/255.f,192.f/255.f,129.f/255.f},sand{104.f/255.f,102.f/255.f,47.f/255.f};
 for(size_t i=0;i<3;++i)f.color[i]=normal[i]+(sand[i]-normal[i])*(.35f+.3f*f.strength);
 if(!sandstorm){f.color={.64f,.67f,.70f};return f;}
 if(collision&&collision->ray(eye,{0,1,0},12000)){f.outdoors=false;return f;}
 const float t=float(std::fmod(seconds,600.));f.dust.reserve(128);
 // Camera-centered coverage follows translation, while wind/motion stays world anchored.
 const float anchorX=std::floor(eye[0]/4000.f)*4000.f,anchorZ=std::floor(eye[2]/4000.f)*4000.f;
 for(unsigned i=0;i<128;++i){
  const bool cloud=i<40;const float width=cloud?40000.f:24000.f;
  Vec3 p{anchorX+wrap(random(i*11+1)*width+t*(cloud?2100.f:3900.f)-anchorX,width)-width*.5f,
         eye[1]+(random(i*11+2)-.5f)*(cloud?6500.f:4500.f),
         anchorZ+wrap(random(i*11+3)*width+t*(cloud?650.f:1100.f)-anchorZ,width)-width*.5f};
  Vec3 delta{p[0]-eye[0],p[1]-eye[1],p[2]-eye[2]};const float distance=std::sqrt(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]);
  if(distance<1200.f)continue;
  if(collision){
   if(collision->ray(p,{0,1,0},12000))continue;
   auto hit=collision->ray(eye,{delta[0]/distance,delta[1]/distance,delta[2]/distance},distance);
   if(hit)continue; // Keep broad billboards from bleeding through intervening walls.
  }
  combat::particles::Sprite s;s.position=p;s.radius=cloud?900.f+random(i+511)*1800.f:18.f+random(i+511)*28.f;
  s.rotation=random(i+900)*6.2831853f+t*(cloud?.025f:.8f);s.texture=cloud?0xd39dd8u:0xb8d79bu;
  const float edge=1.f-smooth(std::clamp((std::max(std::abs(delta[0]),std::abs(delta[2]))-width*.32f)/(width*.18f),0.f,1.f));
  const float close=smooth((distance-1200.f)/2200.f);s.rgba={.77f,.70f,.46f,(cloud?.17f:.48f)*f.strength*edge*close};
  if(s.rgba[3]>.001f)f.dust.push_back(s);
 }
 return f;
}
}
