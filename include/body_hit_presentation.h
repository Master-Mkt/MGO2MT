#pragma once
#include "combat_authority.h"
#include "character_model.h"
#include <limits>

namespace mgo2mt::combat::body_hit {
// Visual adapter only: the HOST has already selected the damaged body/life.
// Project its proxy hit onto the currently skinned original model. This never
// changes HP, collision, victim selection, or the authoritative event.
namespace detail {
inline Vec3 add(Vec3 a,Vec3 b){for(unsigned i=0;i<3;++i)a[i]+=b[i];return a;}
inline Vec3 sub(Vec3 a,Vec3 b){for(unsigned i=0;i<3;++i)a[i]-=b[i];return a;}
inline Vec3 mul(Vec3 a,float b){for(auto&v:a)v*=b;return a;}
inline float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline Vec3 cross(Vec3 a,Vec3 b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
inline bool finite(Vec3 a){for(float v:a)if(!std::isfinite(v)||std::abs(v)>1000000)return false;return true;}
inline Vec3 rotate(Vec3 a,float yaw){const float c=std::cos(yaw),s=std::sin(yaw);return {c*a[0]+s*a[2],a[1],-s*a[0]+c*a[2]};}
inline Vec3 edge(Vec3 p,Vec3 a,Vec3 b){const auto ab=sub(b,a);const float d=dot(ab,ab);return add(a,mul(ab,d>1e-9f?std::clamp(dot(sub(p,a),ab)/d,0.f,1.f):0));}
inline Vec3 closest(Vec3 p,Vec3 a,Vec3 b,Vec3 c){
 const auto ab=sub(b,a),ac=sub(c,a),n=cross(ab,ac);const float nn=dot(n,n);
 if(nn>1e-7f){const auto q=sub(p,mul(n,dot(sub(p,a),n)/nn));
  if(dot(cross(ab,sub(q,a)),n)>=0&&dot(cross(sub(c,b),sub(q,b)),n)>=0&&dot(cross(sub(a,c),sub(q,c)),n)>=0)return q;
 }
 auto best=edge(p,a,b);float distance=dot(sub(p,best),sub(p,best));
 for(auto q:{edge(p,b,c),edge(p,c,a)}){const auto d=dot(sub(p,q),sub(p,q));if(d<distance){distance=d;best=q;}}
 return best;
}
}
inline std::optional<Vec3> surface(const Event&e,const CharacterModel&model,Vec3 origin,float yaw,float maximum){
 using namespace detail;
 if(e.kind!=EventKind::damage||!e.hpDamage||!finite(e.position)||!finite(e.normal)||!finite(origin)||!std::isfinite(yaw)||!std::isfinite(maximum)||maximum<=0||maximum>6000||std::abs(dot(e.normal,e.normal)-1)> .002f||model.indices.size()>900000)return {};
 const auto point=rotate(sub(e.position,origin),-yaw),normal=rotate(e.normal,-yaw);
 float nearest=maximum*maximum,rayNearest=nearest;std::optional<Vec3> projected,rayPoint;
 for(size_t i=0;i+2<model.indices.size();i+=3){
  const auto ia=model.indices[i],ib=model.indices[i+1],ic=model.indices[i+2];if(ia>=model.vertices.size()||ib>=model.vertices.size()||ic>=model.vertices.size())return {};
  auto vertex=[&](uint32_t id){const auto&v=model.vertices[id];return Vec3{v.x,v.y,v.z};};const auto a=vertex(ia),b=vertex(ib),c=vertex(ic);if(!finite(a)||!finite(b)||!finite(c))continue;
  auto q=closest(point,a,b,c);float d=dot(sub(q,point),sub(q,point));if(d<=nearest){nearest=d;projected=q;}
  const auto n=cross(sub(b,a),sub(c,a));const float denom=dot(n,normal);
  if(std::abs(denom)>1e-7f){const float t=dot(n,sub(a,point))/denom;q=add(point,mul(normal,t));
   if(t*t<=rayNearest&&dot(sub(q,closest(q,a,b,c)),sub(q,closest(q,a,b,c)))<.05f){rayNearest=t*t;rayPoint=q;}
  }
 }
 const auto result=rayPoint?rayPoint:projected;return result?std::optional(add(origin,rotate(*result,yaw))):std::nullopt;
}
}
