#pragma once
#include "water_effects.h"
#include "enemy_tag_target.h"
namespace mgo2mt::stage {
// Native procedural water strokes, projected with the existing scene camera.
// World visibility is checked before compositing into the straight-alpha UI.
inline void paint_water(std::span<uint32_t> pixels,int width,int height,std::span<const WaterEffectLine> lines,
                        Vec3 eye,Vec3 direction,const Collision& world,const Collision* objects=nullptr,enemy_tag::Viewport viewport={},unsigned* sharedPixelBudget=nullptr){
 if(!viewport.valid()||width!=1280||height!=720||pixels.size()!=size_t(width)*height||lines.size()>2048)return;
 auto forward=enemy_tag::unit(direction);if(!forward)return;unsigned localPixelBudget=12288;unsigned& pixelBudget=sharedPixelBudget?*sharedPixelBudget:localPixelBudget;pixelBudget=std::min(pixelBudget,12288u);
 for(const auto& line:lines){if(!std::isfinite(line.alpha)||line.alpha<=0||!enemy_tag::visible(eye,line.from,world,objects)||!enemy_tag::visible(eye,line.to,world,objects))continue;
  auto a=enemy_tag::project(line.from,eye,direction,viewport.left,viewport.top,viewport.width,viewport.height,viewport.aspect,viewport.verticalFov),b=enemy_tag::project(line.to,eye,direction,viewport.left,viewport.top,viewport.width,viewport.height,viewport.aspect,viewport.verticalFov);if(!a||!b)continue;
  const int steps=std::max(std::abs(b->x-a->x),std::abs(b->y-a->y));if(steps>800)continue;const unsigned alpha=unsigned(std::clamp(line.alpha,0.f,1.f)*180);
  const float nearDepth=enemy_tag::dot(enemy_tag::sub(line.from,eye),*forward),farDepth=enemy_tag::dot(enemy_tag::sub(line.to,eye),*forward);
  for(int i=0;i<=steps;++i){int x=steps?a->x+(b->x-a->x)*i/steps:a->x,y=steps?a->y+(b->y-a->y)*i/steps:a->y;if(x<viewport.left||x>=viewport.left+viewport.width||y<viewport.top||y>=viewport.top+viewport.height)continue;
   if(!pixelBudget)return;--pixelBudget;
   const float screen=steps?float(i)/steps:0.f,worldT=screen*nearDepth/((1-screen)*farDepth+screen*nearDepth);
   if(!enemy_tag::visible(eye,enemy_tag::add(line.from,enemy_tag::mul(enemy_tag::sub(line.to,line.from),worldT)),world,objects))continue;
   auto& dst=pixels[size_t(y)*width+x];unsigned oldAlpha=dst>>24,den=alpha*255+oldAlpha*(255-alpha);if(!den)continue;
   uint32_t result=((den+127)/255)<<24;for(unsigned shift:{0u,8u,16u}){unsigned source=190;auto channel=(source*alpha*255+((dst>>shift)&255)*oldAlpha*(255-alpha)+den/2)/den;result|=channel<<shift;}dst=result;
  }
 }
}
}
