#include "stage_water.h"
#include "stage_collision.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace mgo2win::stage {
// CC5810 / 9FB98 / 19A428 / 18C1B0: normal player query requires attribute0x10.
// Keep unknown attribute0 as conservative native collision. Full include/exclude
// and primitive-header exceptions are not reconstructed by this adapter.
std::shared_ptr<const Collision> movement_collision(std::shared_ptr<const Collision> input){
 if(!input||std::none_of(input->triangles.begin(),input->triangles.end(),[](const auto& t){return t.attribute&&!(t.attribute&0x10);}))return input;
 std::vector<CollisionTriangle> triangles;triangles.reserve(input->triangles.size());
 for(const auto& t:input->triangles)if(!t.attribute||(t.attribute&0x10))triangles.push_back(t);
 return std::make_shared<Collision>(Collision::make(input->vertices,std::move(triangles),input->materials));
}
namespace {
bool finite(Vec3 p){return std::all_of(p.begin(),p.end(),[](float x){return std::isfinite(x)&&std::abs(x)<=1e7f;});}
std::uint32_t u32(std::istream& in){unsigned char b[4];if(!in.read(reinterpret_cast<char*>(b),4))throw std::runtime_error("truncated water file");return b[0]|std::uint32_t(b[1])<<8|std::uint32_t(b[2])<<16|std::uint32_t(b[3])<<24;}
bool contains(const WaterField& f,Vec3 p){for(unsigned i=0;i<3;++i)if(p[i]-f.center[i]<-f.halfSize[i]||p[i]-f.center[i]>f.halfSize[i])return false;return true;}
using D3=std::array<double,3>;
D3 delta(Vec3 a,Vec3 b){return {double(a[0])-b[0],double(a[1])-b[1],double(a[2])-b[2]};}
D3 cross(D3 a,D3 b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
double dot(D3 a,D3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
}
Water Water::make(std::vector<WaterField> fields){
 if(fields.size()>1024)throw std::runtime_error("water field count");
 Water out;
 for(const auto& f:fields){
  if(!finite(f.center)||!finite(f.halfSize)||std::any_of(f.halfSize.begin(),f.halfSize.end(),[](float x){return x<=0;}))throw std::runtime_error("invalid water field");
  if(std::none_of(out.fields_.begin(),out.fields_.end(),[&](const WaterField& v){return v.center==f.center&&v.halfSize==f.halfSize;}))out.fields_.push_back(f);
 }
 return out;
}
Water Water::read(std::istream& in){
 char magic[4];if(!in.read(magic,4)||std::string(magic,4)!="GWW1"||u32(in)!=1)throw std::runtime_error("water format");
 const auto count=u32(in);if(count>1024)throw std::runtime_error("water field count");
 std::vector<WaterField> fields(count);
 for(auto& f:fields){for(auto& x:f.center)x=std::bit_cast<float>(u32(in));for(auto& x:f.halfSize)x=std::bit_cast<float>(u32(in));}
 if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("water trailing bytes");
 return make(std::move(fields));
}
std::optional<float> Water::level(Vec3 p)const{
 if(!finite(p))return {};
 std::optional<float> result;
 for(const auto& f:fields_)if(contains(f,p)){
  const float candidate=f.center[1]+f.halfSize[1];
  if(result&&*result!=candidate)return {};
  result=candidate;
 }
 return result;
}
std::optional<float> Water::control_level(Vec3 p,float floorY)const{if(!finite(p)||!std::isfinite(floorY))return {};p[1]=floorY;return level(p);}
WaterFoot Water::classify(float level,float floorY,float controlY){
 if(!std::isfinite(level)||!std::isfinite(floorY)||!std::isfinite(controlY)||floorY>=level)return WaterFoot::dry;
 return controlY<=level?WaterFoot::inWater:WaterFoot::aboveSurface;
}
std::optional<WaterContact> Water::on_foot(Vec3 p,float floorY)const{
 if(!std::isfinite(floorY))return {};
 auto h=level(p);if(!h)return {};
 return WaterContact{*h,std::max(0.f,*h-floorY),classify(*h,floorY,p[1])};
}
WaterSurface WaterSurface::make(std::vector<WaterTriangle> triangles){
 if(triangles.size()>4096)throw std::runtime_error("water surface count");
 for(const auto& t:triangles){
  for(auto p:t.vertices)if(!finite(p))throw std::runtime_error("invalid water surface vertex");
  const auto n=cross(delta(t.vertices[1],t.vertices[0]),delta(t.vertices[2],t.vertices[0]));
  if(!(dot(n,n)>0))throw std::runtime_error("degenerate water surface");
 }
 WaterSurface out;out.triangles_=std::move(triangles);return out;
}
WaterSurface WaterSurface::read(std::istream& in){
 char magic[4];if(!in.read(magic,4)||std::string(magic,4)!="GWS1"||u32(in)!=1)throw std::runtime_error("water surface format");
 const auto count=u32(in);if(count>4096)throw std::runtime_error("water surface count");
 std::vector<WaterTriangle> triangles(count);
 for(auto& t:triangles)for(auto& p:t.vertices)for(auto& x:p)x=std::bit_cast<float>(u32(in));
 if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("water surface trailing bytes");
 return make(std::move(triangles));
}
std::optional<WaterSurfaceHit> WaterSurface::crossing(Vec3 from,Vec3 to)const{
 if(!finite(from)||!finite(to)||from==to)return {};
 const auto d=delta(to,from);std::optional<WaterSurfaceHit> result;double nearest=2;
 for(size_t i=0;i<triangles_.size();++i){
  const auto& t=triangles_[i];const auto e1=delta(t.vertices[1],t.vertices[0]),e2=delta(t.vertices[2],t.vertices[0]);
  const auto p=cross(d,e2);const double determinant=dot(e1,p);
  if(determinant==0)continue;
  const auto offset=delta(from,t.vertices[0]);const double u=dot(offset,p)/determinant;
  if(u<0||u>1)continue;
  const auto q=cross(offset,e1);const double v=dot(d,q)/determinant;
  if(v<0||u+v>1)continue;
  const double fraction=dot(e2,q)/determinant;
  if(!(fraction>0&&fraction<=1&&fraction<nearest))continue;
  nearest=fraction;WaterSurfaceHit hit;hit.fraction=static_cast<float>(fraction);hit.triangle=i;
  const auto n=cross(e1,e2);const double length=std::sqrt(dot(n,n));
  for(unsigned axis=0;axis<3;++axis){hit.position[axis]=static_cast<float>(from[axis]+fraction*d[axis]);hit.normal[axis]=static_cast<float>(n[axis]/length);}
  result=hit;
 }
 return result;
}
}
