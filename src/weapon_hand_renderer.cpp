#include "weapon_hand_renderer.h"
#include "combat_presentation.h"
#include <DirectXMath.h>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
namespace mgo2win::weapon_hand {
Models::Models(const std::filesystem::path&dir){for(auto[id,name]:{std::pair{25u,"ak102"},std::pair{3u,"operator"}}){for(bool secondary:{false,true}){std::ifstream in(dir/(std::string(name)+(secondary?"_secondary.gwm":".gwm")),std::ios::binary);if(!in)continue;std::vector<char> bytes{std::istreambuf_iterator<char>(in),{}};(secondary?magazines_:models_).emplace(id,CharacterModel(bytes));}}}
const CharacterModel*Models::find(uint32_t w)const{auto it=models_.find(w);return it==models_.end()?nullptr:&it->second;}
const CharacterModel*Models::magazine(uint32_t w)const{auto it=magazines_.find(w);return it==magazines_.end()?nullptr:&it->second;}
bool Actor::update(ID3D11Device*d,ID3D11DeviceContext*c,const Models&models,const PreparedCharacter&body,const Sample&s,double dt,float rate){
 visible_=false;const auto*source=models.find(s.weapon);if(!source){clear();return false;}
 if(weapon_!=s.weapon||!renderer_){clear();model_=*source;bind_=model_.vertices;renderer_=std::make_unique<CharacterRenderer>(d,model_);weapon_=s.weapon;shown_=from_=target_=s.point;clip_=s.index;}
 if(!magazineRenderer_)if(auto*m=models.magazine(s.weapon)){magazine_=*m;magazineBind_=m->vertices;magazineRenderer_=std::make_unique<CharacterRenderer>(d,magazine_);}
 if(clip_!=s.index||target_.bone!=s.point.bone){from_=shown_;target_=s.point;clip_=s.index;blend_=0;}
 target_=s.point;blend_=std::min(1.f,blend_+float(std::clamp(dt,0.,.1))*std::clamp(rate,.1f,200.f));shown_=target_;
 if(blend_<1&&from_.bone==target_.bone){for(unsigned j=0;j<3;++j)shown_.position[j]=from_.position[j]*(1-blend_)+target_.position[j]*blend_;using namespace DirectX;auto&a=from_.rotation;auto&b=target_.rotation;XMFLOAT4 q;XMStoreFloat4(&q,XMQuaternionSlerp(XMVectorSet(a[0],a[1],a[2],a[3]),XMVectorSet(b[0],b[1],b[2],b[3]),blend_));shown_.rotation={q.x,q.y,q.z,q.w};}
 auto f=frame(body,shown_);if(!f)return false;frame_=*f;transform(bind_,model_.vertices,*f);renderer_->update_vertices(c,model_.vertices);
 // D3CB10 CNP_amp_def -> D3E6E8 secondary model. Both reviewed roots have
 // identity bind transforms. Reload events switch the secondary mesh to the
 // original left-hand MTP point and suppress it during the magazine swap.
 auto mode=s.weapon==25&&s.index>=3&&s.index<=5?combat::presentation::ak_magazine(s.seconds):combat::presentation::Magazine::mounted;magazineVisible_=mode!=combat::presentation::Magazine::hidden;
 if(magazineRenderer_){using namespace DirectX;XMFLOAT4X4 parent;std::memcpy(&parent,f->data(),sizeof(parent));auto local=s.weapon==25?XMMatrixTranslation(0,-42.1999015808f,93.0010986328f):XMMatrixTranslation(-.0002f,-28.048500061f,-50.202899933f);XMFLOAT4X4 combined;XMStoreFloat4x4(&combined,local*XMLoadFloat4x4(&parent));std::array<float,16> matrix;std::memcpy(matrix.data(),&combined,sizeof(combined));if(mode==combat::presentation::Magazine::left){auto left=s.magazine?frame(body,*s.magazine):std::nullopt;if(left)matrix=*left;else magazineVisible_=false;}
 transform(magazineBind_,magazine_.vertices,matrix);magazineRenderer_->update_vertices(c,magazine_.vertices);}
 visible_=true;return true;
}
std::optional<std::array<float,3>> Actor::muzzle(float yaw,const std::array<float,3>&origin)const{
 if(!visible_)return {};using namespace DirectX;XMFLOAT4X4 values;std::memcpy(&values,frame_.data(),sizeof(values));
 auto point=weapon_==25?XMVectorSet(0,47,500,1):XMVectorSet(0,42.4877f,137.0739f,1);
 XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(point,XMLoadFloat4x4(&values)*XMMatrixRotationY(yaw)*XMMatrixTranslation(origin[0],origin[1],origin[2])));return std::array<float,3>{p.x,p.y,p.z};
}
void Actor::draw(ID3D11DeviceContext*c,float yaw,const WorldView&camera,CharacterRenderer*surface,const std::array<float,3>&origin,std::span<const DynamicPointLight>lights,const shadows::Renderer* shadow,const EnvironmentLight* environment)const{if(visible_&&renderer_){renderer_->render(c,yaw,false,&camera,surface,&origin,lights,nullptr,shadow,environment);if(magazineVisible_&&magazineRenderer_)magazineRenderer_->render(c,yaw,false,&camera,surface,&origin,lights,nullptr,shadow,environment);}}
void Actor::shadow_casters(std::vector<shadows::Caster>& out,float yaw,const std::array<float,3>& origin)const{if(visible_&&renderer_){out.push_back({renderer_.get(),yaw,origin});if(magazineVisible_&&magazineRenderer_)out.push_back({magazineRenderer_.get(),yaw,origin});}}

}
