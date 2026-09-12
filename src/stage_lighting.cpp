#include "stage_lighting.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
namespace mgo2win::stage {
static float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static Vec3 normal(Vec3 a){float l=std::sqrt(dot(a,a));if(l<1e-8f)return {0,1,0};for(auto&v:a)v/=l;return a;}
Lighting Lighting::read(std::istream&in){
 auto require=[](bool ok){if(!ok)throw std::runtime_error("Invalid native stage lighting");};
 std::string magic;unsigned version,count;require(bool(in>>magic>>version>>count));require(magic=="MGO2WIN.STAGE_LIGHTS"&&(version>=1&&version<=3)&&count<=4096);
 auto numbers=[&](auto&v){for(auto&x:v){require(bool(in>>x));require(std::isfinite(x)&&std::abs(x)<=1000000);}};
 // Original zero-width feather faces store positive infinity (1 / 0).
 auto fades=[&](Vec3&v){for(auto&x:v){std::string s;require(bool(in>>s));std::size_t used=0;try{x=std::stof(s,&used);}catch(...){require(false);}require(used==s.size()&&!std::isnan(x)&&x>=0);}};
 auto identity=[&](LightIdentity&v){if(version<3)return;require(bool(in>>v.groupFlags>>v.id>>v.key));numbers(v.groupMinimum);numbers(v.groupMaximum);for(int i=0;i<3;++i)require(v.groupMinimum[i]<=v.groupMaximum[i]);};
 Lighting l;numbers(l.direction);numbers(l.direct);numbers(l.front);numbers(l.back);numbers(l.axis);numbers(l.ambientScale);numbers(l.cameraBounds);
 for(int i=0;i<3;++i)require(l.cameraBounds[i]<l.cameraBounds[i+3]);
 for(unsigned i=0;i<count;++i){Hemisphere h;numbers(h.maximum);numbers(h.minimum);numbers(h.center);numbers(h.extent);fades(h.positive);fades(h.negative);numbers(h.quaternion);numbers(h.direction);numbers(h.front);numbers(h.back);numbers(h.rotation);require(bool(in>>h.flags));
  float q=0;for(auto x:h.quaternion)q+=x*x;require(q>.99f&&q<1.01f);
  for(int j=0;j<3;++j)require(h.minimum[j]<=h.maximum[j]&&h.extent[j]>=0&&h.positive[j]>=0&&h.negative[j]>=0&&h.front[j]>=0&&h.back[j]>=0);
  identity(h.identity);l.hemispheres.push_back(h);
 }
 if(version>=2){std::string tag;unsigned n;require(bool(in>>tag>>n)&&tag=="POINTS"&&n<=4096);for(unsigned i=0;i<n;++i){PointLight p;numbers(p.position);numbers(p.color);std::array<float,3>values;numbers(values);p.range=values[0];p.extendedRange=values[1];p.importance=values[2];require(bool(in>>p.flags)&&p.range>0&&p.extendedRange>0);for(float c:p.color)require(c>=0);identity(p.identity);l.points.push_back(p);}}
 std::string tail;require(!(in>>tail));return l;
}
static bool set_enabled(unsigned& flags,bool enabled){unsigned next=enabled?flags&~0x8000u:flags|0x8000u;if(next==flags)return false;flags=next;return true;}
size_t Lighting::enable(unsigned key,unsigned id,bool enabled){
 size_t changed=0;auto apply=[&](auto&light){auto&m=light.identity;if((m.groupFlags&0x100)&&(key==~0u||m.key==key)&&(id==~0u||m.id==id))changed+=set_enabled(light.flags,enabled);};
 for(auto&h:hemispheres)apply(h);for(auto&p:points)apply(p);return changed;
}
size_t Lighting::enable_sphere(Vec3 center,float radius,bool enabled){
 if(!std::isfinite(radius)||radius<=0||radius>1000000)return 0;for(auto x:center)if(!std::isfinite(x))return 0;
 size_t changed=0;auto apply=[&](auto&light,Vec3 p){auto&m=light.identity;if(!(m.groupFlags&0x100))return;for(int i=0;i<3;++i)if(center[i]+radius<m.groupMinimum[i]||center[i]-radius>m.groupMaximum[i])return;
  Vec3 d;for(int i=0;i<3;++i)d[i]=p[i]-center[i];if(dot(d,d)<radius*radius)changed+=set_enabled(light.flags,enabled);
 };for(auto&h:hemispheres)apply(h,h.center);for(auto&p:points)apply(p,p.position);return changed;
}
float Lighting::weight(const Hemisphere&h,Vec3 p){
 if(!(h.flags&0x100)||(h.flags&0x8000))return 0;
 for(int i=0;i<3;++i){if(p[i]<=h.minimum[i]||p[i]>=h.maximum[i])return 0;p[i]-=h.center[i];}
 // Inverse unit-quaternion rotation: world point into the local feather box.
 Vec3 q{-h.quaternion[0],-h.quaternion[1],-h.quaternion[2]};
 auto cross=[](Vec3 a,Vec3 b){return Vec3{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
 auto t=cross(q,p);for(auto&v:t)v*=2;auto u=cross(q,t);float edge=0;
 for(int i=0;i<3;++i){float v=p[i]+h.quaternion[3]*t[i]+u[i];float fade=v>h.extent[i]?(v-h.extent[i])*h.positive[i]:v<-h.extent[i]?(-h.extent[i]-v)*h.negative[i]:0;edge=std::max(edge,fade);}
 return std::clamp(1-edge,0.f,1.f);
}
LightSample Lighting::sample(Vec3 p,Vec3 n)const{
 n=normal(n);Vec3 f{},b{},a{};LightSample out;
 for(const auto&h:hemispheres){float w=weight(h,p);if(w<=0)continue;++out.volumes;out.hemisphereWeight+=w;for(int i=0;i<3;++i){f[i]+=h.front[i]*w;b[i]+=h.back[i]*w;a[i]+=h.direction[i]*w;}}
 if(out.hemisphereWeight>0){for(int i=0;i<3;++i){f[i]/=out.hemisphereWeight;b[i]/=out.hemisphereWeight;a[i]/=out.hemisphereWeight;}}
 else{f=front;b=back;a=axis;}
 // Native diffuse response. Original shader use of the two retained rotation
 // coefficients and pre-lighting attributes is still under investigation.
 float blend=std::clamp((1-dot(n,normal(a)))*.5f,0.f,1.f);float diffuse=std::max(0.f,-dot(n,normal(direction)));
 for(int i=0;i<3;++i)out.color[i]=(b[i]+(f[i]-b[i])*blend)*ambientScale[i]+direct[i]*diffuse;
 for(const auto&light:points){auto c=point_sample(light,p,n);for(int i=0;i<3;++i)out.color[i]+=c[i];}
 return out;
}
Vec3 Lighting::point_sample(const PointLight&l,Vec3 p,Vec3 n){
 // DG_GetLight 0x125E90..0x125FAC: active 0x100, disable 0x8000,
 // component range cull, strict extended-radius sphere, linear attenuation.
 if(!(l.flags&0x100)||(l.flags&0x8000)||!(l.range>0&&l.extendedRange>0))return {};
 Vec3 delta;for(int i=0;i<3;++i){delta[i]=l.position[i]-p[i];if(std::abs(delta[i])>l.range)return {};}
 float d2=dot(delta,delta);if(d2>=l.extendedRange*l.extendedRange)return {};float distance=std::sqrt(d2);
 // Diffuse response is native; original two-light ranking / shader specular
 // and importance handling are not yet reproduced.
 float diffuse=distance>1e-6f?std::max(0.f,dot(normal(n),delta)/distance):1.f;
 float strength=(1-distance/l.extendedRange)*diffuse;Vec3 c;for(int i=0;i<3;++i)c[i]=l.color[i]*strength;return c;
}
bool TransientLights::add(PointLight light,double now,double lifetime){
 if(!std::isfinite(now)||!std::isfinite(lifetime)||now<0||lifetime<=0||lifetime>60||!std::isfinite(now+lifetime))return false;
 for(float x:light.position)if(!std::isfinite(x)||std::abs(x)>1000000)return false;
 for(float x:light.color)if(!std::isfinite(x)||x<0||x>1000)return false;
 if(!std::isfinite(light.range)||!std::isfinite(light.extendedRange)||light.range<=0||light.range>1000000||light.extendedRange<=0||light.extendedRange>1000000)return false;
 expire(now);if(entries.size()>=128)return false;light.flags=0x100;entries.push_back({light,now+lifetime});return true;
}
void TransientLights::expire(double now){if(!std::isfinite(now))return;std::erase_if(entries,[&](const Entry&e){return e.expires<=now;});}
Vec3 TransientLights::sample(Vec3 p,Vec3 n,double now)const{Vec3 result{};if(!std::isfinite(now))return result;for(auto&e:entries)if(e.expires>now){auto c=Lighting::point_sample(e.light,p,n);for(int i=0;i<3;++i)result[i]+=c[i];}return result;}
}
