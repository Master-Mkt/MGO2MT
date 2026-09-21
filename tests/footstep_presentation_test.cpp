#include "footstep_presentation.h"
#include "combat_audio.h"
#include <iostream>
using namespace mgo2mt;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
 auto floor=stage::Collision::make({{-1000,0,-1000},{1000,0,-1000},{1000,0,1000},{-1000,0,1000}},{{{0,2,1},stage::attribute::player|stage::attribute::floor,0,0},{{0,3,2},stage::attribute::player|stage::attribute::floor,0,0}},{{0x15bccc,.5f,.5f,true}});
 auto material=combat::footsteps::dry_floor(floor,{0,2,0});check(material&&material->id==0x15bccc,"actual material from dry floor");
 check(!combat::footsteps::dry_floor(floor,{0,300,0}),"airborne rejected");
 auto water=stage::Water::make({{{0,0,0},{1000,100,1000}}});check(!combat::footsteps::dry_floor(floor,{0,2,0},&water),"wet floor does not play a dry material step");
 auto unknown=stage::Collision::make(floor.vertices,floor.triangles,{{}});check(!combat::footsteps::dry_floor(unknown,{0,2,0}),"unknown material silent");
 auto point=combat::footsteps::world_bone({10,20,30},{100,200,300},1.5707963267948966f);check(point&&std::abs((*point)[0]-130)<.001f&&(*point)[1]==220&&std::abs((*point)[2]-290)<.001f,"renderer bone transform");
 combat::footsteps::Timeline steps({.5});combat::footsteps::Input in{1,1,1,1,0x0460c5,10,0,true,true};check(!steps.advance(in).count,"first observation silent");in.seconds=.3;auto events=steps.advance(in);check(events.count==1&&events.values[0].bone==0x5b4a33,"original run MTSQ event");
 auto cue=combat::material_audio::resolve(combat::material_audio::Stage::n022a,events.values[0].cue,material->id);check(cue&&*cue==8005,"original run material conversion");
 if(argc>1){combat::Effects audio;check(audio.load(argv[1]),"actual footstep WAV bank");unsigned heard=0;check(audio.play_cue(*cue,*point,{100,200,300},[&](const auto& s){check(s.cue==8005&&s.origin==*point&&s.gain>0,"bone-positioned actual sound");++heard;}),"decoded run sound dispatched");check(heard==1&&!steps.advance(in).count,"once per timeline crossing");}
 std::cout<<"dry GEOM contact, wet/air/unknown suppression, bone transform and actual MTSQ material WAV PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
