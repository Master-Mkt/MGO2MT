#include "cover_input.h"
#include "player_control.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){try{
 std::array<float,24> v{};player::Control c;c.step(v,.01f,true,false);c.cover_context(true,false);v[7]=1;c.step(v,.01f,true,false);check(c.coverRequested&&!c.specialRequested&&!c.specialHeld,"Wall Y is contextual attachment, not salute");
 c.cover_context(false,true);v={};c.step(v,.01f,true,false);c.host_reload(true);v[7]=1;c.step(v,.01f,true,false);check(c.coverRequested,"Y detaches even while reloading");
 c.host_reload(false);v={};c.step(v,.01f,true,false);v[3]=1;c.step(v,.01f,true,false);check(c.lean==1,"Cover right digital lean");v[2]=1;c.step(v,.01f,true,false);check(c.lean==0,"Opposite digital inputs cancel");
 c.cover_context(false,false);v={};c.step(v,.01f,true,false);v[2]=1;c.step(v,.01f,true,false);check(c.lean==0,"Third person cannot free lean");c.firstPerson=true;c.step(v,.01f,true,false);check(c.lean==-1,"First person left lean");c.step(v,.01f,false,false);check(!c.lean&&!c.coverRequested,"Focus loss clears actions");
 v={};c.step(v,.01f,true,false);v[7]=1;c.step(v,.01f,true,false);check(c.specialRequested&&!c.coverRequested,"No wall preserves salute");
 player::CoverInput input;combat::Identity id{0,5,9};input.scope(3,id,1);check(input.press(combat::cover::Action::attach,100),"Attach request");auto intent=input.intent(1,true,true);check(intent.request==1&&intent.lean==1&&intent.firstPerson,"Lean coalesces with reliable request");check(!input.press(combat::cover::Action::attach,110),"No duplicate edge");check(input.intent(0,false,false)==combat::cover::Intent{},"Inactive sends no request");
 combat::SopView view;view.recipient=id;view.life=2;view.coverRequest=1;input.acknowledge(view,120);check(input.pending(),"Wrong lifetime cannot acknowledge");view.life=1;input.acknowledge(view,130);check(!input.pending(),"HOST rejection also acknowledges");check(input.press(combat::cover::Action::detach,140),"Detach follows ACK");check(input.intent(0,false,true).request==2,"Monotonic serial");input.cancel();input.press(combat::cover::Action::attach,200);check(input.intent(0,false,true).request==3,"Cancel cannot reuse serial");input.acknowledge(view,1700);check(!input.pending(),"Pending bounded at 1500ms");input.scope(4,id,2);check(input.press(combat::cover::Action::attach,1800)&&input.intent(0,false,true).request==1,"New scope resets counter");
 std::cout<<"cover input/context/ACK PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
