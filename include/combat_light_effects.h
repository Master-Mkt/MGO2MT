#pragma once
#include "combat_authority.h"
#include "combat_particle_effects.h"
#include "dynamic_light.h"
#include <algorithm>
#include <cmath>
#include <optional>
namespace mgo2mt::combat {
// Presentation policy in decoded millimetres. Event arrival time is used because
// GWCB v5 has no original muzzle timestamp/bone transform. No damage is generated.
class LightEffects {
 struct Pulse {DynamicPointLight light;uint64_t born=0,lifetime=0;weapon_effect::Curve curve{{0,1},{1,0}};Identity owner{};uint32_t life=0;};
 std::vector<Pulse> pulses_;
 std::set<uint64_t> seen_;
 std::shared_ptr<const weapon_effect::Config> config_;
 std::map<uint16_t,uint16_t> sources_;
 uint16_t source(uint16_t weapon)const{auto i=sources_.find(weapon);return i==sources_.end()?weapon:i->second;}
 uint64_t epoch_=0,scene_=0,played_=0,exploded_=0,lastNow_=0;
 bool established_=false;
 static bool finite(Vec3 p){return std::all_of(p.begin(),p.end(),[](float x){return std::isfinite(x)&&std::abs(x)<1000000;});}
 void expire(uint64_t now){
  if(now<lastNow_){pulses_.clear();lastNow_=now;return;}lastNow_=now;
  std::erase_if(pulses_,[&](const Pulse&p){return now>=p.born&&now-p.born>=p.lifetime;});
 }
 void add(DynamicPointLight light,uint64_t now,uint64_t lifetime,uint64_t delay=0,weapon_effect::Curve curve={{0,1},{1,0}},Identity owner={},uint32_t life=0){
  expire(now);
  if(pulses_.size()>=maximum_dynamic_lights)pulses_.erase(pulses_.begin());
  pulses_.push_back({light,now+delay,lifetime,std::move(curve),owner,life});
 }
public:
 void configure(std::shared_ptr<const weapon_effect::Config> config){config_=std::move(config);pulses_.clear();}
 void effect_source(uint16_t weapon,uint16_t legacy){sources_[weapon]=legacy;}
 void clear(){pulses_.clear();seen_.clear();epoch_=scene_=played_=exploded_=lastNow_=0;established_=false;}
 // The first snapshot establishes a watermark: joining/changing scene cannot
 // replay historical flashes. Repeated snapshots do not consume fresh events.
 void synchronize(uint64_t epoch,uint64_t scene,uint64_t watermark,uint64_t now){
  if(!epoch){clear();return;}
  if(!established_||epoch!=epoch_||scene!=scene_){clear();established_=true;epoch_=epoch;scene_=scene;played_=watermark;}
  expire(now);
 }
 void dispatch(std::span<const Event>events,const Snapshot&state,uint64_t now){
  expire(now);if(!established_||state.epoch!=epoch_)return;
  std::erase_if(pulses_,[&](const Pulse&p){if(!p.life)return false;return p.owner.slot>=state.players.size()||!state.players[p.owner.slot]||state.players[p.owner.slot]->identity!=p.owner||state.players[p.owner.slot]->life!=p.life;});
  for(const auto&e:events){
   if(e.epoch!=epoch_||!e.id||e.id<=played_||e.id>state.eventWatermark||!seen_.insert(e.id).second)continue;
   if(seen_.size()>4096){played_=*seen_.begin();seen_.erase(seen_.begin());}
   if(e.kind==EventKind::explosion){const auto w=source(e.weapon);if(auto custom=config_?config_->light(e.weapon,"explosion"):nullptr){if(custom->enabled&&finite(e.position))add({e.position,custom->color,custom->radius,custom->intensity},now,custom->durationMs,custom->delayMs,custom->intensityCurve);}else if(w!=55&&w!=63&&w!=65&&w!=67)explosion(e.epoch,e.id,e.position,now);continue;}
   if(e.kind!=EventKind::shot||(!particles::has_muzzle(source(e.weapon))&&!(config_&&config_->light(e.weapon,"shot")))||e.source.slot>=24||!finite(e.position))continue;
   const auto&p=state.players[e.source.slot];
   if(!p||p->identity!=e.source||!e.sourceLife||p->life!=e.sourceLife)continue;
   // Runtime resolves the original weapon CNP muzzle after skinning.
   if(auto custom=config_?config_->light(e.weapon,"shot"):nullptr){if(custom->enabled)add({e.position,custom->color,custom->radius,custom->intensity},now,custom->durationMs,custom->delayMs,custom->intensityCurve,e.source,e.sourceLife);}else add({e.position,{1.f,.64f,.25f},2600.f,2.f},now,80,0,{{0,1},{1,0}},e.source,e.sourceLife);
  }
 }
 // Called for the explicit HOST explosion event; impact/damage are not blasts.
 bool explosion(uint64_t epoch,uint64_t event,Vec3 origin,uint64_t now){
  expire(now);if(!established_||epoch!=epoch_||!event||event<=exploded_||!finite(origin))return false;
  exploded_=event;add({origin,{1.f,.42f,.12f},6500.f,3.f},now,350);return true;
 }
 std::vector<DynamicPointLight> sample(uint64_t now){
  expire(now);std::vector<DynamicPointLight> out;out.reserve(pulses_.size());
  for(const auto&p:pulses_){if(now<p.born)continue;auto light=p.light;light.intensity*=weapon_effect::curve(p.curve,float(now-p.born)/float(p.lifetime));if(light.intensity>0)out.push_back(light);}return out;
 }
};
}
