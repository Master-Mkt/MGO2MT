#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "player_control.h"
#include "controller_input.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace mgo2win;
using player::Evade;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static bool approximately(float a,float b){return std::abs(a-b)<1e-5f;}
struct Fixture {
 player::Control control;std::array<float,24> input{};
 Fixture(){step();control.host_reload(false);}
 void step(float dt=.01f,bool active=true,bool run=true,bool grounded=true){control.step(input,dt,active,run,grounded);}
 void neutral(){input={};step();}
 void running_request(){input[16]=1;step();input[5]=1;step();check(control.evadeRequested==Evade::roll,"running A requests roll");}
 void begin(Evade kind=Evade::roll,float duration=.3f,float speed=750,float yaw=1.2f){check(control.begin_evade(kind,duration,speed,yaw),"caller admits action with explicit fixture-only timing/speed");}
};
int main(){try{
 check(InputConfig{}.gamepad[5]==4&&InputConfig{}.gamepad[4]==5,"default pad A maps to action5, B to action4");
 Fixture f;f.running_request();
 check(!f.control.reloadStarted&&!f.control.specialRequested&&f.control.stance==player::Stance::standing&&f.control.speed==0,"request is separate from admission and existing actions");
 check(f.control.evade_active()==Evade::none&&approximately(f.control.evadeForward,1)&&f.control.evadeRight==0,"request contains movement direction but does not invent duration");
 f.begin();check(f.control.evadeStarted&&f.control.evade_active()==Evade::roll&&f.control.motion()==player::Motion::roll&&f.control.evade_elapsed()==0&&approximately(f.control.evade_remaining(),.3f),"admitted roll starts once");
 check(!f.control.begin_evade(Evade::roll,.3f,750,1.2f),"duplicate admission rejected");
 f.input={};f.input[17]=f.input[18]=f.input[4]=f.input[5]=f.input[7]=f.input[10]=f.input[11]=f.input[23]=f.input[20]=1;
 f.step(.05f);
 check(f.control.forward==1&&f.control.right==0&&f.control.speed==750&&approximately(f.control.bodyYaw,1.2f),"active movement direction and facing are locked despite new movement input");
 check(f.control.turn==1&&f.control.look==1,"look controls continue during evade");
 check(!f.control.evadeStarted&&f.control.evadeRequested==Evade::none&&!f.control.reloadStarted&&!f.control.specialRequested&&!f.control.specialHeld&&!f.control.triggerHeld&&!f.control.firePressed&&!f.control.firing&&!f.control.aiming&&f.control.stance==player::Stance::standing,"active action suppresses fire/reload/stance/special and repeated edges");
 check(approximately(f.control.evade_elapsed(),.05f)&&approximately(f.control.evade_remaining(),.25f),"elapsed/remaining follow provided duration");
 for(unsigned i=0;i<8;++i)f.step(.05f);
 check(f.control.evade_active()==Evade::none&&f.control.evadeRequested==Evade::none&&f.control.stance==player::Stance::standing&&!f.control.reloadStarted&&!f.control.specialRequested,"held A/Y/reload cannot restart or replay after completion");
 f.input[5]=0;f.step();check(f.control.stance==player::Stance::standing,"suppressed A release cannot crouch after an evade");

 Fixture backward;backward.control.bodyYaw=.7f;backward.input[17]=1;backward.step();backward.input[5]=1;backward.step();
 check(backward.control.evadeRequested==Evade::backstep&&backward.control.evadeForward==-1&&backward.control.evadeRight==0&&approximately(backward.control.evadeYaw,.7f),"standing backward+A prefers backstep even when run threshold is met");
 backward.begin(Evade::backstep,.2f,600,.7f);check(backward.control.motion()==player::Motion::backstep,"backstep has an explicit motion kind");
 Fixture gentle;gentle.input[17]=.2f;gentle.step(.01f,true,false);gentle.input[5]=1;gentle.step(.01f,true,false);check(gentle.control.evadeRequested==Evade::backstep,"backward+A does not require running or a deeply tilted stick");
 Fixture diagonal;diagonal.input[17]=1;diagonal.input[19]=.5f;diagonal.step();diagonal.input[5]=1;diagonal.step();check(diagonal.control.evadeRequested==Evade::backstep,"backward-dominant diagonal chooses straight backstep");
 Fixture lateral;lateral.input[17]=.6f;lateral.input[19]=1;lateral.step();lateral.input[5]=1;lateral.step();check(lateral.control.evadeRequested==Evade::rollRight&&lateral.control.evadeForward==0&&lateral.control.evadeRight==1,"side-dominant diagonal selects straight right rolling");

 for(int side:{-1,1}){
  Fixture slow;slow.input[side<0?18:19]=.2f;slow.step(.01f,true,false);slow.input[5]=1;slow.step(.01f,true,false);
  const auto kind=side<0?Evade::rollLeft:Evade::rollRight;
  check(slow.control.evadeRequested==kind&&slow.control.evadeForward==0&&slow.control.evadeRight==side,"slow lateral+A selects explicit left/right without a running requirement");
  slow.begin(kind,.2f,750,float(side)*1.57079632679f);
  check(slow.control.motion()==player::Motion::roll,"lateral kind reuses gameplay roll mapping, never a selection animation");
  slow.input[18]=side>0?1.f:0.f;slow.input[19]=side<0?1.f:0.f;slow.input[16]=slow.input[10]=slow.input[4]=slow.input[7]=slow.input[23]=1;
  slow.step(.05f,true,false);
  check(slow.control.forward==0&&slow.control.right==side&&approximately(slow.control.bodyYaw,float(side)*1.57079632679f)&&slow.control.turn==1,"left/right direction and body yaw lock while camera remains responsive");
  check(!slow.control.firing&&!slow.control.reloadStarted&&!slow.control.specialRequested,"lateral action suppresses attacks reload and special");
  for(unsigned n=0;n<10;++n)slow.step(.05f,true,false);
  check(slow.control.evade_active()==Evade::none&&slow.control.evadeRequested==Evade::none&&slow.control.stance==player::Stance::standing,"held A and reversed direction do not chain lateral rolls or change stance");
  slow.input[5]=0;slow.step();check(slow.control.stance==player::Stance::standing,"lateral A release remains consumed");
  Fixture airborne;airborne.input[side<0?18:19]=airborne.input[5]=1;airborne.step(.01f,true,false,false);check(airborne.control.evadeRequested==Evade::none,"lateral roll cannot start airborne");
  Fixture lost;lost.input[side<0?18:19]=lost.input[5]=1;lost.step(.01f,true,false);lost.begin(kind);lost.step(.01f,false);lost.step();check(lost.control.evade_active()==Evade::none&&lost.control.evadeRequested==Evade::none,"lateral focus/menu suspension requires neutral before retry");
 }
 Fixture stationary;stationary.input[5]=1;stationary.step();check(stationary.control.evadeRequested==Evade::none,"stationary A preserves ordinary stance request");stationary.input[5]=0;stationary.step();check(stationary.control.stance==player::Stance::crouching,"stationary A tap still crouches");
 Fixture walking;walking.input[16]=.4f;walking.step(.01f,true,false);walking.input[5]=1;walking.step(.01f,true,false);walking.input[5]=0;walking.step(.01f,true,false);check(walking.control.stance==player::Stance::crouching&&walking.control.evadeRequested==Evade::none,"walking A keeps existing crouch operation");
 Fixture hold;hold.input[5]=1;for(unsigned i=0;i<60;++i)hold.step();check(hold.control.stance==player::Stance::prone,"stationary A hold still enters prone");
 hold.neutral();hold.input[16]=hold.input[5]=1;hold.step();check(hold.control.evadeRequested==Evade::none,"prone A never requests roll");
 Fixture reload;reload.input[4]=1;reload.step();check(reload.control.reloadStarted&&reload.control.evadeRequested==Evade::none,"B/action4 retains reload");
 Fixture busy;busy.control.host_reload(true);busy.input[16]=busy.input[5]=1;busy.step();check(busy.control.evadeRequested==Evade::none,"host reload blocks evade");
 Fixture air;air.input[16]=air.input[5]=1;air.step(.01f,true,true,false);check(air.control.evadeRequested==Evade::none,"non-grounded start is rejected");
 Fixture rejected;rejected.running_request();
 for(auto duration:{0.f,-1.f,std::numeric_limits<float>::quiet_NaN()})check(!rejected.control.begin_evade(Evade::roll,duration,500,0)&&rejected.control.evade_active()==Evade::none,"invalid timing cannot create an action");
 check(!rejected.control.begin_evade(Evade::roll,.2f,-1,0)&&!rejected.control.begin_evade(Evade::roll,.2f,500,std::numeric_limits<float>::quiet_NaN()),"invalid movement speed/yaw cannot create an action");
 check(!rejected.control.begin_evade(Evade::backstep,.2f,500,0),"admission must match current requested action");
 for(unsigned i=0;i<80;++i)rejected.step();check(rejected.control.stance==player::Stance::standing&&rejected.control.evadeRequested==Evade::none,"unadmitted held A neither retries nor becomes prone");
 rejected.input[5]=0;rejected.step();check(rejected.control.stance==player::Stance::standing,"rejected request release is consumed");

 for(unsigned reason=0;reason<7;++reason){
  Fixture canceled;canceled.running_request();canceled.begin();
  switch(reason){case 0:canceled.step(.01f,false);break;case 1:canceled.input[12]=1;canceled.step();break;
   case 2:canceled.control.dead=true;canceled.step();break;case 3:canceled.control.knock_down(false);break;
   case 4:canceled.step(.01f,true,true,false);break;case 5:canceled.control.host_special(true);break;
   case 6:canceled.control=player::Control{};break;}
  check(canceled.control.evade_active()==Evade::none&&!canceled.control.evadeStarted&&canceled.control.evadeRequested==Evade::none&&canceled.control.speed==0,"focus/menu/death/knockdown/air/special/life reset cancels evade");
 }
 Fixture focus;focus.running_request();focus.begin();focus.step(.01f,false);for(unsigned i=0;i<40;++i)focus.step();check(focus.control.evadeRequested==Evade::none,"focus loss requires neutral, not merely action completion");focus.neutral();focus.running_request();check(focus.control.evadeRequested==Evade::roll,"fresh A edge after neutral can roll again");
 Fixture release;release.running_request();release.begin();release.input[5]=0;release.step();check(release.control.evade_active()==Evade::roll&&release.control.stance==player::Stance::standing,"A release does not truncate admitted animation or change stance");
 std::cout<<"A roll/backstep priority, explicit admission, locked movement, input suppression and lifecycle passed\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
