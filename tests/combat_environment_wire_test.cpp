#include "combat_service.h"
#include "gekko_test_profiles.h"
#include <iostream>
#include <limits>
using namespace mgo2mt;using namespace mgo2mt::combat;
namespace {void check(bool v,const char*m){if(!v)throw std::runtime_error(m);}Identity id(unsigned n){return {uint8_t(n),uint16_t(n+1),100+n};}
template<class F>bool rejects(F f){try{f();}catch(const wire::Invalid&){return true;}return false;}}
int main(){try{
 auto world=std::make_shared<const stage::Collision>(stage::Collision::make({{-50000,0,-50000},{-50000,0,50000},{50000,0,50000},{50000,0,-50000}},{{{0,1,2}},{{0,2,3}}}));
 Weapon gun;gun.id=25;gun.damage=100;gun.intervalMs=100;gun.reloadMs=1000;gun.magazine=30;gun.reserve=90;gun.range=20000;
 Policy policy;policy.freeForAll=true;Service service(1,policy);service.configure(world,gekko_test_profiles(gun));
 for(unsigned n=0;n<24;++n){Pose pose;pose.feet={float(n%6)*2200,2,float(n/6)*2200};check(service.authority().join(id(n),0,pose,1000,1000,std::array<uint16_t,1>{25},0),"24 admitted bodies");check(service.admit(id(n))&&service.receive(id(n),wire::encode(wire::Accept{1}),0),"24 accepted peers");check(service.authority().assign_special(id(n),special_pc::Kind::gekko,true,0)==Reject::none,"24 full special forms");}
 service.authority().active(true);service.deliveries();
 burning::Source source{{1,0,1,100,1},53,0,0};check(bool(service.authority().explode({source,1,{5000,1000,3000},20000,0,true},0)),"HOST ignites all forms");
 auto baseline=service.authority().snapshot();for(const auto&p:baseline.players)check(p&&p->burning&&p->maxHp==1000&&p->hp==1000,"burning snapshot uses current 1000-HP Gekko baseline");
 std::array<Replica,24> replicas;for(auto&r:replicas)check(r.snapshot(baseline),"replica initialized before damage");
 service.poll(1000);auto packets=service.deliveries();std::array<unsigned,24> counts{};size_t maximum=0;
 for(const auto&delivery:packets){maximum=(std::max)(maximum,delivery.payload.size());check(delivery.payload.size()<=2000,"full 24-player environment record fits wire bound");auto record=wire::decode(delivery.payload);if(auto*f=std::get_if<wire::Frame>(&record)){check(replicas[delivery.recipient.slot].snapshot(f->snapshot),"new snapshot valid");auto events=replicas[delivery.recipient.slot].events(f->events);counts[delivery.recipient.slot]+=unsigned(events.size());for(const auto&e:events)check(e.kind==EventKind::damage&&e.hpDamage==50&&e.hp==950&&e.source==id(0)&&e.sourceLife==1,"one second maxHP-five-percent source-scoped damage");}}
 for(auto count:counts)check(count==24,"all damage events delivered exactly once to all 24 peers");
 auto frame=wire::Frame{service.authority().snapshot(),wire::Status::active,{},service.authority().sop_view(id(0)).value()};
 for(auto&p:frame.snapshot.players){p->specialPc.action=special_pc::Action::jump;p->specialPc.serial=7;p->specialPc.elapsedMs=1000;}
 Event event;event.epoch=1;event.id=frame.snapshot.eventWatermark;event.source=id(0);event.weapon=50;event.kind=EventKind::projectile;event.normal={0,0,1};frame.events={event};
 auto bytes=wire::encode(frame);check(bytes.size()<=2000&&std::get<wire::Frame>(wire::decode(bytes))==frame,"24 burning special action states plus projectile roundtrip");
 frame.events[0].kind=EventKind::projectileTrail;check(std::get<wire::Frame>(wire::decode(wire::encode(frame)))==frame,"trail is distinct event kind");
 for(auto&p:frame.snapshot.players){p->specialPc={};p->pose.capsule={260,1700,2};p->weapon=25;p->ammo=10;p->reserve=50;p->ladderAnchor=65000;}
 check(wire::encode(frame).size()<=2000,"24 attached burning humans plus event");
 for(auto&p:frame.snapshot.players){p->ladderAnchor=0;p->reloadUntil=std::numeric_limits<uint64_t>::max();}
 check(wire::encode(frame).size()<=2000&&std::get<wire::Frame>(wire::decode(wire::encode(frame)))==frame,"24 nonzero uint64 reload deadlines retain exact values");
 auto invalid=frame;invalid.snapshot.players[0]->alive=false;invalid.snapshot.players[0]->hp=0;check(rejects([&]{wire::encode(invalid);}),"dead burning invalid");
 invalid=frame;invalid.snapshot.players[0]->ladderAnchor=1;check(rejects([&]{wire::encode(invalid);}),"ladder reload cannot coexist");
 bytes[5]=16;check(!wire::recognized(bytes)&&rejects([&]{wire::decode(bytes);}),"old version cannot consume changed layout");
 wire::Input input;input.epoch=1;input.weapon=25;input.ladder={ladder::Action::enter,23,.5f};check(std::get<wire::Input>(wire::decode(wire::encode(input)))==input,"ladder float axis lossless input");auto newer=input;newer.sequence=2;newer.ladder={};auto coalesced=wire::coalesce_input(input,newer);check(coalesced.ladder==input.ladder,"one pending enter retained");newer.suspended=true;check(wire::coalesce_input(input,newer).ladder==ladder::Intent{},"suspend cancels pending enter");input.fire=true;check(rejects([&]{wire::encode(input);}),"ladder cannot also fire");input.fire=false;input.ladder.axis=std::numeric_limits<float>::quiet_NaN();check(rejects([&]{wire::encode(input);}),"nonfinite axis rejected");
 std::cout<<"GWCB18 / 24 burn recipients / exact damage FIFO / ladder / full special and reload bounds PASS max="<<maximum<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
