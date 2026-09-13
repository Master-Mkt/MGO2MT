#pragma once
#include "water_effects.h"
#include "enemy_tag_target.h"
namespace mgo2win::stage {
// Native procedural water strokes, projected with the existing scene camera.
// World visibility is checked before compositing into the straight-alpha UI.
inline void paint_water(std::span<uint32_t> pixels,int width,int height,std::span<const WaterEffectLine> lines,
                        Vec3 eye,Vec3 direction,const Collision& world,const Collision* objects=nullptr){
 if(width!=1280||height!=720||pixels.size()!=size_t(width)*height||lines.size()>384)return;
 for(const auto& line:lines){if(!std::isfinite(line.alpha)||line.alpha<=0||!enemy_tag::visible(eye,line.from,world,objects)||!enemy_tag::visible(eye,line.to,world,objects))continue;
  auto a=enemy_tag::project(line.from,eye,direction,620,120,616,392),b=enemy_tag::project(line.to,eye,direction,620,120,616,392);if(!a||!b)continue;
  const int steps=std::max(std::abs(b->x-a->x),std::abs(b->y-a->y));if(steps>800)continue;const unsigned alpha=unsigned(std::clamp(line.alpha,0.f,1.f)*180);
  for(int i=0;i<=steps;++i){int x=steps?a->x+(b->x-a->x)*i/steps:a->x,y=steps?a->y+(b->y-a->y)*i/steps:a->y;if(x<620||x>=1236||y<120||y>=512)continue;
   auto& dst=pixels[size_t(y)*width+x];unsigned oldAlpha=dst>>24,den=alpha*255+oldAlpha*(255-alpha);if(!den)continue;
   uint32_t result=((den+127)/255)<<24;for(unsigned shift:{0u,8u,16u}){unsigned source=shift==16?185:shift==8?221:235;auto channel=(source*alpha*255+((dst>>shift)&255)*oldAlpha*(255-alpha)+den/2)/den;result|=channel<<shift;}dst=result;
  }
 }
}
}
