#pragma once
#include "stage_collision.h"
#include "stage_water.h"
#include <memory>
namespace mgo2win::stage {
struct WalkInput {float forward=0,right=0,turn=0,look=0,speed=3500,yawRate=2,pitchRate=1.5f;};
struct NavigationWaterState {std::optional<float> level;float depthAboveFloor=0,horizontalScale=1;WaterFoot foot=WaterFoot::dry;};
// Local collision inspection, not the original player controller or spawn policy.
// Dimensions/speeds are native inspection choices in the decoded world units.
class Navigation {
 Vec3 feet_{},anchor_{};float yaw_=0,pitch_=0,vertical_=0;bool ready_=false,grounded_=false;
 Capsule shape_;
 std::shared_ptr<const Water> water_;float waterRatio_=.65f;NavigationWaterState waterState_;
 Vec3 slide(const Collision&,Vec3,Vec3,bool);
 void sample_water(const Collision&);
public:
 explicit Navigation(Capsule shape={}):shape_(shape){}
 bool place(const Collision&,Vec3 hint,float groundDistance=10000);
 void advance(const Collision&,WalkInput,float seconds);
 void clear(){ready_=grounded_=false;vertical_=0;waterState_={};}
 // Explicit native prototype ratio, NOT a recovered original movement coefficient.
 // Feet/floor ray adapt the original control query; actor/bone/stance water states remain absent.
 bool water(std::shared_ptr<const Water>,float horizontalRatio=.65f);
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
