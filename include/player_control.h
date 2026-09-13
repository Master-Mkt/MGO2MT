#pragma once
#include <array>
#include <optional>
#include <string_view>
namespace mgo2win::player {
enum class Stance {standing,crouching,prone};
enum class Motion {idle,walk,run,crouch_idle,crouch_walk,prone_idle,prone_forward,prone_backward,supine_idle,supine_forward,supine_backward,dead_prone,dead_supine,aim,reload};
enum class Menu {none,settings,chat,weapons,equipment};
// Local action state. Combat/network consumers must explicitly consume intents;
// this controller does not fabricate ammunition, damage, or host authority.
class Control {
 std::array<bool,24> previous_{};bool armed_=false,crouchLong_=false,yLong_=false;
 float crouchTime_=0,yTime_=0,reloadTime_=0;bool yFirstPerson_=false,yProne_=false,hostSpecial_=false;
 std::optional<bool> hostReload_;
public:
 Stance stance=Stance::standing;bool supine=false,dead=false,firstPerson=false,autoAim=false,aiming=false,firing=false,running=false;
 float forward=0,right=0,turn=0,look=0,speed=0,bodyYaw=0;bool resetView=false,reloadStarted=false,triggerHeld=false,firePressed=false,specialRequested=false,specialHeld=false;
 Menu menu=Menu::none;
 explicit Control(bool yFirstPerson=false):yFirstPerson_(yFirstPerson){}
 void step(const std::array<float,24>&,float dt,bool active,bool runRequested);
 void suspend();void knock_down(bool onBack);void reject_stance(Stance previous){stance=previous;}
 void first_person_on_y(bool value){yFirstPerson_=value;suspend();}
 // Network combat observes host state; the inspection-only timer cannot gate it.
 void host_reload(std::optional<bool> state){if(state!=hostReload_)reloadTime_=0;hostReload_=state;}
 // Request is an edge only. HOST owns admission, duration, death/epoch cancellation.
 void host_special(bool active){hostSpecial_=active;}
 bool special_active()const{return hostSpecial_;}
 Motion motion()const;bool reloading()const{return hostReload_.value_or(reloadTime_>0);}
 static constexpr float crouchHold=.55f,deadHold=.70f;
};
std::wstring_view stance_name(const Control&);
}
