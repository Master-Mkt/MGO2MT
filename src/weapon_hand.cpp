#include "weapon_hand.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace mgo2win::weapon_hand {
namespace {
void require(bool v){if(!v)throw std::runtime_error("Invalid GWH1 hand motion");}
struct Reader{std::span<const char>b;size_t at=4;uint32_t u(){require(at+4<=b.size());uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(uint8_t(b[at++]))<<(i*8);return v;}float f(){auto w=u();float v;std::memcpy(&v,&w,4);require(std::isfinite(v)&&std::abs(v)<1000000);return v;}};
template<size_t N>std::array<float,N> vec(Reader&r){std::array<float,N> v;for(auto&x:v)x=r.f();if constexpr(N==4){float n=0;for(auto x:v)n+=x*x;require(n>.99f&&n<1.01f);}return v;}
std::array<float,4> slerp(const std::array<float,4>&a,const std::array<float,4>&b,float t){using namespace DirectX;XMFLOAT4 v;XMStoreFloat4(&v,XMQuaternionSlerp(XMVectorSet(a[0],a[1],a[2],a[3]),XMVectorSet(b[0],b[1],b[2],b[3]),t));return {v.x,v.y,v.z,v.w};}
}
Bank::Bank(std::span<const char>b){require(b.size()>=12&&b.size()<64*1024*1024&&!std::memcmp(b.data(),"GWH1",4));Reader r{b};auto version=r.u();require(version==1||version==2);auto count=r.u();require(count>0&&count<=1024);
 for(unsigned i=0;i<count;++i){Clip c;c.weapon=r.u();c.index=r.u();c.key=r.u();c.frames=r.u();c.fps=r.u();c.rootBone=r.u();c.bone=r.u();auto n=r.u();require(c.weapon>0&&c.weapon<128&&c.index<4096&&c.key&&c.frames>0&&c.frames<=3600&&c.fps==60&&c.rootBone&&c.bone&&n>0&&n<=256);require(uint64_t(c.frames+1)*(40+uint64_t(n)*16)+uint64_t(n)*4<=b.size()-r.at);
  for(unsigned f=0;f<=c.frames;++f){c.roots.push_back(vec<3>(r));c.positions.push_back(vec<3>(r));c.rotations.push_back(vec<4>(r));}
  for(unsigned j=0;j<n;++j){auto key=r.u();require(key&&!c.tracks.contains(key));auto&tr=c.tracks[key];for(unsigned f=0;f<=c.frames;++f)tr.push_back(vec<4>(r));}
  if(version>=2){c.magazineBone=r.u();if(c.magazineBone){require(c.tracks.contains(c.magazineBone));for(unsigned f=0;f<=c.frames;++f){c.magazinePositions.push_back(vec<3>(r));c.magazineRotations.push_back(vec<4>(r));}}}
  require(c.tracks.contains(c.rootBone)&&c.tracks.contains(c.bone));auto key=std::pair{c.weapon,c.index};require(!clips_.contains(key));clips_.emplace(key,std::move(c));
 }require(r.at==b.size());
}
std::optional<Sample> Bank::sample(uint32_t weapon,uint32_t index,double seconds,bool loop)const{
 auto it=clips_.find({weapon,index});if(it==clips_.end()||!std::isfinite(seconds))return {};const auto&c=it->second;double f=std::max(0.,seconds)*c.fps;f=loop?std::fmod(f,double(c.frames)):std::min(f,double(c.frames));unsigned a=unsigned(f),b=std::min(a+1,c.frames);float t=float(f-a);Sample s;s.weapon=weapon;s.index=index;s.key=c.key;s.pose.rootBone=c.rootBone;s.point.bone=c.bone;s.seconds=f/60.;
 if(c.magazineBone){Point p;p.bone=c.magazineBone;for(unsigned j=0;j<3;++j)p.position[j]=c.magazinePositions[a][j]*(1-t)+c.magazinePositions[b][j]*t;p.rotation=slerp(c.magazineRotations[a],c.magazineRotations[b],t);s.magazine=p;}
 for(unsigned j=0;j<3;++j){s.pose.root[j]=j==1?c.roots[a][j]*(1-t)+c.roots[b][j]*t:0;s.point.position[j]=c.positions[a][j]*(1-t)+c.positions[b][j]*t;}s.point.rotation=slerp(c.rotations[a],c.rotations[b],t);
 for(const auto&[key,tr]:c.tracks)s.pose.rotations[key]=slerp(tr[a],tr[b],t);return s;
}
std::optional<Sample> Bank::select(uint32_t weapon,PlayerMotion motion,double seconds,bool aiming)const{
 const bool prone=motion==PlayerMotion::ProneIdle||motion==PlayerMotion::ProneForward||motion==PlayerMotion::ProneBackward;
 const bool supine=motion==PlayerMotion::SupineIdle||motion==PlayerMotion::SupineForward||motion==PlayerMotion::SupineBackward;
 // Runtime action selection is kept explicit; source clip identity is stored
 // separately. Original dynamic state-machine parity is not implied here.
 if(weapon==25)return sample(25,motion==PlayerMotion::Reload?3:(aiming||motion==PlayerMotion::Aim)?(prone?7:supine?5:2):prone?1:0,motion==PlayerMotion::Reload?seconds:0,false);
 if(weapon==3)return sample(3,(aiming||motion==PlayerMotion::Aim)?2:prone?1:0,0,false);
 return {};
}
void upper_body(MotionPose&base,const Sample&s,std::span<const CatalogBone>bones){
 require(base.rootBone==s.pose.rootBone);std::vector<bool> arms(bones.size());
 // 338E60/33BCD0 select the name-matched 333F00 branch. Its hand masks
 // include spine + arm + named fingers (0x1E7/0x1E07), excluding neck/head.
 // Root and locomotion remain owned by the native movement controller.
 for(size_t i=0;i<bones.size();++i){const auto&b=bones[i];require(b.parent<0||size_t(b.parent)<i);arms[i]=b.key==0x019543||b.key==0x619d43||(b.parent>=0&&arms[b.parent]);const bool upper=b.key==0x7011c4||b.key==0x6c02b2||arms[i];if(upper)if(auto it=s.pose.rotations.find(b.key);it!=s.pose.rotations.end())base.rotations[b.key]=it->second;}
}
std::optional<std::array<float,16>> frame(const PreparedCharacter&body,const Point&p){
 using namespace DirectX;auto it=body.boneFrames.find(p.bone);if(it==body.boneFrames.end())return {};float norm=0;for(float v:p.rotation){if(!std::isfinite(v))return {};norm+=v*v;}if(norm<.99f||norm>1.01f)return {};for(float v:p.position)if(!std::isfinite(v))return {};
 XMFLOAT4X4 bone;std::memcpy(&bone,it->second.data(),sizeof(bone));auto&q=p.rotation;auto local=XMMatrixRotationQuaternion(XMVectorSet(q[0],q[1],q[2],q[3]))*XMMatrixTranslation(p.position[0],p.position[1],p.position[2]);XMFLOAT4X4 m;XMStoreFloat4x4(&m,local*XMLoadFloat4x4(&bone));std::array<float,16> out;std::memcpy(out.data(),&m,sizeof(m));return out;
}
void transform(std::span<const ModelVertex>bind,std::span<ModelVertex>out,const std::array<float,16>&f){
 require(bind.size()==out.size());for(float v:f)require(std::isfinite(v));using namespace DirectX;XMFLOAT4X4 values;std::memcpy(&values,f.data(),sizeof(values));auto matrix=XMLoadFloat4x4(&values);
 for(size_t i=0;i<bind.size();++i){auto&v=bind[i];XMFLOAT3 p,n;XMStoreFloat3(&p,XMVector3TransformCoord(XMVectorSet(v.x,v.y,v.z,1),matrix));XMStoreFloat3(&n,XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(v.nx,v.ny,v.nz,0),matrix)));out[i]=v;out[i].x=p.x;out[i].y=p.y;out[i].z=p.z;out[i].nx=n.x;out[i].ny=n.y;out[i].nz=n.z;}
}
}
