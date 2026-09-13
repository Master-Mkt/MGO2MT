#pragma once
#include <cstdint>
namespace mgo2win {
// PC-select presentation only. No stage actor or host gameplay state changes.
class SelectionPresentation {
public:
 enum class Kind {idle,salute,magazine,box};
 struct Frame {Kind kind=Kind::idle;double seconds=0;};
private:
 uint32_t id_=0;uint64_t lastInput_=0,started_=0;unsigned episode_=0;
 Kind kind_=Kind::idle;uint32_t saluteMs_=0,soundDelayMs_=0;bool magazine_=false,box_=false,sound_=false;
public:
 void configure(uint32_t saluteMs,bool magazine,bool box,uint32_t soundDelayMs=0){saluteMs_=saluteMs;magazine_=magazine;box_=box;soundDelayMs_=soundDelayMs;}
 void observe(uint32_t id,uint64_t now){if(id==id_)return;id_=id;lastInput_=started_=now;kind_=Kind::idle;sound_=false;episode_=0;}
 void activity(uint64_t now){lastInput_=now;if(kind_!=Kind::salute){kind_=Kind::idle;started_=now;}}
 void select(uint64_t now){if(!id_||!saluteMs_)return;kind_=Kind::salute;started_=lastInput_=now;sound_=true;}
 bool selecting(uint64_t now)const{return kind_==Kind::salute&&now>=started_&&now-started_<saluteMs_;}
 bool take_sound(uint64_t now=UINT64_MAX){if(!sound_||now<started_||now-started_<soundDelayMs_)return false;sound_=false;return true;}
 Frame frame(uint64_t now){
  if(!id_)return {};
  if(kind_==Kind::salute&&!selecting(now)){kind_=Kind::idle;started_=lastInput_=now;}
  if(kind_==Kind::idle&&now>=lastInput_&&now-lastInput_>=60000&&(magazine_||box_)){
   kind_=magazine_&&box_?((id_+episode_++)%2?Kind::magazine:Kind::box):magazine_?Kind::magazine:Kind::box;started_=now;
  }
  return {kind_,now>=started_?double(now-started_)/1000.:0.};
 }
};
}
