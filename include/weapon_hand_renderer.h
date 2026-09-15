#pragma once
#include "weapon_hand.h"
#include "character_renderer.h"
#include "shadow_renderer.h"
#include <filesystem>
#include <memory>
namespace mgo2win::weapon_hand {
class Models {
 std::map<uint32_t,CharacterModel> models_,magazines_;
public:
 explicit Models(const std::filesystem::path&);
 const CharacterModel* find(uint32_t weapon)const;
 const CharacterModel* magazine(uint32_t weapon)const;
};
class Actor {
 uint32_t weapon_=0;CharacterModel model_;std::vector<ModelVertex> bind_;
 std::unique_ptr<CharacterRenderer> renderer_;bool visible_=false,magazineVisible_=true;std::array<float,16> frame_{};
 CharacterModel magazine_;std::vector<ModelVertex> magazineBind_;std::unique_ptr<CharacterRenderer> magazineRenderer_;
 Point shown_,from_,target_;uint32_t clip_=~0u;float blend_=1;
public:
 void clear(){weapon_=0;model_={};bind_.clear();renderer_.reset();magazine_={};magazineBind_.clear();magazineRenderer_.reset();visible_=false;clip_=~0u;blend_=1;}
 bool update(ID3D11Device*,ID3D11DeviceContext*,const Models&,const PreparedCharacter&,const Sample&,double dt,float blendRate=5);
 void hide(){visible_=false;}
 void draw(ID3D11DeviceContext*,float yaw,const WorldView&,CharacterRenderer*surface,const std::array<float,3>&origin,std::span<const DynamicPointLight>lights={},const shadows::Renderer* shadow=nullptr,const EnvironmentLight* environment=nullptr)const;
 void shadow_casters(std::vector<shadows::Caster>&,float yaw,const std::array<float,3>& origin)const;
 bool visible()const{return visible_;}
 bool magazine_visible()const{return visible_&&magazineVisible_;}
 std::optional<std::array<float,3>> muzzle(float yaw,const std::array<float,3>& origin)const;
};
}
