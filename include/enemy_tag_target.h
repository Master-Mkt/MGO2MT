#include "source_coordinates.h"
#pragma once
#include "combat_authority.h"
#include "host_roster.h"
#include "camera_projection.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace mgo2mt::enemy_tag {
using combat::Vec3;
inline float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline Vec3 sub(Vec3 a,Vec3 b){for(int i=0;i<3;++i)a[i]-=b[i];return a;}
inline Vec3 add(Vec3 a,Vec3 b){for(int i=0;i<3;++i)a[i]+=b[i];return a;}
inline Vec3 mul(Vec3 a,float s){for(auto&v:a)v*=s;return a;}
inline bool finite(Vec3 a){return std::all_of(a.begin(),a.end(),[](float v){return std::isfinite(v)&&std::abs(v)<1e6f;});}
inline std::optional<Vec3> unit(Vec3 a){float n=dot(a,a);if(!finite(a)||n<1e-8f||!std::isfinite(n))return {};return mul(a,1/std::sqrt(n));}
// Native capsule query, including the prone sphere/parallel-ray boundaries.
inline std::optional<float> capsule(Vec3 origin,Vec3 direction,const combat::Pose&p){
 float r=p.capsule.radius,h=p.capsule.height;if(!finite(p.feet)||!std::isfinite(r)||!std::isfinite(h)||r<=0||h<2*r)return {};
 auto a=p.feet,b=a;a[1]+=r;b[1]+=h-r;auto ba=sub(b,a),oa=sub(origin,a);
 float bb=dot(ba,ba),bd=dot(ba,direction),bo=dot(ba,oa),od=dot(oa,direction),oo=dot(oa,oa);
 auto closest=bb>1e-6f?add(a,mul(ba,std::clamp(bo/bb,0.f,1.f))):a;
 if(dot(sub(origin,closest),sub(origin,closest))<=r*r)return 0.f;
 float nearest=std::numeric_limits<float>::infinity(),A=bb-bd*bd,B=bb*od-bo*bd,C=bb*oo-bo*bo-r*r*bb,D=B*B-A*C;
 if(A>1e-5f&&D>=0){float t=(-B-std::sqrt(D))/A,y=bo+t*bd;if(t>=0&&y>=0&&y<=bb)nearest=t;}
 for(auto center:{a,b}){auto q=sub(origin,center);float v=dot(q,direction),d=v*v-dot(q,q)+r*r;if(d>=0){float t=-v-std::sqrt(d);if(t>=0)nearest=std::min(nearest,t);}}
 return std::isfinite(nearest)?std::optional(nearest):std::nullopt;
}
struct Target {combat::Identity identity;Vec3 head;float distance=0;};
// Visibility-only Windows policy. No aim assistance or weapon authority changes.
// All living bodies block the ray, including friendly bodies. Unknown teams/rules
// never establish an enemy. Range follows the existing renderer's far plane.
inline std::optional<Target> select(const combat::Snapshot&s,const host::Roster&r,combat::Identity self,
 unsigned rule,Vec3 origin,Vec3 direction,const stage::Collision&world,bool enabled,const stage::Collision*objects=nullptr){
 if(!enabled||!s.epoch||!r.complete||self.slot>=24||!finite(origin))return {};
 const auto&me=s.players[self.slot];if(!me||me->identity!=self||!me->alive||me->stunned||(rule!=0&&rule!=1))return {};
 auto rd=unit(direction);if(!rd)return {};float limit=500000.f;
 // Native aiming selection follows bullet contacts. Eye visibility below is
 // independently filtered by StopEye; GEOM camera barriers are not sight walls.
 if(auto hit=world.ray(origin,*rd,limit,stage::query::bullet))limit=hit->distance;
 if(objects)if(auto hit=objects->ray(origin,*rd,limit,stage::query::bullet))limit=hit->distance;
 const combat::Player* nearest=nullptr;
 for(const auto&p:s.players)if(p&&p->identity!=self&&p->alive){auto t=capsule(origin,*rd,p->pose);if(t&&*t<limit){limit=*t;nearest=&*p;}}
 if(!nearest||nearest->identity.slot>=24||!nearest->specialPc.nameVisible)return {};
 if(rule==1&&(me->team<1||me->team>2||nearest->team<1||nearest->team>2||me->team==nearest->team))return {};
 const auto&entry=r.slots[nearest->identity.slot];
 if(!entry||entry->slot!=nearest->identity.slot||entry->instance!=nearest->identity.instance||entry->character!=nearest->identity.character||entry->name.empty())return {};
 auto head=nearest->pose.feet;head[1]+=nearest->pose.capsule.height+90;
 return Target{nearest->identity,head,limit};
}
inline bool visible(Vec3 eye,Vec3 point,const stage::Collision&world,const stage::Collision*objects=nullptr){
 if(!finite(eye)||!finite(point))return false;auto delta=sub(point,eye);auto rd=unit(delta);if(!rd)return false;
 float distance=std::sqrt(dot(delta,delta));
 if(auto hit=world.ray(eye,*rd,distance,stage::query::stop_eye);hit&&hit->distance<distance)return false;
 // GM_HIT-only object proxies are explicitly tagged native Bullet targets;
 // no original GEOM StopEye bit is manufactured on those separate volumes.
 if(objects)if(auto hit=objects->ray(eye,*rd,distance,stage::query::bullet);hit&&hit->distance<distance)return false;
 return true;
}
struct Point {int x=0,y=0;};
struct Viewport {int left=620,top=120,width=616,height=392;float aspect=616.f/392.f,verticalFov=default_vertical_fov;
 bool valid()const{return left>=0&&top>=0&&width>0&&height>0&&width<=1280&&height<=720&&left<=1280-width&&top<=720-height&&std::isfinite(aspect)&&aspect>0&&aspect<=32&&valid_vertical_fov(verticalFov);}
};
// Matches the world camera FOV and aspect; the destination UI rectangle is independent.
inline std::optional<Point> project(Vec3 point,Vec3 eye,Vec3 direction,int left,int top,int width,int height,float aspect=616.f/392.f,float verticalFov=default_vertical_fov){
 if(width<=0||height<=0||!std::isfinite(aspect)||aspect<=0||aspect>32||!valid_vertical_fov(verticalFov)||!finite(point)||!finite(eye))return {};auto z=unit(direction);if(!z)return {};
 auto x=unit(Vec3{(*z)[2],0,-(*z)[0]});if(!x)return {};
 Vec3 y{(*z)[1]*(*x)[2]-(*z)[2]*(*x)[1],(*z)[2]*(*x)[0]-(*z)[0]*(*x)[2],(*z)[0]*(*x)[1]-(*z)[1]*(*x)[0]};
 auto delta=sub(point,eye);float depth=dot(delta,*z);if(depth<10||depth>=500000)return {};
 float nx=source_screen_x*dot(delta,*x)/(depth*std::tan(verticalFov*.5f)*aspect),ny=dot(delta,y)/(depth*std::tan(verticalFov*.5f));
 if(!std::isfinite(nx)||!std::isfinite(ny)||std::abs(nx)>1||std::abs(ny)>1)return {};
 return Point{left+int((nx+1)*.5f*width),top+int((1-ny)*.5f*height)};
}
}
