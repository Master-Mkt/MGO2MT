#include "mounted_renderer.h"
#include "gameplay_config.h"
#include <cstring>
#include <fstream>
#include <cmath>
#include <stdexcept>
#include <DirectXMath.h>
namespace mgo2mt::mounted {
namespace {
void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
Vec3 sub(Vec3 a,Vec3 b){for(unsigned j=0;j<3;++j)a[j]-=b[j];return a;}
Vec3 add(Vec3 a,Vec3 b){for(unsigned j=0;j<3;++j)a[j]+=b[j];return a;}
std::vector<char> read(const std::filesystem::path&root,const std::string&name,size_t maximum){
 const auto base=std::filesystem::weakly_canonical(root),path=std::filesystem::weakly_canonical(root/std::filesystem::u8path(name));
 require(gameplay::relative_resource_path(path.lexically_relative(base).generic_string()),"Mounted resource escapes data root");
 std::ifstream file(path,std::ios::binary|std::ios::ate);require(bool(file)&&file.tellg()>0&&uint64_t(file.tellg())<=maximum,"Mounted resource missing or too large");
 std::vector<char> bytes(size_t(file.tellg()));file.seekg(0);require(bool(file.read(bytes.data(),std::streamsize(bytes.size()))),"Mounted resource read failed");return bytes;
}
}
Rig Rig::read(std::span<const char> bytes,size_t count){
 require(bytes.size()>=36&&!std::memcmp(bytes.data(),"GMR1",4),"Mounted rig header");size_t at=4;
 auto word=[&](){require(at+4<=bytes.size(),"Mounted rig truncated");uint32_t v=0;for(unsigned j=0;j<4;++j)v|=uint32_t(uint8_t(bytes[at++]))<<(j*8);return v;};
 auto scalar=[&](){auto v=word();float f;std::memcpy(&f,&v,4);require(std::isfinite(f)&&std::abs(f)<10000,"Mounted rig coordinate");return f;};
 require(word()==1&&word()==count,"Mounted rig version or vertex count");const auto bones=word();
 require(bones>=1&&bones<=64&&bytes.size()==16+20*bones+count,"Mounted rig extent");Rig result;
 for(unsigned i=0;i<bones;++i){const auto key=word();require(key&&std::find(result.keys.begin(),result.keys.end(),key)==result.keys.end(),"Mounted rig bone identity");const auto parent=int32_t(word());require(parent>=-1&&parent<int32_t(i)&&(i||parent==-1),"Mounted rig parent hierarchy");Vec3 pivot{};for(auto&v:pivot)v=scalar();result.keys.push_back(key);result.parents.push_back(parent);result.bonePivots.push_back(pivot);if(i<3)result.pivots[i]=pivot;}
 for(size_t i=0;i<count;++i){const auto b=uint8_t(bytes[at++]);require(b<bones,"Mounted rig vertex bone");result.vertices.push_back(b);}return result;
}
void articulate(const CharacterModel&model,const Rig&rig,const Type&type,float yaw,float pitch,std::span<ModelVertex> out){
 require(out.size()==model.vertices.size()&&rig.vertices.size()==out.size()&&std::isfinite(yaw)&&std::isfinite(pitch),"Mounted articulation extent");
 for(size_t i=0;i<out.size();++i){auto v=model.vertices[i];const auto bone=rig.vertices[i];if(bone&&type.kind!=Kind::catapult){const bool elevated=type.kind==Kind::mortar||bone>=2;const auto pivot=elevated?type.pivot:rig.pivots[1];const auto elevation=elevated?pitch-type.bindPitch:0;auto p=add(pivot,rotate(sub({v.x,v.y,v.z},pivot),yaw,elevation));auto n=rotate({v.nx,v.ny,v.nz},yaw,elevation);v.x=p[0];v.y=p[1];v.z=p[2];v.nx=n[0];v.ny=n[1];v.nz=n[2];}out[i]=v;}
}
bool operator_pitch(PreparedCharacter&body,std::span<const CatalogBone>bones,const Type&type,float pitch){
 if(!std::isfinite(pitch)||body.skin.size()!=body.model.vertices.size())return false;
 if(type.kind==Kind::catapult)return true;
 if(type.kind==Kind::mortar)pitch-=type.initialPitch;
 using namespace DirectX;const auto p=sub(type.pivot,type.operatorOffset);
 const auto matrix=XMMatrixTranslation(-p[0],-p[1],-p[2])*XMMatrixRotationX(-pitch)*XMMatrixTranslation(p[0],p[1],p[2]);
 std::vector<bool> upper(bones.size());for(size_t i=0;i<bones.size();++i){const auto&b=bones[i];upper[i]=b.key==0x6c02b2||b.key==0x019543||b.key==0x619d43||(b.parent>=0&&size_t(b.parent)<i&&upper[b.parent]);}
 for(size_t i=0;i<body.model.vertices.size();++i){float w=0;for(unsigned j=0;j<4;++j){auto index=body.skin[i].bones[j];if(index<upper.size()&&upper[index])w+=body.skin[i].weights[j];}w=std::clamp(w,0.f,1.f);if(w==0)continue;auto&v=body.model.vertices[i];XMFLOAT3 point,normal;XMStoreFloat3(&point,XMVector3TransformCoord(XMVectorSet(v.x,v.y,v.z,1),matrix));XMStoreFloat3(&normal,XMVector3TransformNormal(XMVectorSet(v.nx,v.ny,v.nz,0),matrix));v.x+=(point.x-v.x)*w;v.y+=(point.y-v.y)*w;v.z+=(point.z-v.z)*w;XMStoreFloat3(&normal,XMVector3Normalize(XMVectorSet(v.nx+(normal.x-v.nx)*w,v.ny+(normal.y-v.ny)*w,v.nz+(normal.z-v.nz)*w,0)));v.nx=normal.x;v.ny=normal.y;v.nz=normal.z;}
 for(size_t i=0;i<bones.size();++i)if(upper[i])if(auto it=body.boneFrames.find(bones[i].key);it!=body.boneFrames.end()){XMFLOAT4X4 before,after;std::memcpy(&before,it->second.data(),sizeof(before));XMStoreFloat4x4(&after,XMLoadFloat4x4(&before)*matrix);std::memcpy(it->second.data(),&after,sizeof(after));body.bonePositions[bones[i].key]={after._41,after._42,after._43};}
 return true;
}
Renderer::Renderer(ID3D11Device*device,const std::filesystem::path&root,const Registry&registry):registry_(registry),device_(device){
 require(device!=nullptr,"Mounted renderer device");
 for(const auto&t:registry.types){require(valid(t),"Mounted renderer type");Asset asset;asset.model=CharacterModel(read(root,t.model,128*1024*1024));
  if(t.rig.empty()){asset.rig.vertices.assign(asset.model.vertices.size(),2);asset.rig.pivots[1]=asset.rig.pivots[2]=t.pivot;}
  else asset.rig=Rig::read(read(root,t.rig,16*1024*1024),asset.model.vertices.size());
  for(unsigned gender=0;gender<2;++gender){const auto&motion=gender?t.femaleMotion:t.maleMotion;if(!motion.empty()){asset.motions[gender]=std::make_unique<PlayerMotionBank>(read(root,motion,32*1024*1024));require(asset.motions[gender]->has(PlayerMotion::Aim),"Mounted hold motion missing");}}
  assets_.emplace(t.id,std::move(asset));
 }
}
void Renderer::update(ID3D11DeviceContext*context,uint8_t map,const combat::Snapshot*snapshot){
 if(map_!=map){draws_.clear();map_=map;for(const auto&i:registry_.placements)if(i.map==map){const auto&a=assets_.at(i.type);Draw draw;draw.instance=i;draw.vertices=a.model.vertices;draw.renderer=std::make_unique<CharacterRenderer>(device_,a.model);draws_.emplace(i.id,std::move(draw));}}
 for(auto&[id,draw]:draws_){float yaw=draw.instance.yaw,pitch=registry_.find(draw.instance.type)->initialPitch;if(snapshot)for(const auto&p:snapshot->players)if(p&&p->alive&&!p->stunned&&p->mountedId==id){yaw=p->pose.yaw;pitch=p->pose.pitch;break;}
  const auto&t=*registry_.find(draw.instance.type);clamp_aim(draw.instance,t,yaw,pitch);draw.yaw=yaw;draw.pitch=pitch;const auto&a=assets_.at(t.id);articulate(a.model,a.rig,t,std::remainder(yaw-draw.instance.yaw,6.283185307f),pitch,draw.vertices);draw.renderer->update_vertices(context,draw.vertices);
 }
}
void Renderer::shadow_casters(std::vector<shadows::Caster>&out)const{for(const auto&[id,draw]:draws_)out.push_back({draw.renderer.get(),draw.instance.yaw,draw.instance.origin});}
std::optional<MotionPose> Renderer::action_pose(uint8_t map,uint16_t id,uint32_t gender,PlayerMotion motion,double seconds)const{
 const auto*i=registry_.find(map,id);if(!i||gender>1||!std::isfinite(seconds))return {};const auto&a=assets_.at(i->type);if(!a.motions[gender])return {};
 const bool catapult=registry_.find(i->type)->kind==Kind::catapult;
 // Original launch/flight clips contain platform trajectory in root Y. HOST
 // owns world movement; use their rotations with a native standing root.
 if(catapult&&motion==PlayerMotion::Roll)if(auto clip=a.motions[gender]->find(motion))seconds+=std::max(0.,double(clip->frames)/clip->fps-.65);
 auto pose=a.motions[gender]->sample(motion,std::max(0.,seconds));
 if(pose&&catapult&&motion==PlayerMotion::Run)pose->root[1]=1081;
 if(pose&&catapult&&motion==PlayerMotion::Roll)pose->root[1]=std::clamp(pose->root[1],500.f,1081.f);
 return pose;
}
std::optional<MotionPose> Renderer::pose(uint8_t map,uint16_t id,uint32_t gender,double fireSeconds)const{const auto*i=registry_.find(map,id);if(!i)return {};const auto&t=*registry_.find(i->type);auto motion=fireSeconds>=0&&fireSeconds<.2?PlayerMotion::Aim:PlayerMotion::Idle;double seconds=fireSeconds>=0?fireSeconds:0;if(t.kind==Kind::mortar&&fireSeconds>=.16&&fireSeconds<t.cooldownMs/1000.){motion=PlayerMotion::Reload;seconds=fireSeconds-.16;}return action_pose(map,id,gender,motion,seconds);}
}
