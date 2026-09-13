#include "host_hit_geometry.h"
#include <algorithm>
#include <cmath>
namespace mgo2win::host_hit {
namespace {
#include "host_hit_geometry_data.inc"
float dot(const Vec3&a,const Vec3&b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
bool finite(const Vec3&a){return std::isfinite(a[0])&&std::isfinite(a[1])&&std::isfinite(a[2]);}
Vec3 rotate(const Vec3&v,float c,float s){return {v[0]*c+v[2]*s,v[1],-v[0]*s+v[2]*c};}
bool validRay(const Vec3&o,const Vec3&d,float limit){return finite(o)&&finite(d)&&std::isfinite(limit)&&limit>=0&&std::abs(dot(d,d)-1.f)<=.0001f;}
}
std::span<const BoneTransform> pose(uint8_t gender,Stance stance) noexcept {
 if(gender>1||uint8_t(stance)>2)return {};return baked[gender*3+uint8_t(stance)];
}
std::optional<float> intersect(const Vec3&o,const Vec3&d,float limit,const BoneTransform&b,const Vec3&offset,const Vec3&half) noexcept {
 if(!validRay(o,d,limit)||!finite(b.origin)||!finite(offset)||!finite(half))return {};
 for(unsigned i=0;i<3;++i){if(half[i]<=0||!finite(b.axes[i])||std::abs(dot(b.axes[i],b.axes[i])-1.f)>.0001f)return {};
  for(unsigned j=0;j<i;++j)if(std::abs(dot(b.axes[i],b.axes[j]))>.0001f)return {};}
 Vec3 relative{o[0]-b.origin[0],o[1]-b.origin[1],o[2]-b.origin[2]};if(!finite(relative))return {};
 double near=0,far=limit;
 for(unsigned i=0;i<3;++i){double p=double(dot(relative,b.axes[i]))-offset[i],v=dot(d,b.axes[i]);
  if(std::abs(v)<1.e-10){if(p < -half[i]||p > half[i])return {};continue;}
  double a=(-double(half[i])-p)/v,z=(double(half[i])-p)/v;if(a>z)std::swap(a,z);
  near=(std::max)(near,a);far=(std::min)(far,z);if(near>far)return {};
 }
 return float(near);
}
std::optional<Hit> query(const Vec3&o,const Vec3&d,float limit,const Vec3&feet,float yaw,uint8_t gender,Stance stance) noexcept {
 auto bones=pose(gender,stance);if(bones.empty()||!validRay(o,d,limit)||!finite(feet)||!std::isfinite(yaw))return {};
 const float c=std::cos(yaw),s=std::sin(yaw);std::optional<Hit> best;
 for(size_t i=0;i<original_hit_regions::boxes.size();++i){const auto&box=original_hit_regions::boxes[i];if(box.bone>=bones.size())continue;
  auto transformed=bones[box.bone];for(auto&axis:transformed.axes)axis=rotate(axis,c,s);transformed.origin=rotate(transformed.origin,c,s);
  for(unsigned j=0;j<3;++j)transformed.origin[j]+=feet[j];
  auto t=intersect(o,d,best?best->distance:limit,transformed,box.offset,box.halfExtent);
  if(t&&(!best||*t<best->distance)){Vec3 p;for(unsigned j=0;j<3;++j)p[j]=o[j]+d[j]* *t;best=Hit{*t,box.bone,uint8_t(i),box.nameHash,p};}
 }
 return best;
}
}
