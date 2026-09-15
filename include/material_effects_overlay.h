#pragma once
#include "material_impact_effects.h"
#include "enemy_tag_target.h"
namespace mgo2win::combat::material_effects {
inline void paint(std::span<uint32_t> pixels,std::span<const Line> lines,Vec3 eye,Vec3 direction,
                  const stage::Collision& world,const stage::Collision* objects=nullptr,enemy_tag::Viewport viewport={}){
 if(!viewport.valid()||pixels.size()!=1280*720||lines.size()>Pool::maximumCapacity)return;
 auto forward=enemy_tag::unit(direction);if(!forward)return;
 // Bounded native presentation: an occluded middle must not be filled merely
 // because both endpoints of a trail are visible. Exhaustion omits pixels.
 unsigned rayBudget=4096;
 for(const auto& line:lines){if(!std::all_of(line.rgba.begin(),line.rgba.end(),[](float x){return std::isfinite(x)&&x>=0&&x<=1;})||!enemy_tag::visible(eye,line.from,world,objects)||!enemy_tag::visible(eye,line.to,world,objects))continue;
  auto a=enemy_tag::project(line.from,eye,direction,viewport.left,viewport.top,viewport.width,viewport.height,viewport.aspect),b=enemy_tag::project(line.to,eye,direction,viewport.left,viewport.top,viewport.width,viewport.height,viewport.aspect);if(!a||!b)continue;
  int steps=std::max(std::abs(b->x-a->x),std::abs(b->y-a->y));if(steps>800)continue;unsigned alpha=unsigned(line.rgba[3]*220);
  float fromDepth=enemy_tag::dot(enemy_tag::sub(line.from,eye),*forward),toDepth=enemy_tag::dot(enemy_tag::sub(line.to,eye),*forward);
  for(int i=0;i<=steps;++i){int x=steps?a->x+(b->x-a->x)*i/steps:a->x,y=steps?a->y+(b->y-a->y)*i/steps:a->y;if(x<viewport.left||x>=viewport.left+viewport.width||y<viewport.top||y>=viewport.top+viewport.height)continue;
   if(!rayBudget--)return;
   float screenT=steps?float(i)/float(steps):0.f;
   float worldT=screenT*fromDepth/((1-screenT)*toDepth+screenT*fromDepth);
   auto point=enemy_tag::add(line.from,enemy_tag::mul(enemy_tag::sub(line.to,line.from),worldT));
   if(!enemy_tag::visible(eye,point,world,objects))continue;
   auto& dst=pixels[size_t(y)*1280+x];unsigned old=dst>>24,den=alpha*255+old*(255-alpha);if(!den)continue;uint32_t result=((den+127)/255)<<24;
   for(unsigned channel=0;channel<3;++channel){unsigned shift=(2-channel)*8,color=unsigned(line.rgba[channel]*255);result|=((color*alpha*255+((dst>>shift)&255)*old*(255-alpha)+den/2)/den)<<shift;}dst=result;
  }
 }
}
}
