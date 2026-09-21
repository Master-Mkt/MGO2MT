#include "host_hit_geometry.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <fstream>
#include <memory>
#include <stdexcept>
namespace mgo2mt::host_hit {
namespace {
using Poses=std::array<std::array<BoneTransform,21>,6>;
// Independently authored collision stand-in: simple upright/crouched/lying
// mannequin coordinates in millimetres. These are not sampled game bones.
const Poses fallback=[] {
 Poses result{};
 constexpr std::array<Vec3,21> human{{
  {0,900,0},{0,1050,0},{0,1250,0},{0,1480,0},{0,1600,0},
  {150,1420,0},{260,1400,0},{340,1200,0},{350,1000,0},
  {-150,1420,0},{-260,1400,0},{-340,1200,0},{-350,1000,0},
  {100,800,0},{100,380,0},{100,80,0},{100,40,100},
  {-100,800,0},{-100,380,0},{-100,80,0},{-100,40,100}}};
 for(size_t gender=0;gender<2;++gender)for(size_t stance=0;stance<3;++stance)
  for(size_t i=0;i<human.size();++i){auto&bone=result[gender*3+stance][i];bone.key=uint32_t(i+1);
   bone.axes={{{1,0,0},{0,1,0},{0,0,1}}};bone.origin=human[i];
   if(stance==1){bone.origin[1]*=.58f;bone.origin[2]+=(i>=13?120.f:80.f);}
   if(stance==2){bone.origin={human[i][0],200.f-human[i][2],human[i][1]-850.f};bone.axes={{{1,0,0},{0,0,1},{0,-1,0}}};}
  }
 return result;
}();
std::atomic<std::shared_ptr<const Poses>> local{};
float dot(const Vec3&a,const Vec3&b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
bool finite(const Vec3&a){return std::isfinite(a[0])&&std::isfinite(a[1])&&std::isfinite(a[2]);}
Vec3 rotate(const Vec3&v,float c,float s){return {v[0]*c+v[2]*s,v[1],-v[0]*s+v[2]*c};}
bool validRay(const Vec3&o,const Vec3&d,float limit){return finite(o)&&finite(d)&&std::isfinite(limit)&&limit>=0&&std::abs(dot(d,d)-1.f)<=.0001f;}
bool validBone(const BoneTransform&b){
 if(!b.key||b.key>0xffffff||!finite(b.origin))return false;
 for(float v:b.origin)if(std::abs(v)>5000)return false;
 for(unsigned i=0;i<3;++i){if(!finite(b.axes[i])||std::abs(dot(b.axes[i],b.axes[i])-1.f)>.0001f)return false;
  for(unsigned j=0;j<i;++j)if(std::abs(dot(b.axes[i],b.axes[j]))>.0001f)return false;}
 const auto&a=b.axes;Vec3 cross{a[0][1]*a[1][2]-a[0][2]*a[1][1],a[0][2]*a[1][0]-a[0][0]*a[1][2],a[0][0]*a[1][1]-a[0][1]*a[1][0]};
 return dot(cross,a[2])>.9999f;
}
}
ResourceStatus configure(const std::filesystem::path&file,std::string&error) noexcept {
 error.clear();
 try {
  std::error_code ec;bool exists=std::filesystem::exists(file,ec);
  if(ec)throw std::runtime_error("Hit geometry resource cannot be inspected");
  if(!exists){local.store({});return ResourceStatus::native_fallback;}
  constexpr size_t size=24+6*21*52;
  if(!std::filesystem::is_regular_file(file,ec)||ec||std::filesystem::file_size(file,ec)!=size||ec)
   throw std::runtime_error("Invalid hit geometry resource size/type");
  std::array<unsigned char,size> bytes{};std::ifstream stream(file,std::ios::binary);
  if(!stream.read(reinterpret_cast<char*>(bytes.data()),bytes.size())||stream.peek()!=std::char_traits<char>::eof())
   throw std::runtime_error("Cannot read complete hit geometry resource");
  constexpr std::array<unsigned char,8> magic{'G','W','H','I','T','1',0,0};
  if(!std::equal(magic.begin(),magic.end(),bytes.begin()))throw std::runtime_error("Invalid hit geometry resource magic");
  size_t at=8;auto word=[&]{uint32_t v=uint32_t(bytes[at])|uint32_t(bytes[at+1])<<8|uint32_t(bytes[at+2])<<16|uint32_t(bytes[at+3])<<24;at+=4;return v;};
  if(word()!=1||word()!=6||word()!=21||word()!=0)throw std::runtime_error("Unsupported hit geometry resource layout");
  auto candidate=std::make_shared<Poses>();
  for(size_t p=0;p<6;++p)for(size_t i=0;i<21;++i){auto&bone=(*candidate)[p][i];bone.key=word();
   for(auto&axis:bone.axes)for(auto&v:axis)v=std::bit_cast<float>(word());
   for(auto&v:bone.origin)v=std::bit_cast<float>(word());
   if(!validBone(bone))throw std::runtime_error("Invalid hit geometry bone transform");
   if(p&&bone.key!=(*candidate)[0][i].key)throw std::runtime_error("Hit geometry bone order differs between poses");
   for(size_t j=0;j<i;++j)if((*candidate)[p][j].key==bone.key)throw std::runtime_error("Duplicate hit geometry bone identifier");
  }
  local.store(std::move(candidate));return ResourceStatus::local_resource;
 }catch(const std::exception&e){local.store({});try{error=e.what();}catch(...){}return ResourceStatus::invalid_resource;}
 catch(...){local.store({});try{error="Hit geometry resource error";}catch(...){}return ResourceStatus::invalid_resource;}
}
bool using_local_resource() noexcept{return bool(local.load());}
std::span<const BoneTransform> pose(uint8_t gender,Stance stance) noexcept {
 if(gender>1||uint8_t(stance)>2)return {};
 thread_local std::shared_ptr<const Poses> retained;retained=local.load();
 return (retained?*retained:fallback)[gender*3+uint8_t(stance)];
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
