#include "gekko_jump.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace mgo2win::special_pc {
namespace {
using stage::Vec3;
bool finite(Vec3 v){for(auto x:v)if(!std::isfinite(x)||std::abs(x)>=1000000)return false;return true;}
bool valid(const Jump&j,std::span<const JumpBody> peers){if(!finite(j.feet)||!finite(j.velocity)||j.velocity[1]!=0||std::hypot(j.velocity[0],j.velocity[2])>native_gekko.runSpeed+.01f||j.elapsedMs>native_gekko.jumpMs||peers.size()>24)return false;for(auto&p:peers)if(!finite(p.feet)||!std::isfinite(p.capsule.radius)||!std::isfinite(p.capsule.height)||p.capsule.radius<=0||p.capsule.radius>10000||p.capsule.height<2*p.capsule.radius||p.capsule.height>20000)return false;return true;}
// Minkowski sum of two upright capsules is another upright capsule. Intersect
// the moving feet point with that capsule, including both spherical end caps.
float peer_fraction(Vec3 from,Vec3 delta,const JumpBody& p){
 const auto moving=native_gekko.capsule;const double radius=moving.radius+p.capsule.radius+2.;
 const double ax=p.feet[0],az=p.feet[2],ay=p.feet[1]+p.capsule.radius-(moving.height-moving.radius),by=p.feet[1]+p.capsule.height-p.capsule.radius-moving.radius;
 const double ox=from[0]-ax,oz=from[2]-az,closest=std::clamp(double(from[1]),ay,by),oy=from[1]-closest;
 const double length=std::sqrt(double(delta[0])*delta[0]+double(delta[1])*delta[1]+double(delta[2])*delta[2]);if(length<1e-7)return 1;
 if(ox*ox+oy*oy+oz*oz<=radius*radius){return ox*delta[0]+oy*delta[1]+oz*delta[2]>0?1.f:0.f;}
 const double dx=delta[0]/length,dy=delta[1]/length,dz=delta[2]/length;double nearest=length;
 auto root=[&](double a,double b,double c,auto inside){if(a<1e-12)return;const double discriminant=b*b-a*c;if(discriminant<0)return;const double t=(-b-std::sqrt(discriminant))/a;if(t>=0&&t<nearest&&inside(t))nearest=t;};
 root(dx*dx+dz*dz,ox*dx+oz*dz,ox*ox+oz*oz-radius*radius,[&](double t){const double y=from[1]+dy*t;return y>=ay&&y<=by;});
 for(auto center:{ay,by}){const double y=from[1]-center;root(1.,ox*dx+y*dy+oz*dz,ox*ox+y*y+oz*oz-radius*radius,[](double){return true;});}
 return float(nearest/length);
}
float fraction(Vec3 from,Vec3 delta,const stage::Collision& world,std::span<const JumpBody> peers){float result=1;if(auto h=world.sweep(from,delta,native_gekko.capsule))result=std::min(result,h->fraction);for(auto&p:peers)result=std::min(result,peer_fraction(from,delta,p));return result<1?std::max(0.f,result-.0001f):1;}
void step(Jump& j,uint32_t next,const stage::Collision&w,std::span<const JumpBody> peers){
 const auto travel=[](uint32_t t){return std::clamp(t,jump_move_begin_ms,jump_move_end_ms)-jump_move_begin_ms;};
 const float seconds=float(travel(next)-travel(j.elapsedMs))/1000.f;Vec3 delta{j.horizontalStopped?0:j.velocity[0]*seconds,jump_height(uint16_t(next))-jump_height(uint16_t(j.elapsedMs)),j.horizontalStopped?0:j.velocity[2]*seconds};
 if(j.landed)delta={};else if(j.upwardStopped&&delta[1]>0)delta[1]=0;
 const float amount=fraction(j.feet,delta,w,peers);
 if(amount<1&&delta[1]>0)j.upwardStopped=true;
 if(amount<1&&delta[1]<0){auto support=w.sweep(j.feet,{0,delta[1],0},native_gekko.capsule);if(support&&support->normal[1]>=.70710678f)j.landed=true;}
 if(amount<1&&(delta[0]!=0||delta[2]!=0))j.horizontalStopped=true;
 auto desired=j.feet;for(unsigned i=0;i<3;++i)desired[i]+=delta[i]*amount;if(w.clear(desired,native_gekko.capsule))j.feet=desired;else j.horizontalStopped=true;j.elapsedMs=next;
}
}
std::optional<Jump> begin_jump(stage::Vec3 feet,stage::Vec3 velocity){Jump out{feet,velocity};if(!valid(out,{}))return {};return out;}
float traversal_fraction(Vec3 from,Vec3 delta,const stage::Collision&w,std::span<const JumpBody> peers){
 Jump test{from,{}};if(!finite(delta)||!valid(test,peers))return 0;return fraction(from,delta,w,peers);
}
bool traversal_clear(Vec3 feet,const stage::Collision&w,std::span<const JumpBody> peers){
 Jump test{feet,{}};if(!valid(test,peers)||!w.clear(feet,native_gekko.capsule))return false;
 for(const auto&p:peers){const auto c=native_gekko.capsule;const float y=std::clamp(feet[1],p.feet[1]+p.capsule.radius-(c.height-c.radius),p.feet[1]+p.capsule.height-p.capsule.radius-c.radius);const float r=c.radius+p.capsule.radius+2;
  if((feet[0]-p.feet[0])*(feet[0]-p.feet[0])+(feet[2]-p.feet[2])*(feet[2]-p.feet[2])+(feet[1]-y)*(feet[1]-y)<r*r)return false;}
 return true;
}
bool advance_recovery_fall(RecoveryFall&f,uint64_t now,const stage::Collision&w,std::span<const JumpBody> peers){
 Jump probe{f.feet,{}};if(!valid(probe,peers)||!f.life||!std::isfinite(f.speed)||f.speed<0||f.speed>15000||now<f.at)return false;
 auto next=f;uint32_t remaining=uint32_t((std::min)(now-f.at,uint64_t(1000)));next.at=now;
 while(remaining&&!next.grounded){const auto ms=(std::min)(remaining,16u);remaining-=ms;const float dt=float(ms)/1000;
  const float speed=(std::min)(15000.f,next.speed+9800.f*dt);Vec3 delta{0,-(speed+next.speed)*.5f*dt,0};
  const float amount=fraction(next.feet,delta,w,peers);auto feet=next.feet;feet[1]+=delta[1]*amount;
  if(w.clear(feet,native_gekko.capsule))next.feet=feet;
  if(amount<1){auto support=w.sweep(next.feet,{0,-5,0},native_gekko.capsule);next.grounded=support&&support->normal[1]>=.70710678f;next.speed=0;}else next.speed=speed;
 }
 f=next;return true;
}

bool cancel_jump(Jump&j,const stage::Collision&,std::span<const JumpBody> peers){if(!valid(j,peers))return false;j.horizontalStopped=true;j.upwardStopped=true;return true;}
bool advance_jump(Jump&j,uint32_t elapsed,const stage::Collision&w,std::span<const JumpBody> peers){if(!valid(j,peers)||elapsed<j.elapsedMs)return false;elapsed=std::min<uint32_t>(elapsed,native_gekko.jumpMs);auto next=j;while(next.elapsedMs<elapsed)step(next,std::min(next.elapsedMs+16,elapsed),w,peers);if(elapsed==native_gekko.jumpMs)cancel_jump(next,w,peers);j=next;return true;}
}
