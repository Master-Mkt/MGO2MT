#pragma once
#include "mounted_weapons.h"
#include "combat_authority.h"
#include "character_renderer.h"
#include "shadow_renderer.h"
#include "player_motion.h"
#include "character_catalog.h"
#include <map>
#include <memory>
namespace mgo2mt::mounted {
struct Rig {
 std::array<Vec3,3> pivots{};std::vector<uint8_t> vertices;
 std::vector<Vec3> bonePivots;std::vector<int32_t> parents;std::vector<uint32_t> keys;
 static Rig read(std::span<const char>,size_t vertexCount);
};
// CPU deformation preserves every original vertex/material/UV and uses the
// exported MDN rigid-bone assignments. The tripod does not turn with the gun.
void articulate(const CharacterModel&,const Rig&,const Type&,float relativeYaw,float pitch,std::span<ModelVertex>);
// Original operator pose, pitched around the gun's elevation pivot. The feet
// follow the HOST-approved horizontal operator orbit; legs retain their pose.
bool operator_pitch(PreparedCharacter&,std::span<const CatalogBone>,const Type&,float pitch);
class Renderer {
 struct Asset {CharacterModel model;Rig rig;std::array<std::unique_ptr<PlayerMotionBank>,2> motions;};
 struct Draw {Instance instance;std::vector<ModelVertex> vertices;std::unique_ptr<CharacterRenderer> renderer;float yaw=0,pitch=0;};
 Registry registry_;std::map<std::string,Asset> assets_;std::map<uint16_t,Draw> draws_;uint8_t map_=0;ID3D11Device* device_=nullptr;
public:
 Renderer(ID3D11Device*,const std::filesystem::path& dataRoot,const Registry&);
 void update(ID3D11DeviceContext*,uint8_t map,const combat::Snapshot*);
 void shadow_casters(std::vector<shadows::Caster>&)const;
 std::optional<MotionPose> pose(uint8_t map,uint16_t instance,uint32_t gender,double fireSeconds=-1)const;
 std::optional<MotionPose> action_pose(uint8_t map,uint16_t instance,uint32_t gender,PlayerMotion,double seconds)const;
 size_t size()const{return draws_.size();}
};
}
