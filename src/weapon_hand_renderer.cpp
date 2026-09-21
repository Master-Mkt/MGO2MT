#include "weapon_hand_renderer.h"
#include "combat_presentation.h"
#include "weapon_connection_points.h"
#include "gameplay_config.h"
#include <DirectXMath.h>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <stdexcept>
namespace mgo2mt::weapon_hand {
namespace {
void load_connection_resource(connections::Table&table,const std::filesystem::path&root){
 const auto candidate=root/"weapon/connection_points.gwcp";if(!std::filesystem::exists(candidate))return;
 const auto base=std::filesystem::weakly_canonical(root),file=std::filesystem::weakly_canonical(candidate);
 if(!gameplay::relative_resource_path(file.lexically_relative(base).generic_string()))throw std::runtime_error("CNP resource escapes data directory");
 std::string error;if(!table.load(file,error))throw std::runtime_error(error);
}
bool connection_model(const CharacterModel&model,const connections::Point&point){return !model.parts.empty()&&std::all_of(model.parts.begin(),model.parts.end(),[&](const auto&p){return p.original.present&&p.original.mdnSha256==point.mdnSha256;});}
}
Models::Models(const std::filesystem::path& root,const gameplay::Config& config){
 load_connection_resource(connections_,root);
 const auto base=std::filesystem::weakly_canonical(root);
 auto load=[&](const std::string& name){
  const auto path=std::filesystem::weakly_canonical(root/std::filesystem::u8path(name));
  const auto relative=path.lexically_relative(base);if(!gameplay::relative_resource_path(relative.generic_string()))throw std::runtime_error("Weapon resource escapes data directory");
  std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in||in.tellg()<=0||in.tellg()>128*1024*1024)throw std::runtime_error("Weapon JSON model missing or too large: "+name);
  std::vector<char> bytes(size_t(in.tellg()));in.seekg(0);if(!in.read(bytes.data(),std::streamsize(bytes.size())))throw std::runtime_error("Weapon JSON model read failed");return CharacterModel(bytes);
 };
 for(const auto& definition:config.definitions()){
  const auto&v=definition.visual;if(v.modelPath.empty())continue;
  models_.emplace(v.id,load(v.modelPath));if(!v.secondaryModelPath.empty())magazines_.emplace(v.id,load(v.secondaryModelPath));
  ModelBinding binding;binding.flags=v.flags;binding.muzzle=v.muzzle;binding.magazine.position=v.magPosition;binding.magazine.rotation=v.magRotation;bindings_.emplace(v.id,binding);sources_.emplace(v.id,v.motionId?v.motionId:config.source_id(v.id));
 }
}
Models::Models(const std::filesystem::path&dir){
 load_connection_resource(connections_,dir.parent_path());
 std::ifstream index(dir/"models.gwi",std::ios::binary);
 if(index){
  std::vector<char> bytes{std::istreambuf_iterator<char>(index),{}};size_t at=4;
  auto word=[&](){if(at+4>bytes.size())throw std::runtime_error("Truncated weapon model index");uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(uint8_t(bytes[at++]))<<(i*8);return v;};
  auto scalar=[&](){auto w=word();float v;std::memcpy(&v,&w,4);if(!std::isfinite(v)||std::abs(v)>100000)throw std::runtime_error("Invalid weapon connection value");return v;};
  if(bytes.size()<12||std::memcmp(bytes.data(),"GWI1",4)||word()!=1)throw std::runtime_error("Invalid weapon model index");auto count=word();if(!count||count>127||bytes.size()!=12+size_t(count)*48)throw std::runtime_error("Invalid weapon model index extent");
  for(unsigned i=0;i<count;++i){auto id=word();ModelBinding binding;binding.flags=word();if(!id||id>=128||bindings_.contains(id)||(binding.flags&~7u))throw std::runtime_error("Invalid weapon model identity");for(auto&v:binding.muzzle)v=scalar();for(auto&v:binding.magazine.position)v=scalar();float norm=0;for(auto&v:binding.magazine.rotation){v=scalar();norm+=v*v;}if(norm<.99f||norm>1.01f)throw std::runtime_error("Invalid magazine quaternion");bindings_.emplace(id,binding);
   for(bool secondary:{false,true}){if(secondary&&!(binding.flags&2))continue;std::ostringstream name;name<<"id_"<<std::setfill('0')<<std::setw(3)<<id<<(secondary?"_secondary.gwm":".gwm");std::ifstream in(dir/name.str(),std::ios::binary);if(!in)throw std::runtime_error("Missing indexed weapon model");std::vector<char> model{std::istreambuf_iterator<char>(in),{}};(secondary?magazines_:models_).emplace(id,CharacterModel(model));}
  }return;
 }
 for(auto[id,name]:{std::pair{25u,"ak102"},std::pair{3u,"operator"}}){ModelBinding binding;binding.flags=3;
  // Authored neutral fallback only. Original bindings require the local data.
  binding.muzzle={0,0,250};binding.magazine.position={0,-40,0};
  for(bool secondary:{false,true}){std::ifstream in(dir/(std::string(name)+(secondary?"_secondary.gwm":".gwm")),std::ios::binary);if(!in)continue;std::vector<char> bytes{std::istreambuf_iterator<char>(in),{}};(secondary?magazines_:models_).emplace(id,CharacterModel(bytes));}
  if(auto model=find(id)){if(auto p=connections_.find(id,0xa43256);p&&connection_model(*model,*p))binding.muzzle=p->axis.rear;if(auto p=connections_.find(id,0x442f3d);p&&connection_model(*model,*p))binding.magazine.position=p->axis.rear;}
  bindings_.emplace(id,binding);
 }
}
const CharacterModel*Models::find(uint32_t w)const{auto it=models_.find(w);return it==models_.end()?nullptr:&it->second;}
const CharacterModel*Models::magazine(uint32_t w)const{auto it=magazines_.find(w);return it==magazines_.end()?nullptr:&it->second;}
const ModelBinding*Models::binding(uint32_t w)const{auto it=bindings_.find(w);return it==bindings_.end()?nullptr:&it->second;}
std::vector<uint32_t> Models::weapons()const{std::vector<uint32_t> ids;for(const auto&[id,model]:models_)ids.push_back(id);return ids;}
static CharacterModel first_person_subset(const PreparedCharacter&body,std::span<const CatalogBone>bones,bool arms){
 CharacterModel out=body.model;out.indices.clear();out.parts.clear();std::vector<bool> arm(bones.size());
 // Original male/female MDN chains: clavicle 019543/619D43 -> upper arm
 // 027A4C/62824C -> elbow/forearm FAFA8B/5B028C -> wrist FBFA42/5C0243.
 // First person starts at the forearm. Including clavicles/upper arms puts
 // their open shoulder ends around the eye and exposes huge inside faces.
 // Only indices are filtered: original vertices, skinning and UVs stay intact.
 for(size_t i=0;i<bones.size();++i){const auto&b=bones[i];arm[i]=b.key==0xfafa8b||b.key==0x5b028c||(b.parent>=0&&size_t(b.parent)<i&&arm[b.parent]);}
 auto included=[&](uint32_t vertex){if(vertex>=body.skin.size())return false;const auto&s=body.skin[vertex];float weight=0;for(unsigned i=0;i<4;++i)if(s.bones[i]<arm.size()&&arm[s.bones[i]])weight+=s.weights[i];return weight>=.5f;};
 for(auto part:body.model.parts){const auto end=size_t(part.first)+part.count;if(end>body.model.indices.size())throw std::runtime_error("Invalid character arm extent");const auto first=out.indices.size();for(size_t i=part.first;i+2<end;i+=3){auto a=body.model.indices[i],b=body.model.indices[i+1],c=body.model.indices[i+2];if((included(a)&&included(b)&&included(c))==arms){out.indices.push_back(a);out.indices.push_back(b);out.indices.push_back(c);}}part.first=uint32_t(first);part.count=uint32_t(out.indices.size()-first);if(part.count)out.parts.push_back(std::move(part));}
 return out;
}
CharacterModel first_person_arms(const PreparedCharacter&b,std::span<const CatalogBone>s){return first_person_subset(b,s,true);}
CharacterModel first_person_body(const PreparedCharacter&b,std::span<const CatalogBone>s){return first_person_subset(b,s,false);}
bool Actor::update(ID3D11Device*d,ID3D11DeviceContext*c,const Models&models,const PreparedCharacter&body,const Sample&s,double dt,float rate){
 visible_=false;const auto*source=models.find(s.weapon);if(!source){clear();return false;}
 if(weapon_!=s.weapon||!renderer_){clear();model_=*source;bind_=model_.vertices;renderer_=std::make_unique<CharacterRenderer>(d,model_);weapon_=s.weapon;sight_=first_person_sight::local_axis(models.source(s.weapon),*source,models.connection_points());if(auto*p=models.binding(s.weapon))binding_=*p;else binding_={};shown_=from_=target_=s.point;clip_=s.index;
  for(const auto& point:models.connection_points().rows())if(point.key!=connections::sight_line&&point.weapon==models.source(s.weapon)&&connection_model(*source,point))connections_.emplace_back(point.key,point.axis);
 }
 if(!magazineRenderer_)if(auto*m=models.magazine(s.weapon)){magazine_=*m;magazineBind_=m->vertices;magazineRenderer_=std::make_unique<CharacterRenderer>(d,magazine_);}
 if(clip_!=s.index||target_.bone!=s.point.bone){from_=shown_;target_=s.point;clip_=s.index;blend_=0;}
 target_=s.point;blend_=std::min(1.f,blend_+float(std::clamp(dt,0.,.1))*std::clamp(rate,.1f,200.f));shown_=target_;
 if(blend_<1&&from_.bone==target_.bone){for(unsigned j=0;j<3;++j)shown_.position[j]=from_.position[j]*(1-blend_)+target_.position[j]*blend_;using namespace DirectX;auto&a=from_.rotation;auto&b=target_.rotation;XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionSlerp(XMVectorSet(a[0],a[1],a[2],a[3]),XMVectorSet(b[0],b[1],b[2],b[3]),blend_));shown_.rotation={q.x,q.y,q.z,q.w};}
 auto f=frame(body,shown_);if(!f)return false;frame_=*f;transform(bind_,model_.vertices,*f);renderer_->update_vertices(c,model_.vertices);
 // D3CB10 CNP_amp_def -> D3E6E8 secondary model. Both reviewed roots have
 // identity bind transforms. Reload events switch the secondary mesh to the
 // original left-hand MTP point and suppress it during the magazine swap.
 // Non-AK magazine ownership callbacks are not decoded. The explicit native
 // policy follows the authored left-hand MTP whenever that track is exported.
 auto mode=models.source(s.weapon)==25&&s.index>=3&&s.index<=5?combat::presentation::ak_magazine(s.seconds):s.magazine?combat::presentation::Magazine::left:combat::presentation::Magazine::mounted;magazineVisible_=mode!=combat::presentation::Magazine::hidden;
 if(magazineRenderer_){using namespace DirectX;XMFLOAT4X4 parent;std::memcpy(&parent,f->data(),sizeof(parent));const auto&p=binding_.magazine;auto local=XMMatrixRotationQuaternion(XMVectorSet(p.rotation[0],p.rotation[1],p.rotation[2],p.rotation[3]))*XMMatrixTranslation(p.position[0],p.position[1],p.position[2]);XMFLOAT4X4 combined;XMStoreFloat4x4(&combined,local*XMLoadFloat4x4(&parent));std::array<float,16> matrix;std::memcpy(matrix.data(),&combined,sizeof(combined));if(mode==combat::presentation::Magazine::left){auto left=s.magazine?frame(body,*s.magazine):std::nullopt;if(left)matrix=*left;else magazineVisible_=false;}
 transform(magazineBind_,magazine_.vertices,matrix);magazineRenderer_->update_vertices(c,magazine_.vertices);}
 visible_=true;return true;
}
std::optional<std::array<float,3>> Actor::muzzle(float yaw,const std::array<float,3>&origin)const{
 if(!visible_||!(binding_.flags&1))return {};using namespace DirectX;XMFLOAT4X4 values;std::memcpy(&values,frame_.data(),sizeof(values));
 auto point=XMVectorSet(binding_.muzzle[0],binding_.muzzle[1],binding_.muzzle[2],1);
 XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(point,XMLoadFloat4x4(&values)*XMMatrixRotationY(yaw)*XMMatrixTranslation(origin[0],origin[1],origin[2])));return std::array<float,3>{p.x,p.y,p.z};
}
std::optional<first_person_sight::Axis> Actor::sight_axis(float yaw,const std::array<float,3>&origin)const{
 if(!visible_||!sight_)return {};return first_person_sight::world_axis(*sight_,frame_,yaw,origin);
}
std::optional<first_person_sight::Axis> Actor::connection(uint32_t key,float yaw,const std::array<float,3>&origin)const{
 if(!visible_)return {};for(const auto& [id,axis]:connections_)if(id==key)return first_person_sight::world_axis(axis,frame_,yaw,origin);return {};
}
std::vector<std::pair<uint32_t,first_person_sight::Axis>> Actor::connection_frames(float yaw,const std::array<float,3>&origin)const{
 std::vector<std::pair<uint32_t,first_person_sight::Axis>> result;if(!visible_)return result;for(const auto& [id,axis]:connections_)if(auto world=first_person_sight::world_axis(axis,frame_,yaw,origin))result.emplace_back(id,*world);return result;
}
void Actor::draw(ID3D11DeviceContext*c,float yaw,const WorldView&camera,CharacterRenderer*surface,const std::array<float,3>&origin,std::span<const DynamicPointLight>lights,const shadows::Renderer* shadow,const EnvironmentLight* environment)const{if(visible_&&renderer_){renderer_->render(c,yaw,false,&camera,surface,&origin,lights,nullptr,shadow,environment);if(magazineVisible_&&magazineRenderer_)magazineRenderer_->render(c,yaw,false,&camera,surface,&origin,lights,nullptr,shadow,environment);}}
void Actor::shadow_casters(std::vector<shadows::Caster>& out,float yaw,const std::array<float,3>& origin)const{if(visible_&&renderer_){out.push_back({renderer_.get(),yaw,origin});if(magazineVisible_&&magazineRenderer_)out.push_back({magazineRenderer_.get(),yaw,origin});}}

}
