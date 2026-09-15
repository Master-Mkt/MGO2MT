#include "gekko_climb.h"
#include <algorithm>
#include <cmath>
namespace mgo2win::special_pc {
namespace {
using stage::Vec3;
bool finite(Vec3 p){for(auto v:p)if(!std::isfinite(v)||std::abs(v)>=1000000)return false;return true;}
Vec3 difference(Vec3 a,Vec3 b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
bool path(Vec3 a,Vec3 b,const stage::Collision&w,std::span<const JumpBody> peers){return traversal_clear(b,w,peers)&&traversal_fraction(a,difference(b,a),w,peers)>=1;}
bool supported(Vec3 p,const stage::Collision&w){
 // Five floor probes prevent accepting a thin pole or a shelf smaller than the feet.
 const float spread=native_gekko.capsule.radius*.7f;
 for(auto offset:std::array<Vec3,5>{{{0,0,0},{spread,0,0},{-spread,0,0},{0,0,spread},{0,0,-spread}}}){
  auto origin=p;for(unsigned k=0;k<3;++k)origin[k]+=offset[k];origin[1]+=30;
  auto h=w.ray(origin,{0,-1,0},65);if(!h||std::abs(h->normal[1])<.70710678f||std::abs(h->position[1]+2-p[1])>20)return false;
 }return true;
}
Vec3 position(const Climb&c,uint32_t t){Vec3 a,b;float u;
 if(t<1600){a=c.start;b=c.raised;u=float(t)/1600;}
 else if(t<2400){a=c.raised;b=c.across;u=float(t-1600)/800;}
 else{a=c.across;b=c.landing;u=float(t-2400)/200;}
 u=std::clamp(u,0.f,1.f);u=u*u*(3-2*u);for(unsigned k=0;k<3;++k)a[k]+=(b[k]-a[k])*u;return a;
}
}
std::optional<Climb> begin_climb(Vec3 feet,float yaw,const stage::Collision&w,std::span<const JumpBody> peers){
 if(!finite(feet)||!std::isfinite(yaw)||!traversal_clear(feet,w,peers)||!supported(feet,w))return {};
 const Vec3 forward{std::sin(yaw),0,std::cos(yaw)};auto origin=feet;origin[1]+=2000;
 auto wall=w.ray(origin,forward,2500);if(!wall||std::abs(wall->normal[1])>.3f||wall->distance<native_gekko.capsule.radius)return {};
 auto probe=wall->position;probe[0]+=forward[0]*(native_gekko.capsule.radius+150);probe[2]+=forward[2]*(native_gekko.capsule.radius+150);probe[1]=feet[1]+native_gekko.jumpHeight+30;
 auto top=w.ray(probe,{0,-1,0},native_gekko.jumpHeight+30);if(!top||std::abs(top->normal[1])<.70710678f)return {};
 auto landing=top->position;landing[1]+=2;const float height=landing[1]-feet[1];if(height<300||height>native_gekko.jumpHeight||!supported(landing,w))return {};
 auto raised=feet;raised[1]=landing[1]+100;auto across=landing;across[1]+=100;
 if(!path(feet,raised,w,peers)||!path(raised,across,w,peers)||!path(across,landing,w,peers))return {};
 return Climb{feet,raised,across,landing,feet};
}
void cancel_climb(Climb&c){c.cancelled=true;}
bool advance_climb(Climb&c,uint32_t elapsed,const stage::Collision&w,std::span<const JumpBody> peers){
 if(c.cancelled||c.finished||elapsed<c.elapsedMs||c.elapsedMs>climb_ms||!finite(c.start)||!finite(c.raised)||!finite(c.across)||!finite(c.landing)||!finite(c.feet))return false;
 elapsed=(std::min)(elapsed,uint32_t(climb_ms));auto next=c;
 // Recheck the destination and each traversed segment against the current
 // immutable collision revision. Cancellation never snaps through a new object.
 if(!supported(next.landing,w)||!traversal_clear(next.landing,w,peers)){c.cancelled=true;return false;}
 while(next.elapsedMs<elapsed){const auto tick=(std::min)(next.elapsedMs+16,elapsed);const auto p=position(next,tick);
  if(!path(next.feet,p,w,peers)){next.cancelled=true;c=next;return false;}
  next.feet=p;next.elapsedMs=tick;
 }
 next.finished=elapsed==climb_ms;c=next;return true;
}
}
