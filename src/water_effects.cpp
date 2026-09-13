#include "water_effects.h"
#include <algorithm>
#include <cmath>
namespace mgo2win::stage {
void WaterEffects::reset(){effects_.clear();initialized_=wet_=false;distance_=cooldown_=0;epoch_=life_=0;}
void WaterEffects::emit(Vec3 p,float level){
 // Four effects per emission, bounded independently of render frame rate.
 while(effects_.size()+4>maximumEffects)effects_.erase(effects_.begin());
 p[1]=level+3;effects_.push_back({p,{},level,0,false});
 for(unsigned i=0;i<3;++i){float a=float(i)*2.0943951f+.35f;
  effects_.push_back({p,{std::cos(a)*450,1200,std::sin(a)*450},level,0,true});
 }
}
void WaterEffects::update(std::uint64_t epoch,std::uint64_t life,Vec3 p,const NavigationWaterState& state,bool active,float dt){
 if(!active||!epoch||!life||!std::isfinite(dt)||dt<=0||dt>.25f||
    std::any_of(p.begin(),p.end(),[](float x){return !std::isfinite(x)||std::abs(x)>=1e6f;})){reset();return;}
 const bool wet=state.level&&std::isfinite(*state.level)&&std::isfinite(state.depthAboveFloor)&&state.depthAboveFloor>0&&
                state.foot==WaterFoot::inWater&&p[1]<=*state.level;
 const float level=wet?*state.level:0;
 const float moved=initialized_?std::hypot(p[0]-previous_[0],p[2]-previous_[2]):0;
 // First observation, replacement, long displacement and changing surface are baselines.
 if(!initialized_||epoch!=epoch_||life!=life_||moved>2000||std::abs(p[1]-previous_[1])>2000||
    (wet&&wet_&&std::abs(level-previousLevel_)>1)){
  effects_.clear();initialized_=true;epoch_=epoch;life_=life;previous_=p;previousLevel_=level;wet_=wet;distance_=cooldown_=0;return;
 }
 for(auto& e:effects_){e.age+=dt;if(e.splash){e.velocity[1]-=9800*dt;for(unsigned i=0;i<3;++i)e.position[i]+=e.velocity[i]*dt;}}
 std::erase_if(effects_,[](const Effect& e){return e.age>=.8f||(e.splash&&e.position[1]<e.level);});
 cooldown_=std::max(0.f,cooldown_-dt);
 if(wet){
  if(wet_)distance_+=moved;
  if((!wet_||distance_>=300)&&cooldown_==0){emit(p,level);distance_=0;cooldown_=.18f;}
 }else distance_=0;
 previous_=p;previousLevel_=level;wet_=wet;
}
std::vector<WaterEffectLine> WaterEffects::lines()const{
 std::vector<WaterEffectLine> result;result.reserve(effects_.size()*12);
 for(const auto& e:effects_){float alpha=std::max(0.f,1-e.age/.8f);
  if(e.splash){auto end=e.position;for(unsigned i=0;i<3;++i)end[i]-=e.velocity[i]*.025f;end[1]=std::max(end[1],e.level+1);result.push_back({e.position,end,alpha,true});}
  else{float radius=80+e.age*240;for(unsigned i=0;i<12;++i){float a=float(i)*.52359878f,b=float(i+1)*.52359878f;
   result.push_back({{e.position[0]+std::cos(a)*radius,e.level+3,e.position[2]+std::sin(a)*radius},
                     {e.position[0]+std::cos(b)*radius,e.level+3,e.position[2]+std::sin(b)*radius},alpha,false});}}
 }
 return result;
}
}
