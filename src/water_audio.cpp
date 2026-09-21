#include "water_audio.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace mgo2mt::water_audio {
namespace {
bool finite(stage::Vec3 p){return std::all_of(p.begin(),p.end(),[](float v){return std::isfinite(v)&&std::abs(v)<1e6f;});}

void u16(std::vector<uint8_t>& b,uint16_t v){b.push_back(uint8_t(v));b.push_back(uint8_t(v>>8));}
void u32(std::vector<uint8_t>& b,uint32_t v){u16(b,uint16_t(v));u16(b,uint16_t(v>>16));}
void tag(std::vector<uint8_t>& b,const char* s){for(unsigned i=0;i<4;++i)b.push_back(uint8_t(s[i]));}
}
bool valid_wet(const Actor& a){return a.identity&&a.life&&finite(a.feet)&&a.alive&&a.grounded&&a.water.level&&std::isfinite(*a.water.level)&&std::abs(*a.water.level)<1e6f&&std::isfinite(a.water.depthAboveFloor)&&a.water.depthAboveFloor>0&&std::isfinite(a.water.horizontalScale)&&a.water.horizontalScale>=0&&a.water.horizontalScale<=1&&a.water.foot==stage::WaterFoot::inWater&&a.feet[1]<=*a.water.level;}
void Steps::reset(){scope_={};lastNow_=0;initialized_=false;tracks_.clear();}
std::vector<Step> Steps::update(Scope scope,std::span<const Actor> actors,uint64_t now,bool active){
 std::vector<Step> result;
 if(!active||!scope.epoch||!scope.scene||actors.size()>maximumActors){reset();return result;}
 std::set<uint64_t> present;for(const auto& a:actors)if(!a.identity||!a.life||!finite(a.feet)||!present.insert(a.identity).second){reset();return result;}
 const auto baseline=[&](const Actor& a){const bool w=valid_wet(a);return Track{a.life,0,a.feet,w?*a.water.level:0,0,w,false};};
 if(!initialized_||scope!=scope_||now<lastNow_||now-lastNow_>maximumGapMs){reset();initialized_=true;scope_=scope;lastNow_=now;for(const auto& a:actors)tracks_[a.identity]=baseline(a);return result;}
 std::erase_if(tracks_,[&](const auto& pair){return !present.contains(pair.first);});
 if(now==lastNow_){for(const auto& a:actors){auto it=tracks_.find(a.identity);if(it==tracks_.end()||it->second.life!=a.life||!a.alive||!a.grounded||!valid_wet(a))tracks_[a.identity]=baseline(a);}return result;}
 lastNow_=now;
 for(const auto& a:actors){auto it=tracks_.find(a.identity);if(it==tracks_.end()){tracks_[a.identity]=baseline(a);continue;}
  auto& t=it->second;const bool w=valid_wet(a);const float level=w?*a.water.level:0;
  const float moved=std::hypot(a.feet[0]-t.feet[0],a.feet[2]-t.feet[2]);
  if(t.life!=a.life||moved>maximumDisplacement||std::abs(a.feet[1]-t.feet[1])>maximumDisplacement||(w&&t.wet&&std::abs(level-t.level)>1)||!a.alive||!a.grounded){t=baseline(a);continue;}
  if(w){if(t.wet)t.distance+=moved;const bool cadence=!t.emitted||now-t.lastEmission>=minimumIntervalMs;
   if(cadence&&(!t.wet||t.distance>=stepDistance)){
    // Retire the event even when the per-tick budget is exhausted. No backlog.
    if(result.size()<maximumEvents)result.push_back({a.identity,a.life,{a.feet[0],level+3,a.feet[2]},.32f});
    t.lastEmission=now;t.emitted=true;t.distance=0;
   }
  }else{t.distance=0;t.emitted=false;}
  t.feet=a.feet;t.level=level;t.wet=w;
 }
 return result;
}
std::vector<uint8_t> native_step_wav(){
 constexpr uint32_t rate=22050,count=5292;std::vector<uint8_t> result;result.reserve(44+count*2);
 tag(result,"RIFF");u32(result,36+count*2);tag(result,"WAVE");tag(result,"fmt ");u32(result,16);u16(result,1);u16(result,1);u32(result,rate);u32(result,rate*2);u16(result,2);u16(result,16);tag(result,"data");u32(result,count*2);
 uint32_t random=0x57415452;double low=0,previous=0;
 for(uint32_t i=0;i<count;++i){const double t=double(i)/rate;random^=random<<13;random^=random>>17;random^=random<<5;const double noise=(double(random)/4294967295.)*2-1;
  low+=.18*(noise-low);const double texture=(noise-previous)*.18+low*.9;previous=noise;
  const double attack=std::min(t/.008,1.),tail=std::max(0.,1-double(i)/count);
  const double bubbles=std::sin(6.283185307*(380*t-430*t*t))*std::exp(-t*24)+.45*std::sin(6.283185307*(610*t-740*t*t))*std::exp(-t*32);
  const double sample=attack*tail*tail*(texture*.8+bubbles*.20)*8500;
  const auto value=int16_t(std::clamp(std::lround(sample),-10000L,10000L));u16(result,uint16_t(value));
 }
 return result;
}
std::optional<combat::Sound> make_sound(const Step& step,const std::filesystem::path& file,stage::Vec3 listener){
 if(!step.identity||!step.life||!finite(step.position)||!finite(listener)||!std::isfinite(step.gain)||step.gain<=0||step.gain>.32f||file.filename()!=filename)return std::nullopt;
 double distance=0;for(unsigned k=0;k<3;++k){const double d=double(step.position[k])-listener[k];distance+=d*d;}distance=std::sqrt(distance);if(distance>12000)return std::nullopt;
 return combat::Sound{0,file,float(step.gain/(1+distance/2500)),step.position};
}
}
