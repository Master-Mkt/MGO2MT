#include "source_coordinates.h"
#include "cover_policy.h"
#include <algorithm>
#include <cmath>
namespace mgo2mt::combat::cover {
namespace {
float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vec3 add(Vec3 a,Vec3 b){for(unsigned i=0;i<3;++i)a[i]+=b[i];return a;}
Vec3 sub(Vec3 a,Vec3 b){for(unsigned i=0;i<3;++i)a[i]-=b[i];return a;}
Vec3 mul(Vec3 a,float s){for(auto& v:a)v*=s;return a;}
Vec3 facing(float yaw){return {std::sin(yaw),0,std::cos(yaw)};}
bool finite(Vec3 p){for(auto v:p)if(!std::isfinite(v)||std::abs(v)>=1000000)return false;return true;}
bool supported(stage::Capsule c){return (c.radius==260||c.radius==350)&&(c.height==1700||c.height==1100)&&c.skin==2;}
bool ground(const stage::Collision& w,Vec3 feet){auto p=feet;p[1]+=40;auto hit=w.ray(p,{0,-1,0},90,stage::query::player_floor);return hit&&std::abs(hit->normal[1])>.65f&&std::abs(hit->position[1]-feet[1])<=40;}
std::optional<Contact> wall(const stage::Collision&w,Vec3 feet,stage::Capsule c,Vec3 direction){
 std::optional<Contact> result;
 for(float ratio:{.4f,.8f}){auto p=feet;p[1]+=c.height*ratio;auto hit=w.ray(p,direction,c.radius+native_policy.probeGap);
  if(!hit||std::abs(hit->normal[1])>.15f)return {};auto n=hit->normal;if(dot(n,direction)>0)n=mul(n,-1);
  const float length=std::hypot(n[0],n[2]);if(length<.98f)return {};n={n[0]/length,0,n[2]/length};
  if(result&&(dot(n,result->normal)<.995f||std::abs(dot(n,sub(hit->position,result->point)))>3))return {};
  result=Contact{n,hit->position};
 }return result;
}
bool same_wall(const stage::Collision&w,Vec3 feet,stage::Capsule c,const State&s){auto n=facing(s.normalYaw);auto hit=wall(w,feet,c,mul(n,-1));return hit&&dot(hit->normal,n)>.995f;}
bool clear_upper(const stage::Collision&w,Vec3 feet,stage::Capsule c,Vec3 offset){
 auto a=feet,b=feet;a[1]+=c.height*.5f;b[1]+=c.height-100;
 if(auto hit=w.sweep_segment(a,b,offset,native_policy.upperRadius,2);hit&&hit->fraction<.9999f)return false;
 const auto contacts=w.contacts(add(a,offset),add(b,offset),native_policy.upperRadius,2,1);return contacts.empty();
}
}
std::optional<Contact> acquire(const stage::Collision&w,Vec3 feet,stage::Capsule c,float yaw){
 if(!finite(feet)||!std::isfinite(yaw)||!supported(c)||!ground(w,feet)||!w.clear(feet,c))return {};
 auto hit=wall(w,feet,c,facing(yaw));if(!hit||dot(hit->normal,facing(yaw))>-.7f)return {};return hit;
}
Vec3 projected_destination(const stage::Collision&w,Vec3 feet,Vec3 requested,stage::Capsule c,const State&s){
 if(!valid(s)||!finite(feet)||!finite(requested))return feet;if(!s.attached)return requested;
 if(!supported(c)||!same_wall(w,feet,c,s))return feet;
 auto delta=sub(requested,feet);const auto n=facing(s.normalYaw);delta=sub(delta,mul(n,dot(delta,n)));
 auto good=[&](float t){const auto p=add(feet,mul(delta,t));if(!w.clear(p,c)||!ground(w,p)||!same_wall(w,p,c,s))return false;auto hit=w.sweep(feet,mul(delta,t),c);return !hit||hit->fraction>=.9999f;};
 if(good(1))return add(feet,delta);float lo=0,hi=1;for(unsigned i=0;i<16;++i){float mid=(lo+hi)*.5f;if(good(mid))lo=mid;else hi=mid;}return add(feet,mul(delta,lo));
}
Vec3 eye_offset(const State&s,float yaw){if(!valid(s)||!std::isfinite(yaw)||!s.lean)return {};const auto angle=s.attached?s.normalYaw+3.14159265359f:yaw;return {source_screen_x*std::cos(angle)*float(s.lean)*(s.attached?native_policy.coverLean:native_policy.freeLean),0,-source_screen_x*std::sin(angle)*float(s.lean)*(s.attached?native_policy.coverLean:native_policy.freeLean)};}
Vec3 eye(Vec3 feet,stage::Capsule c,const State&s,float yaw){feet[1]+=c.height-150;return add(feet,eye_offset(s,yaw));}
State evaluate(const stage::Collision&w,Vec3 feet,stage::Capsule c,float yaw,State s,int8_t requestedLean,bool firstPerson){
 if(!valid(s)||!finite(feet)||!supported(c)||!std::isfinite(yaw)||!w.clear(feet,c)||!ground(w,feet))return {};
 if(s.attached&&!same_wall(w,feet,c,s))return {};s.lean=0;
 if(requestedLean< -1||requestedLean>1||(!s.attached&&!firstPerson)||!requestedLean)return s;
 auto candidate=s;candidate.lean=requestedLean;const auto offset=eye_offset(candidate,yaw);if(!clear_upper(w,feet,c,offset))return s;
 if(s.attached){
  // Both torso and eye probes must pass the actual finite wall end. A low
  // aperture or a merely different corner normal does not authorize popout.
  auto forward=mul(facing(s.normalYaw),-1);
  for(float height:{c.height*.55f,c.height-150}){auto p=add(feet,offset);p[1]+=height;if(w.ray(p,forward,c.radius+native_policy.probeGap))return s;}
 }
 return candidate;
}
bool fire_allowed(const stage::Collision&w,Vec3 feet,stage::Capsule c,float yaw,const State&s){
 if(!valid(s)||(s.attached&&!s.lean))return false;
 if(!s.attached&&!s.lean)return true;
 return evaluate(w,feet,c,yaw,s,s.lean,true)==s;
}
}
