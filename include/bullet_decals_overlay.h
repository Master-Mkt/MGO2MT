#pragma once
#include "bullet_decals.h"
#include "enemy_tag_target.h"
namespace mgo2win::combat::decals {
// Local straight-alpha procedural mark, not an extracted original texture.
inline void paint(std::span<uint32_t> pixels,std::span<const Decal> decals,
                  Vec3 eye,Vec3 direction,const stage::Collision& world,const stage::Collision* objects=nullptr,enemy_tag::Viewport viewport={}){
 if(!viewport.valid()||pixels.size()!=1280*720)return;
 unsigned visible=0;
 for(auto it=decals.rbegin();it!=decals.rend()&&visible<256;++it){const auto& d=*it;
  auto delta=enemy_tag::sub(d.position,eye);if(enemy_tag::dot(delta,delta)>35000.f*35000.f||enemy_tag::dot(delta,d.normal)>=0||!enemy_tag::visible(eye,d.position,world,objects))continue;
  auto center=enemy_tag::project(d.position,eye,direction,viewport.left,viewport.top,viewport.width,viewport.height,viewport.aspect);if(!center)continue;++visible;
  std::array<std::optional<enemy_tag::Point>,12> points;
  for(unsigned n=0;n<12;++n){float angle=float(n)*6.28318530718f/12;float r=d.radius*(n%3==0?.78f:1.f);auto p=d.position;for(unsigned c=0;c<3;++c)p[c]+=r*(d.tangent[c]*std::cos(angle)+d.bitangent[c]*std::sin(angle));if(enemy_tag::visible(eye,p,world,objects))points[n]=enemy_tag::project(p,eye,direction,viewport.left,viewport.top,viewport.width,viewport.height,viewport.aspect);}
  for(unsigned n=0;n<12;++n){if(!points[n]||!points[(n+1)%12])continue;auto a=*center,b=*points[n],c=*points[(n+1)%12];
   int left=std::max(viewport.left,std::min({a.x,b.x,c.x})),right=std::min(viewport.left+viewport.width-1,std::max({a.x,b.x,c.x})),top=std::max(viewport.top,std::min({a.y,b.y,c.y})),bottom=std::min(viewport.top+viewport.height-1,std::max({a.y,b.y,c.y}));
   if(right-left>128||bottom-top>128)continue;
   auto edge=[](enemy_tag::Point p,enemy_tag::Point q,int x,int y){return (q.x-p.x)*(y-p.y)-(q.y-p.y)*(x-p.x);};int area=edge(a,b,c.x,c.y);if(!area)continue;
   for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x){int ab=edge(a,b,x,y),bc=edge(b,c,x,y),ca=edge(c,a,x,y);if(area>0?(ab<0||bc<0||ca<0):(ab>0||bc>0||ca>0))continue;
    unsigned alpha=unsigned(std::clamp(d.alpha,0.f,1.f)*210);auto& dst=pixels[size_t(y)*1280+x];unsigned old=dst>>24,den=alpha*255+old*(255-alpha);if(!den)continue;uint32_t result=((den+127)/255)<<24;
    for(unsigned shift:{0u,8u,16u}){unsigned color=shift==16?36:shift==8?30:24;result|=((color*alpha*255+((dst>>shift)&255)*old*(255-alpha)+den/2)/den)<<shift;}dst=result;
   }
  }
 }
}
// Revalidate an event against the current static GEOM; never attach a mark to a
// body, moving object, water plane, or an unclassified non-solid trigger.
inline std::optional<Impact> static_impact(const Event& e,Scope scope,const stage::Collision& world){
 if(e.epoch!=scope.epoch||e.kind!=EventKind::impact||e.target.slot<24||e.object)return {};
 auto normal=enemy_tag::unit(e.normal);if(!normal)return {};auto start=enemy_tag::add(e.position,enemy_tag::mul(*normal,4));
 auto hit=world.ray(start,enemy_tag::mul(*normal,-1),8);if(!hit||std::abs(hit->distance-4)>1||enemy_tag::dot(hit->normal,*normal)<.99f)return {};
 const auto& triangle=world.triangles[hit->triangle];if(triangle.object||triangle.attribute==0x40048000ULL||!(triangle.attribute&0x34)||triangle.attribute&0x8000)return {};
 return Impact{scope,e.id,hit->position,*normal,world.material(hit->triangle).id,Surface::static_solid};
}
}
