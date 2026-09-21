#include "bullet_decals.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace mgo2mt::combat::decals {
namespace {
bool finite(Vec3 v){return std::all_of(v.begin(),v.end(),[](float f){return std::isfinite(f)&&std::abs(f)<1000000.f;});}
float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vec3 cross(Vec3 a,Vec3 b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
Vec3 scaled(Vec3 a,float k){for(auto&f:a)f*=k;return a;}
}
bool Pool::valid(const Policy&p)noexcept{return p.capacity<=maximumCapacity&&p.lifetimeMs>0&&p.lifetimeMs<=86400000&&p.fadeMs<=p.lifetimeMs&&std::isfinite(p.radius)&&p.radius>=.1f&&p.radius<=500.f&&std::isfinite(p.surfaceOffset)&&p.surfaceOffset>=.01f&&p.surfaceOffset<=10.f;}
Pool::Pool(Policy p):policy_(p){if(!valid(p))throw std::invalid_argument("Invalid native bullet decal policy");}
bool Pool::configure(Policy p){if(!valid(p))return false;policy_=p;while(entries_.size()>p.capacity)entries_.pop_front();return true;}
void Pool::clear(){entries_.clear();scope_={};played_=watermark_=lastNow_=0;established_=false;}
bool Pool::expire(uint64_t now){if(now<lastNow_){entries_.clear();lastNow_=now;return false;}lastNow_=now;while(!entries_.empty()&&now-entries_.front().born>=policy_.lifetimeMs)entries_.pop_front();return true;}
void Pool::synchronize(Scope scope,uint64_t watermark,uint64_t now){
 if(!scope.epoch||!scope.scene){clear();return;}
 if(!established_||scope!=scope_){clear();scope_=scope;played_=watermark_=watermark;lastNow_=now;established_=true;return;}
 watermark_=std::max(watermark_,watermark);expire(now);
}
bool Pool::emit(const Impact& impact,uint64_t now){
 if(!expire(now)||!established_||impact.scope!=scope_||!impact.eventId||impact.eventId<=played_||impact.eventId>watermark_)return false;
 played_=impact.eventId;
 if(!policy_.capacity||impact.surface!=Surface::static_solid||!finite(impact.position)||!finite(impact.normal))return false;
 const float length2=dot(impact.normal,impact.normal);if(length2<1e-10f||!std::isfinite(length2))return false;
 const Vec3 normal=scaled(impact.normal,1.f/std::sqrt(length2));
 const Vec3 reference=std::abs(normal[1])<.9f?Vec3{0,1,0}:Vec3{1,0,0};
 auto tangent=cross(reference,normal);tangent=scaled(tangent,1.f/std::sqrt(dot(tangent,tangent)));const auto second=cross(normal,tangent);
 // Deterministic variation avoids identical oriented marks without global RNG.
 const uint64_t hash=impact.eventId*0x9e3779b97f4a7c15ULL;const float angle=float((hash>>40)&0xffff)/65536.f*6.283185307179586f;
 const float c=std::cos(angle),s=std::sin(angle);for(size_t i=0;i<3;++i)tangent[i]=tangent[i]*c+second[i]*s;
 auto position=impact.position;for(size_t i=0;i<3;++i)position[i]+=normal[i]*policy_.surfaceOffset;if(!finite(position))return false;
 Decal decal{scope_,impact.eventId,position,normal,tangent,cross(normal,tangent),impact.material,policy_.radius,1};
 if(entries_.size()>=policy_.capacity)entries_.pop_front();entries_.push_back({decal,now});return true;
}
std::vector<Decal> Pool::sample(uint64_t now){if(!expire(now))return {};std::vector<Decal> out;out.reserve(entries_.size());
 for(const auto&e:entries_){auto d=e.decal;const auto remaining=policy_.lifetimeMs-(now-e.born);d.alpha=policy_.fadeMs&&remaining<policy_.fadeMs?float(remaining)/float(policy_.fadeMs):1.f;out.push_back(d);}return out;
}
}
