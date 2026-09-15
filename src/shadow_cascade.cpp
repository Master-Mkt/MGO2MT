#include "shadow_cascade.h"
#include "character_renderer.h"
#include "world_depth.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace mgo2win::shadows {

Allocation allocation(const Settings&s,uint64_t budget,unsigned maxDimension){
 if(!valid(s))throw std::invalid_argument("Shadow settings");if(!s.enabled)return {};
 auto size=s.resolution;while(size>=1024&&(size>maxDimension||uint64_t(size)*size*s.cascades*4>budget))size/=2;
 if(size<1024)return {0,0,0,true};return {s.cascades,size,uint64_t(size)*size*s.cascades*4,size!=s.resolution};
}
Plan plan(const WorldView&camera,std::array<float,3> direction,std::array<float,3> sun,const std::array<float,6>&bounds,const Settings&s,Allocation a){
 if(!valid(s)||!a.cascades||a.cascades>6||!a.resolution||camera.aspect<=0||camera.aspect>32||!std::isfinite(camera.aspect))throw std::invalid_argument("Shadow camera/settings");
 for(auto v:bounds)if(!std::isfinite(v)||std::abs(v)>=1e7)throw std::invalid_argument("Shadow bounds");for(unsigned i=0;i<3;++i)if(bounds[i]>bounds[i+3])throw std::invalid_argument("Shadow bounds order");
 for(auto group:{camera.eye,camera.direction,direction,sun})for(float v:group)if(!std::isfinite(v)||std::abs(v)>=1e7)throw std::invalid_argument("Shadow input");
 using namespace DirectX;auto vec=[](const auto&p){return XMVectorSet(p[0],p[1],p[2],0);};
 auto light=vec(direction),forward=vec(camera.direction);if(XMVectorGetX(XMVector3LengthSq(light))<1e-8||XMVectorGetX(XMVector3LengthSq(forward))<1e-8)throw std::invalid_argument("Shadow direction");
 light=XMVector3Normalize(light);forward=XMVector3Normalize(forward);auto up=XMVectorSet(0,1,0,0);if(std::abs(XMVectorGetX(XMVector3Dot(light,up)))>.99f)up=XMVectorSet(0,0,1,0);auto basis=XMMatrixLookToLH(XMVectorZero(),light,up);
 float zmin=1e30f,zmax=-1e30f;for(unsigned mask=0;mask<8;++mask){auto corner=XMVectorSet(bounds[(mask&1)?3:0],bounds[(mask&2)?4:1],bounds[(mask&4)?5:2],1);auto z=XMVectorGetZ(XMVector3TransformCoord(corner,basis));zmin=std::min(zmin,z);zmax=std::max(zmax,z);}
 // Fixed scene interval avoids camera-induced depth changes; include high jumps.
 zmin=std::floor((zmin-12000)/1024)*1024;zmax=std::ceil((zmax+12000)/1024)*1024;
 Plan p;p.count=a.cascades;p.resolution=a.resolution;p.eye=camera.eye;p.sun=sun;p.nearPlane=world_near_plane;XMFLOAT3 v;XMStoreFloat3(&v,forward);p.forward={v.x,v.y,v.z};XMStoreFloat3(&v,light);p.direction={v.x,v.y,v.z};
 float previous=world_near_plane,previousStart=world_near_plane;const float farDistance=std::min(s.distance,world_far_plane),tangent=std::tan(.5f);
 for(unsigned i=0;i<a.cascades;++i){float t=float(i+1)/a.cascades;float end=.6f*world_near_plane*std::pow(farDistance/world_near_plane,t)+.4f*(world_near_plane+(farDistance-world_near_plane)*t);if(i+1==a.cascades)end=farDistance;p.splits[i]=end;
  const float begin=i?previous-(previous-previousStart)*.1f:previous;const float half=(end-begin)*.5f;const float radius=std::ceil(std::sqrt(half*half+end*end*tangent*tangent*(1+camera.aspect*camera.aspect))/16)*16;
  const float padded=(radius+s.normalBias)*a.resolution/(a.resolution-8.f),texel=2*padded/a.resolution;p.texelSize[i]=texel;
  auto center=vec(camera.eye)+forward*((begin+end)*.5f);XMStoreFloat3(&v,XMVector3TransformCoord(center,basis));float x=std::floor(v.x/texel+.5f)*texel,y=std::floor(v.y/texel+.5f)*texel;
  auto projection=XMMatrixOrthographicOffCenterLH(x-padded,x+padded,y-padded,y+padded,zmin,zmax);XMFLOAT4X4 matrix;XMStoreFloat4x4(&matrix,basis*projection);std::memcpy(p.matrices[i].data(),&matrix,64);previousStart=previous;previous=end;
 }
 return p;
}
}
