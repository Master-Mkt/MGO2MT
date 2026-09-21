#pragma once
#include "gameplay_config.h"
#include "weapon_hand.h"
#include "original_lock_policy.h"
#include "weapon_visual_policy.h"
namespace mgo2mt::gameplay {
inline uint16_t motion_id(const Config* config,uint16_t id){const auto*v=config?config->visual(id):nullptr;return v?v->motionId:id;}
inline bool auto_aim(const Config* config,uint16_t id){const auto*v=config?config->visual(id):nullptr;return v?v->autoAim:combat::automatic_aim_weapon(id);}
inline std::optional<weapon_hand::Sample> hand(const Config* config,const weapon_hand::Bank* bank,uint16_t id,PlayerMotion motion,double seconds,bool aiming=false,double fire=-1,double cqc=-1,int posture=-1){
 if(!bank)return {};auto result=bank->select(motion_id(config,id),motion,seconds,aiming,fire,cqc,posture);if(result)result->weapon=id;return result;
}
inline std::optional<original_lock::Parameters> lock(const Config* config,uint16_t id,unsigned surveyor){
 if(!config)return original_lock::weapon_parameters(id,0,0,surveyor);
 const auto*v=config->visual(id);if(!v||!v->autoAim||v->lockRange<=0||surveyor>3)return {};
 const float range=v->lockRange*original_lock::surveyor_range_multiplier[surveyor],acquire=std::max(3000.f,range);
 return original_lock::Parameters{{acquire,v->lockWidth,v->lockYaw,original_lock::vertical_limit},{acquire+1000,v->lockWidth,v->lockYaw*2,original_lock::vertical_limit},{range,v->lockWidth,v->lockYaw,original_lock::vertical_limit}};
}
inline void effects(const Config* config,std::span<combat::Event> events){if(config)for(auto&e:events)if(const auto*v=config->visual(e.weapon);v&&v->effectId)e.weapon=v->effectId;}
}
