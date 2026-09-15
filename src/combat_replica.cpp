#include "combat_authority.h"
#include "combat_sop_view.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace mgo2win::combat {
namespace {
bool valid(Identity id){return id.slot<24&&id.instance&&id.character;}
bool finite(Vec3 a){return std::all_of(a.begin(),a.end(),[](float f){return std::isfinite(f)&&std::abs(f)<1000000;});}
bool valid(const Pose&p){return finite(p.feet)&&std::isfinite(p.yaw)&&std::abs(p.yaw)<=3.14159274f&&std::isfinite(p.pitch)&&std::abs(p.pitch)<=1.4f&&(p.capsule.radius==260||p.capsule.radius==350||p.capsule.radius==800)&&p.capsule.skin==2&&p.capsule.height>=p.capsule.radius*2&&(p.capsule.height==1700||p.capsule.height==1100||p.capsule.height==560||p.capsule.height==4200);}
}
bool Replica::snapshot(const Snapshot&s){
 if(!s.epoch||!s.revision)return false;std::set<uint32_t> ids;
 for(size_t i=0;i<s.players.size();++i)if(const auto&p=s.players[i])if(!valid(p->identity)||p->identity.slot!=i||!ids.insert(p->identity.character).second||!valid(p->pose)||!p->maxHp||p->maxHp>1000000||!p->maxStamina||p->maxStamina>1000000||p->hp>p->maxHp||p->stamina>p->maxStamina||p->alive!=(p->hp!=0)||p->stunned!=(p->alive&&p->stamina==0)||p->team>2||!p->life||(!p->weapon&&(p->ammo||p->reserve||p->reloadUntil))||p->ammo>1000||p->reserve>10000||p->oxygen>water_gameplay::Oxygen::full||!valid_skills(*p)||!valid_special(*p)||!valid_evade(*p)||!valid_cover(*p)||!valid_special_pc(*p)||(p->burning&&!p->alive)||(p->ladderAnchor&&(!p->alive||p->stunned||p->specialPc.kind!=special_pc::Kind::human||p->pose.capsule.height!=1700||p->reloadUntil||p->cover.attached||p->cover.lean||p->evadeKind!=EvadeKind::none||p->specialPhase!=SpecialPhase::none)))return false;
 if(state_){if(s.epoch<state_->epoch)return false;if(s.epoch==state_->epoch){if(s.revision<state_->revision||s.eventWatermark<state_->eventWatermark)return false;for(unsigned i=0;i<24;++i)if(s.players[i]&&state_->players[i]&&s.players[i]->identity==state_->players[i]->identity&&s.players[i]->life<state_->players[i]->life)return false;if(s.revision==state_->revision)return s==*state_;}}
 if(!state_||s.epoch!=state_->epoch)played_=s.eventWatermark;state_=s;return true;
}
std::vector<Event> Replica::events(std::span<const Event>events){
 std::vector<Event> out;if(!state_||events.size()>128)return out;uint64_t cursor=played_;
 for(const auto&e:events){if(e.epoch!=state_->epoch||!e.id||!valid(e.source)||!finite(e.position)||!finite(e.normal)||!e.sourceLife||(e.target.slot<24&&!e.targetLife)||unsigned(e.kind)>unsigned(EventKind::itemPickup)||!valid_shot_distance(e))return {};
  if(e.id<=cursor)continue;if(e.id!=cursor+1)return {};cursor=e.id;auto current=[&](Identity id,uint32_t life){return id.slot<24&&state_->players[id.slot]&&state_->players[id.slot]->identity==id&&state_->players[id.slot]->life==life;};if(current(e.source,e.sourceLife)&&(e.target.slot>=24||current(e.target,e.targetLife)))out.push_back(e);
 }played_=cursor;return out;
}
}

