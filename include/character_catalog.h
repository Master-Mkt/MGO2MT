#pragma once
#include "character_model.h"
#include "player_motion.h"
#include <map>
#include <optional>
namespace mgo2mt {
struct SkinBinding {std::array<uint16_t,4> bones;std::array<float,4> weights;std::array<std::array<float,3>,4> offsets;};
struct CatalogBone {uint32_t key;int32_t parent;std::array<float,3> position;std::vector<std::array<float,4>> rotation;};
struct CatalogMesh {uint32_t gender,key;std::vector<ModelVertex> vertices;std::vector<SkinBinding> skin;std::vector<uint32_t> indices;std::vector<ModelPart> parts;};
struct AppearanceRule {uint32_t gender,id,kind,color,model,flags;std::array<uint32_t,6> replacements;uint32_t materialMode=0;std::array<float,3> tint{1,1,1};};
struct AppearanceIssue {unsigned slot,id,color,texture;const char* reason;};
struct PreparedCharacter {
 CharacterModel model;std::vector<ModelVertex> bind;std::vector<SkinBinding> skin;
 unsigned gender=0,missingModels=0,missingColors=0,selectedParts=0;
 bool defaultedLower=false;
 std::vector<AppearanceIssue> issues;
 // Latest posed bone origins in model-local coordinates, including motion root
 // translation. Actor placement/yaw are applied by the caller, as for vertices.
 std::map<uint32_t,std::array<float,3>> bonePositions;
 // Full posed frames used by skinning. Row vectors, model-local; actor yaw
 // and origin have not been applied. Required for original MTP attachments.
 std::map<uint32_t,std::array<float,16>> boneFrames;
 std::optional<std::array<float,3>> bone_position(uint32_t hash)const{
  auto found=bonePositions.find(hash);if(found==bonePositions.end())return std::nullopt;return found->second;
 }
 bool ready()const{return !model.vertices.empty();}
};
class CharacterCatalog {
 std::array<std::vector<CatalogBone>,2> bones_;
 std::vector<std::array<float,3>>root_;
 std::vector<CatalogMesh>meshes_;std::vector<AppearanceRule>rules_;
 std::map<uint32_t,ModelTexture>textures_;
 unsigned frames_=0,fps_=0;uint32_t clip_=0;
public:
 explicit CharacterCatalog(std::span<const char>);
 PreparedCharacter assemble(const std::array<uint8_t,28>&)const;
 // Local renderable choices; these do not assert server ownership or unlocks.
 std::vector<AppearanceRule> creation_choices(unsigned gender,unsigned kind)const;
 // The catalog's original looping fallback clip, using the same sampling as
 // pose(seconds). Negative/nonfinite time samples its start; invalid rig throws.
 MotionPose sample_pose(unsigned gender,double seconds)const;
 // Exact pose(MotionPose) fallback: absent catalog bones use identity, not
 // frame zero of the fallback clip. Unknown hashes are discarded. Root identity,
 // root bounds and all retained quaternion values are validated before return.
 MotionPose complete_pose(unsigned gender,const MotionPose&)const;
 void pose(PreparedCharacter&,double seconds)const;
 void pose(PreparedCharacter&,const MotionPose&)const;
 std::span<const CatalogBone> skeleton(unsigned gender)const{return gender<2?std::span<const CatalogBone>(bones_[gender]):std::span<const CatalogBone>{};}
 unsigned frames()const{return frames_;}uint32_t clip()const{return clip_;}
 size_t mesh_count()const{return meshes_.size();}
};
}
