#pragma once
#include "weapon_hand.h"
#include "character_renderer.h"
#include "shadow_renderer.h"
#include "first_person_sight.h"
#include "weapon_connection_points.h"
#include <filesystem>
#include <memory>
namespace mgo2mt::gameplay {class Config;}
namespace mgo2mt::weapon_hand {
struct ModelBinding {uint32_t flags=0;std::array<float,3> muzzle{};Point magazine;};
class Models {
 std::map<uint32_t,CharacterModel> models_,magazines_;
 std::map<uint32_t,ModelBinding> bindings_;
 std::map<uint32_t,uint32_t> sources_;
 connections::Table connections_;
public:
 explicit Models(const std::filesystem::path&);
 Models(const std::filesystem::path& dataRoot,const gameplay::Config&);
 bool load_connections(const std::filesystem::path&path,std::string&error){return connections_.load(path,error);}
 const connections::Table& connection_points()const{return connections_;}
 uint32_t source(uint32_t id)const{auto i=sources_.find(id);return i==sources_.end()?id:i->second;}
 const CharacterModel* find(uint32_t weapon)const;
 const CharacterModel* magazine(uint32_t weapon)const;
 const ModelBinding* binding(uint32_t weapon)const;
 std::vector<uint32_t> weapons()const;
};
// Uses the selected character's original skinned arm triangles in first person.
CharacterModel first_person_arms(const PreparedCharacter&,std::span<const CatalogBone>);
// Exact complementary triangles; used only for fading the local non-arm body.
CharacterModel first_person_body(const PreparedCharacter&,std::span<const CatalogBone>);
class Actor {
 uint32_t weapon_=0;CharacterModel model_;std::vector<ModelVertex> bind_;
 std::unique_ptr<CharacterRenderer> renderer_;bool visible_=false,magazineVisible_=true;std::array<float,16> frame_{};
 CharacterModel magazine_;std::vector<ModelVertex> magazineBind_;std::unique_ptr<CharacterRenderer> magazineRenderer_;
 ModelBinding binding_;
 std::optional<first_person_sight::Axis> sight_;
 std::vector<std::pair<uint32_t,first_person_sight::Axis>> connections_;
 Point shown_,from_,target_;uint32_t clip_=~0u;float blend_=1;
public:
 void clear(){weapon_=0;model_={};bind_.clear();renderer_.reset();magazine_={};magazineBind_.clear();magazineRenderer_.reset();visible_=false;clip_=~0u;blend_=1;sight_.reset();connections_.clear();}
 bool update(ID3D11Device*,ID3D11DeviceContext*,const Models&,const PreparedCharacter&,const Sample&,double dt,float blendRate=5);
 void hide(){visible_=false;}
 void draw(ID3D11DeviceContext*,float yaw,const WorldView&,CharacterRenderer*surface,const std::array<float,3>&origin,std::span<const DynamicPointLight>lights={},const shadows::Renderer* shadow=nullptr,const EnvironmentLight* environment=nullptr)const;
 void shadow_casters(std::vector<shadows::Caster>&,float yaw,const std::array<float,3>& origin)const;
 bool visible()const{return visible_;}
 bool magazine_visible()const{return visible_&&magazineVisible_;}
 std::optional<std::array<float,3>> muzzle(float yaw,const std::array<float,3>& origin)const;
 std::optional<first_person_sight::Axis> sight_axis(float yaw,const std::array<float,3>& origin)const;
 // Raw original CNP +Z axes (100 units for diagnostics), composed after the
 // same held-model frame. No claim of a selected attachment or hand IK.
 uint32_t weapon_id()const{return visible_?weapon_:0;}
 std::optional<first_person_sight::Axis> connection(uint32_t key,float yaw,const std::array<float,3>& origin)const;
 std::vector<std::pair<uint32_t,first_person_sight::Axis>> connection_frames(float yaw,const std::array<float,3>& origin)const;
};
}
