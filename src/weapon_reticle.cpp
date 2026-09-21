#include "weapon_reticle.h"
#include <algorithm>
#include <cmath>
namespace mgo2mt::reticle {
namespace {
bool valid(Viewport v){return v.left>=0&&v.top>=0&&v.width>0&&v.height>0&&v.width<=1280&&v.height<=720&&v.left<=1280-v.width&&v.top<=720-v.height&&std::isfinite(v.aspect)&&v.aspect>=.01f&&v.aspect<=32&&valid_vertical_fov(v.verticalFov);}
bool center(float x,float y,Viewport v){return valid(v)&&std::isfinite(x)&&std::isfinite(y)&&x>=v.left&&y>=v.top&&x<v.left+v.width&&y<v.top+v.height;}
bool valid(Scope s){return s.epoch&&s.scene&&s.identity.slot<24&&s.identity.instance&&s.identity.character&&s.life&&s.weapon;}
}
std::optional<Geometry> geometry(float angle,float x,float y,Viewport v){
 if(!center(x,y,v)||!std::isfinite(angle)||angle<0||angle>1.4f)return {};
 const double radius=std::tan(double(angle))/std::tan(double(v.verticalFov)*.5);
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
void paint(std::span<uint32_t> pixels,int width,int height,const Geometry&g,const weapon_effect::Reticle&style){
 if(!style.enabled||width!=1280||height!=720||pixels.size()!=size_t(width)*height||!center(g.centerX,g.centerY,g.viewport)||!std::isfinite(g.radiusX)||!std::isfinite(g.radiusY)||g.radiusX<0||g.radiusY<0||g.radiusX>1000000||g.radiusY>1000000)return;
 if(!std::isfinite(style.thickness)||style.thickness<.5f||style.thickness>16||!std::isfinite(style.length)||style.length<1||style.length>100||!std::isfinite(style.minGap)||style.minGap<0||style.minGap>100||!std::all_of(style.color.begin(),style.color.end(),[](float f){return std::isfinite(f)&&f>=0&&f<=1;}))return;
 const auto& color=style.color;const float sa=color[3];if(sa<=0)return;
 auto rect=[&](int x,int y,int w,int h){for(int row=(std::max)(y,g.viewport.top);row<(std::min)(y+h,g.viewport.top+g.viewport.height);++row)for(int col=(std::max)(x,g.viewport.left);col<(std::min)(x+w,g.viewport.left+g.viewport.width);++col){auto&d=pixels[size_t(row)*width+col];const float da=float(d>>24)/255,oa=sa+da*(1-sa);uint32_t out=uint32_t(std::lround(oa*255))<<24;for(unsigned channel=0;channel<3;++channel){const unsigned shift=(2-channel)*8;float dc=float((d>>shift)&255)/255;out|=uint32_t(std::lround((color[channel]*sa+dc*da*(1-sa))/oa*255))<<shift;}d=out;}};
 const int x=int(std::floor(g.centerX)),y=int(std::floor(g.centerY)),rx=int(std::ceil((std::max)(style.minGap,g.radiusX))),ry=int(std::ceil((std::max)(style.minGap,g.radiusY)));
 const int t=int(std::lround(style.thickness)),length=int(std::lround(style.length)),half=length/2,cap=(std::max)(t,length/2);
 rect(x-rx-t/2,y-half,t,length);rect(x-rx-t/2,y-half,cap,t);rect(x-rx-t/2,y+half-t,cap,t);
 rect(x+rx-t/2,y-half,t,length);rect(x+rx-cap+t/2,y-half,cap,t);rect(x+rx-cap+t/2,y+half-t,cap,t);
 rect(x-half,y-ry-t/2,length,t);rect(x-half,y-ry-t/2,t,cap);rect(x+half-t,y-ry-t/2,t,cap);
 rect(x-half,y+ry-t/2,length,t);rect(x-half,y+ry-cap+t/2,t,cap);rect(x+half-t,y+ry-cap+t/2,t,cap);
 if(style.centerDot)rect(x-t/2,y-t/2,t,t);
}
}
