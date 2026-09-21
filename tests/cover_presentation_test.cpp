#include "cover_presentation.h"
#include "stage_navigation.h"
#include "cover_hud.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
void check(bool v,const char* text){if(!v)throw std::runtime_error(text);}
int main(int argc,char** argv){try{
 check(argc==2,"bank path");std::ifstream in(argv[1],std::ios::binary);std::vector<char> bytes{std::istreambuf_iterator<char>(in),{}};cover::CoverMotionBank bank(bytes);cover::Timeline clock;
 combat::cover::State wall{true,0,0};auto pose=clock.sample(&bank,wall,false,1,.1);check(pose&&pose->action==cover::Action::move_right,"right slide source");
 pose=clock.sample(&bank,wall,false,-1,.1);check(pose->action==cover::Action::move_left&&clock.seconds()==0,"left slide enters new clip");
 wall.lean=-1;pose=clock.sample(&bank,wall,false,0,.01);check(pose->action==cover::Action::peek_left_enter,"original left peek start");for(int i=0;i<100;++i)pose=clock.sample(&bank,wall,false,0,.01);check(pose->action==cover::Action::peek_left_hold,"peek hold follows source duration");
 wall.lean=0;pose=clock.sample(&bank,wall,false,0,.01);check(pose->action==cover::Action::peek_left_exit,"release preserves side");for(int i=0;i<100;++i)pose=clock.sample(&bank,wall,false,0,.01);check(pose->action==cover::Action::stand_left,"release returns to same-side idle");
 wall.lean=1;pose=clock.sample(&bank,wall,true,0,.01);check(pose->action==cover::Action::crouch_peek_right_enter,"crouch switches source");check(!clock.sample(&bank,{},false,0,.01),"detach removes wall pose and outer blend owns handoff");
 const auto base=bank.sample(cover::Action::stand_right,0)->pose;cover::FreeLean lean;
 auto result=lean.sample(&base,1,.1);check(result&&lean.side()==1,"native upper-body lean starts");for(unsigned i=0;i<5;++i)result=lean.sample(&base,1,.1);check(result->rotations==cover::native_side_lean(base,1,1)->rotations,"300ms native full tilt");
 for(unsigned i=0;i<5;++i)result=lean.sample(&base,0,.1);check(!result&&!lean.side(),"release returns completely");
 cover_hud::Renderer hud;std::vector<uint32_t> image(1280*720);hud.paint(image,{true,false,false,0,false,false,false,L"Y"});check(std::count_if(image.begin(),image.end(),[](auto p){return p!=0;})>20000,"actual contextual HUD visible");
 std::fill(image.begin(),image.end(),0);hud.paint(image,{});check(std::none_of(image.begin(),image.end(),[](auto p){return p!=0;}),"HUD absent away from cover");
 std::cout<<"cover phase, native FPP lean, HUD visibility PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
