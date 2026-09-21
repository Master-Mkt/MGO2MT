#include "weapon_hand.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace mgo2mt::weapon_hand {
namespace {
void require(bool v){if(!v)throw std::runtime_error("Invalid GWH1 hand motion");}
struct Reader{std::span<const char>b;size_t at=4;uint32_t u(){require(at+4<=b.size());uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(uint8_t(b[at++]))<<(i*8);return v;}float f(){auto w=u();float v;std::memcpy(&v,&w,4);require(std::isfinite(v)&&std::abs(v)<1000000);return v;}};
template<size_t N>std::array<float,N> vec(Reader&r){std::array<float,N> v;for(auto&x:v)x=r.f();if constexpr(N==4){float n=0;for(auto x:v)n+=x*x;require(n>.99f&&n<1.01f);}return v;}
std::array<float,4> slerp(const std::array<float,4>&a,const std::array<float,4>&b,float t){using namespace DirectX;XMFLOAT4 v;XMStoreFloat4(&v,XMQuaternionSlerp(XMVectorSet(a[0],a[1],a[2],a[3]),XMVectorSet(b[0],b[1],b[2],b[3]),t));return {v.x,v.y,v.z,v.w};}
}
Bank::Bank(std::span<const char>b){require(b.size()>=12&&b.size()<128*1024*1024&&!std::memcmp(b.data(),"GWH1",4));Reader r{b};auto version=r.u();require(version>=1&&version<=3);auto count=r.u();require(count>0&&count<=4096);
 for(unsigned i=0;i<count;++i){Clip c;c.weapon=r.u();c.index=r.u();c.key=r.u();c.frames=r.u();c.fps=r.u();c.rootBone=r.u();c.bone=r.u();auto n=r.u();require(c.weapon>0&&c.weapon<128&&c.index<4096&&c.key&&c.frames>0&&c.frames<=3600&&c.fps==60&&c.rootBone&&c.bone&&n>0&&n<=256);require(uint64_t(c.frames+1)*(40+uint64_t(n)*16)+uint64_t(n)*4<=b.size()-r.at);
  for(unsigned f=0;f<=c.frames;++f){c.roots.push_back(vec<3>(r));c.positions.push_back(vec<3>(r));c.rotations.push_back(vec<4>(r));}
  for(unsigned j=0;j<n;++j){auto key=r.u();require(key&&!c.tracks.contains(key));auto&tr=c.tracks[key];for(unsigned f=0;f<=c.frames;++f)tr.push_back(vec<4>(r));}
  if(version>=2){c.magazineBone=r.u();if(c.magazineBone){require(c.tracks.contains(c.magazineBone));for(unsigned f=0;f<=c.frames;++f){c.magazinePositions.push_back(vec<3>(r));c.magazineRotations.push_back(vec<4>(r));}}}
  require(c.tracks.contains(c.rootBone)&&c.tracks.contains(c.bone));auto key=std::pair{c.weapon,c.index};require(!clips_.contains(key));clips_.emplace(key,std::move(c));
 }
 if(version>=3){auto n=r.u();require(n>0&&n<=127);for(unsigned i=0;i<n;++i){auto weapon=r.u();require(weapon>0&&weapon<128&&!selections_.contains(weapon));Selection s;for(auto*a:{&s.hold,&s.aim,&s.reload,&s.fire,&s.cqc})for(auto& index:*a){index=r.u();require(index==~0u||clips_.contains({weapon,index}));}require(s.hold[0]!=~0u);selections_.emplace(weapon,s);}}
 require(r.at==b.size());
}
std::optional<double> Bank::duration(uint32_t weapon,uint32_t index)const{auto it=clips_.find({weapon,index});if(it==clips_.end())return {};return double(it->second.frames)/it->second.fps;}
std::vector<uint32_t> Bank::weapons()const{std::vector<uint32_t> out;for(const auto&[key,clip]:clips_)if(out.empty()||out.back()!=key.first)out.push_back(key.first);return out;}
std::optional<Sample> Bank::sample(uint32_t weapon,uint32_t index,double seconds,bool loop)const{
 auto it=clips_.find({weapon,index});if(it==clips_.end()||!std::isfinite(seconds))return {};const auto&c=it->second;double f=std::max(0.,seconds)*c.fps;f=loop?std::fmod(f,double(c.frames)):std::min(f,double(c.frames));unsigned a=unsigned(f),b=std::min(a+1,c.frames);float t=float(f-a);Sample s;s.weapon=weapon;s.index=index;s.key=c.key;s.pose.rootBone=c.rootBone;s.point.bone=c.bone;s.seconds=f/60.;
 if(c.magazineBone){Point p;p.bone=c.magazineBone;for(unsigned j=0;j<3;++j)p.position[j]=c.magazinePositions[a][j]*(1-t)+c.magazinePositions[b][j]*t;p.rotation=slerp(c.magazineRotations[a],c.magazineRotations[b],t);s.magazine=p;}
 for(unsigned j=0;j<3;++j){s.pose.root[j]=j==1?c.roots[a][j]*(1-t)+c.roots[b][j]*t:0;s.point.position[j]=c.positions[a][j]*(1-t)+c.positions[b][j]*t;}s.point.rotation=slerp(c.rotations[a],c.rotations[b],t);
 for(const auto&[key,tr]:c.tracks)s.pose.rotations[key]=slerp(tr[a],tr[b],t);return s;
}
std::optional<Sample> Bank::select(uint32_t weapon,PlayerMotion motion,double seconds,bool aiming,double fireSeconds,double cqcSeconds,int posture)const{
 const bool prone=motion==PlayerMotion::ProneIdle||motion==PlayerMotion::ProneForward||motion==PlayerMotion::ProneBackward;
 const bool supine=motion==PlayerMotion::SupineIdle||motion==PlayerMotion::SupineForward||motion==PlayerMotion::SupineBackward;
 if(auto it=selections_.find(weapon);it!=selections_.end()){
  const unsigned stance=posture>=0?unsigned(std::clamp(posture,0,2)):(prone||supine)?2:(motion==PlayerMotion::CrouchIdle||motion==PlayerMotion::CrouchWalk)?1:0;
  const auto&s=it->second;auto choose=[&](const std::array<uint32_t,3>&indices,double time,bool active){if(!active||!std::isfinite(time)||time<0)return std::optional<Sample>{};auto index=indices[stance]!=~0u?indices[stance]:indices[0];auto length=duration(weapon,index);return length&&time<=*length?sample(weapon,index,time,false):std::nullopt;};
  if(auto p=choose(s.cqc,cqcSeconds,cqcSeconds>=0)){p->cqc=true;return p;}
  if(motion==PlayerMotion::Reload){auto index=s.reload[stance]!=~0u?s.reload[stance]:s.reload[0];if(auto p=sample(weapon,index,seconds,false))return p;}
  if(auto p=choose(s.fire,fireSeconds,fireSeconds>=0)){p->sighting=aiming||motion==PlayerMotion::Aim;return p;}
  const auto&indices=aiming||motion==PlayerMotion::Aim?s.aim:s.hold;auto index=indices[stance]!=~0u?indices[stance]:indices[0];auto result=sample(weapon,index,0,false);
  // These original HG archives provide weapon-specific MTP offsets but an
  // identity-only body track. Layer the original HG body pose beneath those
  // offsets; do not pose an entire character with the identity placeholder.
  if(result&&(weapon==4||weapon==7)&&(aiming||motion==PlayerMotion::Aim)){
   constexpr uint32_t bodyKeys[]={0xa89233,0x7011c4,0x6c02b2,0xf8d3cc,0xf5d387,0x019543,0x027a4c,0xfafa8b,0xfbfa42,0x619d43,0x62824c,0x5b028c,0x5c0243,0x449d08,0xf7f0c4,0xfb4232,0xf81206,0x459d14,0xfaf104,0x5b4a33,0xfb1246};
   const bool empty=std::all_of(std::begin(bodyKeys),std::end(bodyKeys),[&](uint32_t key){auto it=result->pose.rotations.find(key);if(it==result->pose.rotations.end())return true;const auto&q=it->second;return std::abs(q[0])+std::abs(q[1])+std::abs(q[2])<1e-7f&&std::abs(q[3]-1)<1e-7f;});
   if(empty)if(auto body=sample(3,2,0,false)){result->pose.root=body->pose.root;for(auto key:bodyKeys)if(auto it=body->pose.rotations.find(key);it!=body->pose.rotations.end())result->pose.rotations[key]=it->second;}
  }
  if(result)result->sighting=aiming||motion==PlayerMotion::Aim;
  // Native recoil overlays the authored stance when there is no separate
  // source firing clip. Hands remain attached through the same posed bones.
  if(result&&std::isfinite(fireSeconds)&&fireSeconds>=0&&fireSeconds<.15&&weapon>1&&weapon<=50){using namespace DirectX;float kick=.045f*float(std::sin(fireSeconds/.15*3.141592653589793));if(auto q=result->pose.rotations.find(0x6c02b2);q!=result->pose.rotations.end()){auto&a=q->second;XMFLOAT4 out;XMStoreFloat4(&out,XMQuaternionMultiply(XMVectorSet(a[0],a[1],a[2],a[3]),XMQuaternionRotationAxis(XMVectorSet(1,0,0,0),kick)));a={out.x,out.y,out.z,out.w};}}
  return result;
 }
 // Runtime action selection is kept explicit; source clip identity is stored
 // separately. Original dynamic state-machine parity is not implied here.
 if(weapon==25)return sample(25,motion==PlayerMotion::Reload?3:(aiming||motion==PlayerMotion::Aim)?(prone?7:supine?5:2):prone?1:0,motion==PlayerMotion::Reload?seconds:0,false);
 if(weapon==3)return sample(3,(aiming||motion==PlayerMotion::Aim)?2:prone?1:0,0,false);
 return {};
}
void upper_body(MotionPose&base,const Sample&s,std::span<const CatalogBone>bones){
 require(base.rootBone==s.pose.rootBone);std::vector<bool> arms(bones.size());
 if(s.sighting){
  // Native pose composition: source local rotations assume the source hip
  // orientation. Copying them below a prone locomotion hip rotates the gun
  // down/backward. Preserve the authored upper-chain MODEL-space rotation,
  // reconstructing its local rotation under the current locomotion parent.
  using namespace DirectX;std::vector<XMMATRIX> sourceWorld(bones.size()),world(bones.size());
  auto rotation=[](const MotionPose&p,uint32_t key){auto it=p.rotations.find(key);auto q=it==p.rotations.end()?std::array<float,4>{0,0,0,1}:it->second;return XMMatrixRotationQuaternion(XMVectorSet(q[0],q[1],q[2],q[3]));};
  for(size_t i=0;i<bones.size();++i){const auto&b=bones[i];require(b.parent<0||size_t(b.parent)<i);arms[i]=b.key==0x019543||b.key==0x619d43||(b.parent>=0&&arms[b.parent]);const bool upper=b.key==0x7011c4||b.key==0x6c02b2||arms[i];sourceWorld[i]=rotation(s.pose,b.key);if(b.parent>=0)sourceWorld[i]*=sourceWorld[b.parent];
   auto local=rotation(base,b.key);if(upper&&s.pose.rotations.contains(b.key)){local=sourceWorld[i];if(b.parent>=0)local*=XMMatrixTranspose(world[b.parent]);XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionNormalize(XMQuaternionRotationMatrix(local)));base.rotations[b.key]={q.x,q.y,q.z,q.w};}
   world[i]=local;if(b.parent>=0)world[i]*=world[b.parent];
  }return;
 }
 // 338E60/33BCD0 select the name-matched 333F00 branch. Its hand masks
 // include spine + arm + named fingers (0x1E7/0x1E07), excluding neck/head.
 // Root and locomotion remain owned by the native movement controller.
 for(size_t i=0;i<bones.size();++i){const auto&b=bones[i];require(b.parent<0||size_t(b.parent)<i);arms[i]=b.key==0x019543||b.key==0x619d43||(b.parent>=0&&arms[b.parent]);const bool upper=b.key==0x7011c4||b.key==0x6c02b2||arms[i];if(upper)if(auto it=s.pose.rotations.find(b.key);it!=s.pose.rotations.end())base.rotations[b.key]=it->second;}
}
std::optional<std::array<float,16>> frame(const PreparedCharacter&body,const Point&p){
 using namespace DirectX;auto it=body.boneFrames.find(p.bone);if(it==body.boneFrames.end())return {};float norm=0;for(float v:p.rotation){if(!std::isfinite(v))return {};norm+=v*v;}if(norm<.99f||norm>1.01f)return {};for(float v:p.position)if(!std::isfinite(v))return {};
 XMFLOAT4X4 bone;std::memcpy(&bone,it->second.data(),sizeof(bone));auto&q=p.rotation;auto local=XMMatrixRotationQuaternion(XMVectorSet(q[0],q[1],q[2],q[3]))*XMMatrixTranslation(p.position[0],p.position[1],p.position[2]);XMFLOAT4X4 m;XMStoreFloat4x4(&m,local*XMLoadFloat4x4(&bone));std::array<float,16> out;std::memcpy(out.data(),&m,sizeof(m));return out;
}
bool aim_pitch(PreparedCharacter&body,std::span<const CatalogBone>bones,float radians){
 if(!std::isfinite(radians)||body.skin.size()!=body.model.vertices.size())return false;
 auto pivot=body.bone_position(0x6c02b2);if(!pivot)return false;
 using namespace DirectX;const auto&p=*pivot;auto matrix=XMMatrixTranslation(-p[0],-p[1],-p[2])*XMMatrixRotationX(-std::clamp(radians,-1.45f,1.45f))*XMMatrixTranslation(p[0],p[1],p[2]);
 std::vector<bool> upper(bones.size());for(size_t i=0;i<bones.size();++i){const auto&b=bones[i];upper[i]=b.key==0x6c02b2||b.key==0x019543||b.key==0x619d43||(b.parent>=0&&size_t(b.parent)<i&&upper[b.parent]);}
 for(size_t i=0;i<body.model.vertices.size();++i){float w=0;for(unsigned j=0;j<4;++j){auto index=body.skin[i].bones[j];if(index<upper.size()&&upper[index])w+=body.skin[i].weights[j];}w=std::clamp(w,0.f,1.f);if(w==0)continue;auto&v=body.model.vertices[i];XMFLOAT3 point,normal;XMStoreFloat3(&point,XMVector3TransformCoord(XMVectorSet(v.x,v.y,v.z,1),matrix));XMStoreFloat3(&normal,XMVector3TransformNormal(XMVectorSet(v.nx,v.ny,v.nz,0),matrix));v.x+=(point.x-v.x)*w;v.y+=(point.y-v.y)*w;v.z+=(point.z-v.z)*w;auto n=XMVector3Normalize(XMVectorSet(v.nx+(normal.x-v.nx)*w,v.ny+(normal.y-v.ny)*w,v.nz+(normal.z-v.nz)*w,0));XMStoreFloat3(&normal,n);v.nx=normal.x;v.ny=normal.y;v.nz=normal.z;}
 for(size_t i=0;i<bones.size();++i)if(upper[i]){auto key=bones[i].key;if(auto it=body.boneFrames.find(key);it!=body.boneFrames.end()){XMFLOAT4X4 before,after;std::memcpy(&before,it->second.data(),sizeof(before));XMStoreFloat4x4(&after,XMLoadFloat4x4(&before)*matrix);std::memcpy(it->second.data(),&after,sizeof(after));body.bonePositions[key]={after._41,after._42,after._43};}}
 return true;
}
void transform(std::span<const ModelVertex>bind,std::span<ModelVertex>out,const std::array<float,16>&f){
 require(bind.size()==out.size());for(float v:f)require(std::isfinite(v));using namespace DirectX;XMFLOAT4X4 values;std::memcpy(&values,f.data(),sizeof(values));auto matrix=XMLoadFloat4x4(&values);
 for(size_t i=0;i<bind.size();++i){auto&v=bind[i];XMFLOAT3 p,n;XMStoreFloat3(&p,XMVector3TransformCoord(XMVectorSet(v.x,v.y,v.z,1),matrix));XMStoreFloat3(&n,XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(v.nx,v.ny,v.nz,0),matrix)));out[i]=v;out[i].x=p.x;out[i].y=p.y;out[i].z=p.z;out[i].nx=n.x;out[i].ny=n.y;out[i].nz=n.z;}
}
}
