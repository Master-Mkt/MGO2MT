#include "character_catalog.h"
#include <DirectXMath.h>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <limits>
namespace mgo2win {
namespace {
void require(bool v){if(!v)throw std::runtime_error("Invalid GWC1 appearance catalog");}
struct Reader {
 std::span<const char>b;size_t at=4;
 uint32_t u(){require(at+4<=b.size());uint32_t x=0;for(unsigned i=0;i<4;++i)x|=uint32_t(uint8_t(b[at++]))<<(i*8);return x;}
 uint16_t h(){require(at+2<=b.size());auto x=uint16_t(uint8_t(b[at]))|(uint16_t(uint8_t(b[at+1]))<<8);at+=2;return uint16_t(x);}
 float f(){uint32_t x=u();float v;std::memcpy(&v,&x,4);require(std::isfinite(v)&&std::abs(v)<=1000000);return v;}
};
}
CharacterCatalog::CharacterCatalog(std::span<const char>b){
 require(b.size()>=32&&b.size()<=256*1024*1024&&!std::memcmp(b.data(),"GWC1",4));Reader r{b};auto version=r.u();require(version==2||version==3);
 auto nm=r.u(),nt=r.u(),nr=r.u();frames_=r.u();fps_=r.u();clip_=r.u();
 require(nm&&nm<=512&&nt&&nt<=8192&&nr&&nr<=20000&&frames_&&frames_<=3600&&fps_==60);
 for(auto&bones:bones_){auto n=r.u();require(n&&n<=256);require(uint64_t(n)*(20+uint64_t(frames_+1)*16)<=b.size()-r.at);std::map<uint32_t,bool>keys;
  for(uint32_t i=0;i<n;++i){CatalogBone bone{};bone.key=r.u();bone.parent=int32_t(r.u());require(bone.key&&!keys.contains(bone.key)&&bone.parent>=-1&&bone.parent<int32_t(i));keys[bone.key]=true;for(auto&v:bone.position)v=r.f();
   for(unsigned f=0;f<=frames_;++f){std::array<float,4>q;float sum=0;for(auto&x:q){x=r.f();sum+=x*x;}require(sum>.99f&&sum<1.01f);bone.rotation.push_back(q);}bones.push_back(std::move(bone));}}
 for(unsigned f=0;f<=frames_;++f){std::array<float,3>v;for(auto&x:v)x=r.f();root_.push_back(v);}
 uint64_t totalVertices=0,totalIndices=0;
 for(unsigned i=0;i<nm;++i){CatalogMesh mesh{};mesh.gender=r.u();mesh.key=r.u();auto nv=r.u(),ni=r.u(),np=r.u();totalVertices+=nv;totalIndices+=ni;
  require(mesh.gender<=1&&nv&&nv<=100000&&ni&&ni<=300000&&ni%3==0&&np&&np<=4096&&totalVertices<=2000000&&totalIndices<=6000000);require(uint64_t(nv)*112+uint64_t(ni)*4+uint64_t(np)*(version==3?20:16)<=b.size()-r.at);
  for(unsigned v=0;v<nv;++v){ModelVertex vert{r.f(),r.f(),r.f(),r.f(),r.f(),r.f(),r.f(),r.f(),r.f(),r.f()};SkinBinding skin;float sum=0;
   for(auto&j:skin.bones){j=r.h();require(j<bones_[mesh.gender].size());}for(auto&w:skin.weights){w=r.f();require(w>=0&&w<=1);sum+=w;}require(std::abs(sum-1)<.0001f);for(auto&off:skin.offsets)for(auto&x:off)x=r.f();float length=vert.nx*vert.nx+vert.ny*vert.ny+vert.nz*vert.nz;require(length>.9f&&length<1.1f);mesh.vertices.push_back(vert);mesh.skin.push_back(skin);}
  for(unsigned n=0;n<ni;++n){auto j=r.u();require(j<nv);mesh.indices.push_back(j);}
  unsigned end=0;for(unsigned p=0;p<np;++p){ModelPart part{r.u(),r.u(),r.u(),r.u()};if(version==3)part.materialShader=r.u();require(part.first==end&&part.count&&part.count%3==0&&part.count<=ni-end&&part.texture);end+=part.count;mesh.parts.push_back(part);}require(end==ni);meshes_.push_back(std::move(mesh));}
 std::map<std::array<uint32_t,3>,bool>ruleKeys;
 for(unsigned i=0;i<nr;++i){AppearanceRule rule{r.u(),r.u(),r.u(),r.u(),r.u(),r.u(),{}};require(rule.gender<=1&&rule.id<=255&&rule.color<=255&&rule.model<meshes_.size()&&meshes_[rule.model].gender==rule.gender&&rule.flags<=3);auto key=std::array<uint32_t,3>{rule.gender,rule.id,rule.color};require(!ruleKeys.contains(key));ruleKeys[key]=true;for(auto&x:rule.replacements)x=r.u();if(version==3){rule.materialMode=r.u();require(rule.materialMode<=3);for(auto&x:rule.tint){x=r.f();require(x>=0&&x<=4);}}rules_.push_back(rule);}
 for(unsigned i=0;i<nt;++i){auto key=r.u();ModelTexture t{r.u(),r.u(),r.u(),{}};auto n=r.u();require(key&&!textures_.contains(key)&&t.width&&t.height&&t.width<=8192&&t.height<=8192&&(t.codec==9||t.codec==11));require(n==uint64_t((t.width+3)/4)*((t.height+3)/4)*(t.codec==9?8:16)&&n<=b.size()-r.at);t.pixels.assign(reinterpret_cast<const uint8_t*>(b.data()+r.at),reinterpret_cast<const uint8_t*>(b.data()+r.at+n));r.at+=n;textures_[key]=std::move(t);}
 require(r.at==b.size());
}
std::vector<AppearanceRule> CharacterCatalog::creation_choices(unsigned gender,unsigned kind)const{
 std::vector<AppearanceRule> out;
 for(const auto&r:rules_)if(r.gender==gender&&r.kind==kind&&!r.flags){
  auto resolved=[&](uint32_t key){for(unsigned k=0;k<6;k+=2)if(r.replacements[k]==key&&r.replacements[k+1])return r.replacements[k+1];return key;};
  bool valid=true;for(const auto&p:meshes_[r.model].parts)if(!textures_.contains(resolved(p.texture))||(p.flags&&!textures_.contains(resolved(p.flags))))valid=false;
  if(valid)out.push_back(r);
 }
 std::sort(out.begin(),out.end(),[](const auto&a,const auto&b){return a.id!=b.id?a.id<b.id:a.color<b.color;});return out;
}
PreparedCharacter CharacterCatalog::assemble(const std::array<uint8_t,28>&a)const{
 PreparedCharacter out;out.gender=a[0];if(out.gender>1){out.missingModels=10;return out;}
 const unsigned idFields[]={1,2,3,15,17,13,14,16,18,19},colorFields[]={4,5,6,22,24,20,21,23,25,26},kinds[]={100,200,300,400,500,600,700,800,0,0};
 std::map<uint32_t,unsigned>imageIds;
 auto texture=[&](uint32_t key)->int{auto found=textures_.find(key);if(found==textures_.end())return -1;if(!imageIds.contains(key)){imageIds[key]=unsigned(out.model.textures.size());out.model.textures.push_back(found->second);}return int(imageIds[key]);};
 for(unsigned slot=0;slot<10;++slot){auto id=a[idFields[slot]],color=a[colorFields[slot]];if(slot==3&&id==46)color=uint8_t(a[1]/2);
  auto issue=[&](const char* reason,unsigned key=0){if(std::none_of(out.issues.begin(),out.issues.end(),[&](const auto&i){return i.slot==slot&&i.texture==key&&std::strcmp(i.reason,reason)==0;}))out.issues.push_back({slot,id,color,key,reason});};
  // Native display fallback for the candidate server's zero lower ID, not a
  // rewrite of received/sent appearance. Original equipment validation uses 22
  // (A87AB0); identical account-preview behavior is not yet established.
  // Evidence/ELF hash: notes/CHARACTER_LOWER_PREVIEW_20260910.md.
  if(slot==2&&id==0){id=22;out.defaultedLower=true;}
  // Original unequipped entries are absent from the model table.
  if(slot>=5&&(id==0||id==28||id==68||id==86||id==102))continue;
  const AppearanceRule*chosen=nullptr;
  for(const auto&r:rules_)if(r.gender==out.gender&&r.id==id&&(kinds[slot]?r.kind==kinds[slot]:(r.kind==550||r.kind==900))){if(!chosen||r.color==color)chosen=&r;if(r.color==color)break;}
  if(!chosen){++out.missingModels;issue("unknown_model");continue;}if(chosen->color!=color||chosen->flags)++out.missingColors;
  if(chosen->color!=color)issue("unknown_palette");if(chosen->flags&1)issue("material_parameters_pending");
  const auto&m=meshes_[chosen->model];auto base=unsigned(out.model.vertices.size());bool used=false;
  auto imageKey=[&](uint32_t key){for(unsigned k=0;k<6;k+=2)if(chosen->replacements[k]==key&&chosen->replacements[k+1]){auto dest=chosen->replacements[k+1];if(textures_.contains(dest))return dest;++out.missingColors;issue("missing_replacement_texture",dest);}return key;};
  for(const auto&p:m.parts){int diffuse=texture(imageKey(p.texture));if(diffuse<0){++out.missingColors;issue("missing_diffuse_texture",p.texture);continue;}int pattern=p.flags?texture(imageKey(p.flags)):-1;
   if(p.flags&&pattern<0){++out.missingColors;issue("missing_pattern_texture",p.flags);}
   ModelPart part{unsigned(out.model.indices.size()),p.count,unsigned(diffuse),pattern<0?0u:unsigned(pattern)+1};part.materialShader=p.materialShader;if(chosen->materialMode==2&&p.materialShader==0x10)part.tint=chosen->tint;out.model.parts.push_back(part);for(unsigned i=0;i<p.count;++i)out.model.indices.push_back(base+m.indices[p.first+i]);used=true;}
  if(used){out.model.vertices.insert(out.model.vertices.end(),m.vertices.begin(),m.vertices.end());out.skin.insert(out.skin.end(),m.skin.begin(),m.skin.end());++out.selectedParts;}
 }
 out.bind=out.model.vertices;if(out.ready()){pose(out,0);auto&b=out.model.bounds;b={1e9f,1e9f,1e9f,-1e9f,-1e9f,-1e9f};for(const auto&v:out.model.vertices){float xyz[]={v.x,v.y,v.z};for(int i=0;i<3;++i){b[i]=std::min(b[i],xyz[i]);b[i+3]=std::max(b[i+3],xyz[i]);}}}return out;
}
void CharacterCatalog::pose(PreparedCharacter&out,double seconds)const{
 using namespace DirectX;require(out.gender<2&&out.bind.size()==out.skin.size());if(!std::isfinite(seconds))seconds=0;
 double time=std::fmod(std::max(0.,seconds),double(frames_)/fps_)*fps_;unsigned frame=std::min(unsigned(time),frames_-1);float t=float(time-frame);MotionPose sampled;sampled.rootBone=bones_[out.gender].front().key;
 for(const auto&b:bones_[out.gender]){auto&a=b.rotation[frame];auto&z=b.rotation[frame+1];auto q=XMQuaternionSlerp(XMVectorSet(a[0],a[1],a[2],a[3]),XMVectorSet(z[0],z[1],z[2],z[3]),t);XMFLOAT4 value;XMStoreFloat4(&value,q);sampled.rotations[b.key]={value.x,value.y,value.z,value.w};}
 for(unsigned j=0;j<3;++j)sampled.root[j]=root_[frame][j]*(1-t)+root_[frame+1][j]*t;
 pose(out,sampled);
}
void CharacterCatalog::pose(PreparedCharacter&out,const MotionPose&sampled)const{
 using namespace DirectX;require(out.gender<2&&out.bind.size()==out.skin.size()&&out.model.vertices.size()==out.bind.size());
 for(float v:sampled.root)require(std::isfinite(v)&&std::abs(v)<=1000000);
 require(sampled.rootBone==bones_[out.gender].front().key);
 const auto&bones=bones_[out.gender];std::vector<XMMATRIX>world(bones.size()),skin(bones.size());
 for(size_t i=0;i<bones.size();++i){const auto&b=bones[i];auto found=sampled.rotations.find(b.key);std::array<float,4>a=found==sampled.rotations.end()?std::array<float,4>{0,0,0,1}:found->second;float norm=0;for(float v:a){require(std::isfinite(v));norm+=v*v;}require(norm>.99f&&norm<1.01f);auto q=XMVectorSet(a[0],a[1],a[2],a[3]);float pos[3];for(int j=0;j<3;++j)pos[j]=b.position[j]-(b.parent<0?0:bones[b.parent].position[j]);
  if(b.parent<0)for(int j=0;j<3;++j)pos[j]+=sampled.root[j];
  world[i]=XMMatrixRotationQuaternion(q)*XMMatrixTranslation(pos[0],pos[1],pos[2]);if(b.parent>=0)world[i]=world[i]*world[b.parent];skin[i]=XMMatrixTranslation(-b.position[0],-b.position[1],-b.position[2])*world[i];}
 std::map<uint32_t,std::array<float,3>> bonePositions;
 for(size_t i=0;i<bones.size();++i){XMFLOAT3 xyz;XMStoreFloat3(&xyz,XMVector3TransformCoord(XMVectorZero(),world[i]));bonePositions.emplace(bones[i].key,std::array<float,3>{xyz.x,xyz.y,xyz.z});}
 for(size_t i=0;i<out.bind.size();++i){const auto&v=out.bind[i];auto p=XMVectorSet(v.x,v.y,v.z,1),n=XMVectorSet(v.nx,v.ny,v.nz,0),result=XMVectorZero(),normal=XMVectorZero();
  for(int j=0;j<4;++j){float weight=out.skin[i].weights[j];if(weight){auto bone=out.skin[i].bones[j];const auto&off=out.skin[i].offsets[j];auto bound=XMVectorAdd(p,XMVectorSet(off[0],off[1],off[2],0));result=XMVectorMultiplyAdd(XMVectorReplicate(weight),XMVector3TransformCoord(bound,skin[bone]),result);normal=XMVectorMultiplyAdd(XMVectorReplicate(weight),XMVector3TransformNormal(n,skin[bone]),normal);}}
  XMFLOAT3 xyz,no;XMStoreFloat3(&xyz,result);XMStoreFloat3(&no,XMVector3Normalize(normal));auto&dest=out.model.vertices[i];dest=v;dest.x=xyz.x;dest.y=xyz.y;dest.z=xyz.z;dest.nx=no.x;dest.ny=no.y;dest.nz=no.z;
 }
 out.bonePositions=std::move(bonePositions);
}
}
