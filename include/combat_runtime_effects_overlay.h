#pragma once
#include "combat_particle_effects.h"
#include "material_effects_overlay.h"
namespace mgo2win::combat::runtime_effects {
// Native procedural presentation only. Flames follow the current HOST burning
// flag; smoke/casings originate exclusively from validated HOST events.
inline std::vector<material_effects::Line> lines(std::span<const particles::Segment> particles,const Snapshot& s,uint64_t now){
 std::vector<material_effects::Line> result;result.reserve(256);
 for(const auto&p:s.players)if(p&&p->alive&&p->burning){
  for(unsigned n=0;n<4;++n){float phase=float((now+uint64_t(n)*173)%900)/900.f,angle=float(n)*1.57079633f+float(now%2000)*.003f;auto from=p->pose.feet;
   from[0]+=std::sin(angle)*p->pose.capsule.radius*.65f;from[2]+=std::cos(angle)*p->pose.capsule.radius*.65f;from[1]+=phase*p->pose.capsule.height*.7f;auto to=from;to[1]+=250+phase*150;to[0]+=std::sin(angle+phase)*65;
   result.push_back({from,to,{1.f,.35f+phase*.4f,.06f,.85f},0,material_effects::Kind::unknown});
  }
 }
 for(const auto&p:particles){if(result.size()==256)break;result.push_back({p.from,p.to,p.rgba,0,material_effects::Kind::unknown});}
 return result;
}
}
