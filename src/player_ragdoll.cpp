#include "player_ragdoll.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace mgo2win::player {
using namespace physics;
namespace {
// MGO2 ELF 1a55a41e...: FD4014 body->model, FD3E60 constraint pairs.
// MDN IDs were matched for both current GWC skeletons; see the evidence note.
constexpr std::array<unsigned,13> modelMap{0,2,4,6,7,10,11,13,14,15,17,18,19};
constexpr std::array<uint32_t,21> boneKeys{0xa89233,0x7011c4,0x6c02b2,0xf8d3cc,0xf5d387,0x019543,0x027a4c,0xfafa8b,0xfbfa42,0x619d43,0x62824c,0x5b028c,0x5c0243,0x449d08,0xf7f0c4,0xfb4232,0xf81206,0x459d14,0xfaf104,0x5b4a33,0xfb1246};
constexpr std::array<std::array<unsigned,2>,12> edges{{{1,0},{2,1},{3,1},{4,3},{5,1},{6,5},{7,0},{8,7},{9,8},{10,0},{11,10},{12,11}}};
constexpr std::array<unsigned,13> endMap{2,4,4,7,8,11,12,14,15,16,18,19,20};
// Capsule dimensions/mass/cones are explicit native policies, not original data.
constexpr std::array<float,13> radii{115,135,105,65,55,65,55,85,65,50,85,65,50};
constexpr std::array<float,13> masses{12,18,5,3,2,3,2,8,5,1.5f,8,5,1.5f};
constexpr std::array<float,12> cones{.8f,.9f,1.6f,2.4f,1.6f,2.4f,1.4f,2.4f,.9f,1.4f,2.4f,.9f};
Quat portion(Quat q,float fraction){q=normalize(q);if(q[3]<0)for(auto&v:q)v=-v;float half=std::acos(std::clamp(q[3],-1.f,1.f));float sin=std::sin(half);if(sin<1e-6f)return {0,0,0,1};float s=std::sin(half*fraction)/sin;return normalize({q[0]*s,q[1]*s,q[2]*s,std::cos(half*fraction)});}
Vec3 omega(Quat now,Quat before,float dt){auto q=normalize(multiply(now,conjugate(before)));if(q[3]<0)for(auto&v:q)v=-v;float angle=2*std::acos(std::clamp(q[3],-1.f,1.f));auto axis=unit(Vec3{q[0],q[1],q[2]});return mul(axis,std::min(15.f,angle/dt));}
Vec3 capped(Vec3 v,float maximum){float n=length(v);return n>maximum?mul(v,maximum/n):v;}
void turn(RigidBody&b,Vec3 correction){float angle=length(correction);if(angle<1e-8f)return;auto axis=mul(correction,1/angle);angle=std::min(angle,.2f);float s=std::sin(angle*.5f);b.rotation=normalize(multiply({axis[0]*s,axis[1]*s,axis[2]*s,std::cos(angle*.5f)},b.rotation));}
}
bool Ragdoll::start(const CharacterCatalog&catalog,unsigned gender,const MotionPose&input,Vec3 actorOrigin,float yaw){
 active_=false;skeleton_.clear();auto source=catalog.skeleton(gender);if(source.size()<21||source.size()>256||!finite(actorOrigin)||!finite(input.root)||!std::isfinite(yaw)||input.rootBone!=boneKeys[0])return false;for(float v:input.root)if(std::abs(v)>1000000)return false;
 // Match hashes instead of treating the debug index layout as a Win struct.
 std::array<unsigned,21> indices{};for(unsigned i=0;i<boneKeys.size();++i){auto at=std::find_if(source.begin(),source.end(),[&](auto&b){return b.key==boneKeys[i];});if(at==source.end())return false;indices[i]=unsigned(at-source.begin());}
 Quat yawRotation{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};std::vector<Quat> worldRot;std::vector<Vec3> worldPos;
 for(unsigned i=0;i<source.size();++i){auto&b=source[i];if(b.parent>=int(i)||b.parent< -1||!finite(b.position))return false;auto at=input.rotations.find(b.key);Quat q=at==input.rotations.end()?Quat{0,0,0,1}:at->second;if(!finite(q))return false;q=normalize(q);skeleton_.push_back({b.key,b.parent,b.position,q});Vec3 local=b.parent<0?add(b.position,input.root):sub(b.position,source[b.parent].position);
  if(b.parent<0){worldPos.push_back(add(actorOrigin,rotate(yawRotation,local)));worldRot.push_back(normalize(multiply(yawRotation,q)));}else{worldPos.push_back(add(worldPos[b.parent],rotate(worldRot[b.parent],local)));worldRot.push_back(normalize(multiply(worldRot[b.parent],q)));}}
 if(indices[0]!=0||source[0].parent!=-1)return false;
 for(unsigned i=0;i<bodies_.size();++i){unsigned bone=indices[modelMap[i]],end=indices[endMap[i]];modelIndices_[i]=bone;Vec3 local=sub(source[end].position,source[bone].position);if(i==2)local={0,190,0};float size=length(local);if(size<1)return false;
  axes_[i]=between({0,1,0},local);offsets_[i]=mul(local,.5f);auto&body=bodies_[i];body={};body.radius=std::min(radii[i],size*.48f);body.halfLength=std::max(0.f,size*.5f-body.radius);body.mass=masses[i];body.friction=.7f;body.restitution=.05f;body.linearDamping=.2f;body.angularDamping=1.0f;body.position=add(worldPos[bone],rotate(worldRot[bone],offsets_[i]));body.rotation=normalize(multiply(worldRot[bone],axes_[i]));if(!body.valid())return false;
 }
 for(unsigned i=0;i<links_.size();++i){auto[child,parent]=edges[i];auto anchor=worldPos[modelIndices_[child]];links_[i]={child,parent,rotate(conjugate(bodies_[child].rotation),sub(anchor,bodies_[child].position)),rotate(conjugate(bodies_[parent].rotation),sub(anchor,bodies_[parent].position)),normalize(multiply(conjugate(bodies_[parent].rotation),bodies_[child].rotation)),cones[i]};}
 originY_=actorOrigin[1];active_=true;return true;
}
void Ragdoll::constraints(){
 for(const auto&l:links_){auto&c=bodies_[l.child];auto&p=bodies_[l.parent];float ci=1/c.mass,pi=1/p.mass,sum=ci+pi;
  // Point constraints transfer leverage into angular motion as well as centre
  // translation. This lets knees/hips yield during impact instead of keeping
  // an upright collection of translated capsules suspended over the floor.
  for(unsigned axis=0;axis<3;++axis){Vec3 n{};n[axis]=1;auto cr=rotate(c.rotation,l.childAnchor),pr=rotate(p.rotation,l.parentAnchor);auto error=sub(add(c.position,cr),add(p.position,pr));float k=sum+dot(n,cross(c.inverse_inertia(cross(cr,n)),cr))+dot(n,cross(p.inverse_inertia(cross(pr,n)),pr));auto j=mul(n,-error[axis]*.8f/std::max(k,1e-8f));c.position=add(c.position,mul(j,ci));p.position=sub(p.position,mul(j,pi));turn(c,c.inverse_inertia(cross(cr,j)));turn(p,p.inverse_inertia(cross(pr,mul(j,-1))));}
  auto target=normalize(multiply(p.rotation,l.rest));auto q=normalize(multiply(target,conjugate(c.rotation)));float angle=2*std::acos(std::clamp(std::abs(q[3]),0.f,1.f));if(angle>l.cone){float fraction=(angle-l.cone)/angle*.4f;auto childQ=portion(q,fraction*ci/sum);auto parentQ=portion(conjugate(q),fraction*pi/sum);c.rotation=normalize(multiply(childQ,c.rotation));p.rotation=normalize(multiply(parentQ,p.rotation));}
 }
}
void Ragdoll::step(const stage::Collision&world,float seconds){
 if(!active_||!std::isfinite(seconds)||seconds<=0)return;float remaining=std::min(seconds,.2f);
 while(remaining>1e-7f){float dt=std::min(remaining,1.f/120);remaining-=dt;std::array<Vec3,13> before;std::array<Quat,13> rotations;
  for(unsigned i=0;i<bodies_.size();++i){before[i]=bodies_[i].position;rotations[i]=bodies_[i].rotation;integrate(bodies_[i],world,dt,{0,-9800,0});}
  // Keep all twelve joint iterations, but re-query the terrain after each
  // group of four. Doing so only after the entire pass allows deep limb
  // penetration; doing so after every correction dominated real-stage time.
  for(unsigned iteration=0;iteration<12;++iteration){constraints();if(iteration%4==3)for(auto&b:bodies_)solve_static(b,world,0,false);}
  for(unsigned i=0;i<bodies_.size();++i){auto&b=bodies_[i];b.velocity=capped(mul(sub(b.position,before[i]),1/dt),15000);b.angularVelocity=omega(b.rotation,rotations[i],dt);if(b.sleeping&&(length(b.velocity)>20||length(b.angularVelocity)>.1f))b.wake();solve_static(b,world,dt,false);if(!b.valid()){active_=false;return;}}
  if(std::abs(root_position()[1]-originY_)>1000000){active_=false;return;}
 }
}
void Ragdoll::impulse(Vec3 impulse,Vec3 point){if(!active_||!finite(impulse)||!finite(point))return;unsigned closest=0;float distance=std::numeric_limits<float>::max();for(unsigned i=0;i<bodies_.size();++i){float d=length(sub(bodies_[i].position,point));if(d<distance){distance=d;closest=i;}bodies_[i].wake();}bodies_[closest].impulse(impulse,point);}
Vec3 Ragdoll::root_position()const{if(!active_)return {};auto rootRotation=normalize(multiply(bodies_[0].rotation,conjugate(axes_[0])));return sub(bodies_[0].position,rotate(rootRotation,offsets_[0]));}
Vec3 Ragdoll::origin()const{auto root=root_position();return active_?Vec3{root[0],originY_,root[2]}:Vec3{};}
bool Ragdoll::supine()const{if(!active_)return false;auto q=normalize(multiply(bodies_[0].rotation,conjugate(axes_[0])));return rotate(q,{0,0,1})[1]>0;}
MotionPose Ragdoll::pose()const{MotionPose out;if(!active_)return out;out.rootBone=skeleton_[0].key;auto root=root_position();out.root={0,root[1]-originY_,0};std::vector<Quat> world(skeleton_.size());
 for(unsigned i=0;i<skeleton_.size();++i){auto&b=skeleton_[i];auto found=std::find(modelIndices_.begin(),modelIndices_.end(),i);Quat q=b.local;if(found!=modelIndices_.end()){unsigned body=unsigned(found-modelIndices_.begin());world[i]=normalize(multiply(bodies_[body].rotation,conjugate(axes_[body])));q=b.parent<0?world[i]:normalize(multiply(conjugate(world[b.parent]),world[i]));}else world[i]=b.parent<0?q:normalize(multiply(world[b.parent],q));out.rotations.emplace(b.key,q);}
 return out;
}
float Ragdoll::maximum_joint_error()const{float error=0;if(!active_)return error;for(const auto&l:links_){const auto&c=bodies_[l.child];const auto&p=bodies_[l.parent];error=std::max(error,length(sub(add(c.position,rotate(c.rotation,l.childAnchor)),add(p.position,rotate(p.rotation,l.parentAnchor)))));}return error;}
}
