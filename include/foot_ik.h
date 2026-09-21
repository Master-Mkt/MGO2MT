#pragma once
#include "character_catalog.h"
#include "motion_blend_presentation.h"
#include "rigid_physics.h"
namespace mgo2mt::foot_ik {
// Native visual contact correction, using the recovered human MDN leg chains.
// No actor/capsule/network coordinates or source animation data are changed.
struct Foot {bool supported=false;stage::Vec3 ankle{},target{},normal{0,1,0};float correction=0,error=0;};
struct Result {bool rig=false,active=false;float pelvis=0;std::array<Foot,2> feet{};};
bool eligible(PlayerMotion);
// Remote snapshots have no grounded bit. Check the real body support instead
// of mistaking a floor close to an airborne actor for a planted capsule.
inline bool grounded(const stage::Collision* world,stage::Vec3 feet,stage::Capsule capsule){
 if(!world)return false;auto hit=world->sweep(feet,{0,-12,0},capsule,stage::query::player_floor);
 return hit&&hit->normal[1]>=.70710678f;
}
class Solver {
 struct Leg {unsigned hip=0,knee=0,ankle=0,toe=0;std::vector<size_t> sole;};
 std::array<Leg,2> legs_{};
 std::array<float,2> offsets_{};
 std::array<stage::Vec3,2> normals_{{{0,1,0},{0,1,0}}};
 float pelvis_=0;
 motion_blend::Scope scope_{};
 stage::Vec3 lastOrigin_{};
 bool initialized_=false,rig_=false;
 Result result_{};
 bool prepare(const CharacterCatalog&,const PreparedCharacter&);
public:
 void reset();
 MotionPose solve(const CharacterCatalog&,const PreparedCharacter&,const MotionPose&,
                  const stage::Collision*,stage::Vec3 origin,float yaw,float seconds,
                  bool groundedAndEligible,motion_blend::Scope);
 const Result& result()const{return result_;}
};
}
