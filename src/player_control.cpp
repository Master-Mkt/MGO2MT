#include "player_control.h"
#include "original_reload_timing.h"
#include <algorithm>
#include <cmath>
namespace mgo2win::player {
void Control::suspend(){armed_=false;previous_={};crouchTime_=yTime_=0;crouchLong_=yLong_=yProne_=false;aiming=firing=running=false;forward=right=turn=look=speed=0;menu=Menu::none;resetView=reloadStarted=triggerHeld=firePressed=specialRequested=specialHeld=false;}
void Control::knock_down(bool back){stance=Stance::prone;supine=back;dead=false;reloadTime_=0;hostSpecial_=false;suspend();}
void Control::step(const std::array<float,24>& values,float dt,bool active,bool runRequested){
 menu=Menu::none;resetView=reloadStarted=triggerHeld=firePressed=specialRequested=specialHeld=false;
 if(!std::isfinite(dt)||dt<0){suspend();return;}dt=std::min(dt,.1f);
 reloadTime_=std::max(0.f,reloadTime_-dt);
 std::array<bool,24> held{};for(unsigned i=0;i<24;++i){if(!std::isfinite(values[i])){suspend();return;}held[i]=values[i]>.12f;}
 if(!active){suspend();return;}
 if(!armed_){if(std::none_of(held.begin(),held.end(),[](bool v){return v;}))armed_=true;return;}
 specialHeld=held[7]&&!dead&&(hostSpecial_||(stance!=Stance::prone&&!reloading()));
 auto press=[&](unsigned i){return held[i]&&!previous_[i];};auto release=[&](unsigned i){return !held[i]&&previous_[i];};
 if(press(12))menu=Menu::settings;else if(press(13))menu=Menu::chat;else if(press(14))menu=Menu::weapons;else if(press(15))menu=Menu::equipment;
 if(menu!=Menu::none){auto requested=menu;suspend();menu=requested;return;}
 if(hostSpecial_){
  // Consume held edges during the HOST action; they must not replay upon completion.
  previous_=held;crouchTime_=yTime_=0;crouchLong_=held[5];yLong_=held[7];yProne_=false;
  aiming=firing=running=false;forward=right=turn=look=speed=0;return;
 }
 if(press(7)&&stance!=Stance::prone&&!dead&&!reloading()){
  specialRequested=true;yProne_=false;yLong_=true;previous_=held;
  crouchTime_=0;crouchLong_=true;aiming=firing=running=false;forward=right=turn=look=speed=0;return;
 }
 if(press(5)){crouchTime_=0;crouchLong_=false;dead=false;}
 if(held[5]){crouchTime_+=dt;if(!crouchLong_&&crouchTime_>=crouchHold){if(stance!=Stance::prone)supine=false;stance=Stance::prone;dead=false;crouchLong_=true;}}
 if(release(5)&&!crouchLong_){stance=stance==Stance::standing?Stance::crouching:stance==Stance::crouching?Stance::standing:Stance::crouching;dead=false;}
 if(press(7)){yTime_=0;yLong_=false;yProne_=stance==Stance::prone;}
 if(held[7]&&yProne_){yTime_+=dt;if(stance==Stance::prone&&!yLong_&&yTime_>=deadHold){dead=!dead;yLong_=true;}}
 if(release(7)&&!yLong_&&yProne_&&stance==Stance::prone){if(!yFirstPerson_){supine=!supine;dead=false;}else firstPerson=!firstPerson;}
 if(press(8)){if(stance==Stance::prone&&yFirstPerson_){supine=!supine;dead=false;}else firstPerson=!firstPerson;}
 if(press(6))autoAim=!autoAim;
 if(press(4)&&!dead&&!reloading()){if(!hostReload_)reloadTime_=float(original::reload_timing(original::ak102_reload)->endSeconds);reloadStarted=true;}
 triggerHeld=held[10]&&!dead;firePressed=triggerHeld&&press(10);
 aiming=held[11]&&!dead&&!reloading();firing=triggerHeld&&!reloading();resetView=press(9);
 forward=std::clamp(values[16]-values[17],-1.f,1.f);right=std::clamp(values[19]-values[18],-1.f,1.f);
 turn=std::clamp(values[23]-values[22],-1.f,1.f);look=std::clamp(values[20]-values[21],-1.f,1.f);
 float magnitude=std::hypot(forward,right);if(magnitude>1){forward/=magnitude;right/=magnitude;}
 if(dead){forward=right=0;}
 running=runRequested&&stance==Stance::standing&&!aiming&&!reloading()&&!dead&&magnitude>.01f;
 speed=dead?0:stance==Stance::prone?(forward<0?300.f:450.f):stance==Stance::crouching?1000.f:running?3800.f:1573.f;
 previous_=held;
}
Motion Control::motion()const{
 if(dead)return supine?Motion::dead_supine:Motion::dead_prone;
 bool moving=std::hypot(forward,right)>.01f;
 if(stance==Stance::prone){if(supine)return !moving?Motion::supine_idle:forward<0?Motion::supine_backward:Motion::supine_forward;return !moving?Motion::prone_idle:forward<0?Motion::prone_backward:Motion::prone_forward;}
 if(reloading()&&stance==Stance::standing)return Motion::reload;
 if(stance==Stance::crouching)return moving?Motion::crouch_walk:Motion::crouch_idle;
 if(moving)return running?Motion::run:Motion::walk;
 return aiming?Motion::aim:Motion::idle;
}
std::wstring_view stance_name(const Control& c){if(c.dead)return c.supine?L"死んだふり（仰向け）":L"死んだふり（うつ伏せ）";if(c.stance==Stance::prone)return c.supine?L"仰向け":L"うつ伏せ";if(c.stance==Stance::crouching)return L"しゃがみ";return c.running?L"走り":L"立ち／歩き";}
}
