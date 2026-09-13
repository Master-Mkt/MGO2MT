#include "rigid_physics.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace mgo2win::physics {
Vec3 add(Vec3 a,Vec3 b){for(unsigned i=0;i<3;++i)a[i]+=b[i];return a;}
Vec3 sub(Vec3 a,Vec3 b){for(unsigned i=0;i<3;++i)a[i]-=b[i];return a;}
Vec3 mul(Vec3 a,float s){for(auto&v:a)v*=s;return a;}
float dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vec3 cross(Vec3 a,Vec3 b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
float length(Vec3 a){return std::sqrt(dot(a,a));}Vec3 unit(Vec3 a){float n=length(a);return n>1e-8f?mul(a,1/n):Vec3{};}
bool finite(Vec3 a){for(float v:a)if(!std::isfinite(v)||std::abs(v)>=1e7f)return false;return true;}
bool finite(Quat q){float norm=0;for(float v:q){if(!std::isfinite(v))return false;norm+=v*v;}return norm>.9f&&norm<1.1f;}
Quat normalize(Quat q){float n=0;for(float v:q)n+=v*v;if(!std::isfinite(n)||n<1e-12f)return {0,0,0,1};n=std::sqrt(n);for(auto&v:q)v/=n;return q;}
Quat multiply(Quat a,Quat b){return {a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1],a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0],a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3],a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2]};}
Quat conjugate(Quat q){return {-q[0],-q[1],-q[2],q[3]};}
Vec3 rotate(Quat q,Vec3 v){auto t=mul(cross({q[0],q[1],q[2]},v),2);return add(v,add(mul(t,q[3]),cross({q[0],q[1],q[2]},t)));}
Quat between(Vec3 a,Vec3 b){a=unit(a);b=unit(b);float d=dot(a,b);if(length(a)<.5f||length(b)<.5f)return {0,0,0,1};if(d<-.99999f){auto axis=unit(cross(a,std::abs(a[0])<.8f?Vec3{1,0,0}:Vec3{0,1,0}));return {axis[0],axis[1],axis[2],0};}auto c=cross(a,b);return normalize({c[0],c[1],c[2],1+d});}
static Vec3 limited(Vec3 v,float cap){float n=length(v);return n>cap?mul(v,cap/n):v;}
// Existing native sleep-support policy; not an original PH slope constant.
static constexpr float sleepSupportCosine=.4f;
bool RigidBody::valid()const{return finite(position)&&finite(velocity)&&finite(angularVelocity)&&finite(rotation)&&std::isfinite(mass)&&mass>0&&mass<=100000&&std::isfinite(radius)&&radius>0&&radius<=10000&&std::isfinite(halfLength)&&halfLength>=0&&halfLength<=50000&&std::isfinite(friction)&&friction>=0&&friction<=10&&std::isfinite(restitution)&&restitution>=0&&restitution<=1&&std::isfinite(linearDamping)&&linearDamping>=0&&std::isfinite(angularDamping)&&angularDamping>=0;}
std::pair<Vec3,Vec3> RigidBody::segment()const{auto d=rotate(rotation,{0,halfLength,0});return {sub(position,d),add(position,d)};}
Vec3 RigidBody::inverse_inertia(Vec3 torque)const{auto local=rotate(conjugate(rotation),torque);float height=2*(halfLength+radius),axial=.5f*mass*radius*radius,other=mass*(3*radius*radius+height*height)/12;local={local[0]/other,local[1]/axial,local[2]/other};return rotate(rotation,local);}
void RigidBody::impulse(Vec3 impulse,Vec3 point){if(!valid()||!finite(impulse)||!finite(point))return;velocity=limited(add(velocity,mul(impulse,1/mass)),50000);angularVelocity=limited(add(angularVelocity,inverse_inertia(cross(sub(point,position),impulse))),30);wake();}
void solve_static(RigidBody&b,const stage::Collision&world,float dt,bool bounce,std::vector<ContactImpact>* impacts){
 if(!b.valid())return;auto[a,z]=b.segment();auto contacts=world.contacts(a,z,b.radius,2,8);bool resting=false;
 for(const auto&c:contacts){auto n=c.normal;auto r=sub(c.point,b.position);auto pointVelocity=add(b.velocity,cross(b.angularVelocity,r));float normalSpeed=dot(pointVelocity,n);float normalImpulse=0;auto mat=world.material(c.triangle);
  // Native audible contact threshold; report pre-impulse approach velocity.
  // One bounded strongest contact per material/object in this caller batch.
  if(impacts&&normalSpeed<-300){ContactImpact hit{c.point,n,-normalSpeed,mat,world.triangles[c.triangle].object};auto same=std::find_if(impacts->begin(),impacts->end(),[&](const auto& old){return old.object==hit.object&&old.material.id==hit.material.id;});if(same!=impacts->end()){if(hit.approachSpeed>same->approachSpeed)*same=hit;}else if(impacts->size()<32)impacts->push_back(hit);}
  if(normalSpeed<0){float k=1/b.mass+dot(n,cross(b.inverse_inertia(cross(r,n)),r));float restitution=bounce&&normalSpeed<-150?std::min(b.restitution,mat.restitution):0;normalImpulse=-(1+restitution)*normalSpeed/std::max(k,1e-9f);auto j=mul(n,normalImpulse);b.velocity=add(b.velocity,mul(j,1/b.mass));b.angularVelocity=add(b.angularVelocity,b.inverse_inertia(cross(r,j)));}
  pointVelocity=add(b.velocity,cross(b.angularVelocity,r));auto tangent=sub(pointVelocity,mul(n,dot(pointVelocity,n)));float speed=length(tangent);
  if(speed>.001f){auto t=mul(tangent,1/speed);float k=1/b.mass+dot(t,cross(b.inverse_inertia(cross(r,t)),r));float friction=std::sqrt(b.friction*mat.friction);float support=std::max(normalImpulse,b.mass*9800*std::max(0.f,n[1])*std::max(dt,0.f));float strength=std::min(speed/std::max(k,1e-9f),friction*support);auto j=mul(t,-strength);b.velocity=add(b.velocity,mul(j,1/b.mass));b.angularVelocity=add(b.angularVelocity,b.inverse_inertia(cross(r,j)));}
  if(c.penetration>.1f)b.position=add(b.position,mul(n,std::min(c.penetration+.1f,b.radius*.25f)));
  resting|=n[1]>sleepSupportCosine;
 }
 b.velocity=limited(b.velocity,50000);b.angularVelocity=limited(b.angularVelocity,30);
 if(resting&&length(b.velocity)<15&&length(b.angularVelocity)<.06f){b.quietTime+=std::max(0.f,dt);if(b.quietTime>.6f){b.sleeping=true;b.velocity={};b.angularVelocity={};}}else b.quietTime=0;
}
void integrate(RigidBody&b,const stage::Collision&world,float dt,Vec3 gravity,std::vector<ContactImpact>* impacts){
 if(!b.valid()||!finite(gravity)||!std::isfinite(dt)||dt<=0||dt>1.f/120+.000001f)return;
 if(b.sleeping){
  // Removing a supporting object must wake the body even while it touches a
  // side wall or ceiling. A contact alone does not oppose gravity. Keep the
  // native support threshold used when entering sleep, and inspect the same
  // bounded contact count as the static solver so a wall cannot hide a floor.
  if(length(gravity)<=1e-8f)return; // no force requires support in zero gravity
  auto[a,z]=b.segment();auto contacts=world.contacts(a,z,b.radius,3,8);auto up=unit(mul(gravity,-1));
  if(std::any_of(contacts.begin(),contacts.end(),[&](const auto&c){return dot(c.normal,up)>sleepSupportCosine;}))return;
  b.wake();
 }
 auto[a,z]=b.segment();b.velocity=mul(add(b.velocity,mul(gravity,dt)),std::exp(-b.linearDamping*dt));b.angularVelocity=mul(b.angularVelocity,std::exp(-b.angularDamping*dt));auto delta=mul(b.velocity,dt);
 // Preserve tangential travel at support/wall contacts. Truncating the whole
 // delta at a floor hit would incorrectly freeze all horizontal movement.
 for(unsigned slide=0;slide<4&&length(delta)>.0001f;++slide){auto hit=world.sweep_segment(a,z,delta,b.radius,1);if(!hit){b.position=add(b.position,delta);break;}auto travel=mul(delta,std::clamp(hit->fraction,0.f,1.f));b.position=add(b.position,travel);a=add(a,travel);z=add(z,travel);delta=sub(delta,travel);float into=dot(delta,hit->normal);if(into>=0)break;delta=sub(delta,mul(hit->normal,into));}
 float angle=length(b.angularVelocity)*dt;if(angle>1e-8f){auto axis=unit(b.angularVelocity);float s=std::sin(angle*.5f);b.rotation=normalize(multiply({axis[0]*s,axis[1]*s,axis[2]*s,std::cos(angle*.5f)},b.rotation));}
 solve_static(b,world,dt,true,impacts);
}
void RigidBody::step(const stage::Collision&world,float seconds,Vec3 gravity,std::vector<ContactImpact>* impacts){if(!valid()||!std::isfinite(seconds)||seconds<=0||!finite(gravity))return;float remaining=std::min(seconds,.2f);while(remaining>1e-7f){float dt=std::min(remaining,1.f/240);integrate(*this,world,dt,gravity,impacts);remaining-=dt;}}
}
