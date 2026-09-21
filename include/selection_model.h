#pragma once
#include "character_model.h"
#include "player_motion.h"
namespace mgo2mt {
// The existing PC catalog subtracts the first original lobby root (Y=1081).
// Gameplay banks retain raw actor coordinates; only this preview adapter shifts.
inline float selection_origin_y(const PlayerMotionBank& bank){auto*clip=bank.find(PlayerMotion::SelectionSalute);return clip&&!clip->roots.empty()?clip->roots.front()[1]:0.f;}
inline std::optional<MotionPose> selection_pose(const PlayerMotionBank& bank,PlayerMotion action,double seconds){auto pose=bank.sample(action,seconds);if(pose)pose->root[1]-=selection_origin_y(bank);return pose;}
// PC preview props share the actor's origin/yaw. Keep standing bounds so the
// camera does not jump when the character crouches under the original box.
inline CharacterModel selection_with_prop(const CharacterModel& body,const CharacterModel& prop,float offsetY=0){
 auto result=body;auto vertices=uint32_t(result.vertices.size()),indices=uint32_t(result.indices.size()),textures=uint32_t(result.textures.size());
 for(auto vertex:prop.vertices){vertex.y+=offsetY;result.vertices.push_back(vertex);}
 for(auto i:prop.indices)result.indices.push_back(i+vertices);
 for(auto part:prop.parts){part.first+=indices;part.texture+=textures;if(part.flags)part.flags+=textures;result.parts.push_back(part);}
 result.textures.insert(result.textures.end(),prop.textures.begin(),prop.textures.end());return result;
}
}
