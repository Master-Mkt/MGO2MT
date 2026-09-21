#pragma once
#include "combat_authority.h"
#include "original_reload_timing.h"
#include <algorithm>
#include <cmath>
namespace mgo2mt::combat::presentation {
// Original MTSQ clip 3: (tick, local cue index), tick base 5. The byte 4
// in its commands is record length, not an invented sound opcode.
inline constexpr std::array<std::pair<unsigned,unsigned>,6> akSounds{{
 {115,17000},{270,17001},{410,17004},{620,17005},{840,17006},{900,17007}}};
inline constexpr std::array<std::pair<unsigned,unsigned>,6> akCrouchSounds{{
 {125,17000},{300,17001},{440,17003},{630,17005},{855,17006},{910,17007}}};
inline constexpr std::array<std::pair<unsigned,unsigned>,6> akLowSounds{{
 {120,17000},{275,17001},{400,17004},{660,17005},{855,17006},{905,17007}}};
class Reload {
 Identity id_{};uint64_t epoch_=0,deadline_=0,at_=0;uint32_t life_=0;
 double age_=0,emitted_=0,length_=1045,timeScale_=1;uint16_t received_=0,weapon_=0;unsigned level_=0,clip_=3;bool active_=false;
 std::vector<std::pair<unsigned,unsigned>> sounds_;
public:
 void clear(){*this={};}
 std::vector<unsigned> update(uint64_t epoch,const Player* p,uint64_t now,unsigned sourceClip=~0u,double duration=0,std::span<const std::pair<unsigned,unsigned>> cues={},double nativeDurationMs=0){
  std::vector<unsigned> sounds;
  if(!epoch||!p||!p->alive||p->stunned||!p->weapon||!p->reloadUntil){clear();return sounds;}
  bool fresh=!active_||epoch!=epoch_||id_!=p->identity||life_!=p->life||weapon_!=p->weapon||deadline_!=p->reloadUntil;
  if(fresh){clear();active_=true;epoch_=epoch;id_=p->identity;life_=p->life;weapon_=p->weapon;deadline_=p->reloadUntil;level_=std::min<unsigned>(p->reloadLevel,3);clip_=sourceClip==~0u?(p->pose.capsule.height==560?5:p->pose.capsule.height==1100?4:3):sourceClip;
   length_=std::isfinite(duration)&&duration>0?duration*300.:p->weapon==25?1045.:18000.;
   if(duration>0&&std::isfinite(nativeDurationMs)&&nativeDurationMs>0)timeScale_=length_/(nativeDurationMs*original::nominal_motion_fps*.005*original::rifle_reload_rates[level_]);
   if(!cues.empty())sounds_.assign(cues.begin(),cues.end());else if(p->weapon==25){const auto& original=clip_==4?akCrouchSounds:clip_==5?akLowSounds:akSounds;sounds_.assign(original.begin(),original.end());}
   age_=p->reloadElapsedMs;emitted_=ticks(age_);
  }
  else if(now<at_){age_=std::max(age_,double(p->reloadElapsedMs));}
  else {age_+=double(std::min<uint64_t>(now-at_,250));if(received_!=p->reloadElapsedMs)age_=std::max(age_,double(p->reloadElapsedMs));}
  at_=now;received_=p->reloadElapsedMs;
  double tick=ticks(age_);
  // A stalled window skips expired effects; it never bursts every old cue.
  if(!fresh)for(auto [time,cue]:sounds_)if(emitted_<time&&time<=tick&&tick-time<=75)sounds.push_back(cue);
  emitted_=std::max(emitted_,tick);return sounds;
 }
 double ticks(double ms)const{return std::min(length_,ms*original::nominal_motion_fps*.005*original::rifle_reload_rates[level_]*timeScale_);}
 double seconds()const{return active_?ticks(age_)/300.:0.;}
 bool active()const{return active_;}
 unsigned clip()const{return clip_;}
};
enum class Magazine {mounted,left,hidden};
inline Magazine ak_magazine(double clipSeconds){auto tick=clipSeconds*300.;return tick<115||tick>=650?Magazine::mounted:tick>=310&&tick<430?Magazine::hidden:Magazine::left;}
}
