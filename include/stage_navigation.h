#pragma once
#include "stage_collision.h"
#include "stage_water.h"
#include "water_gameplay.h"
#include "cover_policy.h"
#include <memory>
namespace mgo2mt::stage {
// A maneuver can keep its travel heading while the player looks elsewhere.
struct WalkInput {float forward=0,right=0,turn=0,look=0,speed=3500,yawRate=2,pitchRate=1.5f;std::optional<float> movementYaw;};
using NavigationWaterState=water_gameplay::Contact;
// Local collision inspection, not the original player controller or spawn policy.
// Dimensions/speeds are native inspection choices in the decoded world units.
class Navigation {
 Vec3 feet_{},anchor_{};float yaw_=0,pitch_=0,vertical_=0;bool ready_=false,grounded_=false;
 Capsule shape_;
 std::shared_ptr<const Water> water_;water_gameplay::Policy waterPolicy_;NavigationWaterState waterState_;bool waterTransitionBlocked_=false;
 Vec3 slide(const Collision&,Vec3,Vec3,bool);
 void sample_water(const Collision&);
public:
 explicit Navigation(Capsule shape={}):shape_(shape){}
 bool place(const Collision&,Vec3 hint,float groundDistance=10000);
 void advance(const Collision&,WalkInput,float seconds);
 void rotate_view(WalkInput,float seconds);
 void advance_cover(const Collision&,WalkInput,float seconds,const combat::cover::State&);
 void clear(){ready_=grounded_=false;vertical_=0;waterState_={};waterTransitionBlocked_=false;}
 // Explicit native prototype ratio, NOT a recovered original movement coefficient.
 // FIELD/floor query plus native stance/face proxy; no animated face bone yet.
 bool water(std::shared_ptr<const Water>,float horizontalRatio=.65f);
 bool water_policy(water_gameplay::Policy);
 NavigationWaterState water_state()const{return waterState_;}
 bool ready()const{return ready_;}bool grounded()const{return grounded_;}
 Vec3 feet()const{return feet_;}Vec3 eye()const;
 Vec3 direction()const;
 float yaw()const{return yaw_;}float pitch()const{return pitch_;}Capsule capsule()const{return shape_;}
 bool authoritative(const Collision&,Vec3,float yaw,float pitch,Capsule);
 bool shape(const Collision&,Capsule);
 void reset_view(float yaw){facing(yaw);}
 // Native lock presentation follows at the existing manual camera rates.
 bool track_view(Vec3 target,float seconds);
 void facing(float yaw){yaw_=yaw;pitch_=0;}
};
}
