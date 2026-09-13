#include "native_radio_context.h"
#include "preset_radio_wire.h"
#include "stage_profiles.h"
namespace mgo2win::radio {
namespace detail {
Context build(uint64_t epoch,const host::LoadRequest* request,const combat::Snapshot* snapshot,
              const combat::wire::Preparation* preparation,std::span<const Identity> identities,bool active){
 Context c;c.epoch=epoch;if(!epoch||identities.size()>24)return c;
 const bool scope=request&&stage::runtime_stage_supported(request->rotation.map)&&request->rotation.rule<=1&&!request->rotation.flags;
 const bool prepared=preparation&&preparation->epoch==epoch&&request&&preparation->generation==request->generation;
 const bool live=scope&&active&&snapshot&&snapshot->epoch==epoch&&prepared&&preparation->runtimeReady&&preparation->phase==combat::wire::RoundPhase::active;
 for(auto id:identities){
  if(id.slot>=24||!id.instance||!id.character){c.members.clear();return c;}
  for(const auto& old:c.members)if(old.identity.slot==id.slot||old.identity.character==id.character){c.members.clear();return c;}
  Member m{id};m.freeForAll=scope&&request->rotation.rule==0;
  const auto* p=snapshot&&snapshot->epoch==epoch&&snapshot->players[id.slot]&&snapshot->players[id.slot]->identity==id?&*snapshot->players[id.slot]:nullptr;
  const auto* r=prepared&&preparation->players[id.slot]&&preparation->players[id.slot]->id==id?&*preparation->players[id.slot]:nullptr;
  if(p){m.life=p->life;m.team=p->team<=2?p->team:0;}else if(r){m.team=r->team<=2?r->team:0;}
  m.eligible=live&&p&&r&&p->alive&&p->life&&r->life==p->life&&r->loaded&&r->deployed&&(m.freeForAll?p->team==0:p->team>=1&&p->team<=2)&&r->team==p->team;
  c.members.push_back(m);
 }
 return c;
}
}
Context client_context(const host::Result& r){
 if(r.stage!=host::Stage::joined||!r.combat_offer||!r.roster.complete)return {};
 std::vector<Identity> ids;for(const auto& p:r.roster.slots)if(p)ids.push_back({p->slot,p->instance,p->character});
 return detail::build(r.combat_offer->epoch,r.match.request?&*r.match.request:nullptr,r.combat_state?&*r.combat_state:nullptr,
              r.preparation?&*r.preparation:nullptr,ids,r.combat_status==combat::wire::Status::active);
}
bool same_team(const Member& s,const Member& r,uint8_t preset) noexcept {
 return s.eligible&&r.eligible&&s.life&&r.life&&((s.freeForAll&&r.freeForAll&&s.team==0&&r.team==0)||(!s.freeForAll&&!r.freeForAll&&s.team>=1&&s.team<=2&&s.team==r.team))&&mgo2::radio::wire::reviewed_id(preset);
}
}
