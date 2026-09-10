#pragma once
#include "character_model.h"
#include <map>
namespace mgo2win {
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
 void pose(PreparedCharacter&,double seconds)const;
 unsigned frames()const{return frames_;}uint32_t clip()const{return clip_;}
 size_t mesh_count()const{return meshes_.size();}
};
}
