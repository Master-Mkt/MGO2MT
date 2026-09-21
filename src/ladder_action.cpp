#include "ladder_action.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>
namespace mgo2mt::ladder {
namespace {
using stage::Vec3;
bool finite(Vec3 p){for(float x:p)if(!std::isfinite(x)||std::abs(x)>=1000000)return false;return true;}
bool capsule(stage::Capsule c){return std::isfinite(c.radius)&&std::isfinite(c.height)&&std::isfinite(c.skin)&&c.radius>0&&c.radius<=2000&&c.height>=2*c.radius&&c.height<=6000&&c.skin>=.001f&&c.skin<=10&&c.skin<c.radius*.25f;}
float distance(Vec3 a,Vec3 b){return std::sqrt((a[0]-b[0])*(a[0]-b[0])+(a[1]-b[1])*(a[1]-b[1])+(a[2]-b[2])*(a[2]-b[2]));}
bool peers_valid(std::span<const Body> ps){if(ps.size()>24)return false;for(const auto&p:ps)if(!finite(p.feet)||!capsule(p.capsule))return false;return true;}
bool free(Vec3 at,stage::Capsule c,const stage::Collision&w,std::span<const Body>ps){
 if(!w.clear(at,c))return false;
 for(const auto&p:ps){const float lo=p.feet[1]+p.capsule.radius-(c.height-c.radius),hi=p.feet[1]+p.capsule.height-p.capsule.radius-c.radius;const float y=at[1]-std::clamp(at[1],lo,hi),r=c.radius+p.capsule.radius+2;
 if((at[0]-p.feet[0])*(at[0]-p.feet[0])+(at[2]-p.feet[2])*(at[2]-p.feet[2])+y*y<r*r)return false;}return true;
}
float peer_fraction(Vec3 from,Vec3 delta,stage::Capsule c,const Body&p){
 const double radius=c.radius+p.capsule.radius+2.,ax=p.feet[0],az=p.feet[2],ay=p.feet[1]+p.capsule.radius-(c.height-c.radius),by=p.feet[1]+p.capsule.height-p.capsule.radius-c.radius;
 const double ox=from[0]-ax,oz=from[2]-az,oy=from[1]-std::clamp(double(from[1]),ay,by),length=distance(delta,{});if(length<1e-7)return 1;
 if(ox*ox+oy*oy+oz*oz<=radius*radius)return 0;
 const double dx=delta[0]/length,dy=delta[1]/length,dz=delta[2]/length;double nearest=length;
 auto root=[&](double a,double b,double v,auto inside){if(a<1e-12)return;const double d=b*b-a*v;if(d<0)return;const double t=(-b-std::sqrt(d))/a;if(t>=0&&t<nearest&&inside(t))nearest=t;};
 root(dx*dx+dz*dz,ox*dx+oz*dz,ox*ox+oz*oz-radius*radius,[&](double t){const auto y=from[1]+dy*t;return y>=ay&&y<=by;});
 for(auto center:{ay,by}){const auto y=from[1]-center;root(1,ox*dx+y*dy+oz*dz,ox*ox+y*y+oz*oz-radius*radius,[](double){return true;});}return float(nearest/length);
}
Vec3 swept(Vec3 from,Vec3 to,stage::Capsule c,const stage::Collision&w,std::span<const Body>ps){
 Vec3 d{to[0]-from[0],to[1]-from[1],to[2]-from[2]};float fraction=1;if(auto h=w.sweep(from,d,c))fraction=h->fraction;for(const auto&p:ps)fraction=(std::min)(fraction,peer_fraction(from,d,c,p));if(fraction<1)fraction=(std::max)(0.f,fraction-.0001f);
 for(unsigned i=0;i<3;++i)to[i]=from[i]+d[i]*fraction;return free(to,c,w,ps)?to:from;
}
bool route(Vec3 from,Vec3 to,stage::Capsule c,const stage::Collision&w,std::span<const Body>ps){
 // Native endpoint transfer lifts before moving across a real platform lip,
 // then lowers onto its floor. Every segment is continuously swept.
 auto high=from;high[1]=(std::max)(from[1],to[1]);if(distance(swept(from,high,c,w,ps),high)>.05f)return false;
 auto across=to;across[1]=high[1];
 for(unsigned axis:{0u,2u}){auto corner=high;corner[axis]=across[axis];
  if(distance(swept(high,corner,c,w,ps),corner)>.05f||distance(swept(corner,across,c,w,ps),across)>.05f)continue;
  if(distance(swept(across,to,c,w,ps),to)<=.05f)return true;
 }return false;
}
bool state_valid(const State&s){return s.active&&valid(s.anchor)&&capsule(s.capsule)&&s.capsule.height==1700&&s.capsule.radius<=350&&finite(s.feet)&&std::hypot(s.feet[0]-s.anchor.bottom[0],s.feet[2]-s.anchor.bottom[2])<.05f&&s.feet[1]>=s.anchor.bottom[1]-.01f&&s.feet[1]<=s.anchor.top[1]+.01f;}
bool support(Vec3 at,const stage::Collision&w){at[1]+=20;auto hit=w.ray(at,{0,-1,0},50,stage::query::player_floor);return hit&&std::abs(hit->normal[1])>=.7f&&std::abs(hit->position[1]-(at[1]-20))<=5;}
}
bool valid(const Anchor&a){return a.id&&finite(a.bottom)&&finite(a.top)&&finite(a.bottomExit)&&finite(a.topExit)&&std::isfinite(a.facingYaw)&&std::abs(a.facingYaw)<=3.141593f&&std::hypot(a.top[0]-a.bottom[0],a.top[2]-a.bottom[2])<.01f&&a.top[1]-a.bottom[1]>=300&&a.top[1]-a.bottom[1]<=20000&&distance(a.bottom,a.bottomExit)<=2000&&distance(a.top,a.topExit)<=2000;}
std::vector<Anchor> load(const std::filesystem::path&path){
 if(std::filesystem::file_size(path)>65536)throw std::runtime_error("Ladder file size");
 std::ifstream in(path);std::string magic;unsigned count=0;if(!(in>>magic>>count)||magic!="GWLA1"||count>128)throw std::runtime_error("Ladder header");
 std::vector<Anchor> out;std::set<unsigned> ids;
 for(unsigned i=0;i<count;++i){Anchor a;unsigned id=0;if(!(in>>id)||!id||id>65535||!ids.insert(id).second)throw std::runtime_error("Ladder identity");a.id=uint16_t(id);for(auto*p:{&a.bottom,&a.top,&a.bottomExit,&a.topExit})for(auto&v:*p)if(!(in>>v))throw std::runtime_error("Ladder coordinates");if(!(in>>a.facingYaw)||!valid(a))throw std::runtime_error("Ladder anchor");out.push_back(a);}
 in>>std::ws;if(!in.eof())throw std::runtime_error("Ladder trailing data");return out;
}
std::optional<State> enter(const Anchor&a,Vec3 feet,stage::Capsule c,const stage::Collision&w,std::span<const Body>ps){
 if(!valid(a)||!finite(feet)||!capsule(c)||c.height!=1700||c.radius>350||!peers_valid(ps)||!free(feet,c,w,ps)||!support(feet,w))return {};
 const bool upper=distance(feet,a.topExit)<distance(feet,a.bottomExit);const auto exit=upper?a.topExit:a.bottomExit,to=upper?a.top:a.bottom;
 if(distance(feet,exit)>maximum_enter_distance||std::abs(feet[1]-to[1])>650)return {};
 if(!route(feet,to,c,w,ps))return {};return State{a,to,c,true};
}
bool advance(State&s,float axis,double seconds,const stage::Collision&w,std::span<const Body>ps){
 if(!state_valid(s)||!peers_valid(ps)||!std::isfinite(axis)||std::abs(axis)>1||!std::isfinite(seconds)||seconds<0||seconds>.25)return false;
 auto next=s;double remaining=seconds;while(remaining>0){const double dt=(std::min)(remaining,.016);auto to=next.feet;to[1]=std::clamp(to[1]+axis*speed*float(dt),next.anchor.bottom[1],next.anchor.top[1]);next.feet=swept(next.feet,to,next.capsule,w,ps);remaining-=dt;}s=next;return true;
}
bool leave(State&s,const stage::Collision&w,std::span<const Body>ps){
 if(!state_valid(s)||!peers_valid(ps))return false;Vec3 to;
 if(std::abs(s.feet[1]-s.anchor.bottom[1])<=endpoint_tolerance)to=s.anchor.bottomExit;else if(std::abs(s.feet[1]-s.anchor.top[1])<=endpoint_tolerance)to=s.anchor.topExit;else return false;
 if(!support(to,w)||!route(s.feet,to,s.capsule,w,ps))return false;s.feet=to;s.active=false;return true;
}
}
