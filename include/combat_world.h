#pragma once
#include "stage_assets.h"
namespace mgo2win::combat {
// Collision-only host world. Does not load textures, character meshes or a GPU.
class World {
 std::shared_ptr<const stage::Collision> base_,world_,hits_;
 stage::ObjectRegistry registry_;std::vector<stage::ObjectBinding> bindings_;stage::CboxLayout cboxes_;
 std::optional<stage::SceneSnapshot> snapshot_;
public:
 static World load(const std::filesystem::path&stageRoot,uint8_t map=20);
 bool apply(const stage::SceneSnapshot&);
 const auto& collision()const{return world_;}
 const auto& targets()const{return hits_;}
 const auto& snapshot()const{return snapshot_;}
};
}
