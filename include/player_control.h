#pragma once
#include <array>
#include <optional>
#include <string_view>
namespace mgo2win::player {
enum class Stance {standing,crouching,prone};
enum class Motion {idle,walk,run,crouch_idle,crouch_walk,prone_idle,prone_forward,prone_backward,supine_idle,supine_forward,supine_backward,dead_prone,dead_supine,aim,reload,roll,backstep};
enum class Evade {none,roll,backstep,rollLeft,rollRight};
inline constexpr bool is_roll(Evade kind){return kind==Evade::roll||kind==Evade::rollLeft||kind==Evade::rollRight;}
enum class Menu {none,settings,chat,weapons,equipment};
// Local action state. Combat/network consumers must explicitly consume intents;
// this controller does not fabricate ammunition, damage, or host authority.
class Control {
 std::array<bool,24> previous_{};bool armed_=false,crouchLong_=false,yLong_=false;
 float crouchTime_=0,yTime_=0,reloadTime_=0;bool yFirstPerson_=false,yProne_=false,hostSpecial_=false;
 std::optional<bool> hostReload_;
 bool coverNearby_=false,coverAttached_=false;
 Evade evade_=Evade::none;float evadeElapsed_=0,evadeDuration_=0,evadeSpeed_=0;bool evadeReviewedTravel_=false;
public:
 Stance stance=Stance::standing;bool supine=false,dead=false,firstPerson=false,autoAim=false,aiming=false,firing=false,running=false;
 float forward=0,right=0,turn=0,look=0,speed=0,bodyYaw=0;bool resetView=false,reloadStarted=false,triggerHeld=false,firePressed=false,specialRequested=false,specialHeld=false;
 Evade evadeRequested=Evade::none;bool evadeStarted=false;
 bool coverRequested=false;int lean=0;
 float evadeForward=0,evadeRight=0,evadeYaw=0;
 Menu menu=Menu::none;
 explicit Control(bool yFirstPerson=false):yFirstPerson_(yFirstPerson){}
 void step(const std::array<float,24>&,float dt,bool active,bool runRequested,bool grounded=true);
 // The caller supplies reviewed clip duration/movement speed after collision
 // admission. No invented original timing/distance or invulnerability is used.
 bool begin_evade(Evade,float durationSeconds,float movementSpeed,float lockedYaw);
 // Runtime roll movement follows the reviewed source-root curve. Generic
 // explicit profiles remain available to isolated controller fixtures.
 bool begin_reviewed_evade(Evade,float lockedYaw);
 void cancel_evade();
 Evade evade_active()const{return evade_;}
 float evade_elapsed()const{return evadeElapsed_;}
 float evade_remaining()const{return evadeDuration_>evadeElapsed_?evadeDuration_-evadeElapsed_:0;}
 void suspend();void knock_down(bool onBack);void reject_stance(Stance previous){stance=previous;}
 void first_person_on_y(bool value){yFirstPerson_=value;suspend();}
 // Network combat observes host state; the inspection-only timer cannot gate it.
 void host_reload(std::optional<bool> state){if(state!=hostReload_)reloadTime_=0;hostReload_=state;}
 // Geometry supplies availability; HOST acknowledgement supplies attachment.
 // Y at a wall is a contextual action, otherwise the existing salute remains.
 void cover_context(bool nearby,bool attached){coverNearby_=nearby;coverAttached_=attached;}
 // Request is an edge only. HOST owns admission, duration, death/epoch cancellation.
 void host_special(bool active){hostSpecial_=active;if(active)cancel_evade();}
 bool special_active()const{return hostSpecial_;}
 Motion motion()const;bool reloading()const{return hostReload_.value_or(reloadTime_>0);}
 static constexpr float crouchHold=.55f,deadHold=.70f;
};
std::wstring_view stance_name(const Control&);
}
