#pragma once
#include "material_impact_effects.h"
#include "enemy_tag_target.h"
namespace mgo2win::combat::material_effects {
inline void paint(std::span<uint32_t> pixels,std::span<const Line> lines,Vec3 eye,Vec3 direction,
                  const stage::Collision& world,const stage::Collision* objects=nullptr){
 if(pixels.size()!=1280*720||lines.size()>Pool::maximumCapacity)return;
 for(const auto& line:lines){if(!std::all_of(line.rgba.begin(),line.rgba.end(),[](float x){return std::isfinite(x)&&x>=0&&x<=1;})||!enemy_tag::visible(eye,line.from,world,objects)||!enemy_tag::visible(eye,line.to,world,objects))continue;
  auto a=enemy_tag::project(line.from,eye,direction,620,120,616,392),b=enemy_tag::project(line.to,eye,direction,620,120,616,392);if(!a||!b)continue;
  int steps=std::max(std::abs(b->x-a->x),std::abs(b->y-a->y));if(steps>800)continue;unsigned alpha=unsigned(line.rgba[3]*220);
  for(int i=0;i<=steps;++i){int x=steps?a->x+(b->x-a->x)*i/steps:a->x,y=steps?a->y+(b->y-a->y)*i/steps:a->y;if(x<620||x>=1236||y<120||y>=512)continue;
   auto& dst=pixels[size_t(y)*1280+x];unsigned old=dst>>24,den=alpha*255+old*(255-alpha);if(!den)continue;uint32_t result=((den+127)/255)<<24;
   for(unsigned channel=0;channel<3;++channel){unsigned shift=(2-channel)*8,color=unsigned(line.rgba[channel]*255);result|=((color*alpha*255+((dst>>shift)&255)*old*(255-alpha)+den/2)/den)<<shift;}dst=result;
  }
 }
}
}
