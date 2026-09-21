#include "material_impact_effects.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace mgo2mt::combat::material_effects {
namespace {
// Independently checked against the named callback option in all four GCX files.
// 6690BC -> 85C00 -> 64F8C0 -> flash / two line emitters.
constexpr uint32_t metalMaterials[]={0x45BCB4,0x48C4B8,0x48C4B9,0x48C4BA,0x48C4BB,
 0x48C4BC,0x65C426,0x7818B1,0x7818B2,0x7818B3,0x7ADCCA,0x7ADCCB,0xA1DCCB,0xB920C5,0xB920C6};
// 885BE5 -> 868C0 -> 64EB30 -> wood_frag2_cm / particles 7,73,74.
constexpr uint32_t woodMaterials[]={0x189CD4,0x189CD5,0x189CD6,0x189CD7,0x989CB2};
bool finite(Vec3 v){return std::all_of(v.begin(),v.end(),[](float x){return std::isfinite(x)&&std::abs(x)<1000000.f;});}
float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vec3 cross(Vec3 a,Vec3 b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
Vec3 scale(Vec3 a,float s){for(auto&x:a)x*=s;return a;}
Vec3 at(Vec3 p,Vec3 v,float t,float gravity){for(size_t i=0;i<3;++i)p[i]+=v[i]*t;p[1]-=.5f*gravity*t*t;return p;}
uint64_t mix(uint64_t x){x^=x>>30;x*=0xbf58476d1ce4e5b9ULL;x^=x>>27;x*=0x94d049bb133111ebULL;return x^(x>>31);}
std::array<float,4> color(Kind k){switch(k){case Kind::metal:return {1.f,.78f,.3f,1.f};case Kind::wood:return {.57f,.36f,.16f,1.f};case Kind::stone:return {.65f,.61f,.54f,1.f};case Kind::glass:return {.72f,.9f,1.f,.8f};default:return {};}}
}
Kind verified_kind(uint8_t map,uint32_t material)noexcept{
 if(map!=1&&map!=4&&map!=20&&map!=21)return Kind::unknown;
 if(std::binary_search(std::begin(metalMaterials),std::end(metalMaterials),material))return Kind::metal;
 if(std::binary_search(std::begin(woodMaterials),std::end(woodMaterials),material))return Kind::wood;
 return Kind::unknown;
}
bool Pool::valid(const Policy&p)noexcept{return p.capacity<=maximumCapacity&&p.lifetimeMs>0&&p.lifetimeMs<=3000&&p.particlesPerHit>0&&p.particlesPerHit<=32&&std::isfinite(p.speed)&&p.speed>0&&p.speed<=10000&&std::isfinite(p.gravity)&&p.gravity>=0&&p.gravity<=20000&&std::isfinite(p.surfaceOffset)&&p.surfaceOffset>0&&p.surfaceOffset<=10&&std::isfinite(p.trailSeconds)&&p.trailSeconds>0&&p.trailSeconds<=.1f;}
Pool::Pool(Policy p):policy_(p){if(!valid(p))throw std::invalid_argument("Invalid native impact particle policy");}
void Pool::clear(){particles_.clear();scope_={};played_=watermark_=lastNow_=0;established_=false;}
bool Pool::expire(uint64_t now){if(now<lastNow_){particles_.clear();lastNow_=now;return false;}lastNow_=now;while(!particles_.empty()&&now-particles_.front().born>=policy_.lifetimeMs)particles_.pop_front();return true;}
void Pool::synchronize(Scope scope,uint64_t watermark,uint64_t now){if(!scope.epoch||!scope.scene){clear();return;}if(!established_||scope!=scope_){clear();scope_=scope;played_=watermark_=watermark;lastNow_=now;established_=true;return;}watermark_=std::max(watermark_,watermark);expire(now);}
bool Pool::emit(const Impact&i,Kind kind,uint64_t now){
 if(!expire(now)||!established_||i.scope!=scope_||!i.eventId||i.eventId<=played_||i.eventId>watermark_)return false;
 played_=i.eventId;
 if(!policy_.capacity||i.surface!=decals::Surface::static_solid||kind<Kind::metal||kind>Kind::glass||!finite(i.position)||!finite(i.normal))return false;
 const float length=dot(i.normal,i.normal);if(!std::isfinite(length)||length<1e-10f)return false;
 const auto n=scale(i.normal,1/std::sqrt(length));auto tangent=cross(std::abs(n[1])<.9f?Vec3{0,1,0}:Vec3{1,0,0},n);tangent=scale(tangent,1/std::sqrt(dot(tangent,tangent)));const auto bitangent=cross(n,tangent);
 auto origin=i.position;for(size_t a=0;a<3;++a)origin[a]+=n[a]*policy_.surfaceOffset;if(!finite(origin))return false;
 for(unsigned p=0;p<policy_.particlesPerHit;++p){const auto hash=mix(i.eventId^(uint64_t(p+1)*0x9e3779b97f4a7c15ULL)^uint64_t(kind));const float angle=float(hash&65535)/65536.f*6.283185307f;const float lateral=.25f+.65f*float((hash>>16)&65535)/65535.f;const float speed=policy_.speed*(.45f+.55f*float((hash>>32)&65535)/65535.f);Vec3 v{};for(size_t a=0;a<3;++a)v[a]=(n[a]+lateral*(std::cos(angle)*tangent[a]+std::sin(angle)*bitangent[a]))*speed;
  if(particles_.size()>=policy_.capacity)particles_.pop_front();particles_.push_back({origin,v,now,i.eventId,kind});
 }return true;
}
std::vector<Line> Pool::lines(uint64_t now){if(!expire(now))return {};std::vector<Line> result;result.reserve(particles_.size());for(const auto&p:particles_){const float age=float(now-p.born)*.001f;const float tail=std::max(0.f,age-policy_.trailSeconds);auto rgba=color(p.kind);rgba[3]*=1.f-float(now-p.born)/float(policy_.lifetimeMs);result.push_back({at(p.position,p.velocity,tail,policy_.gravity),at(p.position,p.velocity,age,policy_.gravity),rgba,p.event,p.kind});}return result;}
}
