#include "combat_service.h"
#include "combat_cycle.h"
#include <iostream>
#include <limits>
using namespace mgo2mt;
namespace cw=combat::wire;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
std::shared_ptr<const stage::Collision> floor_world(){return std::make_shared<stage::Collision>(stage::Collision::make({{-10000,0,-10000},{-10000,0,10000},{10000,0,10000},{10000,0,-10000}},{{{0,1,2}},{{0,2,3}}}));}
combat::Identity a{1,257,100},b{2,258,200};
cw::Command command(combat::Identity id,uint32_t seq,cw::CommandKind kind){cw::Command c;c.epoch=1;c.sequence=seq;c.kind=kind;return c;}
combat::Weapon weapon(){combat::Weapon w{25,1000,0,10,100,3,9,10000,0,0,true};w.reloadRefillMs=50;return w;}
std::unique_ptr<combat::Service> prepare(std::shared_ptr<const weapons::Catalog>catalog,uint32_t duration=100){
 auto s=std::make_unique<combat::Service>(1);s->configure(floor_world(),std::array{weapon()});combat::RoundCoordinator::Policy p{60000,1,false,true};p.roundDurationMs=duration;p.endOnTimeout=true;p.respawnDelayMs=0;
 s->configure_round(p,catalog,[](combat::Authority& authority,combat::Identity id,uint8_t team,std::span<const uint16_t> inv,uint64_t now){combat::Pose p;p.feet={0,0,id.slot==1?0.f:3000.f};return authority.join(id,team,p,1000,1000,inv,now);});
 for(auto id:{a,b}){check(s->admit(id,0)&&s->receive(id,cw::encode(cw::Accept{1}),0),"admit/accept");auto c=command(id,1,cw::CommandKind::loaded);c.enabled=true;c.generation=1;c.sceneRevision=1;check(s->receive(id,cw::encode(c),0),"loaded");c=command(id,2,cw::CommandKind::ready);c.enabled=true;check(s->receive(id,cw::encode(c),0),"ready");}
 s->poll(10);check(s->take_round_start(),"start at 10");for(auto id:{a,b}){auto c=command(id,3,cw::CommandKind::loadout);c.weapons={25,0,0};check(s->receive(id,cw::encode(c),11),"grant");}s->deliveries();return s;
}
int main(int argc,char**argv){try{
 check(argc==2,"catalog required");auto catalog=std::make_shared<weapons::Catalog>();std::string error;check(catalog->load(argv[1],error),"catalog");
 auto s=prepare(catalog);cw::Input input;input.epoch=1;input.sequence=1;input.weapon=25;input.pose=s->authority().snapshot().players[a.slot]->pose;input.fire=input.firePressed=true;
 check(s->receive(a,cw::encode(input),109),"held input accepted one millisecond before expiry");auto before=s->authority().snapshot();s->poll(110);check(s->ended()&&!s->authority().active()&&s->current_status()==cw::Status::ended&&s->authority().snapshot()==before,"expiry precedes pending held fire and authority advance");
 check(!s->receive(a,cw::encode(input),111),"ended rejects input");auto c=command(a,4,cw::CommandKind::loadout);c.weapons={25,0,0};check(s->receive(a,cw::encode(c),111)&&s->preparation(a,111)->error==cw::CommandError::phase,"ended command receives explicit phase rejection");s->poll(0);s->poll(999999);check(s->authority().snapshot()==before&&!s->authority().active(),"clock rollback and repeated poll cannot unend or refill");
 auto prep=*s->preparation(a,112);check(prep.phase==cw::RoundPhase::ended&&prep.roundClock&&!prep.roundRemainingMs&&!prep.countdown,"explicit ended snapshot");check(std::get<cw::Preparation>(cw::decode(cw::encode(prep)))==prep,"ended wire roundtrip");auto bytes=cw::encode(prep);bytes[5]=4;check(!cw::recognized(bytes),"GWCB v4 cannot decode v6 lifecycle");
 auto bad=prep;bad.roundRemainingMs=1000;bool rejected=false;try{cw::encode(bad);}catch(const cw::Invalid&){rejected=true;}check(rejected,"ended cannot carry positive timer");
 s=prepare(catalog);auto shot=s->authority().fire(a,{1,1,25,{0,0,1},1},109);check(bool(shot)&&!s->authority().snapshot().players[b.slot]->alive,"one millisecond before expiry can fire");s->poll(110);check(s->preparation(b,110)->players[b.slot]->life==1&&s->preparation(b,110)->players[b.slot]->deployed,"expiry takes precedence over zero-delay death respawn");
 s=prepare(catalog);check(bool(s->authority().fire(a,{1,1,25,{0,0,1},1},20)),"consume magazine");check(bool(s->authority().reload(a,1,80,1)),"start reload before expiry");s->poll(110);before=s->authority().snapshot();s->poll(200);check(s->authority().snapshot()==before&&before.players[a.slot]->ammo==2&&before.players[a.slot]->reserve==9,"reload refill scheduled after end stays frozen");
 s=prepare(catalog);c=command(b,4,cw::CommandKind::loadout);c.weapons={25,0,0};check(s->receive(b,cw::encode(c),110)&&s->ended()&&s->preparation(b,110)->error==cw::CommandError::phase,"command expiry cannot wait for later poll");
 s=prepare(catalog,0);s->poll(100000);check(!s->ended()&&s->authority().active(),"unknown duration never expires");
 host::LoadRequest request{255,255,0,255,{20,1,0},host::MatchTransition::next_round};auto next=combat::next_cycle_request(request);check(next&&next->sequence==256&&next->generation==1&&next->round==0&&next->transition==host::MatchTransition::next_round,"native byte wrap skips reserved generation zero");request.sequence=std::numeric_limits<uint64_t>::max();check(!combat::next_cycle_request(request),"epoch identity overflow cannot reuse old epoch");request.sequence=1;request.rotation.flags=2;check(!combat::next_cycle_request(request),"DP and unsupported rotations cannot use native repetition");
 std::cout<<"GWCB v6 expiry-before-fire/reload/respawn/grant, rollback, unknown clock, ended wire and generation wrap passed\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
