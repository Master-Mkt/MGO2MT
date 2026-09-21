#include "foot_ik.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace mgo2mt::foot_ik {
using namespace physics;
namespace {
constexpr std::array<std::array<uint32_t,4>,2> keys{{
 {0x449d08,0xf7f0c4,0xfb4232,0xf81206},
 {0x459d14,0xfaf104,0x5b4a33,0xfb1246}}};
struct Frame {Vec3 p;Quat q;};
std::vector<Frame> frames(std::span<const CatalogBone> bones,const MotionPose& pose){
 std::vector<Frame> out;out.reserve(bones.size());
 for(const auto& b:bones){auto it=pose.rotations.find(b.key);Quat q=it==pose.rotations.end()?Quat{0,0,0,1}:it->second;
  Vec3 local=b.parent<0?add(b.position,pose.root):sub(b.position,bones[b.parent].position);
  out.push_back(b.parent<0?Frame{local,q}:Frame{add(out[b.parent].p,rotate(out[b.parent].q,local)),normalize(multiply(out[b.parent].q,q))});}
 return out;
}
Vec3 vertex(const PreparedCharacter& body,size_t i,std::span<const CatalogBone> bones,const std::vector<Frame>& fk){
 const auto& v=body.bind[i];const auto& skin=body.skin[i];Vec3 out{};
 for(unsigned j=0;j<4;++j)if(skin.weights[j]>0){auto b=skin.bones[j];auto local=sub(add({v.x,v.y,v.z},skin.offsets[j]),bones[b].position);out=add(out,mul(add(fk[b].p,rotate(fk[b].q,local)),skin.weights[j]));}
 return out;
}
Quat yaw_rotation(float yaw){return {0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};}
std::optional<stage::CollisionRayHit> support(const stage::Collision& world,Vec3 point,float originY,float above,float below,stage::CollisionQuery query=stage::query::ik){
 point[1]=originY+above;
 for(auto h:world.ray_all(point,{0,-1,0},above+below,query)){
  if(h.normal[1]<0)h.normal=mul(h.normal,-1);
  if(h.normal[1]>=.70710678f)return h;
 }
 return {};
}
void set_world_rotation(MotionPose& pose,std::span<const CatalogBone> bones,const std::vector<Frame>& fk,unsigned bone,Quat world){
 auto parent=bones[bone].parent;pose.rotations[bones[bone].key]=normalize(parent<0?world:multiply(conjugate(fk[parent].q),world));
}
void leg_solve(MotionPose& pose,std::span<const CatalogBone> bones,unsigned hip,unsigned knee,unsigned ankle,Vec3 target,Quat footRotation){
 auto fk=frames(bones,pose);auto h=fk[hip].p,k=fk[knee].p,a=fk[ankle].p;
 float upper=length(sub(k,h)),lower=length(sub(a,k));auto axis=unit(sub(target,h));
 float distance=std::clamp(length(sub(target,h)),std::abs(upper-lower)+.01f,upper+lower-.01f);
 auto pole=sub(sub(k,h),mul(axis,dot(sub(k,h),axis)));
 if(length(pole)<.01f)pole=sub(Vec3{0,0,1},mul(axis,axis[2]));
 if(length(pole)<.01f)pole=sub(Vec3{1,0,0},mul(axis,axis[0]));
 pole=unit(pole);float along=(upper*upper+distance*distance-lower*lower)/(2*distance);
 auto nextKnee=add(h,add(mul(axis,along),mul(pole,std::sqrt(std::max(0.f,upper*upper-along*along)))));
 set_world_rotation(pose,bones,fk,hip,multiply(between(sub(k,h),sub(nextKnee,h)),fk[hip].q));
 fk=frames(bones,pose);auto reachable=add(h,mul(axis,distance));
 set_world_rotation(pose,bones,fk,knee,multiply(between(sub(fk[ankle].p,fk[knee].p),sub(reachable,fk[knee].p)),fk[knee].q));
 fk=frames(bones,pose);set_world_rotation(pose,bones,fk,ankle,footRotation);
}
}
bool eligible(PlayerMotion motion){switch(motion){
 case PlayerMotion::Idle:case PlayerMotion::Walk:case PlayerMotion::Run:case PlayerMotion::CrouchIdle:case PlayerMotion::CrouchWalk:case PlayerMotion::Aim:case PlayerMotion::Reload:return true;
 default:return false;}}
void Solver::reset(){initialized_=rig_=false;offsets_={};normals_={{{0,1,0},{0,1,0}}};pelvis_=0;result_={};for(auto& l:legs_)l.sole.clear();}
bool Solver::prepare(const CharacterCatalog& catalog,const PreparedCharacter& body){
 auto bones=catalog.skeleton(body.gender);if(bones.size()<21||bones.size()>256||bones.front().key!=0xa89233||body.bind.size()!=body.skin.size())return false;
 for(unsigned side=0;side<2;++side){auto& leg=legs_[side];unsigned indices[4]{};
  for(unsigned j=0;j<4;++j){auto it=std::find_if(bones.begin(),bones.end(),[&](const auto& b){return b.key==keys[side][j];});if(it==bones.end())return false;indices[j]=unsigned(it-bones.begin());}
  leg.hip=indices[0];leg.knee=indices[1];leg.ankle=indices[2];leg.toe=indices[3];leg.sole.clear();
  if(bones[leg.knee].parent!=int(leg.hip)||bones[leg.ankle].parent!=int(leg.knee)||bones[leg.toe].parent!=int(leg.ankle))return false;
  for(auto pair:{std::pair{leg.hip,leg.knee},std::pair{leg.knee,leg.ankle}}){float n=length(sub(bones[pair.first].position,bones[pair.second].position));if(n<100||n>1000)return false;}
  for(size_t i=0;i<body.skin.size();++i){float weight=0;for(unsigned j=0;j<4;++j)if(body.skin[i].bones[j]==leg.ankle||body.skin[i].bones[j]==leg.toe)weight+=body.skin[i].weights[j];
   if(weight>.8f&&body.bind[i].y<bones[leg.ankle].position[1]+20)leg.sole.push_back(i);}
  if(leg.sole.empty())return false;
 }
 return true;
}
MotionPose Solver::solve(const CharacterCatalog& catalog,const PreparedCharacter& body,const MotionPose& input,const stage::Collision* world,Vec3 origin,float yaw,float seconds,bool enabled,motion_blend::Scope scope){
 result_={};
 if(!enabled||!world||!finite(origin)||!std::isfinite(yaw)||!std::isfinite(seconds)||seconds<0||seconds>.5f){reset();return input;}
 if(!initialized_||scope!=scope_||length(sub(origin,lastOrigin_))>1000){reset();scope_=scope;rig_=prepare(catalog,body);initialized_=true;}
 lastOrigin_=origin;result_.rig=rig_;if(!rig_)return input;
 // Remote actors also need real nearby body support; do not plant airborne feet.
 auto ground=support(*world,origin,origin[1],60,180,stage::query::player_floor);if(!ground||std::abs(ground->position[1]-origin[1])>180){offsets_={};pelvis_=0;return input;}
 auto bones=catalog.skeleton(body.gender);if(input.rootBone!=bones.front().key||!finite(input.root))return input;
 for(const auto&[key,q]:input.rotations)if(!finite(q))return input;
 auto pose=input;auto original=frames(bones,input);auto turn=yaw_rotation(yaw);
 auto toWorld=[&](Vec3 p){return add(origin,rotate(turn,p));};
 std::array<Vec3,2> targets{};std::array<Quat,2> rotations{};std::array<bool,2> supported{};
 const float smoothing=1-std::exp(-18.f*std::min(seconds,.1f));float pelvisTarget=0;
 for(unsigned side=0;side<2;++side){auto& leg=legs_[side];auto ankle=original[leg.ankle];auto& info=result_.feet[side];info.ankle=toWorld(ankle.p);
  Vec3 lo{1e9f,1e9f,1e9f},hi{-1e9f,-1e9f,-1e9f};
  for(auto i:leg.sole){auto p=toWorld(vertex(body,i,bones,original));for(unsigned j=0;j<3;++j){lo[j]=std::min(lo[j],p[j]);hi[j]=std::max(hi[j],p[j]);}}
  float planted=std::clamp((140.f-(lo[1]-origin[1]))/100.f,0.f,1.f);
  std::vector<stage::CollisionRayHit> hits;
  const Vec3 samples[]={{(lo[0]+hi[0])*.5f,0,(lo[2]+hi[2])*.5f},{lo[0],0,lo[2]},{hi[0],0,lo[2]},{lo[0],0,hi[2]},{hi[0],0,hi[2]}};
  for(auto p:samples)if(auto h=support(*world,p,origin[1],300,300))hits.push_back(*h);
  if(hits.empty()){offsets_[side]*=1-smoothing;targets[side]=add(ankle.p,{0,offsets_[side],0});rotations[side]=ankle.q;continue;}
  auto highest=*std::max_element(hits.begin(),hits.end(),[](const auto&a,const auto&b){return a.position[1]<b.position[1];});
  normals_[side]=unit(add(mul(normals_[side],1-smoothing),mul(highest.normal,smoothing)));
  auto normal=rotate(conjugate(turn),normals_[side]);
  auto tilt=between({0,1,0},unit(add(mul(Vec3{0,1,0},1-planted),mul(normal,planted))));
  rotations[side]=normalize(multiply(tilt,ankle.q));
  auto adjusted=input;set_world_rotation(adjusted,bones,original,leg.ankle,rotations[side]);auto footFrames=frames(bones,adjusted);
  float penetration=-1e9f;
  for(auto i:leg.sole){auto p=toWorld(vertex(body,i,bones,footFrames));
   for(const auto& hit:hits)penetration=std::max(penetration,(4.f-dot(sub(p,hit.position),hit.normal))/hit.normal[1]);}
  // Preserve the swing arc. Only low feet follow descending terrain; upward
  // collision correction always wins over the smooth return to avoid sinking.
  auto at=info.ankle;float floorY=highest.position[1]-((at[0]-highest.position[0])*highest.normal[0]+(at[2]-highest.position[2])*highest.normal[2])/highest.normal[1];
  float follow=std::min(0.f,floorY-ground->position[1])*planted;
  float desired=std::clamp(std::max(follow,penetration),-220.f,320.f);
  if(penetration>320)continue;
  offsets_[side]+=(desired-offsets_[side])*smoothing;
  offsets_[side]=std::max(offsets_[side],penetration);
  targets[side]=add(ankle.p,{0,offsets_[side],0});supported[side]=true;
  info.supported=true;info.normal=highest.normal;info.correction=offsets_[side];
  pelvisTarget=std::min(pelvisTarget,offsets_[side]*planted);
 }
 if(!supported[0]&&!supported[1]){pelvis_=0;return input;}
 float reachablePelvis=0;
 for(unsigned side=0;side<2;++side)if(supported[side]){const auto& l=legs_[side];auto h=original[l.hip].p;auto delta=sub(targets[side],h);
  float reach=length(sub(original[l.knee].p,h))+length(sub(original[l.ankle].p,original[l.knee].p))-.1f;
  float vertical=std::sqrt(std::max(0.f,reach*reach-delta[0]*delta[0]-delta[2]*delta[2]));
  reachablePelvis=std::min(reachablePelvis,targets[side][1]+vertical-h[1]);}
 pelvisTarget=std::max(-200.f,pelvisTarget);pelvis_+=(pelvisTarget-pelvis_)*smoothing;
 pelvis_=std::min(pelvis_,std::max(-200.f,reachablePelvis));pose.root[1]+=pelvis_;
 for(unsigned side=0;side<2;++side){auto& leg=legs_[side];
  if(!supported[side]){targets[side]=add(original[leg.ankle].p,{0,pelvis_,0});continue;}
  leg_solve(pose,bones,leg.hip,leg.knee,leg.ankle,targets[side],rotations[side]);
 }
 auto final=frames(bones,pose);result_.active=supported[0]||supported[1];result_.pelvis=pelvis_;
 for(unsigned side=0;side<2;++side){auto& info=result_.feet[side];info.target=toWorld(targets[side]);info.error=length(sub(final[legs_[side].ankle].p,targets[side]));}
 return pose;
}
}
