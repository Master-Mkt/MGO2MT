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
}
