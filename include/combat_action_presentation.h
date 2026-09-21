#pragma once
#include "combat_authority.h"
namespace mgo2mt::combat::presentation {
class Actions {
 struct Action {Identity identity;uint32_t life=0;uint16_t weapon=0;uint64_t id=0,at=0;bool melee=false;};
 uint64_t epoch_=0,cursor_=0;std::array<Action,24> actions_{};
public:
 void update(const Snapshot& state,std::span<const Event> events,uint64_t now){
  if(epoch_!=state.epoch){epoch_=state.epoch;cursor_=0;actions_={};}
  for(unsigned i=0;i<24;++i){auto&a=actions_[i];const auto&p=state.players[i];if(!p||!p->alive||p->stunned||p->identity!=a.identity||p->life!=a.life||p->weapon!=a.weapon||now<a.at)a={};}
  uint64_t newest=cursor_;
  for(const auto&e:events){if(e.epoch!=epoch_||!e.id||e.id<=cursor_)continue;newest=std::max(newest,e.id);if(e.source.slot>=24||(e.kind!=EventKind::shot&&e.kind!=EventKind::projectile&&e.kind!=EventKind::melee))continue;
   const auto&p=state.players[e.source.slot];if(!p||!p->alive||p->stunned||p->identity!=e.source||p->life!=e.sourceLife||p->weapon!=e.weapon)continue;
   auto&a=actions_[e.source.slot];if(e.id<=a.id)continue;a={e.source,e.sourceLife,e.weapon,e.id,now,e.kind==EventKind::melee};
  }
  cursor_=newest;
 }
 double seconds(const Player&p,bool melee,uint64_t now)const{
  if(p.identity.slot>=24||!p.alive||p.stunned||p.reloadUntil)return -1;const auto&a=actions_[p.identity.slot];
  if(!a.id||a.identity!=p.identity||a.life!=p.life||a.weapon!=p.weapon||a.melee!=melee||now<a.at||now-a.at>10000)return -1;
  return double(now-a.at)*original::nominal_motion_fps/60000.;
 }
 uint64_t serial(const Player&p)const{return p.identity.slot<24?actions_[p.identity.slot].id:0;}
};
}
