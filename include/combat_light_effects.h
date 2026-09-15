#pragma once
#include "combat_authority.h"
#include "dynamic_light.h"
#include <algorithm>
#include <cmath>
#include <optional>
namespace mgo2win::combat {
// Presentation policy in decoded millimetres. Event arrival time is used because
// GWCB v5 has no original muzzle timestamp/bone transform. No damage is generated.
class LightEffects {
 struct Pulse {DynamicPointLight light;uint64_t born=0,lifetime=0;};
 std::vector<Pulse> pulses_;
 uint64_t epoch_=0,scene_=0,played_=0,exploded_=0,lastNow_=0;
 bool established_=false;
 static bool finite(Vec3 p){return std::all_of(p.begin(),p.end(),[](float x){return std::isfinite(x)&&std::abs(x)<1000000;});}
 void expire(uint64_t now){
  if(now<lastNow_){pulses_.clear();lastNow_=now;return;}lastNow_=now;
  std::erase_if(pulses_,[&](const Pulse&p){return now-p.born>=p.lifetime;});
 }
 void add(DynamicPointLight light,uint64_t now,uint64_t lifetime){
  expire(now);
  if(pulses_.size()>=maximum_dynamic_lights)pulses_.erase(pulses_.begin());
  pulses_.push_back({light,now,lifetime});
 }
public:
 void clear(){pulses_.clear();epoch_=scene_=played_=exploded_=lastNow_=0;established_=false;}
 // The first snapshot establishes a watermark: joining/changing scene cannot
 // replay historical flashes. Repeated snapshots do not consume fresh events.
 void synchronize(uint64_t epoch,uint64_t scene,uint64_t watermark,uint64_t now){
  if(!epoch){clear();return;}
  if(!established_||epoch!=epoch_||scene!=scene_){clear();established_=true;epoch_=epoch;scene_=scene;played_=watermark;}
  expire(now);
 }
 void dispatch(std::span<const Event>events,const Snapshot&state,uint64_t now){
  expire(now);if(!established_||state.epoch!=epoch_)return;
  for(const auto&e:events){
   if(e.epoch!=epoch_||!e.id||e.id<=played_||e.id>state.eventWatermark)continue;
   played_=e.id;
   if(e.kind!=EventKind::shot||(e.weapon!=25&&e.weapon!=2&&e.weapon!=128&&e.weapon!=129)||e.source.slot>=24||!finite(e.position))continue;
   const auto&p=state.players[e.source.slot];
   if(!p||p->identity!=e.source||!e.sourceLife||p->life!=e.sourceLife)continue;
   // Runtime resolves the original AK CNP muzzle after skinning, before dispatch.
   add({e.position,{1.f,.64f,.25f},2600.f,2.f},now,80);
  }
 }
 // Local effect interface for a future validated explosion producer. There is
 // no explosion event on the current combat wire: impact/damage are not blasts.
 bool explosion(uint64_t epoch,uint64_t event,Vec3 origin,uint64_t now){
  expire(now);if(!established_||epoch!=epoch_||!event||event<=exploded_||!finite(origin))return false;
  exploded_=event;add({origin,{1.f,.42f,.12f},6500.f,3.f},now,350);return true;
 }
 std::vector<DynamicPointLight> sample(uint64_t now){
  expire(now);std::vector<DynamicPointLight> out;out.reserve(pulses_.size());
  for(const auto&p:pulses_){auto light=p.light;light.intensity*=1.f-float(now-p.born)/float(p.lifetime);out.push_back(light);}return out;
 }
};
}
