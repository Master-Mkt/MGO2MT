#include "weapon_reticle.h"
#include <algorithm>
#include <cmath>
namespace mgo2win::reticle {
namespace {
bool valid(Viewport v){return v.left>=0&&v.top>=0&&v.width>0&&v.height>0&&v.width<=1280&&v.height<=720&&v.left<=1280-v.width&&v.top<=720-v.height&&std::isfinite(v.aspect)&&v.aspect>=.01f&&v.aspect<=32;}
bool center(float x,float y,Viewport v){return valid(v)&&std::isfinite(x)&&std::isfinite(y)&&x>=v.left&&y>=v.top&&x<v.left+v.width&&y<v.top+v.height;}
bool valid(Scope s){return s.epoch&&s.scene&&s.identity.slot<24&&s.identity.instance&&s.identity.character&&s.life&&s.weapon;}
}
std::optional<Geometry> geometry(float angle,float x,float y,Viewport v){
 if(!center(x,y,v)||!std::isfinite(angle)||angle<0||angle>1.4f)return {};
 const double radius=std::tan(double(angle))/std::tan(.5);
 return Geometry{x,y,float(radius*v.width/(2*v.aspect)),float(radius*v.height/2),v};
}
std::optional<Geometry> Presentation::update(const Model& m,double dt){
 if(!valid(m.scope)||!m.gameplay||!m.active||!m.alive||m.menuOpen||!m.eligible||!m.spreadRadians||!std::isfinite(dt)||dt<0||dt>.25||!geometry(*m.spreadRadians,m.centerX,m.centerY,m.viewport)){reset();return {};}
 if(scope_&&*scope_==m.scope&&m.shotWatermark<watermark_)return {};
 scope_=m.scope;
 watermark_=m.shotWatermark;
 return geometry(*m.spreadRadians,m.centerX,m.centerY,m.viewport);
}
void paint(std::span<uint32_t> pixels,int width,int height,const Geometry& g){
 if(width!=1280||height!=720||pixels.size()!=size_t(width)*height||!center(g.centerX,g.centerY,g.viewport)||!std::isfinite(g.radiusX)||!std::isfinite(g.radiusY)||g.radiusX<0||g.radiusY<0||g.radiusX>1000000||g.radiusY>1000000)return;
 auto rect=[&](int x,int y,int w,int h){
  const int left=(std::max)(x,g.viewport.left),top=(std::max)(y,g.viewport.top);
  const int right=(std::min)(x+w,g.viewport.left+g.viewport.width),bottom=(std::min)(y+h,g.viewport.top+g.viewport.height);
  for(int row=top;row<bottom;++row)for(int col=left;col<right;++col)pixels[size_t(row)*width+col]=orange;
 };
 const int x=int(std::floor(g.centerX)),y=int(std::floor(g.centerY));
 const int rx=int(std::ceil((std::max)(6.f,g.radiusX))),ry=int(std::ceil((std::max)(6.f,g.radiusY)));
 // Four inward-facing three-stroke brackets and a centered two-pixel dot.
 rect(x-rx-1,y-5,2,10);rect(x-rx-1,y-5,5,2);rect(x-rx-1,y+3,5,2);
 rect(x+rx-1,y-5,2,10);rect(x+rx-4,y-5,5,2);rect(x+rx-4,y+3,5,2);
 rect(x-5,y-ry-1,10,2);rect(x-5,y-ry-1,2,5);rect(x+3,y-ry-1,2,5);
 rect(x-5,y+ry-1,10,2);rect(x-5,y+ry-4,2,5);rect(x+3,y+ry-4,2,5);
 rect(x-1,y-1,2,2);
}
}
