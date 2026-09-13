#include "player_control.h"
#include "stage_navigation.h"
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace mgo2win;
static void require(bool v,const char*s){if(!v)throw std::runtime_error(s);}
int main(){try{
 player::Control c;std::array<float,24> in{};auto step=[&](int frames=1){for(int i=0;i<frames;++i)c.step(in,.01f,true,true);};auto tap=[&](unsigned i){in[i]=1;step();in[i]=0;step();};step();
 in[16]=1;step();require(c.running&&c.motion()==player::Motion::run,"run threshold intent");in[16]=0;step();
 tap(5);require(c.stance==player::Stance::crouching,"A tap crouches on release");
 in[5]=1;step(60);require(c.stance==player::Stance::prone&&!c.supine,"A hold enters face down");in[5]=0;step();require(c.stance==player::Stance::prone,"long release must not rise");
 in[16]=.4f;step();require(c.motion()==player::Motion::prone_forward&&!c.running&&c.forward==.4f,"analog crawl forward");in[16]=0;in[17]=1;step();require(c.motion()==player::Motion::prone_backward&&c.speed==300,"crawl backward");in[17]=0;step();
 tap(7);require(c.supine&&!c.firstPerson,"prone Y flip");in[7]=1;step(75);require(c.dead&&c.motion()==player::Motion::dead_supine,"Y hold playdead");in[7]=0;step();require(c.dead&&c.supine,"hold release does not flip");
 in[16]=1;in[10]=1;step();require(c.forward==0&&!c.firing,"playdead blocks crawl/fire");in={};step();tap(5);require(!c.dead&&c.stance==player::Stance::crouching,"A exits prone");tap(5);in[7]=1;step();require(c.specialRequested&&!c.firstPerson,"standing Y requests HOST special action");in[7]=0;step();require(!c.specialRequested&&!c.firstPerson,"Y release does not change view");
 c.knock_down(true);step();require(c.stance==player::Stance::prone&&c.supine,"damage preserves faceup");in[5]=1;step(60);require(c.supine,"hold while knocked down preserves faceup");in={};step();
 c.first_person_on_y(true);step();tap(7);require(c.firstPerson&&c.supine,"alternate Y firstperson in prone");tap(8);require(!c.supine,"alternate flip binding");
 c=player::Control{};step();tap(6);require(c.autoAim,"X autoaim");in[11]=in[10]=1;step();require(c.aiming&&c.firing,"triggers held");tap(4);require(c.reloading()&&!c.aiming&&!c.firing,"reload action interrupts aim and fire");in={};step(360);require(!c.reloading(),"reload timer completes");
 for(auto [action,menu]:{std::pair{12u,player::Menu::settings},{13u,player::Menu::chat},{14u,player::Menu::weapons},{15u,player::Menu::equipment}}){tap(action);/* tap release clears one-frame command */in[action]=1;step();require(c.menu==menu,"menu command");in={};step();}
 in[5]=1;step(30);c.step(in,.01f,false,true);step(100);require(c.stance==player::Stance::standing,"focus loss cancels hold and requires neutral");in={};step();tap(5);require(c.stance==player::Stance::crouching,"neutral rearms");
 c=player::Control{};in={};c.host_reload(false);step();in[4]=1;step();require(c.reloadStarted&&!c.reloading(),"network reload request waits for host instead of starting preview timer");in={};step();c.host_reload(true);step(400);require(c.reloading(),"host reload cannot end on local 3.5 second timer");in[10]=1;step();require(c.triggerHeld&&c.firePressed&&!c.firing,"trigger intent remains separate from host reload presentation");step();require(!c.firePressed,"held trigger produces one edge");c.host_reload(false);step();require(!c.reloading()&&c.firing,"host completion resumes held trigger presentation");c.step(in,.01f,false,true);require(!c.triggerHeld&&!c.firePressed,"focus loss clears trigger intent");
 c=player::Control{};in={};step();in[16]=1;step();in[7]=1;step();require(c.specialRequested&&!c.running&&c.forward==0&&!c.firePressed,"running Y stops motion and requests once");step();require(!c.specialRequested,"held Y cannot repeat request");in={};step();tap(5);require(c.stance==player::Stance::crouching,"normal crouch before special");in[7]=1;step();require(c.specialRequested&&c.stance==player::Stance::crouching,"crouched special preserves requested stance for HOST");
 c.host_special(true);in[5]=in[4]=in[10]=in[11]=in[16]=in[8]=1;step(90);require(c.special_active()&&!c.specialRequested&&!c.firing&&!c.triggerHeld&&!c.aiming&&!c.reloading()&&!c.firstPerson&&c.forward==0&&c.speed==0&&c.stance==player::Stance::crouching,"HOST special blocks movement fire reload stance view");
 c.host_special(false);in={};step();require(c.stance==player::Stance::crouching&&!c.specialRequested,"release suppressed inputs after HOST finish");tap(8);require(c.firstPerson,"view button remains available outside special");
 c.host_reload(true);in[7]=1;step();require(!c.specialRequested,"reload refuses Y request");c.host_reload(false);step();require(!c.specialRequested,"held rejected Y cannot defer until reload ends");in={};step();
 c.host_special(true);c.knock_down(false);require(!c.special_active()&&c.stance==player::Stance::prone,"knockdown cancels special presentation");in[7]=1;step();require(!c.specialRequested,"knockdown requires neutral rearm");in={};step();tap(7);require(c.supine,"post knockdown prone Y remains flip");
 c=player::Control{};in={};step();in[7]=1;c.step(in,.01f,false,false);step();require(!c.specialRequested,"focus loss prevents delayed Y request");in={};step();in[7]=1;step();require(c.specialRequested,"new Y after neutral requests again");
 require(c.specialHeld,"Y request carries held level");step();require(c.specialHeld&&!c.specialRequested,"held persists without repeated edge");in[7]=0;step();require(!c.specialHeld,"release clears special hold");
 c.host_special(true);in[7]=1;step();require(c.specialHeld,"HOST active carries Y held");c.step(in,.01f,false,true);require(!c.specialHeld&&!c.specialRequested,"focus loss releases host held");in={};step();in[7]=1;step();require(c.specialHeld,"neutral rearm permits held");in[12]=1;step();require(!c.specialHeld,"menu releases special hold");
 std::cout<<"player controls: analog intent, poses, long presses, faceup knockdown, view policies, menus, focus, host reload and special action passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
